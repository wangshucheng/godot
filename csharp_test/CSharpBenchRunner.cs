using Godot;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using CSharpBench.Core;

// ============================================================
// CSharpBench Godot 宿主（Mono 6.12 内嵌运行时）
//
// - 复用 CSharpBench.Core 工作负载（与 .NET 控制台宿主同一份代码）
// - 多帧状态机执行：每帧限量单元，避免阻塞渲染（WASM/移动端友好）
// - 冷启动：先跑一遍全部负载首调（size=100，无预热）
// - 热启动：RunnerCore 自适应测量（warmup 3 + ≥5 次迭代且 ≥150ms）
// - JSON 每单元增量落盘，引擎退出阶段崩溃不丢数据
//
// Mono 运行时崩溃自愈机制（2026-08-19 修复）:
//   Mono 6.12 部分反射路径（PropertyInfo.GetValue warm 循环等）触发
//   进程级运行时崩溃，try/catch 不可捕获。本宿主实现三层防御:
//   1) 断点续跑: 启动时读取既有 JSON，已完成单元原样携带、不重测
//   2) CSBENCH_SKIP: 指定 "Category" 或 "Category/Name" 直接标 skip
//   3) manifest: 启动时写出全部计划单元清单，供驱动脚本对比缺失行
//      并自动二分（崩溃项加入 skip 重跑，直至收敛或全部标注）
//
// 环境变量:
//   CSBENCH_CATEGORIES  类目过滤（逗号分隔）
//   CSBENCH_PASS        输出文件后缀（分趟名）
//   CSBENCH_MAXSIZE     规模上限（Mono 大数组路径规避）
//   CSBENCH_SKIP        跳过项（"Category" 或 "Category/Name"，逗号分隔）
//   CSBENCH_FRESH       置 1 时忽略既有 JSON（强制全量重测）
//
// 场景: res://csbench_test.tscn
// ============================================================
public partial class CSharpBenchRunner : Node
{
    private sealed class Unit
    {
        public Workload W; public int Size; public bool Cold;
        public Unit(Workload w, int size, bool cold) { W = w; Size = size; Cold = cold; }
    }

    private List<Unit> _units;          // 本进程待执行单元
    private int _idx;
    private readonly List<Unit> _coldDone = new List<Unit>();
    private readonly List<Unit> _warmDone = new List<Unit>();
    private readonly Dictionary<Unit, Measured> _results = new Dictionary<Unit, Measured>();
    private readonly Dictionary<Unit, string> _skipReasons = new Dictionary<Unit, string>();
    private readonly TimeSpan _procCpu0 = Process.GetCurrentProcess().TotalProcessorTime;
    private string _outPath, _coldPath, _manifestPath;
    private int _phase; // 0=running 2=done
    private bool _initialized;

    // 上一进程已完成单元的原始行（断点续跑携带）
    private readonly List<string> _carriedWarm = new List<string>();
    private readonly List<string> _carriedCold = new List<string>();
    private readonly HashSet<string> _doneWarm = new HashSet<string>(StringComparer.Ordinal);
    private readonly HashSet<string> _doneCold = new HashSet<string>(StringComparer.Ordinal);

    // Godot 宿主跳过集：await Task.Yield() 的 continuation 经 GodotSynchronizationContext
    // 需要主线程泵送，而宿主线程正阻塞在 GetAwaiter().GetResult() → 必然死锁。
    // （.NET 控制台宿主无同步上下文，该项正常执行）
    private static readonly HashSet<string> GodotSkip =
        new HashSet<string> { "Await_TaskYield" };

    // CSBENCH_SKIP 解析结果（类名集 + 类/名对集）
    private readonly HashSet<string> _skipCats = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
    private readonly HashSet<string> _skipItems = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

    public override void _Ready()
    {
        // 此引擎构建中 _Ready 会被调用两次（id=27 预就绪 + id=13 READY），需幂等守卫
        if (_initialized) return;
        _initialized = true;

        GD.Print("[csbench-godot] runtime=" + RuntimeInformation.FrameworkDescription);

        ParseSkipList();

        var workloads = Workloads.All;
        // 类目过滤（CSBENCH_CATEGORIES，逗号分隔）
        string cats = System.Environment.GetEnvironmentVariable("CSBENCH_CATEGORIES");
        if (!string.IsNullOrEmpty(cats))
        {
            var set = new HashSet<string>(cats.Split(','), StringComparer.OrdinalIgnoreCase);
            workloads = Array.FindAll(workloads, w => set.Contains(w.Category));
        }
        GD.Print("[csbench-godot] workloads=" + workloads.Length
            + " loadErrors=" + (Workloads.LoadErrors.Count > 0
                ? string.Join("; ", Workloads.LoadErrors.ConvertAll(e => e.Item1 + ": " + e.Item2).ToArray()) : "0"));

        // 规模上限（CSBENCH_MAXSIZE）：Mono 大规模数组路径规避
        int maxSize = 0;
        try { string ms = System.Environment.GetEnvironmentVariable("CSBENCH_MAXSIZE"); if (ms != null) maxSize = int.Parse(ms); } catch {}
        if (maxSize > 0) GD.Print("[csbench-godot] maxSize cap=" + maxSize);

        // 输出文件名（CSBENCH_PASS 分趟后缀）
        string pass = System.Environment.GetEnvironmentVariable("CSBENCH_PASS");
        string suffix = string.IsNullOrEmpty(pass) ? "" : "_" + pass;
        string dir = Directory.GetCurrentDirectory();
        _outPath = Path.Combine(dir, "csbench_godot" + suffix + ".json");
        _coldPath = Path.Combine(dir, "csbench_godot" + suffix + "_cold.json");
        _manifestPath = Path.Combine(dir, "csbench_godot" + suffix + "_manifest.txt");

        // ---- 断点续跑：加载既有 JSON，收集已完成单元 ----
        bool fresh = System.Environment.GetEnvironmentVariable("CSBENCH_FRESH") == "1";
        if (!fresh)
        {
            LoadExistingRows(_outPath, _carriedWarm, _doneWarm);
            LoadExistingRows(_coldPath, _carriedCold, _doneCold);
        }
        if (_doneWarm.Count > 0 || _doneCold.Count > 0)
            GD.Print($"[csbench-godot] resume: carry warm={_doneWarm.Count} cold={_doneCold.Count}");

        // ---- 构建单元清单 + manifest ----
        _units = new List<Unit>();
        var manifest = new List<string>();
        foreach (var w in workloads)
        {
            if (w.Run == null) continue;
            // 冷启动单元（全部负载 size=100 首调）
            manifest.Add("cold|" + w.Name + "|100");
            if (!_doneCold.Contains(w.Name + "|100"))
                _units.Add(new Unit(w, 100, true));
            // 热启动单元
            foreach (int s in w.Sizes)
            {
                if (maxSize > 0 && s > maxSize) continue;
                manifest.Add("warm|" + w.Name + "|" + s);
                if (!_doneWarm.Contains(w.Name + "|" + s))
                    _units.Add(new Unit(w, s, false));
            }
        }
        try { File.WriteAllLines(_manifestPath, manifest.ToArray()); } catch {}
        GD.Print($"[csbench-godot] planned={manifest.Count} todoUnits={_units.Count} (resume-aware)");
    }

    private void ParseSkipList()
    {
        string s = System.Environment.GetEnvironmentVariable("CSBENCH_SKIP");
        if (string.IsNullOrEmpty(s)) return;
        foreach (var tok in s.Split(','))
        {
            string t = tok.Trim();
            if (t.Length == 0) continue;
            int k = t.IndexOf('/');
            if (k < 0) _skipCats.Add(t);
            else _skipItems.Add(t);
        }
        if (_skipCats.Count + _skipItems.Count > 0)
            GD.Print("[csbench-godot] skip-list: " + s);
    }

    private bool IsSkipRequested(string cat, string name)
    {
        return _skipCats.Contains(cat) || _skipItems.Contains(cat + "/" + name);
    }

    /// <summary>读取既有 JSON 的原始行与 (name|size) 完成集。行内无嵌套对象，按 {...} 扫描安全。</summary>
    private static void LoadExistingRows(string path, List<string> rawRows, HashSet<string> doneKeys)
    {
        if (!File.Exists(path)) return;
        try
        {
            string txt = File.ReadAllText(path);
            int i = txt.IndexOf("\"results\":[");
            if (i < 0) return;
            i += "\"results\":[".Length;
            int end = txt.LastIndexOf(']');
            if (end <= i) return;
            string body = txt.Substring(i, end - i);
            int start = -1;
            for (int k = 0; k < body.Length; k++)
            {
                char c = body[k];
                if (c == '{') start = k;
                else if (c == '}' && start >= 0)
                {
                    string row = body.Substring(start, k - start + 1);
                    start = -1;
                    string key = ExtractRowKey(row);
                    if (key != null)
                    {
                        rawRows.Add(row);
                        doneKeys.Add(key);
                    }
                }
            }
        }
        catch (Exception ex) { GD.Print("[csbench-godot] resume load error: " + ex.Message); }
    }

    /// <summary>从原始行提取 "name|size" 键（行格式由 BenchJsonWriter 产生）。</summary>
    private static string ExtractRowKey(string row)
    {
        try
        {
            const string nameTag = "\"name\":\"";
            int a = row.IndexOf(nameTag, StringComparison.Ordinal);
            if (a < 0) return null;
            a += nameTag.Length;
            int b = row.IndexOf('"', a);
            if (b < 0) return null;
            string name = row.Substring(a, b - a);
            const string sizeTag = "\"size\":";
            int c = row.IndexOf(sizeTag, b, StringComparison.Ordinal);
            if (c < 0) return null;
            c += sizeTag.Length;
            int d = c;
            while (d < row.Length && (char.IsDigit(row[d]) || row[d] == '-')) d++;
            int size = int.Parse(row.Substring(c, d - c));
            return name + "|" + size;
        }
        catch { return null; }
    }

    public override void _Process(double delta)
    {
        if (_phase == 2) return;

        // 每帧执行若干单元：轻单元多做几个，重单元让出帧
        long frameBudgetTicks = Stopwatch.Frequency / 5; // ~200ms/帧
        var frameSw = Stopwatch.StartNew();

        while (_idx < _units.Count && frameSw.ElapsedTicks < frameBudgetTicks)
        {
            var u = _units[_idx];

            if (GodotSkip.Contains(u.W.Name))
            {
                _skipReasons[u] = "skipped-in-godot (sync-context deadlock)";
                MarkDone(u);
            }
            else if (IsSkipRequested(u.W.Category, u.W.Name))
            {
                // 驱动脚本自动二分指定的崩溃项：诚实标注而非静默缺失
                _skipReasons[u] = "mono-runtime-crash (auto-bisected)";
                MarkDone(u);
            }
            else
            {
                try
                {
                    Measured m = u.Cold ? RunnerCore.MeasureCold(u.W.Run, u.Size)
                                        : RunnerCore.Measure(u.W.Run, u.Size);
                    _results[u] = m;
                    MarkDone(u);
                }
                catch (Exception ex)
                {
                    _skipReasons[u] = ex.GetType().Name;
                    MarkDone(u);
                }
            }
            _idx++;

            if (frameSw.ElapsedTicks > Stopwatch.Frequency / 5) break;
        }

        // 进度与增量落盘（每 4 单元或完成时；Mono 崩溃前排空缓冲）
        if (_idx % 4 == 0 || _idx >= _units.Count)
        {
            Flush();
            if (_idx % 20 == 0 && _idx < _units.Count)
                GD.Print($"[csbench-godot] progress {_idx}/{_units.Count}");
        }

        if (_idx >= _units.Count && _phase < 2)
        {
            _phase = 2;
            var cpu = Process.GetCurrentProcess().TotalProcessorTime;
            int planned = 0;
            try { planned = File.ReadAllLines(_manifestPath).Length; } catch {}
            GD.Print($"[csbench-godot] ALL DONE todo={_units.Count} carriedWarm={_carriedWarm.Count} carriedCold={_carriedCold.Count} plannedTotal={planned} cpuSeconds={(cpu - _procCpu0).TotalSeconds:F1}");
            try { GetTree().Quit(); } catch { }
        }
    }

    private void MarkDone(Unit u)
    {
        if (u.Cold) _coldDone.Add(u); else _warmDone.Add(u);
    }

    private void Flush()
    {
        try
        {
            string rt = RuntimeInformation.FrameworkDescription + " (Godot 4.7 embedded Mono)";
            double cpu = (Process.GetCurrentProcess().TotalProcessorTime - _procCpu0).TotalSeconds;

            var cw = new BenchJsonWriter("godot", rt, "mono6.12-godot", "quick", "cold");
            foreach (var raw in _carriedCold) cw.AddRawRow(raw);   // 断点续跑：历史行原样携带
            foreach (var u in _coldDone)
            {
                Measured m; _results.TryGetValue(u, out m);
                string reason; bool skipped = _skipReasons.TryGetValue(u, out reason);
                cw.AddRow(u.W.Category, u.W.Name, u.Size, m, skipped, reason);
            }
            File.WriteAllText(_coldPath, cw.ToJson());

            var ww = new BenchJsonWriter("godot", rt, "mono6.12-godot", "quick", "warm");
            foreach (var raw in _carriedWarm) ww.AddRawRow(raw);
            foreach (var u in _warmDone)
            {
                Measured m; _results.TryGetValue(u, out m);
                string reason; bool skipped = _skipReasons.TryGetValue(u, out reason);
                ww.AddRow(u.W.Category, u.W.Name, u.Size, m, skipped, reason);
            }
            ww.CpuSeconds = cpu;
            File.WriteAllText(_outPath, ww.ToJson());
        }
        catch (Exception ex)
        {
            GD.Print("[csbench-godot] flush error: " + ex.Message);
        }
    }
}
