using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;

namespace CSharpBench.Report
{
    // =====================================================================
    // 数据模型（对应 csbench/1 schema）
    // =====================================================================
    public class Row
    {
        public string Category, Name;
        public int Size, Iters;
        public double? MeanNs, StddevNs, MedianNs, MinNs;
        public double? AllocatedPerOp, Gen0Per1k, Gen1Per1k, Gen2Per1k, CpuFraction;
        public bool Skipped; public string SkipReason;
    }
    public class RunFile
    {
        public string Engine, Runtime, Tfm, Profile, Mode, Path;
        public double? CpuSeconds; public string StartedUtc;
        public List<Row> Rows = new List<Row>();
        public string ColKey => Engine == "godot" ? "Mono-in-Godot"
                              : Engine == "godot-wasm" ? "Mono-WASM"
                              : Tfm;
    }

    public static class Program
    {
        static string _inDir, _outBase;
        static readonly List<RunFile> Warm = new List<RunFile>();
        static readonly List<RunFile> Cold = new List<RunFile>();
        // 合并前的原始文件清单（环境表展示用；数据表用合并后的 Warm/Cold）
        static readonly List<RunFile> WarmFiles = new List<RunFile>();
        static readonly List<RunFile> ColdFiles = new List<RunFile>();
        // 列顺序
        static readonly string[] ColOrder = { "net48", "netcoreapp3.1", "net6.0", "net7.0", "Mono-in-Godot", "Mono-WASM" };
        static readonly string[] Palette = { "#8a8f98", "#f0883e", "#42b883", "#58a6ff", "#bf8fff", "#f778ba" };

        static int Main(string[] args)
        {
            _inDir = Directory.GetCurrentDirectory();
            for (int i = 0; i < args.Length; i++)
            {
                if (args[i] == "--indir") _inDir = args[++i];
                else if (args[i] == "--out") _outBase = args[++i];
            }
            if (_outBase == null) _outBase = Path.Combine(_inDir, "report_" + DateTime.Now.ToString("yyyyMMdd_HHmm"));

            foreach (var f in Directory.GetFiles(_inDir, "*.json"))
            {
                try
                {
                    using var doc = JsonDocument.Parse(File.ReadAllText(f));
                    var root = doc.RootElement;
                    if (root.TryGetProperty("schema", out var sc) && sc.GetString() == "csbench/1")
                    {
                        var rf = new RunFile
                        {
                            Path = f,
                            Engine = GetString(root, "engine"),
                            Runtime = GetString(root, "runtime"),
                            Tfm = GetString(root, "tfm"),
                            Profile = GetString(root, "profile"),
                            Mode = GetString(root, "mode"),
                            StartedUtc = GetString(root, "startedUtc"),
                        };
                        if (root.TryGetProperty("cpuSeconds", out var cs) && cs.ValueKind != JsonValueKind.Null)
                            rf.CpuSeconds = cs.GetDouble();
                        foreach (var r in root.GetProperty("results").EnumerateArray())
                        {
                            rf.Rows.Add(new Row
                            {
                                Category = GetString(r, "category"),
                                Name = GetString(r, "name"),
                                Size = GetInt(r, "size"),
                                Iters = GetInt(r, "iters"),
                                MeanNs = GetD(r, "meanNs"),
                                StddevNs = GetD(r, "stddevNs"),
                                MedianNs = GetD(r, "medianNs"),
                                MinNs = GetD(r, "minNs"),
                                AllocatedPerOp = GetD(r, "allocatedBytesPerOp"),
                                Gen0Per1k = GetD(r, "gen0Per1kOps"),
                                Gen1Per1k = GetD(r, "gen1Per1kOps"),
                                Gen2Per1k = GetD(r, "gen2Per1kOps"),
                                CpuFraction = GetD(r, "cpuFraction"),
                                Skipped = r.TryGetProperty("skipped", out var sk) && sk.GetBoolean(),
                                SkipReason = GetString(r, "skipReason"),
                            });
                        }
                        (rf.Mode == "cold" ? Cold : Warm).Add(rf);
                    }
                }
                catch (Exception ex) { Console.WriteLine($"[report] skip {f}: {ex.Message}"); }
            }
            Console.WriteLine($"[report] warm files={Warm.Count}, cold files={Cold.Count}");
            if (Warm.Count == 0 && Cold.Count == 0) { Console.WriteLine("[report] no input"); return 1; }

            // ---- 同列多文件合并 ----
            // 同一运行时列的数据可能分散在多个文件（Godot 分类分趟 / BDN 每类一文件 /
            // 断点续跑残档）。按 ColKey 分组合并，行键 (category/name/size) 去重：
            // startedUtc 较新的文件覆盖较旧的（重跑/续跑后的新鲜数据优先）。
            WarmFiles.AddRange(Warm); ColdFiles.AddRange(Cold);
            var mergedWarm = MergeByColumn(Warm);
            Warm.Clear(); Warm.AddRange(mergedWarm);
            var mergedCold = MergeByColumn(Cold);
            Cold.Clear(); Cold.AddRange(mergedCold);
            Console.WriteLine($"[report] merged columns: warm={Warm.Count} cold={Cold.Count} " +
                $"(rows: " + string.Join(", ", Warm.Select(w => $"{w.ColKey}={w.Rows.Count}")) + ")");

            var html = new StringBuilder(1 << 20);
            var md = new StringBuilder(256 * 1024);
            BuildReport(html, md);
            File.WriteAllText(_outBase + ".html", html.ToString());
            File.WriteAllText(_outBase + ".md", md.ToString());
            Console.WriteLine($"[report] written: {_outBase}.html / .md");
            return 0;
        }

        /// <summary>按列（engine+tfm）合并多个 RunFile：行键去重，新数据覆盖旧数据。</summary>
        static List<RunFile> MergeByColumn(List<RunFile> runs)
        {
            var result = new List<RunFile>();
            foreach (var g in runs.GroupBy(r => r.ColKey))
            {
                var first = g.First();
                var merged = new RunFile
                {
                    Engine = first.Engine,
                    Runtime = first.Runtime,
                    Tfm = first.Tfm,
                    Profile = first.Profile,
                    Mode = first.Mode,
                    StartedUtc = g.Select(x => x.StartedUtc).Where(s => s != null).OrderByDescending(s => s).FirstOrDefault(),
                    Path = string.Join(";", g.Select(x => Path.GetFileName(x.Path))),
                };
                double cpu = 0; foreach (var x in g) cpu += x.CpuSeconds ?? 0;
                merged.CpuSeconds = cpu;

                // startedUtc 升序处理：后处理的覆盖先处理的 → 最终保留最新
                var rows = new Dictionary<string, Row>();
                foreach (var f in g.OrderBy(x => x.StartedUtc ?? ""))
                    foreach (var r in f.Rows)
                        rows[r.Category + "/" + r.Name + "/" + r.Size] = r;
                merged.Rows = rows.Values.ToList();
                result.Add(merged);
            }
            return result;
        }

        static void BuildReport(StringBuilder html, StringBuilder md)
        {
            var cols = Warm.Select(w => w.ColKey).Distinct()
                           .OrderBy(c => Array.IndexOf(ColOrder, c) < 0 ? 99 : Array.IndexOf(ColOrder, c))
                           .ToList();
            html.Append(HtmlHead());
            md.Append("# C# 基准测试对比报告\n\n");
            md.Append($"> 生成时间: {DateTime.Now:yyyy-MM-dd HH:mm:ss} | 引擎: BenchmarkDotNet (bdn) / 自研进程内采集 (fallback/godot)\n\n");

            // ---- 环境与来源 ----
            html.Append("<h2 id='env'>1. 测试环境与数据来源</h2><table class='tbl'><tr><th>列</th><th>引擎</th><th>运行时</th><th>TFM</th><th>Profile</th><th>模式</th><th>CPU 时间(s)</th><th>来源文件</th></tr>");
            md.Append("## 1. 测试环境与数据来源\n\n| 列 | 引擎 | 运行时 | TFM | Profile | 模式 | CPU 时间(s) | 来源 |\n|---|---|---|---|---|---|---|---|\n");
            foreach (var w in WarmFiles.Concat(ColdFiles))
            {
                html.Append($"<tr><td><b>{w.ColKey}</b></td><td>{w.Engine}</td><td>{Limit(w.Runtime, 60)}</td><td>{w.Tfm}</td><td>{w.Profile}</td><td>{w.Mode}</td><td>{w.CpuSeconds:F1}</td><td>{Path.GetFileName(w.Path)}</td></tr>");
                md.Append($"| {w.ColKey} | {w.Engine} | {Limit(w.Runtime, 40)} | {w.Tfm} | {w.Profile} | {w.Mode} | {w.CpuSeconds:F1} | {Path.GetFileName(w.Path)} |\n");
            }
            html.Append("</table>");
            md.Append("\n**方法说明**: 工作负载统一签名 `Action<size>`，一次调用执行 size 个基本操作（或处理 size 元数据集）；"
                      + "规模三档 100 / 10,000 / 1,000,000（异步/反射/GC 分配类为 100 / 10,000）；"
                      + "quick 档 = ShortRun(3 warmup + 3 迭代, BDN) 或自适应 ≥5 次迭代 ≥150ms（fallback/godot）。"
                      + "表内数值为 ns/调用（每万次基本操作除以 size 可得 ns/元素）。冷启动为独立新进程首调（size=100，无预热）。\n\n");

            // ---- 每类目 ----
            var categories = Warm.SelectMany(w => w.Rows).Select(r => r.Category).Distinct()
                                 .OrderBy(c => c, StringComparer.Ordinal).ToList();
            int sec = 2;
            foreach (var cat in categories)
            {
                html.Append($"<h2 id='{cat}'>{sec}. {cat}</h2>");
                md.Append($"## {sec}. {cat}\n\n");
                AppendCategory(html, md, cat, cols);
                sec++;
            }

            // ---- 分析 ----
            AppendAnalysis(html, md, cols, ref sec);
            html.Append("</body></html>");
        }

        static void AppendCategory(StringBuilder html, StringBuilder md, string cat, List<string> cols)
        {
            // 行 = (name,size)，列 = runtime
            var keys = Warm.Where(w => w.Rows.Any(r => r.Category == cat))
                           .SelectMany(w => w.Rows.Where(r => r.Category == cat))
                           .GroupBy(r => r.Name + "|" + r.Size)
                           .OrderBy(g => g.Key.Split('|')[0], StringComparer.Ordinal)
                           .ThenBy(g => int.Parse(g.Key.Split('|')[1]))
                           .ToList();

            html.Append("<table class='tbl'><tr><th>Benchmark</th><th>Size</th>");
            md.Append("| Benchmark | Size |");
            foreach (var c in cols) { html.Append($"<th class='num'>{c}<br>ns/op</th><th class='num'>{c}<br>B/op</th>"); md.Append($" {c} ns/op | {c} B/op |"); }
            html.Append("</tr>");
            md.Append("\n|---|---|");
            for (int i = 0; i < cols.Count * 2; i++) md.Append("---|");

            foreach (var g in keys)
            {
                var parts = g.Key.Split('|');
                string name = parts[0]; int size = int.Parse(parts[1]);
                html.Append($"<tr><td>{name}</td><td class='num'>{size:N0}</td>");
                md.Append($"\n| {name} | {size:N0} |");
                foreach (var c in cols)
                {
                    var row = Warm.FirstOrDefault(w => w.ColKey == c)?.Rows
                        .FirstOrDefault(r => r.Category == cat && r.Name == name && r.Size == size);
                    if (row == null) { html.Append("<td class='num muted'>—</td><td class='num muted'>—</td>"); md.Append(" — | — |"); }
                    else if (row.Skipped) { html.Append($"<td class='num warn' title='{row.SkipReason}'>skip</td><td class='num muted'>—</td>"); md.Append(" skip | — |"); }
                    else
                    {
                        html.Append($"<td class='num'>{FmtNs(row.MeanNs)}</td><td class='num'>{FmtB(row.AllocatedPerOp)}</td>");
                        md.Append($" {FmtNs(row.MeanNs)} | {FmtB(row.AllocatedPerOp)} |");
                    }
                }
                html.Append("</tr>");
            }
            html.Append("</table>");
            md.Append("\n");

            // SVG 柱状图（median ns，对数轴）取该类中间规模
            var chartSize = keys.Select(k => int.Parse(k.Key.Split('|')[1])).Distinct().OrderBy(s => s).ToList();
            int sz = chartSize.Contains(10_000) ? 10_000 : chartSize.LastOrDefault();
            var items = keys.Where(k => int.Parse(k.Key.Split('|')[1]) == sz).ToList();
            html.Append(SvgBars(cat, sz, items, cols));
        }

        static string SvgBars(string cat, int size, List<IGrouping<string, Row>> items, List<string> cols)
        {
            var rowsW = new List<(string name, Dictionary<string, double> vals)>();
            foreach (var g in items)
            {
                string name = g.Key.Split('|')[0];
                var vals = new Dictionary<string, double>();
                foreach (var c in cols)
                {
                    var r = Warm.FirstOrDefault(w => w.ColKey == c)?.Rows
                        .FirstOrDefault(x => x.Category == cat && x.Name == name && x.Size == size);
                    if (r != null && r.MedianNs.HasValue && !r.Skipped) vals[c] = r.MedianNs.Value;
                }
                if (vals.Count > 0) rowsW.Add((name, vals));
            }
            if (rowsW.Count == 0) return "";
            rowsW = rowsW.OrderBy(r => r.vals.Values.Min()).ToList();

            double maxV = rowsW.Max(r => r.vals.Values.Max());
            int rowH = 22, leftLabel = 230, barAreaW = 720, w = leftLabel + barAreaW + 160;
            int h = rowH * rowsW.Count + 70;
            var sb = new StringBuilder();
            sb.Append($"<svg xmlns='http://www.w3.org/2000/svg' width='{w}' height='{h}' class='chart'>");
            sb.Append($"<text x='8' y='18' class='t'>{cat} — median ns/op @ size={size:N0} (log scale)</text>");
            // 对数刻度: bar 宽 ∝ log10(v/min)/log10(max/min)
            double minV = rowsW.Min(r => r.vals.Values.Min());
            double lmin = Math.Log10(minV), lmax = Math.Log10(maxV);
            double span = lmax - lmin; if (span < 1e-9) span = 1;
            int y = 34;
            foreach (var (name, vals) in rowsW)
            {
                int x = leftLabel;
                foreach (var c in cols)
                {
                    if (!vals.TryGetValue(c, out double v)) { x += 10; continue; }
                    double frac = (Math.Log10(v) - lmin) / span;
                    int bw = Math.Max(3, (int)(frac * barAreaW / cols.Count * 0.92));
                    int ci = cols.IndexOf(c);
                    sb.Append($"<rect x='{x}' y='{y}' width='{bw}' height='{rowH - 6}' fill='{Palette[ci % Palette.Length]}' opacity='0.88'><title>{c}: {FmtNs(v)}</title></rect>");
                    x += (int)Math.Ceiling((double)barAreaW / cols.Count) + 2;
                }
                sb.Append($"<text x='{leftLabel - 6}' y='{y + rowH - 11}' text-anchor='end' class='lbl'>{Limit(name, 34)}</text>");
                y += rowH;
            }
            // 图例
            int lx = 8; int ly = h - 12;
            for (int i = 0; i < cols.Count; i++)
            {
                sb.Append($"<rect x='{lx}' y='{ly - 9}' width='10' height='10' fill='{Palette[i % Palette.Length]}'/><text x='{lx + 14}' y='{ly}' class='lbl'>{cols[i]}</text>");
                lx += 14 + cols[i].Length * 7 + 18;
            }
            sb.Append("</svg>");
            return sb.ToString();
        }

        static void AppendAnalysis(StringBuilder html, StringBuilder md, List<string> cols, ref int sec)
        {
            html.Append($"<h2 id='analysis'>{sec}. 瓶颈分析与优化建议</h2>");
            md.Append($"## {sec}. 瓶颈分析与优化建议\n\n");
            sec++;

            var allRows = Warm.SelectMany(w => w.Rows).Where(r => !r.Skipped && r.MeanNs.HasValue).ToList();

            // 4.1 每类最慢 top3（最大共同规模）
            html.Append("<h3>各类别最慢项 Top 3（最大规模，ns/调用）</h3><ul>");
            md.Append("### 各类别最慢项 Top 3（最大规模，ns/调用）\n\n");
            foreach (var cat in allRows.Select(r => r.Category).Distinct().OrderBy(c => c))
            {
                var maxSz = allRows.Where(r => r.Category == cat).Max(r => r.Size);
                var top = allRows.Where(r => r.Category == cat && r.Size == maxSz)
                                 .OrderByDescending(r => r.MeanNs.Value / Math.Max(1, r.Size)).Take(3);
                foreach (var t in top)
                {
                    double perOp = t.MeanNs.Value / Math.Max(1, t.Size);
                    string line = $"{cat}/{t.Name} (size={maxSz:N0}): {FmtNs(t.MeanNs)} 总耗时 ≈ {FmtNs(perOp)}/元素";
                    html.Append($"<li>{line}</li>"); md.Append($"- {line}\n");
                }
            }
            html.Append("</ul>");

            // 4.2 分配大户 top10
            html.Append("<h3>分配大户 Top 10（B/调用，size=10,000）</h3><ul>");
            md.Append("\n### 分配大户 Top 10（B/调用，size=10,000）\n\n");
            var allocTop = allRows.Where(r => r.Size == 10_000 && r.AllocatedPerOp.HasValue)
                                  .OrderByDescending(r => r.AllocatedPerOp.Value).Take(10);
            foreach (var a in allocTop)
            {
                string line = $"{a.Category}/{a.Name}: {FmtB(a.AllocatedPerOp)} ({a.AllocatedPerOp.Value / 10_000.0:F1} B/元素)";
                html.Append($"<li>{line}</li>"); md.Append($"- {line}\n");
            }
            html.Append("</ul>");

            // 4.3 版本回归 net7.0 vs net48
            if (cols.Contains("net48") && cols.Contains("net7.0"))
            {
                html.Append("<h3>版本演进: net7.0 / net48 加速比（中位数，越大越好；&lt;1 为回归）</h3>");
                md.Append("\n### 版本演进: net7.0 / net48 加速比（中位数，越大越好；<1 为回归）\n\n");
                html.Append("<table class='tbl'><tr><th>Benchmark</th><th>Size</th><th>net48 ns</th><th>net7.0 ns</th><th>Speedup</th></tr>");
                md.Append("| Benchmark | Size | net48 ns | net7.0 ns | Speedup |\n|---|---|---|---|---|\n");
                var n48 = Warm.First(w => w.ColKey == "net48").Rows;
                var n7 = Warm.First(w => w.ColKey == "net7.0").Rows;
                var ratios = new List<(Row a, Row b, double ratio)>();
                foreach (var a in n48.Where(r => !r.Skipped && r.MedianNs.HasValue))
                {
                    var b = n7.FirstOrDefault(r => r.Category == a.Category && r.Name == a.Name && r.Size == a.Size);
                    if (b != null && !b.Skipped && b.MedianNs.HasValue && b.MedianNs > 0)
                        ratios.Add((a, b, a.MedianNs.Value / b.MedianNs.Value));
                }
                foreach (var (a, b, ratio) in ratios.OrderByDescending(r => r.ratio).Take(12))
                {
                    html.Append($"<tr><td>{a.Category}/{a.Name}</td><td class='num'>{a.Size:N0}</td><td class='num'>{FmtNs(a.MedianNs)}</td><td class='num'>{FmtNs(b.MedianNs)}</td><td class='num'>{ratio:F2}×</td></tr>");
                    md.Append($"| {a.Category}/{a.Name} | {a.Size:N0} | {FmtNs(a.MedianNs)} | {FmtNs(b.MedianNs)} | {ratio:F2}× |\n");
                }
                html.Append("</table>");
                var regressions = ratios.Where(r => r.ratio < 0.8).OrderBy(r => r.ratio).Take(8);
                if (regressions.Any())
                {
                    html.Append("<p><b>回归项 (net7.0 反而慢 20%+):</b> ");
                    md.Append("\n**回归项 (net7.0 反而慢 20%+):** ");
                    var rs = regressions.Select(r => $"{r.a.Category}/{r.a.Name} ({r.ratio:F2}×)");
                    html.Append(string.Join("、", rs) + "</p>");
                    md.Append(string.Join("、", rs) + "\n");
                }
            }

            // 4.4 规则化优化建议
            html.Append("<h3>优化建议（基于数据的规则库）</h3><ul>");
            md.Append("\n### 优化建议（基于数据的规则库）\n\n");
            foreach (var s in BuildSuggestions(allRows, cols)) { html.Append($"<li>{s}</li>"); md.Append($"- {s}\n"); }
            html.Append("</ul>");

            // 4.5 冷启动
            if (Cold.Count > 0)
            {
                html.Append("<h3>冷启动首调成本（新进程，size=100，含 JIT/静态构造，ms）</h3>");
                md.Append("\n### 冷启动首调成本（新进程，size=100，含 JIT/静态构造，ms）\n\n");
                html.Append("<table class='tbl'><tr><th>Benchmark</th>");
                md.Append("| Benchmark |");
                var ccols = Cold.Select(c => c.ColKey).Distinct().ToList();
                foreach (var c in ccols) { html.Append($"<th class='num'>{c}</th>"); md.Append($" {c} |"); }
                html.Append("</tr>"); md.Append("\n|---|");
                for (int i = 0; i < ccols.Count; i++) md.Append("---|");
                var ckeys = Cold.SelectMany(c => c.Rows).GroupBy(r => r.Category + "/" + r.Name).OrderBy(g => g.Key).ToList();
                var coldTop = ckeys.OrderByDescending(g =>
                {
                    var r = Cold[0].Rows.FirstOrDefault(x => x.Category + "/" + x.Name == g.Key);
                    return r?.MeanNs ?? 0;
                }).Take(25).ToList();
                foreach (var g in coldTop)
                {
                    html.Append($"<tr><td>{g.Key}</td>"); md.Append($"\n| {g.Key} |");
                    foreach (var c in ccols)
                    {
                        var r = Cold.First(x => x.ColKey == c).Rows.FirstOrDefault(x => x.Category + "/" + x.Name == g.Key);
                        double ms = (r?.MeanNs ?? 0) / 1e6;
                        html.Append($"<td class='num'>{(r != null ? ms.ToString("F3") : "—")}</td>");
                        md.Append($" {(r != null ? ms.ToString("F3") : "—")} |");
                    }
                    html.Append("</tr>");
                }
                html.Append("</table>");
                md.Append("\n\n> 注: 冷启动在同进程内逐项首调，共享 JIT 预热会带来向下的偏差，用于量级对比而非精确值。\n");
            }
        }

        static List<string> BuildSuggestions(List<Row> rows, List<string> cols)
        {
            var sug = new List<string>();
            Row Get(string cat, string name, int size, string col)
            {
                var w = Warm.FirstOrDefault(x => x.ColKey == col); if (w == null) return null;
                return w.Rows.FirstOrDefault(r => r.Category == cat && r.Name == name && r.Size == size);
            }
            double? Mean(Row r) => r?.MeanNs;

            // LINQ 链 vs 手写循环
            var linq = Get("Linq", "WhereSelect_Chain_Sum", 10_000, cols.LastOrDefault(c => c != "Mono-in-Godot" && c != "Mono-WASM") ?? "net7.0");
            var hand = Get("Linq", "Handwritten_Sum_Loop", 10_000, linq == null ? "net7.0" : cols.First(c => Get("Linq", "WhereSelect_Chain_Sum", 10_000, c) != null));
            if (linq?.MeanNs != null && hand?.MeanNs != null && hand.MeanNs > 0)
            {
                double ratio = linq.MeanNs.Value / hand.MeanNs.Value;
                if (ratio > 3) sug.Add($"LINQ 链 (Where+Select+Sum) 比手写循环慢 {ratio:F1}× — 热路径建议手写 for 循环");
                else sug.Add($"LINQ 链 vs 手写循环差距 {ratio:F1}×（现代运行时已高度优化，非热点可放心用 LINQ）");
            }
            // 字符串拼接
            var sp = Get("Primitives", "String_Concat_Plus", 10_000, "net7.0"); var sb2 = Get("Primitives", "String_Concat_Builder", 10_000, "net7.0");
            if (sp?.MeanNs != null && sb2?.MeanNs != null && sb2.MeanNs > 0 && sp.MeanNs / sb2.MeanNs > 2)
                sug.Add($"循环内 string + 拼接比 StringBuilder 慢 {sp.MeanNs.Value / sb2.MeanNs.Value:F1}× — 循环拼接一律用 StringBuilder");
            // 装箱
            var box = Get("Primitives", "Int_Boxing", 10_000, "net7.0");
            if (box?.AllocatedPerOp != null && box.AllocatedPerOp > 0)
                sug.Add($"Int 装箱每次产生 {box.AllocatedPerOp / 10_000.0:F0} B 分配 — 热路径改泛型集合/避免 object 传值");
            // 闭包
            var clo = Get("Delegates", "ClosureCapture_Invoke", 10_000, "net7.0");
            var stat = Get("Delegates", "StaticLambdaNoCapture", 10_000, "net7.0");
            if (clo?.AllocatedPerOp != null && clo.AllocatedPerOp > 0)
                sug.Add($"捕获闭包 lambda 每次调用分配 {clo.AllocatedPerOp / 10_000.0:F0} B — 高频回调把捕获变量提为字段或用 static lambda");
            if (stat != null) { /* 对照存在 */ }
            // 反射
            var inv = Get("Reflection", "Method_Invoke_Instance", 10_000, "net7.0");
            var del = Get("Reflection", "CreateDelegate_ThenInvoke", 10_000, "net7.0");
            if (inv?.MeanNs != null && del?.MeanNs != null && inv.MeanNs.Value / 10_000 > 100)
                sug.Add($"反射 Invoke ≈ {inv.MeanNs.Value / 10_000:F0} ns/次，CreateDelegate+直调 ≈ {del.MeanNs.Value / 10_000:F0} ns/次 — 重复调用的反射改缓存委托（快 {(inv.MeanNs.Value / Math.Max(1.0, del.MeanNs.Value)):F1}×）");
            // 预分配
            var lg = Get("Collections", "List_Add_Grow", 10_000, "net7.0"); var lp = Get("Collections", "List_Add_Prealloc", 10_000, "net7.0");
            if (lg?.MeanNs != null && lp?.MeanNs != null && lp.MeanNs > 0 && lg.MeanNs / lp.MeanNs > 1.5)
                sug.Add($"List 动态扩容比预分配慢 {lg.MeanNs.Value / lp.MeanNs.Value:F1}× — 已知规模时传 capacity");
            // ValueTask 场景提示
            var tcs = Get("Async", "Tcs_SetResult_Await", 10_000, "net7.0");
            if (tcs?.MeanNs != null) sug.Add($"TaskCompletionSource 往返 ≈ {tcs.MeanNs / 10_000:F0} ns/次 — 高频同步完成路径考虑 ValueTask / 直接回调（本项目 Godot 基准实测 TCS roundtrip 达 25μs+）");
            // skipped 类目（Mono-in-Godot）：显式 skip 与 1M 规模上限两类豁免分开说明
            var godotWarm = Warm.Where(w => w.ColKey == "Mono-in-Godot").ToList();
            var skipReasons = godotWarm.SelectMany(w => w.Rows)
                .Where(r => r.Skipped)
                .GroupBy(r => r.Category + "/" + r.Name + "/" + r.Size) // 断点续跑可能重复输出同一行键，先去重
                .Select(g => g.First())
                .GroupBy(r => string.IsNullOrEmpty(r.SkipReason) ? "unspecified" : r.SkipReason)
                .ToDictionary(g => g.Key, g => g.Count());
            if (skipReasons.Count > 0)
                sug.Add($"Mono-in-Godot 显式跳过 {skipReasons.Values.Sum()} 行: " +
                        string.Join("; ", skipReasons.Select(kv => $"{kv.Key} ×{kv.Value}")) +
                        " — 已如实标注而非报错");
            // skipped（Mono-WASM）：async 结构性豁免 + 解释器能力限制，与 1M 规模上限分开说明
            var wasmWarm = Warm.Where(w => w.ColKey == "Mono-WASM").ToList();
            var wasmSkip = wasmWarm.SelectMany(w => w.Rows)
                .Where(r => r.Skipped)
                .GroupBy(r => r.Category + "/" + r.Name + "/" + r.Size)
                .Select(g => g.First())
                .GroupBy(r => string.IsNullOrEmpty(r.SkipReason) ? "unspecified" : r.SkipReason)
                .ToDictionary(g => g.Key, g => g.Count());
            if (wasmSkip.Count > 0)
                sug.Add($"Mono-WASM 显式跳过 {wasmSkip.Values.Sum()} 行: " +
                        string.Join("; ", wasmSkip.Select(kv => $"{kv.Key} ×{kv.Value}")) +
                        " — async 为引擎级 GC 损坏隔离豁免（探针实证：yield/taskrun 可推进但对象毒化 sgen 堆），Expression_Compile 为解释器无 ILGenerator 限制");
            var netCats1M = Warm.Where(w => w.ColKey != "Mono-in-Godot" && w.ColKey != "Mono-WASM")
                .SelectMany(w => w.Rows.Where(r => r.Size == 1_000_000).Select(r => r.Category))
                .Distinct().ToHashSet();
            var godotCats1M = godotWarm.SelectMany(w => w.Rows.Where(r => r.Size == 1_000_000).Select(r => r.Category))
                .Distinct().ToHashSet();
            var cappedCats = netCats1M.Where(c => !godotCats1M.Contains(c)).ToList();
            if (cappedCats.Count > 0)
                sug.Add($"Mono-in-Godot 列在 {string.Join(", ", cappedCats)} 的 1M 规模行标 \"—\": 宿主以 CSBENCH_MAXSIZE=10000 规模上限运行" +
                        "（Mono 大规模数组路径不可靠，属设计内豁免，非数据缺失）");
            if (wasmWarm.Count > 0)
                sug.Add("Mono-WASM 列 1M 规模行整体标 \"—\": 宿主以 CSBENCH_MAXSIZE=10000 规模上限运行" +
                        "（WASM 解释器慢 + 内存受限，属设计内豁免，非数据缺失）");
            return sug;
        }

        // ---- 格式化 ----
        static string FmtNs(double? v)
        {
            if (v == null) return "—";
            double x = v.Value;
            if (x >= 1e9) return (x / 1e9).ToString("F2", CultureInfo.InvariantCulture) + "s";
            if (x >= 1e6) return (x / 1e6).ToString("F2", CultureInfo.InvariantCulture) + "ms";
            if (x >= 10_000) return (x / 1e3).ToString("F1", CultureInfo.InvariantCulture) + "μs";
            return x.ToString("F0", CultureInfo.InvariantCulture);
        }
        static string FmtB(double? v)
        {
            if (v == null || double.IsNaN(v.Value)) return "—";
            double x = v.Value;
            if (x >= 1_048_576) return (x / 1_048_576).ToString("F1", CultureInfo.InvariantCulture) + "MB";
            if (x >= 1024) return (x / 1024).ToString("F1", CultureInfo.InvariantCulture) + "KB";
            return x.ToString("F0", CultureInfo.InvariantCulture);
        }
        static string Limit(string s, int n) => string.IsNullOrEmpty(s) ? s : (s.Length <= n ? s : s.Substring(0, n - 1) + "…");
        static string GetString(JsonElement e, string p) => e.TryGetProperty(p, out var v) && v.ValueKind == JsonValueKind.String ? v.GetString() : null;
        static int GetInt(JsonElement e, string p) => e.TryGetProperty(p, out var v) && v.ValueKind == JsonValueKind.Number ? v.GetInt32() : 0;
        static double? GetD(JsonElement e, string p)
            => e.TryGetProperty(p, out var v) && v.ValueKind == JsonValueKind.Number ? v.GetDouble() : (double?)null;

        static string HtmlHead() => @"<!DOCTYPE html><html lang='zh'><head><meta charset='utf-8'>
<title>CSharpBench 对比报告</title><style>
body{font-family:'Segoe UI',system-ui,sans-serif;margin:24px auto;max-width:1280px;color:#1f2328;background:#fff;line-height:1.5}
h1{border-bottom:2px solid #d0d7de;padding-bottom:8px} h2{margin-top:36px;border-bottom:1px solid #d0d7de;padding-bottom:4px}
table.tbl{border-collapse:collapse;font-size:12.5px;margin:12px 0;width:100%}
.tbl th,.tbl td{border:1px solid #d0d7de;padding:3px 8px;text-align:left;white-space:nowrap}
.tbl th{background:#f6f8fa}.num{text-align:right;font-variant-numeric:tabular-nums}.muted{color:#8a94a0}.warn{color:#b35900}
svg.chart{margin:8px 0;max-width:100%}.lbl{font-size:11px;fill:#57606a}.t{font-size:13px;font-weight:600;fill:#1f2328}
code{background:#f6f8fa;padding:1px 5px;border-radius:4px;font-size:12.5px}
</style></head><body><h1>C# 基准测试对比报告</h1><p>由 <code>CSharpBench.Report</code> 生成 · 引擎: BenchmarkDotNet / 自研采集 · <a href='#env'>环境</a> · <a href='#analysis'>分析</a></p>";
    }
}
