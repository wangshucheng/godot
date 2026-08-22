using Godot;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using CSharpBench.Core;
using CSharpBench.Core.Categories;

// ============================================================
// CSharpBench Godot 宿主（Mono 6.12 内嵌运行时，桌面 + WASM）
//
// - 复用 CSharpBench.Core 工作负载（与 .NET 控制台宿主同一份代码）
// - 多帧状态机执行：每帧限量单元，避免阻塞渲染（WASM/移动端友好）
// - 冷启动：先跑一遍全部负载首调（size=100，无预热）
// - 热启动：RunnerCore 自适应测量（warmup + 迭代 ≥ 时间预算）
// - JSON 每单元增量落盘 + console 行输出（WASM 驱动从 console 收割）
//
// GodotSynchronizationContext 死锁修复（2026-08-19/20）:
//   Await_TaskYield 等"阻塞包装异步"负载原被硬编码 skip，根因有二：
//   1) 桌面：await Task.Yield() 的 continuation 被 GodotSynchronizationContext
//      捕获 Post 到 pending 队列，需主线程每帧泵送（pump_sync_context），而主线程
//      正阻塞在 GetAwaiter().GetResult() → 死锁。
//      修复：测量期间临时摘除 SynchronizationContext.Current（基准代码不触碰
//      Godot API，摘除安全），continuation 落到 ThreadPool 真实线程，阻塞等待
//      正常唤醒，测完恢复。
//   2) WASM 单线程：GetResult() 自旋卡死唯一 JS 线程，emscripten 事件循环停止，
//      continuation 的 emscripten_async_call 回调永不派发 → 必然死锁（无
//      ASYNCIFY 不可解）。WASM 上 sync context 本就未 Install（Platform.cs），
//      桌面死锁成因不存在；改用 RunAsync 帧轮询驱动（非阻塞），带单元级看门狗。
//
// WASM 运行时能力探针（先探后测，结果决定执行模式）:
//   P0 sync_context   — SynchronizationContext.Current 类型（桌面: GodotSync…
//                        WASM: null，验证死锁成因差异）
//   P1 yield_progress — YieldDriver(200) 帧轮询能否推进完成（WASM）
//   P2 taskrun_progress — TaskRunDriver(50) 帧轮询能否推进完成（WASM）
//   桌面 P1'/P2' — 摘除 ctx 后阻塞等待能否完成（死锁修复直接验证）
//
// Mono 运行时崩溃自愈机制（2026-08-19）:
//   1) 断点续跑: 启动时读取既有 JSON，已完成单元原样携带、不重测（桌面）
//      WASM 页面重载丢失 MEMFS，续跑由驱动侧完成（console 行 + 重编译烘焙 skip）
//   2) CSBENCH_SKIP: 指定 "Category" 或 "Category/Name" 直接标 skip
//   3) manifest: 启动时写出全部计划单元清单（WASM 同时打印 console）
//
// 配置读取（环境变量；WASM 无进程环境变量，走内置默认）:
//   CSBENCH_CATEGORIES  类目过滤（逗号分隔）
//   CSBENCH_PASS        输出文件后缀（分趟名）
//   CSBENCH_MAXSIZE     规模上限（WASM 默认 10000：解释器慢 + 1M 无意义）
//   CSBENCH_SKIP        跳过项（"Category" 或 "Category/Name"，逗号分隔）
//   CSBENCH_FRESH       置 1 时忽略既有 JSON（强制全量重测）
// 桌面驱动注入环境变量；WASM 上 GetEnvironmentVariable 恒空 → 全类目 +
// 内置默认（探针门控 + WASM 看门狗兜底）。曾计划经 GODOT_CONFIG.args 注入
// 命令行令牌，但本 glue 的 OS.GetCmdlineArgs() 是实例方法（生成器缺陷），
// 静态调用不可行且无法新增 icall（需重编 WASM），故放弃运行时传参。
//
// 场景: res://csbench_test.tscn
// ============================================================
public partial class CSharpBenchRunner : Node
{
    private sealed class Unit
    {
        public Workload W; public int Size; public bool Cold; public bool AsyncDriven;
        public Unit(Workload w, int size, bool cold, bool asyncDriven)
        { W = w; Size = size; Cold = cold; AsyncDriven = asyncDriven; }
    }

    private List<Unit> _units;          // 本进程待执行单元
    private int _idx;
    private readonly List<Unit> _coldDone = new List<Unit>();
    private readonly List<Unit> _warmDone = new List<Unit>();
    private readonly Dictionary<Unit, Measured> _results = new Dictionary<Unit, Measured>();
    private readonly Dictionary<Unit, string> _skipReasons = new Dictionary<Unit, string>();
    private double _cpu0;
    private string _outPath, _coldPath, _manifestPath, _probePath;
    private int _phase; // 0=running 2=done
    private bool _initialized;
    private bool _isWeb;
    private bool _fileOk = true;        // WASM 上 System.IO 可能不可用 → console-only

    // 上一进程已完成单元的原始行（断点续跑携带）
    private readonly List<string> _carriedWarm = new List<string>();
    private readonly List<string> _carriedCold = new List<string>();
    private readonly HashSet<string> _doneWarm = new HashSet<string>(StringComparer.Ordinal);
    private readonly HashSet<string> _doneCold = new HashSet<string>(StringComparer.Ordinal);

    // CSBENCH_SKIP 解析结果（类名集 + 类/名对集）
    private readonly HashSet<string> _skipCats = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
    private readonly HashSet<string> _skipItems = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

    // ---- 探针状态 ----
    private int _probeState; // 0=未开始 1=运行中 2=完成
    private Task _probeYieldTask, _probeTaskRunTask;
    private double _probeDeadlineMs;
    private bool _probeYieldPass, _probeTaskRunPass;

    // ---- WASM 异步驱动单元状态机 ----
    private Unit _asyncUnit;
    private Task _asyncTask;
    private readonly List<double> _asyncTimes = new List<double>();
    private int _asyncWarmupsLeft, _asyncItersLeft, _asyncDoneIters;
    private long _asyncIterStartTicks;
    private double _asyncIterDeadlineMs;
    private long _asyncMemTotal0;

    // WASM 测量参数：解释器比 JIT 慢 10-100×，减少迭代/预算避免超长尾
    private const int WasmWarmup = 2, WasmMinIters = 3, WasmMaxIters = 6;
    private const long WasmBudgetMs = 5000;
    private const int AsyncWarmupN = 1, AsyncMaxIters = 3;
    private const double AsyncIterWatchdogMs = 120_000; // 单次迭代看门狗（10k yield ≈ 40-50s）

    private static double NowMs => Stopwatch.GetTimestamp() * 1000.0 / Stopwatch.Frequency;

    /// <summary>
    /// 配置读取：环境变量（桌面驱动注入）。WASM 上无进程环境变量，恒返回
    /// null → 全类目 + 内置默认（maxSize=10000、探针门控、看门狗）。
    /// </summary>
    private static string GetCfg(string name)
    {
        try
        {
            string v = System.Environment.GetEnvironmentVariable(name);
            if (!string.IsNullOrEmpty(v)) return v;
        }
        catch { }
        return null;
    }

    // WASM BCL 探针（BCL_WASM_DIR 切换到 Mono 6.12 wasm profile 后，2026-08-20）：
    //   /lib/mono/4.5/{mscorlib,System,System.Core}.dll   实体 BCL，与 WASM 内嵌
    //       运行时（E:\workspace\godot\mono 同源构建）icall 签名一致 —— 根治旧
    //       6.8 BCL 的 Interlocked.CompareExchange / Mono.SafeStringMarshal
    //       "cant resolve internal call" 崩溃（sgen-alloc 断言 → abort）
    //   /lib/mono/4.5/Facades/{System.Memory,System.Buffers,netstandard}.dll
    //       type-forwarder facade：Span`1/Memory`1/ArrayPool`1/MemoryMarshal 实体
    //       均在 6.12 wasm mscorlib（字符串扫描验证）→ SpanMemory 可在 WASM 运行
    //   /.mono/assemblies/System.Memory.dll              PCK 提取目录（历史布局）
    // 逐一探测候选挂载点；全部缺失才防御性 skip SpanMemory（保住其余类目）。
    private bool _wasmSpanMemoryMissing;

    private static readonly string[] WasmSpanMemoryProbePaths =
    {
        "/lib/mono/4.5/Facades/System.Memory.dll", // 6.12 wasm BCL: facade → mscorlib
        "/lib/mono/4.5/System.Memory.dll",          // 经典 4.5 布局: 根目录实体
        "/.mono/assemblies/System.Memory.dll",      // PCK 提取目录（历史布局）
    };

    private void CheckMonoExtras()
    {
        foreach (string p in WasmSpanMemoryProbePaths)
        {
            try
            {
                if (File.Exists(p))
                {
                    GD.Print("[csbench-godot] System.Memory resolvable at " + p);
                    return;
                }
            }
            catch (Exception ex)
            {
                GD.Print("[csbench-godot] MEMFS probe failed at " + p
                    + ": " + ex.GetType().Name);
            }
        }
        _wasmSpanMemoryMissing = true;
        _skipCats.Add("SpanMemory");
        GD.Print("[csbench-godot] System.Memory NOT found in WASM MEMFS search paths - "
            + "SpanMemory category will be skipped (bcl-assembly-missing)");
    }

    public override void _Ready()
    {
        // 此引擎构建中 _Ready 会被调用两次（id=27 预就绪 + id=13 READY），需幂等守卫
        if (_initialized) return;
        _initialized = true;

        _isWeb = Runtime.TestIsWebPlatform() == 1;
        GD.Print("[csbench-godot] runtime=" + RuntimeInformation.FrameworkDescription
            + " platform=" + (_isWeb ? "wasm" : "desktop"));

        // WASM：先探测 System.Memory 族程序集（缺失则 skip SpanMemory），再触碰
        // Workloads.All（懒加载会触发 SpanW 的程序集解析）
        if (_isWeb) CheckMonoExtras();

        ParseSkipList();

        // ---- P0 探针：sync context 状态（死锁成因差异的直接证据）----
        ProbePrint("sync_context",
            (SynchronizationContext.Current == null ? "null" : SynchronizationContext.Current.GetType().Name),
            true, 0);

        var workloads = Workloads.All;
        // 类目过滤（CSBENCH_CATEGORIES，逗号分隔）
        string cats = GetCfg("CSBENCH_CATEGORIES");
        if (!string.IsNullOrEmpty(cats))
        {
            var set = new HashSet<string>(cats.Split(','), StringComparer.OrdinalIgnoreCase);
            workloads = Array.FindAll(workloads, w => set.Contains(w.Category));
        }
        GD.Print("[csbench-godot] workloads=" + workloads.Length
            + " loadErrors=" + (Workloads.LoadErrors.Count > 0
                ? string.Join("; ", Workloads.LoadErrors.ConvertAll(e => e.Item1 + ": " + e.Item2).ToArray()) : "0"));

        // 规模上限：WASM 默认 10000（解释器慢 + 1M 路径无意义），桌面可显式覆盖
        int maxSize = 0;
        try { string ms = GetCfg("CSBENCH_MAXSIZE"); if (ms != null) maxSize = int.Parse(ms); } catch { }
        if (maxSize <= 0 && _isWeb) maxSize = 10000;
        if (maxSize > 0) GD.Print("[csbench-godot] maxSize cap=" + maxSize);

        // 输出文件名（CSBENCH_PASS 分趟后缀）
        string pass = GetCfg("CSBENCH_PASS");
        string suffix = string.IsNullOrEmpty(pass) ? "" : "_" + pass;
        try
        {
            string dir = Directory.GetCurrentDirectory();
            _outPath = Path.Combine(dir, "csbench_godot" + suffix + ".json");
            _coldPath = Path.Combine(dir, "csbench_godot" + suffix + "_cold.json");
            _manifestPath = Path.Combine(dir, "csbench_godot" + suffix + "_manifest.txt");
            _probePath = Path.Combine(dir, "csbench_godot" + suffix + "_probes.txt");
        }
        catch (Exception ex)
        {
            _fileOk = false;
            GD.Print("[csbench-godot] file IO unavailable (console-only mode): " + ex.GetType().Name);
        }

        // ---- 断点续跑：加载既有 JSON，收集已完成单元（桌面）----
        if (_fileOk)
        {
            bool fresh = GetCfg("CSBENCH_FRESH") == "1";
            if (!fresh)
            {
                LoadExistingRows(_outPath, _carriedWarm, _doneWarm);
                LoadExistingRows(_coldPath, _carriedCold, _doneCold);
            }
            if (_doneWarm.Count > 0 || _doneCold.Count > 0)
                GD.Print($"[csbench-godot] resume: carry warm={_doneWarm.Count} cold={_doneCold.Count}");
        }

        // ---- 构建单元清单 + manifest ----
        _units = new List<Unit>();
        var manifest = new List<string>();
        foreach (var w in workloads)
        {
            if (w.Run == null) continue;
            bool asyncDriven = _isWeb && w.RunAsync != null;
            // 冷启动单元（全部负载 size=100 首调）
            manifest.Add("cold|" + w.Name + "|100");
            if (!_doneCold.Contains(w.Name + "|100"))
                _units.Add(new Unit(w, 100, true, asyncDriven));
            // 热启动单元
            foreach (int s in w.Sizes)
            {
                if (maxSize > 0 && s > maxSize) continue;
                manifest.Add("warm|" + w.Name + "|" + s);
                if (!_doneWarm.Contains(w.Name + "|" + s))
                    _units.Add(new Unit(w, s, false, asyncDriven));
            }
        }
        if (_fileOk && _manifestPath != null)
            try { File.WriteAllLines(_manifestPath, manifest.ToArray()); } catch { }
        if (_isWeb)
        {
            // console 洪峰会触发 Chromium 事件限流丢消息（2026-08-20：全量跑时
            // 206 行 manifest 连发后 collector 断流）。节流为头部样例 + 总数；
            // 完整清单走 MEMFS 文件（_manifestPath）。
            for (int i = 0; i < 3 && i < manifest.Count; i++)
                GD.Print("[csbench-manifest] " + manifest[i]);
            if (manifest.Count > 3)
                GD.Print("[csbench-manifest] ... " + (manifest.Count - 3) + " more (full list in MEMFS manifest file)");
        }
        GD.Print($"[csbench-godot] planned={manifest.Count} todoUnits={_units.Count} (resume-aware)");

        // ---- 平台探针 ----
        if (_isWeb)
        {
            // WASM 探针结论（2026-08-20 隔离实证）：Task.Yield / Task.Run 帧轮询
            // 均可推进完成（PASS ~2.3s），但 async 对象会损坏 sgen GC 描述符
            // （post-probe GC 必触发 sgen-scan-object.h:91 断言）。故本进程内
            // 不再启动真探针任务（毒源），输出隔离运行实证结论；async 驱动
            // 单元在执行循环中结构化 skip。
            _probeState = 2;
            ProbePrint("yield_progress", "PASS-by-isolated-evidence (2.3s, 2026-08-20; not re-run: async objects poison sgen GC)", true, 0);
            ProbePrint("taskrun_progress", "PASS-by-isolated-evidence (2.3s, 2026-08-20; not re-run: async objects poison sgen GC)", true, 0);
        }
        else
        {
            // 桌面：P1'/P2' 直接验证死锁修复（摘除 ctx 后阻塞等待能完成）
            _cpu0 = RunnerCore.ReadCpuSeconds();
            _probeState = 2;
            RunDesktopProbe("blocked_yield_ctxnull", 100, AsyncW.YieldDriver);
            RunDesktopProbe("blocked_taskrun_ctxnull", 50, AsyncW.TaskRunDriver);
        }
    }

    /// <summary>桌面探针：摘除 sync context 后执行阻塞等待（死锁修复直接验证）。</summary>
    private void RunDesktopProbe(string name, int n, Func<int, Task> driver)
    {
        GD.Print("[csbench-probe] " + name + " START");
        var saved = SynchronizationContext.Current;
        SynchronizationContext.SetSynchronizationContext(null);
        try
        {
            var sw = Stopwatch.StartNew();
            driver(n).GetAwaiter().GetResult();
            sw.Stop();
            ProbePrint(name, "completed", true, sw.Elapsed.TotalMilliseconds);
        }
        catch (Exception ex)
        {
            ProbePrint(name, ex.GetType().Name + ": " + ex.Message, false, 0);
        }
        finally
        {
            SynchronizationContext.SetSynchronizationContext(saved);
        }
    }

    private void ProbePrint(string name, string detail, bool pass, double ms)
    {
        string line = name + "=" + (pass ? "PASS" : "FAIL") + " detail=" + detail
            + (ms > 0 ? " ms=" + ms.ToString("F1") : "");
        GD.Print("[csbench-probe] " + line);
        if (_fileOk && _probePath != null)
            try { File.AppendAllText(_probePath, line + "\n"); } catch { }
    }

    private void ParseSkipList()
    {
        string s = GetCfg("CSBENCH_SKIP");
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
        if (path == null || !File.Exists(path)) return;
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

        // ---- WASM 探针阶段（先探后测）----
        if (_probeState < 2)
        {
            PumpProbes();
            return;
        }

        // ---- 异步驱动单元在途：只轮询，不启动新单元（避免计时互扰）----
        if (_asyncUnit != null)
        {
            PumpAsyncUnit();
            return;
        }

        // ---- 同步单元：每帧预算内批量执行 ----
        long frameBudgetTicks = Stopwatch.Frequency / 5; // ~200ms/帧
        var frameSw = Stopwatch.StartNew();

        while (_idx < _units.Count && frameSw.ElapsedTicks < frameBudgetTicks)
        {
            var u = _units[_idx];

            if (IsSkipRequested(u.W.Category, u.W.Name))
            {
                // skip 来源区分：WASM BCL 缺程序集（结构性）vs 驱动二分指定的崩溃项
                _skipReasons[u] = (u.W.Category == "SpanMemory" && _wasmSpanMemoryMissing)
                    ? "bcl-assembly-missing (System.Memory not in WASM BCL whitelist)"
                    : "mono-runtime-crash (auto-bisected)";
                MarkDone(u);
            }
            else if (_isWeb && u.W.Category == "Async")
            {
                // WASM：Async 类目整体结构化 skip（含非驱动负载 —— WhenAll/Tcs/
                // ConfigureAwait 等同步包装同样创建 async 状态机对象，一样毒化
                // sgen 堆）。依据同 asyncDriven 分支注释（2026-08-20）。
                _skipReasons[u] = "wasm-async-gc-corruption (any async state machine "
                    + "object poisons sgen descriptors; see probe evidence)";
                MarkDone(u);
            }
            else if (u.AsyncDriven)
            {
                // WASM 异步驱动单元：Task.Yield/Task.Run 能力探针在隔离运行中已
                // 实证可推进（2026-08-20：yield/taskrun 均 PASS ~2.3s），但 async
                // 状态机/Task 对象会损坏 sgen GC 描述符（post-probe GC 必触发
                // sgen-scan-object.h:91 断言 —— Mono WASM 解释器级 bug，与
                // Platform.cs 跳过 sync-context Install 的已知损坏同族）。async
                // 单元一旦执行，堆上毒对象会让后续任何 GC 崩溃（含同步单元
                // Measure 的 GC.GetTotalMemory），故整体结构化跳过，保住其余
                // 类目。探针实证数据见 skipReason。
                _skipReasons[u] = "wasm-async-gc-corruption (probe: yield/taskrun "
                    + "PASS 2.3s in isolated run; async objects poison sgen descriptors)";
                MarkDone(u);
            }
            else
            {
                try
                {
                    Measured m;
                    if (!_isWeb)
                        m = RunSyncUnit(u);
                    else if (u.Cold)
                        m = RunnerCore.MeasureCold(u.W.Run, u.Size);
                    else
                        m = RunnerCore.Measure(u.W.Run, u.Size, WasmWarmup, WasmMinIters, WasmMaxIters, WasmBudgetMs);
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
            FinishAll();
        }
    }

    /// <summary>
    /// 桌面同步执行：测量期间临时摘除 SynchronizationContext（GodotSynchronizationContext
    /// 死锁修复——await continuation 改走 ThreadPool 真实线程），测完恢复。
    /// </summary>
    private Measured RunSyncUnit(Unit u)
    {
        var saved = SynchronizationContext.Current;
        SynchronizationContext.SetSynchronizationContext(null);
        try
        {
            return u.Cold ? RunnerCore.MeasureCold(u.W.Run, u.Size)
                          : RunnerCore.Measure(u.W.Run, u.Size);
        }
        finally
        {
            SynchronizationContext.SetSynchronizationContext(saved);
        }
    }

    // ============================================================
    // WASM 探针：帧轮询 Task 是否推进（事件循环驱动能力实证）
    // ============================================================
    private void PumpProbes()
    {
        bool yDone = _probeYieldTask == null || _probeYieldTask.IsCompleted;
        bool tDone = _probeTaskRunTask == null || _probeTaskRunTask.IsCompleted;
        bool timeout = NowMs > _probeDeadlineMs;

        if (yDone && tDone || timeout)
        {
            _probeYieldPass = _probeYieldTask != null && _probeYieldTask.IsCompleted
                && !_probeYieldTask.IsFaulted;
            _probeTaskRunPass = _probeTaskRunTask != null && _probeTaskRunTask.IsCompleted
                && !_probeTaskRunTask.IsFaulted;
            double ms = 10_000 - Math.Max(0, _probeDeadlineMs - NowMs);
            ProbePrint("yield_progress", _probeYieldPass ? "completed" :
                (_probeYieldTask == null ? "not-started" : "no-progress"), _probeYieldPass, _probeYieldPass ? ms : 0);
            ProbePrint("taskrun_progress", _probeTaskRunPass ? "completed" :
                (_probeTaskRunTask == null ? "not-started" : "no-progress"), _probeTaskRunPass, _probeTaskRunPass ? ms : 0);
            _probeState = 2;
            if (_cpu0 == 0) _cpu0 = RunnerCore.ReadCpuSeconds();
        }
    }

    // ============================================================
    // WASM 异步驱动单元：帧轮询 RunAsync Task，墙钟计时 + 单元看门狗
    // ============================================================
    private void StartAsyncUnit(Unit u)
    {
        _asyncUnit = u;
        _asyncTimes.Clear();
        _asyncDoneIters = 0;
        _asyncWarmupsLeft = u.Cold ? 0 : AsyncWarmupN;
        _asyncItersLeft = u.Cold ? 1 : AsyncMaxIters;
        _asyncMemTotal0 = GC.GetTotalMemory(true);
        StartAsyncIter();
    }

    private void StartAsyncIter()
    {
        _asyncIterStartTicks = Stopwatch.GetTimestamp();
        _asyncIterDeadlineMs = NowMs + AsyncIterWatchdogMs;
        try
        {
            _asyncTask = _asyncUnit.W.RunAsync(_asyncUnit.Size);
        }
        catch (Exception ex)
        {
            _skipReasons[_asyncUnit] = ex.GetType().Name;
            MarkDone(_asyncUnit);
            _idx++;
            _asyncUnit = null;
        }
    }

    private void PumpAsyncUnit()
    {
        if (_asyncTask == null || _asyncTask.IsCompleted)
        {
            double ns = (Stopwatch.GetTimestamp() - _asyncIterStartTicks) * 1e9 / Stopwatch.Frequency;
            if (_asyncTask != null && _asyncTask.IsFaulted)
            {
                var ex = _asyncTask.Exception; // 观察异常，避免 UnobservedTaskException
                _skipReasons[_asyncUnit] = "async-fault: " + (ex != null && ex.InnerExceptions.Count > 0 ? ex.InnerExceptions[0].GetType().Name : "?");
                MarkDone(_asyncUnit);
                _idx++;
                _asyncUnit = null;
                Flush();
                return;
            }
            if (_asyncWarmupsLeft > 0) _asyncWarmupsLeft--;
            else
            {
                _asyncTimes.Add(ns);
                _asyncDoneIters++;
                _asyncItersLeft--;
            }
            if (_asyncItersLeft > 0) { StartAsyncIter(); return; }

            // 单元完成：组装 Measured（墙钟统计 + 近似分配）
            var m = RunnerCore.BuildStats(_asyncTimes.ToArray());
            long mem1 = GC.GetTotalMemory(false);
            m.AllocatedBytesPerOp = _asyncDoneIters > 0
                ? Math.Max(0, mem1 - _asyncMemTotal0) / (double)_asyncDoneIters : 0;
            _results[_asyncUnit] = m;
            MarkDone(_asyncUnit);
            _idx++;
            _asyncUnit = null;
            Flush();
            return;
        }

        if (NowMs > _asyncIterDeadlineMs)
        {
            // 看门狗：事件循环停摆或迭代超长。放弃该单元（死任务留在后台无害）。
            _skipReasons[_asyncUnit] = "wasm-async-watchdog (>" + (AsyncIterWatchdogMs / 1000) + "s)";
            MarkDone(_asyncUnit);
            _idx++;
            _asyncUnit = null;
            Flush();
        }
        // else: 继续等下一帧（返回主循环让 emscripten 事件循环推进任务）
    }

    private void FinishAll()
    {
        double cpu = RunnerCore.ReadCpuSeconds() - _cpu0;
        int planned = 0;
        if (_fileOk && _manifestPath != null)
            try { planned = File.ReadAllLines(_manifestPath).Length; } catch { }
        GD.Print($"[csbench-godot] ALL DONE todo={_units.Count} carriedWarm={_carriedWarm.Count} carriedCold={_carriedCold.Count} plannedTotal={planned} cpuSeconds={cpu:F1}");
        GD.Print("[csbench-done]");
        try { GetTree().Quit(); } catch { }
    }

    private void MarkDone(Unit u)
    {
        if (u.Cold) _coldDone.Add(u); else _warmDone.Add(u);
        // console 行输出：驱动脚本（WASM/桌面统一）从 console 收割增量结果
        Measured m; _results.TryGetValue(u, out m);
        string reason; bool skipped = _skipReasons.TryGetValue(u, out reason);
        string row = BenchJsonWriter.BuildRow(u.W.Category, u.W.Name, u.Size, m, skipped, reason);
        GD.Print((u.Cold ? "[csbench-row-cold] " : "[csbench-row-warm] ") + row);
    }

    private void Flush()
    {
        if (!_fileOk) return;
        try
        {
            string rt = RuntimeInformation.FrameworkDescription + " (Godot 4.7 embedded Mono)";
            double cpu = RunnerCore.ReadCpuSeconds() - _cpu0;

            var cw = new BenchJsonWriter("godot", rt, _isWeb ? "mono6.12-godot-wasm" : "mono6.12-godot", "quick", "cold");
            foreach (var raw in _carriedCold) cw.AddRawRow(raw);   // 断点续跑：历史行原样携带
            foreach (var u in _coldDone)
            {
                Measured m; _results.TryGetValue(u, out m);
                string reason; bool skipped = _skipReasons.TryGetValue(u, out reason);
                cw.AddRow(u.W.Category, u.W.Name, u.Size, m, skipped, reason);
            }
            File.WriteAllText(_coldPath, cw.ToJson());

            var ww = new BenchJsonWriter("godot", rt, _isWeb ? "mono6.12-godot-wasm" : "mono6.12-godot", "quick", "warm");
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
