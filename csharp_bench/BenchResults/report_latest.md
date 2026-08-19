# C# 基准测试对比报告

> 生成时间: 2026-08-19 20:47:11 | 引擎: BenchmarkDotNet (bdn) / 自研进程内采集 (fallback/godot)

## 1. 测试环境与数据来源

| 列 | 引擎 | 运行时 | TFM | Profile | 模式 | CPU 时间(s) | 来源 |
|---|---|---|---|---|---|---|---|
| net48 | bdn | .NET Framework 4.8.9339.0 | net48 | accurate | warm | 0.1 | bdn_net48_warm_103410388.json |
| net48 | bdn | .NET Framework 4.8.9339.0 | net48 | accurate | warm | 0.1 | bdn_net48_warm_104803846.json |
| net48 | bdn | .NET Framework 4.8.9339.0 | net48 | accurate | warm | 0.1 | bdn_net48_warm_110211513.json |
| net48 | bdn | .NET Framework 4.8.9339.0 | net48 | accurate | warm | 0.1 | bdn_net48_warm_111124447.json |
| net48 | bdn | .NET Framework 4.8.9339.0 | net48 | accurate | warm | 0.1 | bdn_net48_warm_111904320.json |
| net48 | bdn | .NET Framework 4.8.9339.0 | net48 | accurate | warm | 0.1 | bdn_net48_warm_112937489.json |
| net48 | bdn | .NET Framework 4.8.9339.0 | net48 | accurate | warm | 0.1 | bdn_net48_warm_114038333.json |
| net48 | bdn | .NET Framework 4.8.9339.0 | net48 | accurate | warm | 0.1 | bdn_net48_warm_115106732.json |
| net48 | bdn | .NET Framework 4.8.9339.0 | net48 | accurate | warm | 0.1 | bdn_net48_warm_202041128.json |
| net6.0 | bdn | .NET 6.0.22 | net6.0 | accurate | warm | 0.1 | bdn_net6.0_warm_132306760.json |
| net6.0 | bdn | .NET 6.0.22 | net6.0 | accurate | warm | 0.1 | bdn_net6.0_warm_133411182.json |
| net6.0 | bdn | .NET 6.0.22 | net6.0 | accurate | warm | 0.1 | bdn_net6.0_warm_134932784.json |
| net6.0 | bdn | .NET 6.0.22 | net6.0 | accurate | warm | 0.1 | bdn_net6.0_warm_135958042.json |
| net6.0 | bdn | .NET 6.0.22 | net6.0 | accurate | warm | 0.1 | bdn_net6.0_warm_140629745.json |
| net6.0 | bdn | .NET 6.0.22 | net6.0 | accurate | warm | 0.1 | bdn_net6.0_warm_141610262.json |
| net6.0 | bdn | .NET 6.0.22 | net6.0 | accurate | warm | 0.1 | bdn_net6.0_warm_142421356.json |
| net6.0 | bdn | .NET 6.0.22 | net6.0 | accurate | warm | 0.1 | bdn_net6.0_warm_143338398.json |
| net6.0 | bdn | .NET 6.0.22 | net6.0 | accurate | warm | 0.1 | bdn_net6.0_warm_204013229.json |
| net7.0 | bdn | .NET 7.0.11 | net7.0 | accurate | warm | 0.1 | bdn_net7.0_warm_144751145.json |
| net7.0 | bdn | .NET 7.0.11 | net7.0 | accurate | warm | 0.1 | bdn_net7.0_warm_145847214.json |
| net7.0 | bdn | .NET 7.0.11 | net7.0 | accurate | warm | 0.1 | bdn_net7.0_warm_151249104.json |
| net7.0 | bdn | .NET 7.0.11 | net7.0 | accurate | warm | 0.1 | bdn_net7.0_warm_152146024.json |
| net7.0 | bdn | .NET 7.0.11 | net7.0 | accurate | warm | 0.1 | bdn_net7.0_warm_152839455.json |
| net7.0 | bdn | .NET 7.0.11 | net7.0 | accurate | warm | 0.1 | bdn_net7.0_warm_153923012.json |
| net7.0 | bdn | .NET 7.0.11 | net7.0 | accurate | warm | 0.1 | bdn_net7.0_warm_194500769.json |
| net7.0 | bdn | .NET 7.0.11 | net7.0 | accurate | warm | 0.1 | bdn_net7.0_warm_200118722.json |
| net7.0 | bdn | .NET 7.0.11 | net7.0 | accurate | warm | 0.1 | bdn_net7.0_warm_201053552.json |
| netcoreapp3.1 | bdn | .NET Core 3.1.2 | netcoreapp3.1 | accurate | warm | 0.1 | bdn_netcoreapp3.1_warm_120428503.json |
| netcoreapp3.1 | bdn | .NET Core 3.1.2 | netcoreapp3.1 | accurate | warm | 0.1 | bdn_netcoreapp3.1_warm_121441776.json |
| netcoreapp3.1 | bdn | .NET Core 3.1.2 | netcoreapp3.1 | accurate | warm | 0.1 | bdn_netcoreapp3.1_warm_122759192.json |
| netcoreapp3.1 | bdn | .NET Core 3.1.2 | netcoreapp3.1 | accurate | warm | 0.1 | bdn_netcoreapp3.1_warm_123559903.json |
| netcoreapp3.1 | bdn | .NET Core 3.1.2 | netcoreapp3.1 | accurate | warm | 0.1 | bdn_netcoreapp3.1_warm_124254654.json |
| netcoreapp3.1 | bdn | .NET Core 3.1.2 | netcoreapp3.1 | accurate | warm | 0.1 | bdn_netcoreapp3.1_warm_125256090.json |
| netcoreapp3.1 | bdn | .NET Core 3.1.2 | netcoreapp3.1 | accurate | warm | 0.1 | bdn_netcoreapp3.1_warm_125936825.json |
| netcoreapp3.1 | bdn | .NET Core 3.1.2 | netcoreapp3.1 | accurate | warm | 0.1 | bdn_netcoreapp3.1_warm_130932498.json |
| netcoreapp3.1 | bdn | .NET Core 3.1.2 | netcoreapp3.1 | accurate | warm | 0.1 | bdn_netcoreapp3.1_warm_203029131.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | warm | 0.1 | csbench_godot_Async.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | warm | 0.1 | csbench_godot_Collections.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | warm | 0.1 | csbench_godot_Delegates.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | warm | 0.1 | csbench_godot_GcAlloc.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | warm | 0.1 | csbench_godot_Linq.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | warm | 0.1 | csbench_godot_Primitives.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | warm | 0.1 | csbench_godot_Reflection.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | warm | 0.1 | csbench_godot_Scenarios.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | warm | 0.1 | csbench_godot_SpanMemory.json |
| net48 | fallback | .NET Framework 4.8.9339.0 | net48 | quick | warm | 47.6 | fallback_net48_warm.json |
| net6.0 | fallback | .NET 6.0.22 | net6.0 | quick | warm | 35.4 | fallback_net6.0_warm.json |
| net7.0 | fallback | .NET 7.0.11 | net7.0 | quick | warm | 34.3 | fallback_net7.0_warm.json |
| netcoreapp3.1 | fallback | .NET Core 3.1.2 | netcoreapp3.1 | quick | warm | 35.8 | fallback_netcoreapp3.1_warm.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | cold | 0.0 | csbench_godot_Async_cold.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | cold | 0.0 | csbench_godot_cold.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | cold | 0.0 | csbench_godot_Collections_cold.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | cold | 0.0 | csbench_godot_Delegates_cold.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | cold | 0.0 | csbench_godot_GcAlloc_cold.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | cold | 0.0 | csbench_godot_Linq_cold.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | cold | 0.0 | csbench_godot_Primitives_cold.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | cold | 0.0 | csbench_godot_Reflection_cold.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | cold | 0.0 | csbench_godot_Scenarios_cold.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | cold | 0.0 | csbench_godot_SpanMemory_cold.json |
| net48 | fallback | .NET Framework 4.8.9339.0 | net48 | accurate | cold | 0.1 | fallback_net48_cold.json |
| net48 | fallback | .NET Framework 4.8.9339.0 | net48 | accurate | cold | 0.1 | fallback_net48_cold_SpanMemory.json |
| net6.0 | fallback | .NET 6.0.22 | net6.0 | accurate | cold | 0.2 | fallback_net6.0_cold.json |
| net6.0 | fallback | .NET 6.0.22 | net6.0 | accurate | cold | 0.1 | fallback_net6.0_cold_SpanMemory.json |
| net7.0 | fallback | .NET 7.0.11 | net7.0 | accurate | cold | 0.2 | fallback_net7.0_cold.json |
| netcoreapp3.1 | fallback | .NET Core 3.1.2 | netcoreapp3.1 | accurate | cold | 0.2 | fallback_netcoreapp3.1_cold.json |
| netcoreapp3.1 | fallback | .NET Core 3.1.2 | netcoreapp3.1 | accurate | cold | 0.1 | fallback_netcoreapp3.1_cold_SpanMemory.json |

**方法说明**: 工作负载统一签名 `Action<size>`，一次调用执行 size 个基本操作（或处理 size 元数据集）；规模三档 100 / 10,000 / 1,000,000（异步/反射/GC 分配类为 100 / 10,000）；quick 档 = ShortRun(3 warmup + 3 迭代, BDN) 或自适应 ≥5 次迭代 ≥150ms（fallback/godot）。表内数值为 ns/调用（每万次基本操作除以 size 可得 ns/元素）。冷启动为独立新进程首调（size=100，无预热）。

## 2. Async

| Benchmark | Size | net48 ns/op | net48 B/op | netcoreapp3.1 ns/op | netcoreapp3.1 B/op | net6.0 ns/op | net6.0 B/op | net7.0 ns/op | net7.0 B/op | Mono-in-Godot ns/op | Mono-in-Godot B/op |
|---|---|---|---|---|---|---|---|---|---|---|---|
| AwaitChain_Depth16 | 100 | 6785 | 0 | 3207 | 0 | 3374 | 0 | 2494 | 0 | 10.1μs | 0 |
| AwaitChain_Depth16 | 10,000 | 666.9μs | 0 | 320.0μs | 0 | 313.4μs | 1 | 250.4μs | 1 | 998.5μs | 0 |
| AwaitChain_Depth4 | 100 | 1692 | 0 | 923 | 0 | 681 | 0 | 686 | 0 | 3312 | 0 |
| AwaitChain_Depth4 | 10,000 | 165.7μs | 0 | 88.7μs | 0 | 65.6μs | 0 | 65.6μs | 0 | 249.6μs | 0 |
| Await_CompletedTask | 100 | 426 | 0 | 208 | 0 | 178 | 0 | 174 | 0 | 696 | 0 |
| Await_CompletedTask | 10,000 | 36.6μs | 0 | 18.4μs | 0 | 15.5μs | 0 | 15.3μs | 0 | 55.1μs | 0 |
| Await_FromResult | 100 | 843 | 7.8KB | 769 | 7.0KB | 751 | 6.4KB | 837 | 6.4KB | 2292 | 0 |
| Await_FromResult | 10,000 | 77.1μs | 783.6KB | 74.0μs | 703.1KB | 81.5μs | 702.5KB | 88.4μs | 702.5KB | 285.5μs | 0 |
| Await_TaskYield | 100 | 99.7μs | 5.2KB | 71.0μs | 183 | 63.9μs | 181 | 34.5μs | 173 | skip | — |
| Await_TaskYield | 10,000 | 8.65ms | 488.5KB | 6.54ms | 184 | 6.18ms | 195 | 3.13ms | 191 | skip | — |
| ConfigureAwaitFalse_Chain4 | 100 | 3957 | 0 | 3293 | 0 | 3122 | 0 | 661 | 0 | 4850 | 0 |
| ConfigureAwaitFalse_Chain4 | 10,000 | 391.8μs | 0 | 324.3μs | 0 | 310.1μs | 1 | 64.5μs | 0 | 459.3μs | 0 |
| ProducerConsumer_Queue | 100 | 19.8μs | 1.5KB | 12.7μs | 952 | 12.1μs | 920 | 11.4μs | 920 | 28.6μs | 0 |
| ProducerConsumer_Queue | 10,000 | 1.95ms | 83.8KB | 1.25ms | 952 | 1.19ms | 923 | 1.13ms | 923 | 2.82ms | 0 |
| SemaphoreSlim_WaitRelease | 100 | 8698 | 88 | 3779 | 88 | 3506 | 88 | 3415 | 88 | 11.8μs | 0 |
| SemaphoreSlim_WaitRelease | 10,000 | 854.0μs | 96 | 378.5μs | 88 | 352.8μs | 89 | 338.2μs | 89 | 1.15ms | 40 |
| TaskRun_Offload_Wait | 100 | 128.2μs | 8.6KB | 80.9μs | 7.0KB | 75.0μs | 7.0KB | 70.0μs | 7.0KB | 552.8μs | 0 |
| TaskRun_Offload_Wait | 10,000 | 13.33ms | 864.4KB | 8.17ms | 703.1KB | 7.53ms | 703.2KB | 6.98ms | 703.2KB | 55.53ms | 75.5KB |
| Tcs_SetResult_Await | 100 | 6025 | 10.2KB | 2910 | 9.4KB | 2784 | 9.4KB | 3037 | 9.4KB | 5650 | 0 |
| Tcs_SetResult_Await | 10,000 | 585.8μs | 1018.6KB | 291.0μs | 937.5KB | 281.6μs | 937.5KB | 304.9μs | 937.5KB | 636.6μs | 0 |
| WhenAll_64 | 100 | 80.6μs | 61.6KB | 72.6μs | 60.7KB | 101.0μs | 60.7KB | 79.4μs | 60.7KB | 113.4μs | 0 |
| WhenAll_64 | 10,000 | 8.11ms | 6.0MB | 7.25ms | 5.9MB | 10.20ms | 5.9MB | 7.83ms | 5.9MB | 12.60ms | 0 |
| WhenAll_8 | 100 | 15.6μs | 17.3KB | 10.7μs | 16.5KB | 14.4μs | 16.5KB | 12.1μs | 16.5KB | 19.5μs | 0 |
| WhenAll_8 | 10,000 | 1.54ms | 1.7MB | 1.08ms | 1.6MB | 1.44ms | 1.6MB | 1.19ms | 1.6MB | 2.16ms | 0 |
## 3. Collections

| Benchmark | Size | net48 ns/op | net48 B/op | netcoreapp3.1 ns/op | netcoreapp3.1 B/op | net6.0 ns/op | net6.0 B/op | net7.0 ns/op | net7.0 B/op | Mono-in-Godot ns/op | Mono-in-Godot B/op |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Array_Clone_Sort | 100 | 751 | 425 | 559 | 424 | 432 | 424 | 424 | 424 | 3071 | 0 |
| Array_Clone_Sort | 10,000 | 107.3μs | 39.1KB | 82.7μs | 39.1KB | 77.4μs | 39.1KB | 77.9μs | 39.1KB | 691.8μs | 39.1KB |
| Array_ForIterate | 100 | 78 | 0 | 70 | 0 | 65 | 0 | 58 | 0 | 225 | 0 |
| Array_ForIterate | 10,000 | 4056 | 0 | 4059 | 0 | 4052 | 0 | 3671 | 0 | 8054 | 0 |
| Dictionary_Add_Grow | 100 | 2196 | 7.2KB | 1524 | 7.2KB | 1309 | 7.2KB | 1470 | 7.2KB | 3025 | 0 |
| Dictionary_Add_Grow | 10,000 | 446.8μs | 658.0KB | 470.8μs | 657.4KB | 459.1μs | 657.4KB | 470.6μs | 657.4KB | 370.3μs | 629.4KB |
| Dictionary_Add_Prealloc | 100 | 1026 | 2.2KB | 737 | 2.2KB | 624 | 2.2KB | 689 | 2.2KB | 1367 | 0 |
| Dictionary_Add_Prealloc | 10,000 | 98.0μs | 197.8KB | 79.3μs | 197.5KB | 68.0μs | 197.5KB | 67.9μs | 197.5KB | 164.9μs | 197.2KB |
| Dictionary_IterateForeach | 100 | 694 | 0 | 639 | 0 | 260 | 0 | 257 | 0 | 550 | 0 |
| Dictionary_IterateForeach | 10,000 | 65.8μs | 0 | 61.0μs | 0 | 30.7μs | 0 | 22.6μs | 0 | 47.9μs | 0 |
| Dictionary_Lookup_Hit | 100 | 829 | 0 | 684 | 0 | 457 | 0 | 428 | 0 | 1125 | 0 |
| Dictionary_Lookup_Hit | 10,000 | 74.9μs | 0 | 65.4μs | 0 | 42.5μs | 0 | 38.1μs | 0 | 100.1μs | 0 |
| Dictionary_Lookup_Miss | 100 | 643 | 0 | 523 | 0 | 390 | 0 | 412 | 0 | 771 | 0 |
| Dictionary_Lookup_Miss | 10,000 | 60.9μs | 0 | 51.2μs | 0 | 38.4μs | 0 | 40.6μs | 0 | 67.2μs | 0 |
| HashSet_Add | 100 | 2015 | 5.9KB | 1720 | 5.9KB | 1200 | 5.9KB | 1370 | 5.9KB | 2979 | 0 |
| HashSet_Add | 10,000 | 362.4μs | 526.4KB | 396.4μs | 526.0KB | 399.4μs | 526.1KB | 394.2μs | 526.0KB | 343.9μs | 502.2KB |
| HashSet_Contains_Hit | 100 | 764 | 24 | 769 | 24 | 444 | 24 | 393 | 24 | 1129 | 0 |
| HashSet_Contains_Hit | 10,000 | 72.8μs | 25 | 74.5μs | 24 | 41.6μs | 24 | 36.1μs | 24 | 103.5μs | 0 |
| List_Add_Grow | 100 | 381 | 1.2KB | 286 | 1.2KB | 266 | 1.2KB | 286 | 1.2KB | 592 | 0 |
| List_Add_Grow | 10,000 | 27.0μs | 128.5KB | 19.5μs | 128.3KB | 18.8μs | 128.3KB | 19.7μs | 128.3KB | 52.5μs | 119.8KB |
| List_Add_Prealloc | 100 | 243 | 465 | 156 | 456 | 161 | 456 | 172 | 456 | 258 | 0 |
| List_Add_Prealloc | 10,000 | 20.8μs | 39.2KB | 14.1μs | 39.1KB | 13.8μs | 39.1KB | 14.5μs | 39.1KB | 19.1μs | 39.0KB |
| List_Contains_Miss | 100 | 15.0μs | 24 | 2159 | 24 | 1733 | 24 | 872 | 24 | 12.3μs | 0 |
| List_Contains_Miss | 10,000 | 137.10ms | 0 | 18.61ms | 26 | 14.50ms | 45 | 9.27ms | 50 | 107.50ms | 0 |
| List_IndexGet | 100 | 105 | 0 | 84 | 0 | 78 | 0 | 76 | 0 | 275 | 0 |
| List_IndexGet | 10,000 | 6881 | 0 | 5362 | 0 | 5428 | 0 | 5340 | 0 | 20.3μs | 0 |
| List_IterateFor | 100 | 103 | 0 | 84 | 0 | 81 | 0 | 76 | 0 | 338 | 0 |
| List_IterateFor | 10,000 | 6869 | 0 | 5360 | 0 | 5382 | 0 | 5347 | 0 | 20.9μs | 0 |
| List_IterateForeach | 100 | 218 | 0 | 197 | 0 | 96 | 0 | 84 | 0 | 375 | 0 |
| List_IterateForeach | 10,000 | 16.7μs | 0 | 16.6μs | 0 | 7004 | 0 | 6114 | 0 | 25.3μs | 0 |
| Queue_Stack_PushPop | 100 | 1929 | 2.3KB | 1129 | 2.3KB | 899 | 2.3KB | 913 | 2.3KB | 2404 | 0 |
| Queue_Stack_PushPop | 10,000 | 164.4μs | 256.9KB | 89.4μs | 256.6KB | 73.2μs | 256.6KB | 70.5μs | 256.6KB | 210.5μs | 239.8KB |
## 4. Delegates

| Benchmark | Size | net48 ns/op | net48 B/op | netcoreapp3.1 ns/op | netcoreapp3.1 B/op | net6.0 ns/op | net6.0 B/op | net7.0 ns/op | net7.0 B/op | Mono-in-Godot ns/op | Mono-in-Godot B/op |
|---|---|---|---|---|---|---|---|---|---|---|---|
| ActionInt_Invoke | 100 | 186 | 0 | 186 | 0 | 208 | 0 | 231 | 0 | 196 | 0 |
| ActionInt_Invoke | 10,000 | 18.0μs | 0 | 18.0μs | 0 | 20.4μs | 0 | 22.4μs | 0 | 16.0μs | 0 |
| ActionInt_Invoke | 1,000,000 | 1.81ms | 0 | 1.81ms | 0 | 2.03ms | 5 | 2.24ms | 7 | — | — |
| Action_Invoke | 100 | 230 | 0 | 189 | 0 | 188 | 0 | 208 | 0 | 246 | 0 |
| Action_Invoke | 10,000 | 22.7μs | 0 | 18.1μs | 0 | 18.2μs | 0 | 20.2μs | 0 | 17.9μs | 0 |
| Action_Invoke | 1,000,000 | 2.26ms | 0 | 1.81ms | 0 | 1.81ms | 3 | 2.02ms | 7 | — | — |
| ClosureCapture_Invoke | 100 | 865 | 8.6KB | 895 | 8.6KB | 944 | 8.6KB | 998 | 8.6KB | 4625 | 0 |
| ClosureCapture_Invoke | 10,000 | 85.6μs | 861.9KB | 86.1μs | 859.4KB | 91.4μs | 859.4KB | 99.5μs | 859.4KB | 197.9μs | 0 |
| ClosureCapture_Invoke | 1,000,000 | 8.61ms | 84.2MB | 8.91ms | 83.9MB | 8.98ms | 83.9MB | 9.98ms | 83.9MB | — | — |
| Event_AddRemove | 100 | 3500 | 0 | 2523 | 0 | 2527 | 0 | 2399 | 0 | 4662 | 0 |
| Event_AddRemove | 10,000 | 351.1μs | 0 | 254.5μs | 0 | 250.1μs | 1 | 236.7μs | 0 | 464.2μs | 0 |
| Event_AddRemove | 1,000,000 | 35.11ms | 0 | 25.25ms | 2 | 25.14ms | 42 | 23.77ms | 53 | — | — |
| FuncInt_Invoke | 100 | 208 | 0 | 186 | 0 | 207 | 0 | 230 | 0 | 233 | 0 |
| FuncInt_Invoke | 10,000 | 20.3μs | 0 | 18.1μs | 0 | 20.3μs | 0 | 22.4μs | 0 | 20.1μs | 0 |
| FuncInt_Invoke | 1,000,000 | 2.02ms | 0 | 1.80ms | 0 | 2.03ms | 5 | 2.25ms | 7 | — | — |
| Interface_vs_DirectCall | 100 | 253 | 0 | 52 | 0 | 48 | 0 | 47 | 0 | 392 | 441 |
| Interface_vs_DirectCall | 10,000 | 25.0μs | 0 | 4545 | 0 | 4476 | 0 | 4477 | 0 | 19.1μs | 0 |
| Interface_vs_DirectCall | 1,000,000 | 2.50ms | 0 | 454.2μs | 0 | 453.4μs | 1 | 449.6μs | 1 | — | — |
| Multicast2_Invoke | 100 | 736 | 0 | 908 | 0 | 860 | 0 | 845 | 0 | 558 | 0 |
| Multicast2_Invoke | 10,000 | 72.3μs | 0 | 88.5μs | 0 | 85.6μs | 0 | 83.1μs | 0 | 54.2μs | 0 |
| Multicast2_Invoke | 1,000,000 | 7.24ms | 0 | 9.02ms | 1 | 8.57ms | 21 | 8.29ms | 26 | — | — |
| Multicast8_Invoke | 100 | 2105 | 0 | 2778 | 0 | 2620 | 0 | 2474 | 0 | 1975 | 0 |
| Multicast8_Invoke | 10,000 | 213.2μs | 0 | 272.6μs | 0 | 263.2μs | 1 | 243.3μs | 0 | 161.4μs | 0 |
| Multicast8_Invoke | 1,000,000 | 21.25ms | 0 | 26.30ms | 2 | 26.07ms | 42 | 24.27ms | 53 | — | — |
| NewDelegate_Creation | 100 | 574 | 6.3KB | 536 | 6.2KB | 553 | 6.2KB | 594 | 6.2KB | 2683 | 0 |
| NewDelegate_Creation | 10,000 | 57.0μs | 626.8KB | 52.6μs | 625.0KB | 54.2μs | 625.0KB | 60.9μs | 625.0KB | 220.9μs | 0 |
| NewDelegate_Creation | 1,000,000 | 5.71ms | 61.2MB | 5.23ms | 61.0MB | 5.30ms | 61.0MB | 5.94ms | 61.0MB | — | — |
| StaticLambdaNoCapture | 100 | 167 | 0 | 164 | 0 | 141 | 0 | 162 | 0 | 179 | 0 |
| StaticLambdaNoCapture | 10,000 | 15.8μs | 0 | 15.8μs | 0 | 13.5μs | 0 | 15.8μs | 0 | 16.0μs | 0 |
| StaticLambdaNoCapture | 1,000,000 | 1.57ms | 0 | 1.58ms | 0 | 1.37ms | 3 | 1.57ms | 3 | — | — |
## 5. GcAlloc

| Benchmark | Size | net48 ns/op | net48 B/op | netcoreapp3.1 ns/op | netcoreapp3.1 B/op | net6.0 ns/op | net6.0 B/op | net7.0 ns/op | net7.0 B/op | Mono-in-Godot ns/op | Mono-in-Godot B/op |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Array_New_Class16 | 100 | 7739 | 65.0KB | 7709 | 64.8KB | 9447 | 64.8KB | 10.6μs | 64.8KB | 16.9μs | 0 |
| Array_New_Class16 | 10,000 | 793.9μs | 6.4MB | 757.6μs | 6.3MB | 951.9μs | 6.3MB | 1.02ms | 6.3MB | 1.89ms | 1.1KB |
| Array_New_Int128 | 100 | 2437 | 52.5KB | 2040 | 52.3KB | 2098 | 52.3KB | 2809 | 52.3KB | 3612 | 432 |
| Array_New_Int128 | 10,000 | 250.0μs | 5.1MB | 202.6μs | 5.1MB | 207.8μs | 5.1MB | 273.0μs | 5.1MB | 335.1μs | 0 |
| Boxing_Plus_Equals | 100 | 347 | 2.4KB | 309 | 2.4KB | 314 | 2.4KB | 373 | 2.4KB | 1433 | 0 |
| Boxing_Plus_Equals | 10,000 | 33.3μs | 235.1KB | 29.6μs | 234.4KB | 29.8μs | 234.4KB | 35.4μs | 234.4KB | 136.4μs | 0 |
| Class_New_Big | 100 | 566 | 7.1KB | 572 | 7.0KB | 591 | 7.0KB | 763 | 7.0KB | 1104 | 0 |
| Class_New_Big | 10,000 | 56.1μs | 705.2KB | 55.6μs | 703.1KB | 57.1μs | 703.1KB | 67.5μs | 703.1KB | 108.4μs | 7 |
| Class_New_Small | 100 | 347 | 3.1KB | 307 | 3.1KB | 315 | 3.1KB | 453 | 3.1KB | 750 | 0 |
| Class_New_Small | 10,000 | 34.1μs | 313.4KB | 29.9μs | 312.5KB | 30.9μs | 312.5KB | 36.9μs | 312.5KB | 115.8μs | 0 |
| Finalizable_New | 100 | 4415 | 2.4KB | 4532 | 2.3KB | 4754 | 2.3KB | 4326 | 2.3KB | 17.1μs | 0 |
| Finalizable_New | 10,000 | 430.9μs | 235.1KB | 452.8μs | 234.4KB | 483.9μs | 234.4KB | 432.3μs | 234.4KB | 1.82ms | 0 |
| GC_Collect_Gen0 | 100 | 1.83ms | 0 | 2.17ms | 0 | 2.34ms | 2 | 2.38ms | 3 | 5.21ms | 738 |
| GC_Collect_Gen0 | 10,000 | 184.92ms | 0 | 213.78ms | 21 | 231.92ms | 680 | 237.96ms | 229 | 457.80ms | 0 |
| ListPool_Reuse8 | 100 | 6395 | 0 | 4021 | 0 | 3903 | 0 | 3966 | 0 | 8604 | 0 |
| ListPool_Reuse8 | 10,000 | 630.4μs | 0 | 401.3μs | 0 | 390.1μs | 1 | 391.4μs | 0 | 934.0μs | 0 |
| String_New100 | 100 | 3012 | 22.7KB | 2829 | 21.9KB | 2389 | 21.9KB | 2060 | 21.9KB | 4238 | 0 |
| String_New100 | 10,000 | 309.3μs | 2.2MB | 267.7μs | 2.1MB | 240.5μs | 2.1MB | 205.6μs | 2.1MB | 372.5μs | 0 |
| Struct_CopyAssign_Baseline | 100 | 99 | 281 | 93 | 280 | 91 | 280 | 101 | 280 | 588 | 0 |
| Struct_CopyAssign_Baseline | 10,000 | 6833 | 281 | 6819 | 280 | 6897 | 280 | 6074 | 280 | 49.4μs | 0 |
## 6. Linq

| Benchmark | Size | net48 ns/op | net48 B/op | netcoreapp3.1 ns/op | netcoreapp3.1 B/op | net6.0 ns/op | net6.0 B/op | net7.0 ns/op | net7.0 B/op | Mono-in-Godot ns/op | Mono-in-Godot B/op |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Aggregate_Max | 100 | 600 | 32 | 562 | 32 | 548 | 32 | 555 | 32 | 925 | 0 |
| Aggregate_Max | 10,000 | 54.3μs | 32 | 52.4μs | 32 | 51.1μs | 32 | 52.4μs | 32 | 74.1μs | 0 |
| Aggregate_Max | 1,000,000 | 5.54ms | 0 | 5.28ms | 32 | 5.21ms | 43 | 5.29ms | 45 | — | — |
| Any_Miss_FullScan | 100 | 601 | 56 | 520 | 56 | 495 | 56 | 520 | 56 | 875 | 0 |
| Any_Miss_FullScan | 10,000 | 54.1μs | 56 | 47.2μs | 56 | 45.5μs | 56 | 48.0μs | 56 | 75.0μs | 0 |
| Any_Miss_FullScan | 1,000,000 | 5.55ms | 0 | 4.79ms | 56 | 4.59ms | 67 | 4.86ms | 69 | — | — |
| Count_Predicate | 100 | 672 | 32 | 555 | 32 | 657 | 32 | 553 | 32 | 925 | 0 |
| Count_Predicate | 10,000 | 62.2μs | 33 | 51.2μs | 32 | 66.1μs | 32 | 50.9μs | 32 | 79.8μs | 0 |
| Count_Predicate | 1,000,000 | 6.37ms | 0 | 5.25ms | 32 | 6.54ms | 43 | 5.25ms | 45 | — | — |
| Distinct_Count | 100 | 2541 | 4.2KB | 1893 | 4.2KB | 897 | 1.9KB | 922 | 1.9KB | 5296 | 0 |
| Distinct_Count | 10,000 | 122.2μs | 4.2KB | 111.8μs | 4.2KB | 78.0μs | 159.8KB | 74.6μs | 159.8KB | 166.7μs | 0 |
| Distinct_Count | 1,000,000 | 12.25ms | 4.2KB | 11.09ms | 4.2KB | 7.68ms | 17.7MB | 7.66ms | 17.7MB | — | — |
| FirstOrDefault_Miss | 100 | 643 | 32 | 507 | 32 | 500 | 32 | 510 | 32 | 896 | 0 |
| FirstOrDefault_Miss | 10,000 | 58.0μs | 32 | 45.2μs | 32 | 46.9μs | 32 | 47.3μs | 32 | 74.2μs | 0 |
| FirstOrDefault_Miss | 1,000,000 | 5.92ms | 0 | 4.64ms | 32 | 4.88ms | 43 | 4.88ms | 45 | — | — |
| GroupBy_Count | 100 | 5765 | 10.8KB | 4745 | 10.8KB | 4892 | 10.8KB | 4822 | 10.8KB | 8958 | 0 |
| GroupBy_Count | 10,000 | 193.2μs | 126.8KB | 163.3μs | 126.4KB | 144.9μs | 126.4KB | 144.9μs | 126.4KB | 283.5μs | 0 |
| GroupBy_Count | 1,000,000 | 21.90ms | 12.6MB | 19.88ms | 12.5MB | 17.05ms | 12.5MB | 17.11ms | 12.5MB | — | — |
| Handwritten_Sum_Loop | 100 | 92 | 0 | 78 | 0 | 69 | 0 | 68 | 0 | 229 | 0 |
| Handwritten_Sum_Loop | 10,000 | 4574 | 0 | 4590 | 0 | 4591 | 0 | 4572 | 0 | 12.9μs | 0 |
| Handwritten_Sum_Loop | 1,000,000 | 509.7μs | 0 | 495.7μs | 0 | 496.9μs | 1 | 497.3μs | 2 | — | — |
| OrderBy_First | 100 | 3484 | 1.4KB | 792 | 128 | 858 | 128 | 897 | 128 | 1858 | 229 |
| OrderBy_First | 10,000 | 595.3μs | 117.6KB | 73.1μs | 128 | 77.3μs | 128 | 83.4μs | 128 | 114.5μs | 0 |
| OrderBy_First | 1,000,000 | 88.26ms | 11.4MB | 7.47ms | 128 | 7.81ms | 149 | 8.46ms | 154 | — | — |
| OrderBy_Take10 | 100 | 3678 | 1.5KB | 1490 | 1.4KB | 1132 | 1.4KB | 1160 | 1.4KB | 2262 | 0 |
| OrderBy_Take10 | 10,000 | 597.9μs | 117.6KB | 130.1μs | 117.4KB | 96.0μs | 117.4KB | 95.2μs | 117.4KB | 191.3μs | 117.3KB |
| OrderBy_Take10 | 1,000,000 | 88.20ms | 11.4MB | 13.93ms | 11.4MB | 11.39ms | 11.4MB | 11.64ms | 11.4MB | — | — |
| Select_Project_Last | 100 | 620 | 56 | 57 | 48 | 41 | 48 | 39 | 48 | 108 | 0 |
| Select_Project_Last | 10,000 | 61.3μs | 56 | 62 | 48 | 41 | 48 | 39 | 48 | 142 | 0 |
| Select_Project_Last | 1,000,000 | 6.23ms | 0 | 61 | 48 | 41 | 48 | 39 | 48 | — | — |
| ToArray_Materialize | 100 | 611 | 891 | 428 | 720 | 418 | 720 | 400 | 720 | 758 | 0 |
| ToArray_Materialize | 10,000 | 43.4μs | 84.0KB | 23.2μs | 52.2KB | 29.4μs | 52.2KB | 27.1μs | 52.2KB | 35.2μs | 43.6KB |
| ToArray_Materialize | 1,000,000 | 4.70ms | 5.9MB | 2.63ms | 3.9MB | 4.27ms | 3.9MB | 3.43ms | 3.9MB | — | — |
| ToList_Materialize | 100 | 708 | 706 | 408 | 696 | 365 | 696 | 359 | 696 | 725 | 0 |
| ToList_Materialize | 10,000 | 51.3μs | 64.5KB | 26.4μs | 64.3KB | 26.5μs | 64.3KB | 26.3μs | 64.3KB | 43.2μs | 55.5KB |
| ToList_Materialize | 1,000,000 | 5.13ms | 4.0MB | 2.88ms | 4.0MB | 3.61ms | 4.0MB | 3.29ms | 4.0MB | — | — |
| WhereSelect_Chain_Sum | 100 | 526 | 104 | 507 | 104 | 492 | 104 | 531 | 104 | 1233 | 0 |
| WhereSelect_Chain_Sum | 10,000 | 42.5μs | 104 | 41.4μs | 104 | 42.4μs | 104 | 46.9μs | 104 | 53.1μs | 0 |
| WhereSelect_Chain_Sum | 1,000,000 | skip | — | skip | — | skip | — | skip | — | — | — |
| Where_Filter_Count | 100 | 375 | 48 | 320 | 48 | 335 | 48 | 361 | 48 | 592 | 0 |
| Where_Filter_Count | 10,000 | 29.9μs | 48 | 26.2μs | 48 | 29.0μs | 48 | 32.1μs | 48 | 38.3μs | 0 |
| Where_Filter_Count | 1,000,000 | 3.04ms | 64 | 2.74ms | 48 | 2.90ms | 53 | 3.22ms | 55 | — | — |
## 7. Primitives

| Benchmark | Size | net48 ns/op | net48 B/op | netcoreapp3.1 ns/op | netcoreapp3.1 B/op | net6.0 ns/op | net6.0 B/op | net7.0 ns/op | net7.0 B/op | Mono-in-Godot ns/op | Mono-in-Godot B/op |
|---|---|---|---|---|---|---|---|---|---|---|---|
| DateTime_Now | 100 | 6955 | 24 | 7772 | 24 | 3180 | 24 | 2724 | 24 | 4458 | 0 |
| DateTime_Now | 10,000 | 679.8μs | 32 | 775.7μs | 24 | 316.8μs | 24 | 271.2μs | 24 | 505.5μs | 0 |
| DateTime_Now | 1,000,000 | 68.00ms | 0 | 77.23ms | 33 | 31.65ms | 67 | 27.10ms | 46 | — | — |
| Decimal_Add_Loop | 100 | 1171 | 32 | 1083 | 32 | 434 | 32 | 431 | 32 | 2117 | 0 |
| Decimal_Add_Loop | 10,000 | 111.6μs | 33 | 103.1μs | 32 | 41.4μs | 32 | 41.0μs | 32 | 238.8μs | 0 |
| Decimal_Add_Loop | 1,000,000 | 11.12ms | 0 | 10.34ms | 33 | 4.20ms | 47 | 4.11ms | 37 | — | — |
| Double_Mul_Loop | 100 | 55 | 0 | 55 | 0 | 55 | 0 | 54 | 0 | 229 | 0 |
| Double_Mul_Loop | 10,000 | 8884 | 0 | 8886 | 0 | 8910 | 0 | 8879 | 0 | 20.2μs | 0 |
| Double_Mul_Loop | 1,000,000 | 893.6μs | 0 | 900.4μs | 1 | 894.1μs | 1 | 901.9μs | 1 | — | — |
| Double_Parse | 100 | 7747 | 0 | 5894 | 0 | 4659 | 0 | 4939 | 0 | 13.8μs | 0 |
| Double_Parse | 10,000 | 782.2μs | 0 | 597.5μs | 0 | 470.1μs | 0 | 495.8μs | 1 | 1.66ms | 0 |
| Double_Parse | 1,000,000 | 78.10ms | 0 | 60.70ms | 7 | 47.16ms | 63 | 49.56ms | 69 | — | — |
| Enum_Parse | 100 | 23.5μs | 7.1KB | 8797 | 2.3KB | 7302 | 2.3KB | 5583 | 2.3KB | 69.0μs | 0 |
| Enum_Parse | 10,000 | 2.34ms | 705.2KB | 831.0μs | 234.4KB | 721.9μs | 234.4KB | 551.6μs | 234.4KB | 5.40ms | 219 |
| Enum_Parse | 1,000,000 | 233.64ms | 68.9MB | 83.37ms | 22.9MB | 72.42ms | 22.9MB | 55.48ms | 22.9MB | — | — |
| Guid_NewGuid | 100 | 6351 | 32 | 6356 | 32 | 6381 | 32 | 6181 | 32 | 11.8μs | 0 |
| Guid_NewGuid | 10,000 | 632.2μs | 40 | 622.0μs | 32 | 624.4μs | 33 | 614.4μs | 33 | 1.04ms | 0 |
| Guid_NewGuid | 1,000,000 | 63.55ms | 0 | 62.42ms | 40 | 62.47ms | 118 | 62.40ms | 108 | — | — |
| Int_Add_Loop | 100 | 50 | 0 | 49 | 0 | 50 | 0 | 48 | 0 | 154 | 0 |
| Int_Add_Loop | 10,000 | 4258 | 0 | 4250 | 0 | 4289 | 0 | 4251 | 0 | 6867 | 0 |
| Int_Add_Loop | 1,000,000 | 422.8μs | 0 | 423.3μs | 0 | 423.7μs | 0 | 427.6μs | 0 | — | — |
| Int_Boxing | 100 | 349 | 2.4KB | 289 | 2.3KB | 287 | 2.3KB | 329 | 2.3KB | 796 | 0 |
| Int_Boxing | 10,000 | 33.3μs | 235.1KB | 28.0μs | 234.4KB | 28.5μs | 234.4KB | 33.6μs | 234.4KB | 123.6μs | 0 |
| Int_Boxing | 1,000,000 | 3.12ms | 23.0MB | 2.70ms | 22.9MB | 2.69ms | 22.9MB | 3.07ms | 22.9MB | — | — |
| Int_Parse | 100 | 6515 | 0 | 1318 | 0 | 1283 | 0 | 1266 | 0 | 8754 | 0 |
| Int_Parse | 10,000 | 645.1μs | 0 | 132.4μs | 0 | 130.1μs | 0 | 126.3μs | 0 | 812.2μs | 0 |
| Int_Parse | 1,000,000 | 65.46ms | 0 | 13.34ms | 1 | 12.59ms | 11 | 12.68ms | 11 | — | — |
| Int_TryParse | 100 | 6592 | 0 | 1332 | 0 | 1331 | 0 | 1303 | 0 | 6938 | 0 |
| Int_TryParse | 10,000 | 661.5μs | 0 | 131.7μs | 0 | 127.0μs | 0 | 131.4μs | 0 | 838.6μs | 0 |
| Int_TryParse | 1,000,000 | 69.81ms | 0 | 13.19ms | 1 | 12.73ms | 11 | 13.04ms | 11 | — | — |
| String_Compare_Ordinal | 100 | 239 | 0 | 305 | 0 | 261 | 0 | 259 | 0 | 704 | 0 |
| String_Compare_Ordinal | 10,000 | 23.2μs | 0 | 29.8μs | 0 | 25.1μs | 0 | 24.8μs | 0 | 43.2μs | 0 |
| String_Compare_Ordinal | 1,000,000 | 2.34ms | 0 | 2.94ms | 0 | 2.49ms | 3 | 2.50ms | 3 | — | — |
| String_Concat_Builder | 100 | 5820 | 5.2KB | 2186 | 2.1KB | 947 | 2.1KB | 956 | 2.1KB | 8146 | 0 |
| String_Concat_Builder | 10,000 | 708.5μs | 590.7KB | 222.2μs | 206.8KB | 202.4μs | 206.8KB | 202.2μs | 206.9KB | 884.4μs | 197.3KB |
| String_Concat_Builder | 1,000,000 | 79.12ms | 64.6MB | 33.56ms | 26.4MB | 24.62ms | 26.4MB | 28.20ms | 26.4MB | — | — |
| String_Concat_Interp | 100 | 19.0μs | 14.8KB | 8977 | 7.8KB | 4995 | 7.8KB | 5583 | 7.8KB | 22.8μs | 0 |
| String_Concat_Interp | 10,000 | 1.94ms | 1.5MB | 935.1μs | 851.6KB | 519.3μs | 851.6KB | 581.4μs | 851.6KB | 2.13ms | 0 |
| String_Concat_Interp | 1,000,000 | 204.83ms | 159.9MB | 97.20ms | 83.9MB | 53.26ms | 83.9MB | 60.51ms | 83.9MB | — | — |
| String_Concat_Plus | 100 | 5529 | 6.3KB | 3156 | 5.9KB | 1824 | 5.9KB | 1747 | 5.9KB | 9067 | 0 |
| String_Concat_Plus | 10,000 | 593.8μs | 774.9KB | 331.2μs | 624.7KB | 200.4μs | 624.7KB | 194.3μs | 624.7KB | 913.8μs | 0 |
| String_Concat_Plus | 1,000,000 | 58.71ms | 76.5MB | 34.83ms | 75.5MB | 20.50ms | 75.5MB | 21.03ms | 75.5MB | — | — |
## 8. Reflection

| Benchmark | Size | net48 ns/op | net48 B/op | netcoreapp3.1 ns/op | netcoreapp3.1 B/op | net6.0 ns/op | net6.0 B/op | net7.0 ns/op | net7.0 B/op | Mono-in-Godot ns/op | Mono-in-Godot B/op |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Activator_CreateInstance | 100 | 4521 | 3.1KB | 3143 | 3.1KB | 1200 | 3.1KB | 1231 | 3.1KB | 19.2μs | 0 |
| Activator_CreateInstance | 10,000 | 466.2μs | 313.4KB | 271.7μs | 312.5KB | 117.9μs | 312.5KB | 119.7μs | 312.5KB | 2.02ms | 0 |
| CreateDelegate_ThenInvoke | 100 | 178.1μs | 6.3KB | 41.1μs | 6.2KB | 41.3μs | 6.2KB | 33.3μs | 6.2KB | 128.4μs | 0 |
| CreateDelegate_ThenInvoke | 10,000 | 17.63ms | 627.0KB | 4.07ms | 625.0KB | 4.15ms | 625.0KB | 3.35ms | 625.0KB | 13.17ms | 0 |
| Expression_Compile | 100 | 6.23ms | 525.3KB | 4.41ms | 454.6KB | 4.33ms | 469.4KB | 4.46ms | 472.5KB | 8.25ms | 0 |
| Expression_Compile | 10,000 | 617.78ms | 51.3MB | 440.24ms | 44.4MB | 431.28ms | 45.8MB | 439.88ms | 46.1MB | 30.78s | 16.6KB |
| GetCustomAttribute | 100 | 84.9μs | 15.7KB | 25.0μs | 0 | 18.9μs | 0 | 10.2μs | 0 | 98.5μs | 0 |
| GetCustomAttribute | 10,000 | 8.67ms | 1.5MB | 2.47ms | 1 | 1.86ms | 3 | 1.01ms | 4 | 11.49ms | 0 |
| Method_Invoke_Instance | 100 | 16.3μs | 5.5KB | 12.5μs | 5.5KB | 10.5μs | 2.4KB | 2747 | 2.4KB | 54.6μs | 0 |
| Method_Invoke_Instance | 10,000 | 1.63ms | 548.6KB | 1.25ms | 546.9KB | 1.04ms | 234.4KB | 289.1μs | 234.4KB | 5.60ms | 4 |
| Method_Invoke_Static | 100 | 15.0μs | 5.5KB | 12.1μs | 5.5KB | 10.1μs | 2.4KB | 2661 | 2.4KB | 37.7μs | 262 |
| Method_Invoke_Static | 10,000 | 1.49ms | 548.6KB | 1.22ms | 546.9KB | 1.02ms | 234.4KB | 272.7μs | 234.4KB | 3.77ms | 5 |
| Property_GetValue | 100 | 10000 | 2.4KB | 8525 | 2.3KB | 6424 | 2.3KB | 1402 | 2.3KB | 1454 | 0 |
| Property_GetValue | 10,000 | 998.4μs | 235.1KB | 796.1μs | 234.4KB | 637.5μs | 234.4KB | 139.0μs | 234.4KB | 136.9μs | 0 |
| Property_GetValue_ViaGetter | 100 | 9411 | 2.4KB | 7587 | 2.3KB | 6283 | 2.3KB | 1353 | 2.3KB | 51.3μs | 0 |
| Property_GetValue_ViaGetter | 10,000 | 967.2μs | 235.1KB | 757.5μs | 234.4KB | 633.9μs | 234.4KB | 136.6μs | 234.4KB | 5.45ms | 0 |
| Property_SetValue | 100 | 15.9μs | 8.6KB | 12.1μs | 8.6KB | 7843 | 2.3KB | 3190 | 2.3KB | 56.7μs | 0 |
| Property_SetValue | 10,000 | 1.59ms | 861.9KB | 1.20ms | 859.4KB | 812.4μs | 234.4KB | 330.8μs | 234.4KB | 5.74ms | 158 |
| Type_GetMethod | 100 | 5141 | 0 | 3848 | 0 | 2531 | 0 | 2369 | 0 | 59.6μs | 0 |
| Type_GetMethod | 10,000 | 507.9μs | 0 | 386.5μs | 0 | 260.5μs | 1 | 238.6μs | 0 | 6.11ms | 0 |
| Type_GetProperties | 100 | 5398 | 3.9KB | 4041 | 3.9KB | 3142 | 3.9KB | 3141 | 3.9KB | 64.5μs | 0 |
| Type_GetProperties | 10,000 | 533.4μs | 391.8KB | 407.1μs | 390.6KB | 307.7μs | 390.6KB | 310.2μs | 390.6KB | 6.58ms | 0 |
## 9. Scenarios

| Benchmark | Size | net48 ns/op | net48 B/op | netcoreapp3.1 ns/op | netcoreapp3.1 B/op | net6.0 ns/op | net6.0 B/op | net7.0 ns/op | net7.0 B/op | Mono-in-Godot ns/op | Mono-in-Godot B/op |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Csv_Parse | 100 | 25.2μs | 31.1KB | 16.7μs | 18.7KB | 14.3μs | 18.7KB | 13.8μs | 18.7KB | 40.3μs | 0 |
| Csv_Parse | 10,000 | 2.68ms | 3.3MB | 1.81ms | 1.9MB | 1.45ms | 1.9MB | 1.45ms | 1.9MB | 4.17ms | 0 |
| Csv_Parse | 1,000,000 | 282.46ms | 350.9MB | 186.29ms | 203.6MB | 152.37ms | 203.6MB | 153.62ms | 203.6MB | — | — |
| Json_ManualSerialize | 100 | 59.2μs | 26.0KB | 33.3μs | 22.7KB | 19.9μs | 22.7KB | 19.7μs | 22.7KB | 79.8μs | 18.8KB |
| Json_ManualSerialize | 10,000 | 6.04ms | 1.9MB | 3.48ms | 1.5MB | 2.22ms | 1.5MB | 2.52ms | 1.5MB | 8.46ms | 333.7KB |
| Json_ManualSerialize | 1,000,000 | 658.35ms | 197.0MB | 392.35ms | 157.0MB | 239.91ms | 157.1MB | 220.94ms | 157.1MB | — | — |
| Json_ReflectionSerialize | 100 | 95.1μs | 41.6KB | 53.8μs | 40.9KB | 46.0μs | 40.9KB | 46.5μs | 40.9KB | 173.5μs | 25.1KB |
| Json_ReflectionSerialize | 10,000 | 9.56ms | 3.0MB | 6.33ms | 3.0MB | 5.72ms | 3.0MB | 5.70ms | 3.0MB | 19.63ms | 1.4MB |
| Json_ReflectionSerialize | 1,000,000 | 1.01s | 307.3MB | 613.39ms | 305.9MB | 529.62ms | 305.9MB | 520.68ms | 305.9MB | — | — |
| PrimeSieve | 100 | 128 | 128 | 112 | 128 | 103 | 128 | 107 | 128 | 329 | 0 |
| PrimeSieve | 10,000 | 20.0μs | 9.8KB | 19.2μs | 9.8KB | 19.0μs | 9.8KB | 18.7μs | 9.8KB | 33.6μs | 9.6KB |
| PrimeSieve | 1,000,000 | 2.95ms | 976.6KB | 3.00ms | 976.6KB | 2.97ms | 976.6KB | 2.73ms | 976.6KB | — | — |
| SortAggregate_Pipeline | 100 | 10.2μs | 12.6KB | 8066 | 9.6KB | 7904 | 9.5KB | 7647 | 9.5KB | 12.3μs | 0 |
| SortAggregate_Pipeline | 10,000 | 1.34ms | 454.2KB | 1.20ms | 397.5KB | 1.12ms | 397.4KB | 1.03ms | 397.4KB | 2.10ms | 155.2KB |
| SortAggregate_Pipeline | 1,000,000 | 327.16ms | 21.8MB | 310.32ms | 21.5MB | 299.16ms | 21.5MB | 290.80ms | 21.5MB | — | — |
| Text_SplitJoin | 100 | 2698 | 6.3KB | 2150 | 4.7KB | 1594 | 4.7KB | 1740 | 4.7KB | 5833 | 0 |
| Text_SplitJoin | 10,000 | 543.8μs | 748.8KB | 305.6μs | 486.2KB | 280.2μs | 486.2KB | 239.3μs | 486.2KB | 682.3μs | 346.3KB |
| Text_SplitJoin | 1,000,000 | 102.64ms | 73.1MB | 92.45ms | 47.5MB | 85.69ms | 47.5MB | 88.60ms | 47.5MB | — | — |
| Tree_BuildWalk | 100 | 353 | 425 | 351 | 424 | 391 | 424 | 390 | 424 | 671 | 0 |
| Tree_BuildWalk | 10,000 | 32.6μs | 39.1KB | 32.9μs | 39.1KB | 37.6μs | 39.1KB | 36.2μs | 39.1KB | 61.9μs | 38.4KB |
| Tree_BuildWalk | 1,000,000 | 5.30ms | 3.8MB | 5.49ms | 3.8MB | 5.85ms | 3.8MB | 5.30ms | 3.8MB | — | — |
| WordFreq_Dictionary | 100 | 4659 | 30.3KB | 3553 | 30.3KB | 2935 | 30.3KB | 3574 | 30.3KB | 6562 | 25.5KB |
| WordFreq_Dictionary | 10,000 | 424.5μs | 30.3KB | 408.2μs | 30.3KB | 320.2μs | 30.3KB | 301.2μs | 30.3KB | 737.8μs | 24.9KB |
| WordFreq_Dictionary | 1,000,000 | 46.81ms | 31.0KB | 45.85ms | 30.3KB | 35.63ms | 30.4KB | 33.36ms | 30.4KB | — | — |
## 10. SpanMemory

| Benchmark | Size | net48 ns/op | net48 B/op | netcoreapp3.1 ns/op | netcoreapp3.1 B/op | net6.0 ns/op | net6.0 B/op | net7.0 ns/op | net7.0 B/op | Mono-in-Godot ns/op | Mono-in-Godot B/op |
|---|---|---|---|---|---|---|---|---|---|---|---|
| ArrayPool_RentReturn1k | 100 | 3820 | 0 | 2254 | 0 | 1727 | 0 | 1751 | 0 | 8667 | 0 |
| ArrayPool_RentReturn1k | 10,000 | 375.5μs | 0 | 254.0μs | 0 | 174.2μs | 0 | 209.0μs | 0 | 544.8μs | 0 |
| ArrayPool_RentReturn1k | 1,000,000 | 37.56ms | 0 | 23.71ms | 2 | 17.62ms | 32 | 18.38ms | 32 | — | — |
| Array_Copy256 | 100 | 1132 | 281 | 1938 | 280 | 875 | 280 | 759 | 280 | 3954 | 0 |
| Array_Copy256 | 10,000 | 111.4μs | 281 | 189.0μs | 280 | 83.8μs | 280 | 73.6μs | 280 | 328.9μs | 0 |
| Array_Copy256 | 1,000,000 | 10.81ms | 384 | 18.92ms | 282 | 8.52ms | 296 | 7.24ms | 288 | — | — |
| Buffer_BlockCopy256 | 100 | 1165 | 281 | 1607 | 280 | 966 | 280 | 876 | 280 | 2946 | 533 |
| Buffer_BlockCopy256 | 10,000 | 112.4μs | 281 | 158.6μs | 280 | 89.2μs | 280 | 87.6μs | 280 | 353.5μs | 0 |
| Buffer_BlockCopy256 | 1,000,000 | 11.07ms | 384 | 15.48ms | 281 | 8.67ms | 296 | 8.65ms | 296 | — | — |
| CharSpan_Copy128 | 100 | 1404 | 305 | 631 | 304 | 595 | 304 | 620 | 304 | 4679 | 0 |
| CharSpan_Copy128 | 10,000 | 132.5μs | 306 | 57.6μs | 304 | 55.0μs | 304 | 57.6μs | 304 | 585.8μs | 0 |
| CharSpan_Copy128 | 1,000,000 | 13.16ms | 384 | 5.64ms | 313 | 5.59ms | 312 | 5.72ms | 312 | — | — |
| Span_Fill64 | 100 | 599 | 0 | 656 | 0 | 808 | 0 | 824 | 0 | 1554 | 0 |
| Span_Fill64 | 10,000 | 56.1μs | 0 | 65.2μs | 0 | 79.6μs | 0 | 81.9μs | 0 | 141.8μs | 0 |
| Span_Fill64 | 1,000,000 | 5.77ms | 0 | 6.57ms | 0 | 8.12ms | 16 | 8.14ms | 16 | — | — |
| Span_IndexerLoop | 100 | 116 | 0 | 75 | 0 | 72 | 0 | 68 | 0 | 321 | 0 |
| Span_IndexerLoop | 10,000 | 8125 | 0 | 4560 | 0 | 4556 | 0 | 4807 | 0 | 18.0μs | 0 |
| Span_IndexerLoop | 1,000,000 | 796.0μs | 0 | 454.6μs | 0 | 456.8μs | 0 | 471.9μs | 0 | — | — |
| Span_Reverse64 | 100 | 3351 | 0 | 2153 | 0 | 2169 | 0 | 438 | 0 | 4525 | 0 |
| Span_Reverse64 | 10,000 | 314.8μs | 0 | 210.9μs | 0 | 210.2μs | 0 | 46.9μs | 0 | 478.7μs | 0 |
| Span_Reverse64 | 1,000,000 | 31.14ms | 0 | 21.53ms | 2 | 21.51ms | 32 | 5.30ms | 8 | — | — |
| Span_SequenceEqual256 | 100 | 554 | 24 | 262 | 24 | 254 | 24 | 210 | 24 | 2417 | 0 |
| Span_SequenceEqual256 | 10,000 | 47.8μs | 24 | 21.4μs | 24 | 20.8μs | 24 | 16.5μs | 24 | 200.9μs | 0 |
| Span_SequenceEqual256 | 1,000,000 | 4.80ms | 0 | 2.22ms | 24 | 2.06ms | 28 | 1.63ms | 26 | — | — |
| Span_SliceCopy8 | 100 | 920 | 24 | 395 | 24 | 400 | 24 | 401 | 24 | 4712 | 425 |
| Span_SliceCopy8 | 10,000 | 89.0μs | 25 | 36.1μs | 24 | 37.9μs | 24 | 36.7μs | 24 | 426.6μs | 112 |
| Span_SliceCopy8 | 1,000,000 | 8.87ms | 0 | 3.54ms | 24 | 3.72ms | 28 | 3.86ms | 28 | — | — |
| StackAlloc_Fill128 | 100 | 609 | 0 | 619 | 0 | 715 | 0 | 668 | 0 | 3404 | 0 |
| StackAlloc_Fill128 | 10,000 | 61.9μs | 0 | 59.4μs | 0 | 67.4μs | 0 | 68.5μs | 0 | 282.6μs | 0 |
| StackAlloc_Fill128 | 1,000,000 | 6.19ms | 0 | 5.97ms | 0 | 6.57ms | 8 | 6.74ms | 8 | — | — |
## 11. 瓶颈分析与优化建议

### 各类别最慢项 Top 3（最大规模，ns/调用）

- Async/TaskRun_Offload_Wait (size=10,000): 55.53ms 总耗时 ≈ 5553/元素
- Async/TaskRun_Offload_Wait (size=10,000): 13.33ms 总耗时 ≈ 1333/元素
- Async/WhenAll_64 (size=10,000): 12.60ms 总耗时 ≈ 1260/元素
- Collections/List_Contains_Miss (size=10,000): 137.10ms 总耗时 ≈ 13.7μs/元素
- Collections/List_Contains_Miss (size=10,000): 107.50ms 总耗时 ≈ 10.8μs/元素
- Collections/List_Contains_Miss (size=10,000): 18.61ms 总耗时 ≈ 1861/元素
- Delegates/Event_AddRemove (size=1,000,000): 35.11ms 总耗时 ≈ 35/元素
- Delegates/Multicast8_Invoke (size=1,000,000): 26.30ms 总耗时 ≈ 26/元素
- Delegates/Multicast8_Invoke (size=1,000,000): 26.07ms 总耗时 ≈ 26/元素
- GcAlloc/GC_Collect_Gen0 (size=10,000): 457.80ms 总耗时 ≈ 45.8μs/元素
- GcAlloc/GC_Collect_Gen0 (size=10,000): 237.96ms 总耗时 ≈ 23.8μs/元素
- GcAlloc/GC_Collect_Gen0 (size=10,000): 231.92ms 总耗时 ≈ 23.2μs/元素
- Linq/OrderBy_First (size=1,000,000): 88.26ms 总耗时 ≈ 88/元素
- Linq/OrderBy_Take10 (size=1,000,000): 88.20ms 总耗时 ≈ 88/元素
- Linq/GroupBy_Count (size=1,000,000): 21.90ms 总耗时 ≈ 22/元素
- Primitives/Enum_Parse (size=1,000,000): 233.64ms 总耗时 ≈ 234/元素
- Primitives/String_Concat_Interp (size=1,000,000): 204.83ms 总耗时 ≈ 205/元素
- Primitives/String_Concat_Interp (size=1,000,000): 97.20ms 总耗时 ≈ 97/元素
- Reflection/Expression_Compile (size=10,000): 30.78s 总耗时 ≈ 3.08ms/元素
- Reflection/Expression_Compile (size=10,000): 617.78ms 总耗时 ≈ 61.8μs/元素
- Reflection/Expression_Compile (size=10,000): 440.24ms 总耗时 ≈ 44.0μs/元素
- Scenarios/Json_ReflectionSerialize (size=1,000,000): 1.01s 总耗时 ≈ 1006/元素
- Scenarios/Json_ManualSerialize (size=1,000,000): 658.35ms 总耗时 ≈ 658/元素
- Scenarios/Json_ReflectionSerialize (size=1,000,000): 613.39ms 总耗时 ≈ 613/元素
- SpanMemory/ArrayPool_RentReturn1k (size=1,000,000): 37.56ms 总耗时 ≈ 38/元素
- SpanMemory/Span_Reverse64 (size=1,000,000): 31.14ms 总耗时 ≈ 31/元素
- SpanMemory/ArrayPool_RentReturn1k (size=1,000,000): 23.71ms 总耗时 ≈ 24/元素

### 分配大户 Top 10（B/调用，size=10,000）

- Reflection/Expression_Compile: 51.3MB (5379.7 B/元素)
- Reflection/Expression_Compile: 46.1MB (4838.1 B/元素)
- Reflection/Expression_Compile: 45.8MB (4806.2 B/元素)
- Reflection/Expression_Compile: 44.4MB (4654.8 B/元素)
- GcAlloc/Array_New_Class16: 6.4MB (666.0 B/元素)
- GcAlloc/Array_New_Class16: 6.3MB (664.0 B/元素)
- GcAlloc/Array_New_Class16: 6.3MB (664.0 B/元素)
- GcAlloc/Array_New_Class16: 6.3MB (664.0 B/元素)
- Async/WhenAll_64: 6.0MB (625.9 B/元素)
- Async/WhenAll_64: 5.9MB (616.1 B/元素)

### 版本演进: net7.0 / net48 加速比（中位数，越大越好；<1 为回归）

| Benchmark | Size | net48 ns | net7.0 ns | Speedup |
|---|---|---|---|---|
| Linq/Select_Project_Last | 1,000,000 | 6.23ms | 39 | 160037.21× |
| Linq/Select_Project_Last | 10,000 | 61.3μs | 39 | 1565.26× |
| Collections/List_Contains_Miss | 100 | 15.0μs | 870 | 17.23× |
| Linq/Select_Project_Last | 100 | 619 | 39 | 16.04× |
| Collections/List_Contains_Miss | 10,000 | 137.33ms | 9.24ms | 14.86× |
| Linq/OrderBy_First | 1,000,000 | 87.99ms | 8.44ms | 10.43× |
| Reflection/GetCustomAttribute | 10,000 | 8.60ms | 1.01ms | 8.50× |
| Reflection/GetCustomAttribute | 100 | 84.5μs | 10.2μs | 8.30× |
| SpanMemory/Span_Reverse64 | 100 | 3345 | 436 | 7.67× |
| Linq/OrderBy_Take10 | 1,000,000 | 88.16ms | 11.65ms | 7.57× |
| Reflection/Property_GetValue | 10,000 | 994.6μs | 138.9μs | 7.16× |
| Reflection/Property_GetValue | 100 | 10000 | 1403 | 7.13× |

**回归项 (net7.0 反而慢 20%+):** SpanMemory/Span_Fill64 (0.68×)、SpanMemory/Span_Fill64 (0.71×)、SpanMemory/Span_Fill64 (0.73×)、GcAlloc/Array_New_Class16 (0.74×)、GcAlloc/Class_New_Big (0.76×)、GcAlloc/Array_New_Class16 (0.76×)、GcAlloc/GC_Collect_Gen0 (0.78×)、GcAlloc/Class_New_Small (0.78×)

### 优化建议（基于数据的规则库）

- LINQ 链 (Where+Select+Sum) 比手写循环慢 10.3× — 热路径建议手写 for 循环
- Int 装箱每次产生 24 B 分配 — 热路径改泛型集合/避免 object 传值
- 捕获闭包 lambda 每次调用分配 88 B — 高频回调把捕获变量提为字段或用 static lambda
- TaskCompletionSource 往返 ≈ 30 ns/次 — 高频同步完成路径考虑 ValueTask / 直接回调（本项目 Godot 基准实测 TCS roundtrip 达 25μs+）
- Mono-in-Godot 显式跳过 2 行: skipped-in-godot (sync-context deadlock) ×2 — 已如实标注而非报错
- Mono-in-Godot 列在 Primitives, Linq, Delegates, Scenarios, SpanMemory 的 1M 规模行标 "—": 宿主以 CSBENCH_MAXSIZE=10000 规模上限运行（Mono 大规模数组路径不可靠，属设计内豁免，非数据缺失）

### 冷启动首调成本（新进程，size=100，含 JIT/静态构造，ms）

| Benchmark | Mono-in-Godot | net48 | net6.0 | net7.0 | netcoreapp3.1 |
|---|---|---|---|---|---|
| Reflection/Activator_CreateInstance | 46.523 | 0.236 | 0.380 | 0.252 | 0.471 |
| Reflection/Expression_Compile | 45.190 | 8.294 | 22.828 | 24.391 | 18.769 |
| Reflection/GetCustomAttribute | 31.284 | 0.223 | 0.322 | 0.106 | 0.178 |
| Async/TaskRun_Offload_Wait | 10.367 | 1.153 | 0.944 | 0.561 | 0.643 |
| Scenarios/SortAggregate_Pipeline | 8.868 | 3.090 | 2.726 | 5.142 | 4.650 |
| SpanMemory/Span_SliceCopy8 | 8.286 | 16.521 | 1.698 | 2.756 | 1.264 |
| GcAlloc/GC_Collect_Gen0 | 7.064 | 1.858 | 5.036 | 3.755 | 2.349 |
| SpanMemory/ArrayPool_RentReturn1k | 6.935 | 6.504 | 2.075 | 2.025 | 2.519 |
| SpanMemory/Span_SequenceEqual256 | 5.448 | 4.359 | 0.586 | 0.134 | 0.640 |
| Scenarios/Csv_Parse | 5.254 | 1.044 | 1.235 | 0.432 | 1.714 |
| Primitives/DateTime_Now | 5.144 | 1.011 | 0.955 | 0.924 | 1.649 |
| Scenarios/Json_ManualSerialize | 4.905 | 1.694 | 1.784 | 0.893 | 2.222 |
| Reflection/Property_GetValue | 3.660 | 0.147 | 0.276 | 0.254 | 0.152 |
| SpanMemory/CharSpan_Copy128 | 3.411 | 2.419 | 0.755 | 0.213 | 0.722 |
| Primitives/Enum_Parse | 3.014 | 0.350 | 0.258 | 0.180 | 0.595 |
| Primitives/Double_Parse | 2.947 | 0.133 | 0.269 | 0.152 | 0.315 |
| Async/Await_CompletedTask | 2.889 | 1.150 | 2.151 | 0.564 | 2.040 |
| Async/ProducerConsumer_Queue | 2.705 | 6.076 | 5.178 | 4.909 | 4.967 |
| Primitives/String_Concat_Interp | 2.423 | 0.284 | 0.205 | 0.079 | 0.239 |
| Linq/Where_Filter_Count | 2.381 | 2.832 | 3.525 | 11.245 | 2.592 |
| Reflection/Method_Invoke_Instance | 1.918 | 0.223 | 0.199 | 1.848 | 0.264 |
| Scenarios/Json_ReflectionSerialize | 1.877 | 1.161 | 1.298 | 0.513 | 1.205 |
| Linq/OrderBy_First | 1.842 | 3.262 | 1.226 | 1.941 | 2.073 |
| Linq/GroupBy_Count | 1.665 | 4.156 | 2.889 | 1.991 | 2.891 |
| Async/WhenAll_8 | 1.580 | 1.399 | 1.225 | 0.552 | 0.960 |

> 注: 冷启动在同进程内逐项首调，共享 JIT 预热会带来向下的偏差，用于量级对比而非精确值。
