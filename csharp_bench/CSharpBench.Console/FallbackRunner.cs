using System;
using System.Diagnostics;
using System.IO;
using CSharpBench.Core;

namespace CSharpBench.ConsoleHost
{
    /// <summary>
    /// 进程内兜底采集器（不依赖 BenchmarkDotNet 工具链）：
    /// 1) net48 上 BDN 工具链失败时的 warm 兜底；
    /// 2) 所有 TFM 的 cold 模式（新进程首调，无 warmup）。
    /// 输出与 UnifiedJsonExporter 相同的 csbench/1 schema。
    /// </summary>
    public static class FallbackRunner
    {
        /// <summary>
        /// 环境变量 CSBENCH_SKIP：逗号分隔的跳过项。
        /// 支持 "Category"（整类）或 "Category/Name"（单项）。
        /// 用于规避运行时级崩溃（如 netcoreapp3.1 上 SpanMemory 栈溢出）——
        /// 这类崩溃无法用 try/catch 捕获，只能预防性跳过并如实标注。
        /// </summary>
        static bool ShouldSkip(string cat, string name)
        {
            string s = Environment.GetEnvironmentVariable("CSBENCH_SKIP");
            if (string.IsNullOrEmpty(s)) return false;
            foreach (var tok in s.Split(','))
            {
                string t = tok.Trim();
                if (t.Length == 0) continue;
                if (t.IndexOf('/') < 0)
                {
                    if (t.Equals(cat, StringComparison.OrdinalIgnoreCase)) return true;
                }
                else
                {
                    int k = t.IndexOf('/');
                    if (t.Substring(0, k).Equals(cat, StringComparison.OrdinalIgnoreCase) &&
                        t.Substring(k + 1).Equals(name, StringComparison.OrdinalIgnoreCase)) return true;
                }
            }
            return false;
        }

        public static int Run(string outPath, string tfm, string profile, bool cold, string categoryFilter)
        {
            var proc = Process.GetCurrentProcess();
            var w = new BenchJsonWriter("fallback",
                System.Runtime.InteropServices.RuntimeInformation.FrameworkDescription,
                tfm, profile, cold ? "cold" : "warm");

            int done = 0, skipped = 0;
            foreach (var wl in Workloads.All)
            {
                if (categoryFilter != null && !wl.Category.Equals(categoryFilter, StringComparison.OrdinalIgnoreCase)) continue;
                if (ShouldSkip(wl.Category, wl.Name))
                {
                    w.AddRow(wl.Category, wl.Name, wl.Sizes[0], new Measured(), true, "skipped: runtime-crash risk (CSBENCH_SKIP)");
                    skipped++;
                    continue;
                }
                if (wl.Run == null)
                {
                    // 类目加载失败占位（如 Mono 缺 System.Memory）
                    w.AddRow(wl.Category, wl.Name, wl.Sizes[0], new Measured(), true, "category-load-failed");
                    skipped++;
                    continue;
                }
                try
                {
                    if (cold)
                    {
                        var m = RunnerCore.MeasureCold(wl.Run, 100); // 冷启动固定 size=100
                        w.AddRow(wl.Category, wl.Name, 100, m, false, null);
                    }
                    else
                    {
                        foreach (int size in wl.Sizes)
                        {
                            var m = RunnerCore.Measure(wl.Run, size);
                            w.AddRow(wl.Category, wl.Name, size, m, false, null);
                        }
                    }
                    done++;
                }
                catch (Exception ex)
                {
                    w.AddRow(wl.Category, wl.Name, wl.Sizes[0], new Measured(), true, ex.GetType().Name);
                    skipped++;
                }
            }

            w.CpuSeconds = proc.TotalProcessorTime.TotalSeconds;
            File.WriteAllText(outPath, w.ToJson());
            Console.WriteLine($"[csbench] fallback ({(cold ? "cold" : "warm")}) written: {outPath} (ok={done}, skipped={skipped})");
            return 0;
        }
    }
}
