using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;
using BenchmarkDotNet.Configs;
using BenchmarkDotNet.Diagnosers;
using BenchmarkDotNet.Jobs;
using BenchmarkDotNet.Running;
using BenchmarkDotNet.Toolchains.InProcess.Emit;

namespace CSharpBench.ConsoleHost
{
    /// <summary>
    /// CSharpBench 控制台宿主（多 TFM: net48;netcoreapp3.1;net6.0;net7.0）。
    ///
    /// 用法:
    ///   dotnet CSharpBench.Console.dll --profile quick|accurate --mode warm|cold
    ///        --engine auto|bdn|fallback --category &lt;Name&gt; --out &lt;dir&gt;
    /// </summary>
    internal static class Program
    {
        static string _profile = "quick";
        static string _mode = "warm";
        static string _engine = "auto";
        static string _category;
        static string _outDir;

        static int Main(string[] args)
        {
            for (int i = 0; i < args.Length; i++)
            {
                switch (args[i])
                {
                    case "--profile": _profile = args[++i]; break;
                    case "--mode": _mode = args[++i]; break;
                    case "--engine": _engine = args[++i]; break;
                    case "--category": _category = args[++i]; break;
                    case "--out": _outDir = args[++i]; break;
                }
            }
            if (_outDir == null) _outDir = FindDefaultOutDir();
            Directory.CreateDirectory(_outDir);

            string tfm = DetectTfm();
            string runtime = RuntimeInformation.FrameworkDescription;
            Console.WriteLine($"[csbench] tfm={tfm} runtime={runtime} profile={_profile} mode={_mode} engine={_engine} out={_outDir}");

            var proc = Process.GetCurrentProcess();
            var cpu0 = proc.TotalProcessorTime;

            int rc;
            if (_mode == "cold" || _engine == "fallback")
            {
                // 冷启动一律走进程内首调采集（BDN 的 ColdStart 策略需逐迭代新进程，成本过高）
                rc = FallbackRunner.Run(Path.Combine(_outDir, $"fallback_{tfm}_{_mode}.json"),
                                        tfm, _profile, _mode == "cold", _category);
            }
            else
            {
                rc = RunBdn(tfm, _outDir);
                if (rc != 0 && _engine == "auto")
                {
                    Console.WriteLine("[csbench] BDN failed -> falling back to in-process runner");
                    rc = FallbackRunner.Run(Path.Combine(_outDir, $"fallback_{tfm}_warm.json"),
                                            tfm, _profile, false, _category);
                }
            }

            Console.WriteLine($"[csbench] done rc={rc} cpuSeconds={(proc.TotalProcessorTime - cpu0).TotalSeconds:F1}");
            return rc;
        }

        static int RunBdn(string tfm, string outDir)
        {
            try
            {
                var job = (_profile == "quick")
                    ? Job.ShortRun.WithWarmupCount(3).WithIterationCount(3)
                    : Job.Default;
                job = job.WithToolchain(InProcessEmitToolchain.Instance)
                         .WithId($"{tfm}-{_profile}");

                var cfg = ManualConfig.Create(DefaultConfig.Instance)
                    .AddJob(job)
                    .AddDiagnoser(MemoryDiagnoser.Default)
                    .AddExporter(new UnifiedJsonExporter())
                    .WithArtifactsPath(Path.Combine(outDir, "bdn_artifacts"));
#if NETCOREAPP3_1_OR_GREATER
                cfg = cfg.AddDiagnoser(ThreadingDiagnoser.Default);
#endif
                UnifiedJsonExporter.OutPath = Path.Combine(outDir, $"bdn_{tfm}_warm.json");
                UnifiedJsonExporter.Tfm = tfm;
                UnifiedJsonExporter.Profile = _profile;
                UnifiedJsonExporter.Mode = "warm";
                UnifiedJsonExporter.CpuSeconds = Process.GetCurrentProcess().TotalProcessorTime.TotalSeconds;

                var allTypes = new[]
                {
                    typeof(BenchPrimitives), typeof(BenchCollections), typeof(BenchLinq),
                    typeof(BenchAsync), typeof(BenchReflection), typeof(BenchDelegates),
                    typeof(BenchSpanMemory), typeof(BenchGcAlloc), typeof(BenchScenarios)
                };
                // CSBENCH_SKIP 整类排除（与 FallbackRunner 一致；进程级崩溃无法 try/catch）
                string skipEnv = Environment.GetEnvironmentVariable("CSBENCH_SKIP") ?? "";
                var skipCats = new System.Collections.Generic.HashSet<string>(
                    skipEnv.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries));
                allTypes = allTypes.Where(t =>
                {
                    string cat = t.Name.StartsWith("Bench") ? t.Name.Substring(5) : t.Name;
                    return !skipCats.Contains(cat.Trim());
                }).ToArray();

                Type[] types = _category == null ? allTypes
                    : allTypes.Where(t => t.Name.Equals("Bench" + _category, StringComparison.OrdinalIgnoreCase)).ToArray();
                if (types.Length == 0) { Console.WriteLine($"[csbench] unknown category: {_category}"); return 2; }

                var summaries = BenchmarkRunner.Run(types, cfg);
                return summaries.Any(s => s.HasCriticalValidationErrors) ? 1 : 0;
            }
            catch (Exception ex)
            {
                Console.WriteLine("[csbench] BDN exception: " + ex.GetType().Name + ": " + ex.Message);
                return 1;
            }
        }

        static string DetectTfm()
        {
            var attr = (TargetFrameworkAttribute)Assembly.GetExecutingAssembly()
                .GetCustomAttribute(typeof(TargetFrameworkAttribute));
            string fn = attr?.FrameworkName ?? ""; // e.g. ".NETCoreApp,Version=v7.0" / ".NETFramework,Version=v4.8"
            if (fn.Contains(".NETFramework"))
            {
                var v = fn.Substring(fn.IndexOf('v') + 1); // 4.8
                return "net" + v.Replace(".", "");
            }
            int s = fn.IndexOf("Version=v");
            if (s > 0)
            {
                string ver = fn.Substring(s + 9);           // "3.1" 或 "7.0"
                if (ver.EndsWith(".0")) ver = ver.Substring(0, ver.Length - 2); // 7.0 -> 7
                if (ver == "3.1") return "netcoreapp3.1";
                return "net" + ver + ".0";                  // 7 -> net7.0
            }
            return "unknown";
        }

        static string FindDefaultOutDir()
        {
            // 从输出目录向上找 csharp_bench 根
            var dir = new DirectoryInfo(AppContext.BaseDirectory);
            while (dir != null)
            {
                if (dir.Name.Equals("csharp_bench", StringComparison.OrdinalIgnoreCase))
                    return Path.Combine(dir.FullName, "BenchResults");
                dir = dir.Parent;
            }
            return Path.Combine(Directory.GetCurrentDirectory(), "BenchResults");
        }
    }
}
