using System;
using System.Collections.Generic;
using System.Diagnostics;

namespace CSharpBench.Core
{
    /// <summary>单次测量结果（进程内自适应采集引擎，供 FallbackRunner 与 Godot 宿主共用）。</summary>
    public struct Measured
    {
        public int Iters;
        public double MeanNs, StddevNs, MedianNs, MinNs, MaxNs;
        public double AllocatedBytesPerOp;
        public double Gen0Per1kOps, Gen1Per1kOps, Gen2Per1kOps;
        public double CpuFraction; // 测量期间 CPU 时间 / 墙钟时间
    }

    /// <summary>
    /// 自适应测量引擎：warmup 后重复调用直到达到时间预算或迭代上限，
    /// 输出每次调用的耗时统计 + GC 分配/回收近似值（netstandard2.0 兼容，无 ETW）。
    /// </summary>
    public static class RunnerCore
    {
        // ---- Process API 防御（WASM 单线程解释器无进程信息，Process.GetCurrentProcess()
        //      或 TotalProcessorTime 可能抛 PlatformNotSupportedException）----
        private static readonly Process _proc = TryGetProcess();
        private static Process TryGetProcess()
        {
            try { return Process.GetCurrentProcess(); }
            catch { return null; }
        }

        /// <summary>防御式读取进程 CPU 时间（秒）。不可用平台返回 0。</summary>
        public static double ReadCpuSeconds()
        {
            if (_proc == null) return 0;
            try { return _proc.TotalProcessorTime.TotalSeconds; }
            catch { return 0; }
        }

        /// <summary>由每迭代耗时（ns）数组计算统计量（iters/mean/median/min/max/stddev），供同步与异步驱动路径共用。</summary>
        public static Measured BuildStats(double[] t)
        {
            var m = new Measured();
            int n = t.Length;
            if (n == 0) return m;
            Array.Sort(t);
            m.Iters = n;
            double sum = 0; for (int i = 0; i < n; i++) sum += t[i];
            m.MeanNs = sum / n;
            m.MinNs = t[0]; m.MaxNs = t[n - 1];
            m.MedianNs = (n % 2 == 1) ? t[n / 2] : (t[n / 2 - 1] + t[n / 2]) / 2.0;
            double var = 0; for (int i = 0; i < n; i++) { double d = t[i] - m.MeanNs; var += d * d; }
            m.StddevNs = n > 1 ? Math.Sqrt(var / (n - 1)) : 0;
            return m;
        }

        public static Measured Measure(Action<int> run, int size,
            int warmup = 3, int minIters = 5, int maxIters = 24, long budgetMs = 150)
        {
            var m = new Measured();
            // ---- warmup（同时触发 JIT / 类静态构造）----
            for (int i = 0; i < warmup; i++) run(size);

            // ---- measure ----
            var times = new List<double>(maxIters);
            var sw = Stopwatch.StartNew();
            double cpu0 = ReadCpuSeconds();
            long mem0 = GC.GetTotalMemory(true);
            int g0a = GC.CollectionCount(0), g1a = GC.CollectionCount(1), g2a = GC.CollectionCount(2);

            for (int i = 0; i < maxIters; i++)
            {
                var one = Stopwatch.StartNew();
                run(size);
                one.Stop();
                times.Add(one.Elapsed.TotalMilliseconds * 1e6); // ns
                if (times.Count >= minIters && sw.ElapsedMilliseconds >= budgetMs) break;
            }

            double cpu1 = ReadCpuSeconds();
            long mem1 = GC.GetTotalMemory(false);
            int g0b = GC.CollectionCount(0), g1b = GC.CollectionCount(1), g2b = GC.CollectionCount(2);

            // ---- stats（复用 BuildStats，统计口径与异步驱动路径一致）----
            int n = times.Count;
            var stats = BuildStats(times.ToArray());
            m.Iters = stats.Iters;
            m.MeanNs = stats.MeanNs; m.MedianNs = stats.MedianNs; m.MinNs = stats.MinNs;
            m.MaxNs = stats.MaxNs; m.StddevNs = stats.StddevNs;

            m.AllocatedBytesPerOp = Math.Max(0, mem1 - mem0) / (double)n; // 近似：净分配（GC 干扰下偏保守）
            m.Gen0Per1kOps = (g0b - g0a) * 1000.0 / n;
            m.Gen1Per1kOps = (g1b - g1a) * 1000.0 / n;
            m.Gen2Per1kOps = (g2b - g2a) * 1000.0 / n;
            m.CpuFraction = sw.Elapsed.TotalSeconds > 0
                ? (cpu1 - cpu0) / sw.Elapsed.TotalSeconds : 0;
            return m;
        }

        /// <summary>冷启动测量：新进程中首个调用（无 warmup），含 JIT/静态构造首触发。</summary>
        public static Measured MeasureCold(Action<int> run, int size)
        {
            var m = new Measured();
            var one = Stopwatch.StartNew();
            run(size);
            one.Stop();
            m.Iters = 1;
            m.MeanNs = m.MedianNs = m.MinNs = m.MaxNs = one.Elapsed.TotalMilliseconds * 1e6;
            m.StddevNs = 0;
            return m;
        }
    }

    /// <summary>统一 schema JSON 写出器（零依赖，手工序列化；Fallback/Godot 共用）。</summary>
    public sealed class BenchJsonWriter
    {
        private readonly List<string> _rows = new List<string>();
        public readonly string Engine, RuntimeDesc, Tfm, Profile, Mode;
        public double CpuSeconds;
        public string StartedUtc = DateTime.UtcNow.ToString("yyyy-MM-dd'T'HH:mm:ss'Z'");

        public BenchJsonWriter(string engine, string runtimeDesc, string tfm, string profile, string mode)
        {
            Engine = engine; RuntimeDesc = runtimeDesc; Tfm = tfm; Profile = profile; Mode = mode;
        }

        public void AddRow(string category, string name, int size, Measured m, bool skipped, string skipReason)
        {
            _rows.Add(BuildRow(category, name, size, m, skipped, skipReason));
        }

        /// <summary>
        /// 构建单行结果 JSON（不含外层数组）。提取为公共静态方法：
        /// Godot 宿主可在 WASM 等受限平台用它直接构建行（如异步驱动路径），
        /// 保证行 schema 与同步路径完全一致。
        /// </summary>
        public static string BuildRow(string category, string name, int size, Measured m, bool skipped, string skipReason)
        {
            return string.Format(
                System.Globalization.CultureInfo.InvariantCulture,
                "{{\"category\":\"{0}\",\"name\":\"{1}\",\"size\":{2},\"iters\":{3}," +
                "\"meanNs\":{4},\"stddevNs\":{5},\"medianNs\":{6},\"minNs\":{7}," +
                "\"allocatedBytesPerOp\":{8},\"gen0Per1kOps\":{9},\"gen1Per1kOps\":{10},\"gen2Per1kOps\":{11}," +
                "\"cpuFraction\":{12},\"skipped\":{13},\"skipReason\":{14}}}",
                Esc(category), Esc(name), size, m.Iters,
                F(m.MeanNs), F(m.StddevNs), F(m.MedianNs), F(m.MinNs),
                F(m.AllocatedBytesPerOp), F(m.Gen0Per1kOps), F(m.Gen1Per1kOps), F(m.Gen2Per1kOps),
                F(m.CpuFraction), skipped ? "true" : "false", skipReason == null ? "null" : "\"" + Esc(skipReason) + "\"");
        }

        /// <summary>
        /// 原样携带一行（用于断点续跑：上一进程已落盘的行在新进程中原样重写，
        /// 不重新测量）。行格式由本类产生，不含嵌套对象，可安全透传。
        /// </summary>
        public void AddRawRow(string rawJson)
        {
            if (!string.IsNullOrEmpty(rawJson)) _rows.Add(rawJson);
        }

        public string ToJson()
        {
            var sb = new System.Text.StringBuilder();
            sb.Append("{\"schema\":\"csbench/1\"");
            sb.Append(",\"engine\":\"").Append(Esc(Engine)).Append("\"");
            sb.Append(",\"runtime\":\"").Append(Esc(RuntimeDesc)).Append("\"");
            sb.Append(",\"tfm\":\"").Append(Esc(Tfm)).Append("\"");
            sb.Append(",\"profile\":\"").Append(Esc(Profile)).Append("\"");
            sb.Append(",\"mode\":\"").Append(Esc(Mode)).Append("\"");
            sb.Append(",\"cpuSeconds\":").Append(F(CpuSeconds));
            sb.Append(",\"startedUtc\":\"").Append(StartedUtc).Append("\"");
            sb.Append(",\"results\":[");
            sb.Append(string.Join(",", _rows));
            sb.Append("]}");
            return sb.ToString();
        }

        private static string F(double v)
        {
            if (double.IsNaN(v) || double.IsInfinity(v)) return "null";
            return v.ToString("R", System.Globalization.CultureInfo.InvariantCulture);
        }
        private static string Esc(string s)
        {
            if (s == null) return "";
            return s.Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("\r", " ").Replace("\n", " ");
        }
    }
}
