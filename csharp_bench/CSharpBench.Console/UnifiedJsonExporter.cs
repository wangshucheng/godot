using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using BenchmarkDotNet.Environments;
using BenchmarkDotNet.Exporters;
using BenchmarkDotNet.Loggers;
using BenchmarkDotNet.Reports;

namespace CSharpBench.ConsoleHost
{
    /// <summary>
    /// 把 BenchmarkDotNet 的 Summary 直接导出为统一 csbench/1 schema JSON，
    /// 与 FallbackRunner / Godot 宿主输出同构，供 Report 汇总器合并。
    /// </summary>
    public class UnifiedJsonExporter : IExporter
    {
        public static string OutPath;      // Program 在运行前设置
        public static string Engine = "bdn";
        public static string Tfm = "unknown";
        public static string Profile = "quick";
        public static string Mode = "warm";
        public static double CpuSeconds;

        public string Name => "UnifiedJson";

        public System.Collections.Generic.IEnumerable<string> ExportToFiles(Summary summary, ILogger consoleLogger)
        {
            // 每个 Summary（每个基准类）写独立文件，避免多类运行时相互覆盖；
            // 报告端 glob 合并同引擎多文件。
            string basePath = OutPath.EndsWith(".json", StringComparison.OrdinalIgnoreCase)
                ? OutPath.Substring(0, OutPath.Length - 5) : OutPath;
            string outPath = basePath + "_" + DateTime.Now.ToString("HHmmssfff") + ".json";
            var sb = new StringBuilder(64 * 1024);
            sb.Append("{\"schema\":\"csbench/1\"");
            sb.Append(",\"engine\":\"").Append(Engine).Append("\"");
            sb.Append(",\"runtime\":\"").Append(Esc(System.Runtime.InteropServices.RuntimeInformation.FrameworkDescription)).Append("\"");
            sb.Append(",\"tfm\":\"").Append(Esc(Tfm)).Append("\"");
            sb.Append(",\"profile\":\"").Append(Esc(Profile)).Append("\"");
            sb.Append(",\"mode\":\"").Append(Esc(Mode)).Append("\"");
            sb.Append(",\"cpuSeconds\":").Append(F(CpuSeconds));
            sb.Append(",\"startedUtc\":\"").Append(DateTime.UtcNow.ToString("yyyy-MM-dd'T'HH:mm:ss'Z'")).Append("\"");
            sb.Append(",\"results\":[");

            bool first = true;
            foreach (var report in summary.Reports)
            {
                var bc = report.BenchmarkCase;
                string cat = bc.Descriptor.Type.Name;
                if (cat.StartsWith("Bench")) cat = cat.Substring(5); // BenchPrimitives -> Primitives
                string name = bc.Descriptor.WorkloadMethod.Name;

                int size = 0;
                try
                {
                    foreach (var p in bc.Parameters.Items)
                        if (p.Name == "Size") { size = Convert.ToInt32(p.Value, CultureInfo.InvariantCulture); break; }
                }
                catch { }

                if (!first) sb.Append(',');
                first = false;

                var st = report.ResultStatistics;
                if (st == null)
                {
                    sb.Append("{\"category\":\"").Append(Esc(cat)).Append("\",\"name\":\"").Append(Esc(name))
                      .Append("\",\"size\":").Append(size)
                      .Append(",\"iters\":0,\"meanNs\":null,\"stddevNs\":null,\"medianNs\":null,\"minNs\":null")
                      .Append(",\"allocatedBytesPerOp\":null,\"gen0Per1kOps\":null,\"gen1Per1kOps\":null,\"gen2Per1kOps\":null")
                      .Append(",\"cpuFraction\":null,\"skipped\":true,\"skipReason\":\"bdn-no-result\"}");
                    continue;
                }

                double alloc = double.NaN, gen0 = double.NaN, gen1 = double.NaN, gen2 = double.NaN;
                try
                {
                    foreach (var kv in report.Metrics)
                    {
                        string id = (kv.Key ?? "").ToLowerInvariant();
                        string id2 = (kv.Value.Descriptor?.Id ?? "").ToLowerInvariant();
                        double v = kv.Value.Value;
                        if (id.Contains("allocated") || id2.Contains("allocated")) alloc = v;
                        else if (id.Contains("gen0") || id2.Contains("gen0")) gen0 = v;
                        else if ((id.Contains("gen1") || id2.Contains("gen1")) && !id.Contains("gen1x")) gen1 = v;
                        else if (id.Contains("gen2") || id2.Contains("gen2")) gen2 = v;
                    }
                }
                catch { }

                sb.Append("{\"category\":\"").Append(Esc(cat)).Append("\",\"name\":\"").Append(Esc(name))
                  .Append("\",\"size\":").Append(size)
                  .Append(",\"iters\":").Append((int)st.N)
                  .Append(",\"meanNs\":").Append(F(st.Mean))
                  .Append(",\"stddevNs\":").Append(F(st.StandardDeviation))
                  .Append(",\"medianNs\":").Append(F(st.Median))
                  .Append(",\"minNs\":").Append(F(st.Min))
                  .Append(",\"allocatedBytesPerOp\":").Append(F(alloc))
                  .Append(",\"gen0Per1kOps\":").Append(F(gen0))
                  .Append(",\"gen1Per1kOps\":").Append(F(gen1))
                  .Append(",\"gen2Per1kOps\":").Append(F(gen2))
                  .Append(",\"cpuFraction\":null,\"skipped\":false,\"skipReason\":null}");
            }
            sb.Append("]}");

            File.WriteAllText(outPath, sb.ToString());
            Console.WriteLine("[csbench] unified json written: " + outPath + " (" + summary.Reports.Count() + " rows)");
            yield return outPath;
        }

        public void ExportToLog(Summary summary, ILogger logger) { }

        private static string F(double v)
        {
            if (double.IsNaN(v) || double.IsInfinity(v)) return "null";
            return v.ToString("R", CultureInfo.InvariantCulture);
        }
        private static string Esc(string s)
        {
            if (s == null) return "";
            return s.Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("\r", " ").Replace("\n", " ");
        }
    }
}
