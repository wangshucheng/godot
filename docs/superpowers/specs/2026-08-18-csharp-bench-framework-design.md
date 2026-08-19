# C# 基准测试框架设计（CSharpBench）

> 日期: 2026-08-18
> 状态: 已确认（用户批准）
> 定位: 扩展现有 Godot 基准（csharp_test/Benchmark.cs）为双宿主共享工作负载库

## 1. 目标

- 以成熟开源工具（BenchmarkDotNet, .NET 官方生态）为核心测量引擎
- 覆盖 C# 核心技术点: 基础类型 / 集合 / LINQ / 异步 / 反射 / 委托 / Span / GC 分配 / 综合场景
- 多 .NET 版本对比: net48 / netcoreapp3.1 / net6.0 / net7.0 / Mono 6.12 (Godot 内嵌)
- 冷启动 + 热启动; 三档数据规模 (100 / 10,000 / 1,000,000)
- 环境一致、可重复: 钉死 SDK、包版本、GC 模式、随机种子; 重复迭代取统计
- 报告: HTML(内嵌 SVG 图表) + Markdown; 含环境、方法说明、均值±标准差、分配字节、CPU 时间、瓶颈分析与优化建议

## 2. 架构（双宿主共享库）

```
CSharpBench.Core (netstandard2.0 + System.Memory)   ~106 项工作负载, 一处编写
        ├─ Godot 宿主: csharp_test/CSharpBenchRunner.cs (Mono 6.12, 多帧状态机, 自研 RunnerCore 采集)
        └─ Console 宿主: CSharpBench.Console (net48;netcoreapp3.1;net6.0;net7.0)
                ├─ 主引擎: BenchmarkDotNet + InProcessEmitToolchain + MemoryDiagnoser + 自定义 UnifiedJsonExporter
                └─ 兜底: FallbackRunner (net48 工具链失败时进程内采集, 同 JSON schema)
两宿主输出统一 schema JSON → CSharpBench.Report 合并 → HTML + MD
```

## 3. 目录

```
godot4.7_mono/
├── csharp_test/                      # 现有 Godot 项目 (新增引用 Core)
│   ├── CSharpBenchRunner.cs, csbench_test.tscn
└── csharp_bench/
    ├── global.json (SDK 7.0.401)
    ├── CSharpBench.Core/             (9 个类别文件 + WorkloadRegistry + RunnerCore + JsonWriter)
    ├── CSharpBench.Console/          (Program + Benchmarks 包装 + UnifiedJsonExporter + FallbackRunner)
    ├── CSharpBench.Report/           (net7.0, System.Text.Json, 零前端依赖 SVG)
    ├── run-bench.ps1                 (restore→build→4 TFM→Godot→报告)
    └── BenchResults/                 (统一 schema JSON + 报告)
```

## 4. 基准清单（~106 项 × 规模）

| 类别 | 项数 | 规模 |
|---|---:|---|
| Primitives | 14 | 100/10k/1M |
| Collections | 16 | 100/10k/1M |
| Linq | 14 | 100/10k/1M |
| Async | 12 | 100/10k |
| Reflection | 10 | 100/10k |
| Delegates | 10 | 100/10k/1M |
| SpanMemory | 10 | 100/10k/1M |
| GcAlloc | 10 | 100/10k |
| Scenarios | 8 | 100/10k/1M |

约定: 工作负载签名 `Action<int size>`，一次调用执行 size 个基本操作（或处理 size 元数据集）；报告同时给 ns/调用 与 ns/元素。

## 5. 冷/热启动

- 热启动: BDN pilot+warmup+稳态迭代（quick 档 3+3）; RunnerCore 自适应（warmup 3 + ≥5 次迭代且 ≥150ms）
- 冷启动: FallbackRunner/Godot 以 `--mode cold` 在新进程首调各负载（size=100，无预热），测 JIT/静态构造首触发；文档注明"进程级冷启动，含跨项共享 JIT 预热偏差"

## 6. 执行规范

- 档位: `--profile quick` (ShortRun 3+3, InProcess) / `--profile accurate` (BDN Default 全迭代)
- 诊断: MemoryDiagnoser（分配/Gen0-2）+ ThreadingDiagnoser(netcoreapp3.1+) + 进程级 TotalProcessorTime
- 环境钉死: global.json、packages.lock.json、Release 编译、固定种子、Workstation+Concurrent GC（可参数切 Server）
- 跨版本 API 差异: 工作负载内 #if / try-catch，失败标记 `skipped` 不中断
- 结果 sink: 静态字段接收返回值防 JIT 消除

## 7. JSON schema（统一）

```json
{ "schema":"csbench/1", "engine":"bdn|fallback|godot", "runtime":"...", "tfm":"...",
  "profile":"quick", "mode":"warm|cold", "cpuSeconds":123.4, "results":[
  { "category":"Linq", "name":"OrderBy", "size":10000, "iters":3,
    "meanNs":123.4, "stddevNs":5.6, "medianNs":120.0, "minNs":118.0,
    "allocatedBytesPerOp":64, "gen0Per1kOps":0.5, "skipped":false, "skipReason":null }]}
```

## 8. 报告

- HTML: 环境指纹（CPU/OS/运行时/编译器）、方法说明、每类 SVG 柱状图（ns 对数轴、5 运行时并排）、分配字节对比图、全量表（mean±σ/median/min/alloc/gen0）、跨版本回归（net7 vs net48 倍率）、top-10 分配大户、规则库优化建议
- MD: 同数据表格版，可贴项目文档

## 9. 风险与缓解

| 风险 | 缓解 |
|---|---|
| net48 BDN 工具链失败 | FallbackRunner 进程内兜底，同 schema |
| NuGet 无网络 | 早期 restore 验证；失败降级仅 net6/7+Godot |
| 快速档总时长 | InProcess 工具链 + 分 TFM 串行 + 单项失败继续 |
| Mono 加载 System.Memory 失败 | Span 类 try-catch 标 skipped（版本兼容性发现，非错误） |
| GPU 驱动在 Godot 退出时崩溃 | JSON 每单元增量落盘，崩溃不丢数据 |

## 10. 复现

```powershell
cd godot4.7_mono/csharp_bench
.\run-bench.ps1 -Profile quick          # 4 TFM + Godot + 报告
.\run-bench.ps1 -Profile quick -SkipGodot
```
