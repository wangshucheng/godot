# C# 基准测试对比报告

> 生成时间: 2026-08-19 09:33:53 | 引擎: BenchmarkDotNet (bdn) / 自研进程内采集 (fallback/godot)

## 1. 测试环境与数据来源

| 列 | 引擎 | 运行时 | TFM | Profile | 模式 | CPU 时间(s) | 来源 |
|---|---|---|---|---|---|---|---|
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | warm | 1.7 | csbench_godot_Async.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | warm | 1.0 | csbench_godot_Collections.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | warm | 0.1 | csbench_godot_Delegates.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | warm | 3.9 | csbench_godot_GcAlloc.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | warm | 0.1 | csbench_godot_Linq.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | warm | 0.5 | csbench_godot_Primitives.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | warm | 164.7 | csbench_godot_Reflection.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | warm | 0.8 | csbench_godot_Scenarios.json |
| Mono-in-Godot | godot | Mono 6.12.0 (Visual Studio built mono) … | mono6.12-godot | quick | warm | 0.2 | csbench_godot_SpanMemory.json |
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
| net48 | fallback | .NET Framework 4.8.9339.0 | net48 | quick | cold | 0.2 | fallback_net48_cold.json |
| net6.0 | fallback | .NET 6.0.22 | net6.0 | quick | cold | 0.2 | fallback_net6.0_cold.json |
| net7.0 | fallback | .NET 7.0.11 | net7.0 | quick | cold | 0.2 | fallback_net7.0_cold.json |
| netcoreapp3.1 | fallback | .NET Core 3.1.2 | netcoreapp3.1 | quick | cold | 0.2 | fallback_netcoreapp3.1_cold.json |

**方法说明**: 工作负载统一签名 `Action<size>`，一次调用执行 size 个基本操作（或处理 size 元数据集）；规模三档 100 / 10,000 / 1,000,000（异步/反射/GC 分配类为 100 / 10,000）；quick 档 = ShortRun(3 warmup + 3 迭代, BDN) 或自适应 ≥5 次迭代 ≥150ms（fallback/godot）。表内数值为 ns/调用（每万次基本操作除以 size 可得 ns/元素）。冷启动为独立新进程首调（size=100，无预热）。

## 2. Async

| Benchmark | Size | net48 ns/op | net48 B/op | netcoreapp3.1 ns/op | netcoreapp3.1 B/op | net6.0 ns/op | net6.0 B/op | net7.0 ns/op | net7.0 B/op | Mono-in-Godot ns/op | Mono-in-Godot B/op |
|---|---|---|---|---|---|---|---|---|---|---|---|
| AwaitChain_Depth16 | 100 | 6983 | 341 | 3446 | 341 | 3317 | 683 | 6983 | 510 | 10.1μs | 0 |
| AwaitChain_Depth16 | 10,000 | 757.3μs | 341 | 316.5μs | 341 | 326.2μs | 683 | 210.9μs | 510 | 998.5μs | 0 |
| AwaitChain_Depth4 | 100 | 1738 | 341 | 942 | 341 | 729 | 341 | 2492 | 510 | 3312 | 0 |
| AwaitChain_Depth4 | 10,000 | 163.9μs | 341 | 100.5μs | 341 | 64.8μs | 683 | 79.7μs | 510 | 249.6μs | 0 |
| Await_CompletedTask | 100 | 488 | 341 | 321 | 341 | 221 | 341 | 658 | 683 | 696 | 0 |
| Await_CompletedTask | 10,000 | 60.9μs | 341 | 18.8μs | 341 | 15.1μs | 683 | 17.8μs | 683 | 55.1μs | 0 |
| Await_FromResult | 100 | 1917 | 8.0KB | 954 | 7.3KB | 946 | 7.0KB | 2042 | 7.0KB | 2292 | 0 |
| Await_FromResult | 10,000 | 146.8μs | 15.7KB | 81.0μs | 193.3KB | 85.2μs | 193.0KB | 159.3μs | 193.0KB | 285.5μs | 0 |
| Await_TaskYield | 100 | 97.5μs | 7.5KB | 75.6μs | 2.3KB | 73.4μs | 1.7KB | 44.3μs | 4.2KB | skip | — |
| Await_TaskYield | 10,000 | 9.02ms | 185.9KB | 6.47ms | 341 | 6.04ms | 1.3KB | 3.39ms | 1.5KB | skip | — |
| ConfigureAwaitFalse_Chain4 | 100 | 4050 | 341 | 3367 | 341 | 3162 | 683 | 4592 | 683 | 4850 | 0 |
| ConfigureAwaitFalse_Chain4 | 10,000 | 392.9μs | 341 | 331.8μs | 341 | 315.0μs | 683 | 280.9μs | 683 | 459.3μs | 0 |
| ProducerConsumer_Queue | 100 | 19.3μs | 1.7KB | 16.1μs | 1.0KB | 14.3μs | 1.3KB | 20.2μs | 1.5KB | 28.6μs | 0 |
| ProducerConsumer_Queue | 10,000 | 1.96ms | 84.0KB | 1.58ms | 1.0KB | 1.50ms | 1.3KB | 1.76ms | 1.5KB | 2.82ms | 0 |
| SemaphoreSlim_WaitRelease | 100 | 8546 | 341 | 3838 | 341 | 3825 | 683 | 4058 | 683 | 11.8μs | 0 |
| SemaphoreSlim_WaitRelease | 10,000 | 884.1μs | 341 | 384.7μs | 341 | 385.0μs | 683 | 405.9μs | 510 | 1.15ms | 40 |
| TaskRun_Offload_Wait | 100 | 169.7μs | 9.0KB | 107.9μs | 7.3KB | 78.7μs | 7.7KB | 73.1μs | 7.7KB | 552.8μs | 0 |
| TaskRun_Offload_Wait | 10,000 | 13.87ms | 358.9KB | 8.54ms | 23.2KB | 8.02ms | 59.8KB | 7.93ms | 59.0KB | 55.53ms | 75.5KB |
| Tcs_SetResult_Await | 100 | 8125 | 10.3KB | 2892 | 9.7KB | 2942 | 10.0KB | 3912 | 10.0KB | 5650 | 0 |
| Tcs_SetResult_Await | 10,000 | 621.0μs | 250.7KB | 301.7μs | 172.7KB | 285.8μs | 172.7KB | 326.2μs | 172.7KB | 636.6μs | 0 |
| WhenAll_64 | 100 | 79.7μs | 62.0KB | 84.9μs | 61.0KB | 113.4μs | 61.3KB | 82.8μs | 61.3KB | 113.4μs | 0 |
| WhenAll_64 | 10,000 | 8.54ms | 310.7KB | 8.12ms | 216.0KB | 13.07ms | 403.8KB | 8.84ms | 252.9KB | 12.60ms | 0 |
| WhenAll_8 | 100 | 15.7μs | 17.7KB | 11.5μs | 16.7KB | 21.9μs | 17.0KB | 14.8μs | 17.0KB | 19.5μs | 0 |
| WhenAll_8 | 10,000 | 1.56ms | 188.1KB | 1.20ms | 109.8KB | 1.89ms | 110.1KB | 1.41ms | 110.1KB | 2.16ms | 0 |
## 3. Collections

| Benchmark | Size | net48 ns/op | net48 B/op | netcoreapp3.1 ns/op | netcoreapp3.1 B/op | net6.0 ns/op | net6.0 B/op | net7.0 ns/op | net7.0 B/op | Mono-in-Godot ns/op | Mono-in-Godot B/op |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Array_Clone_Sort | 100 | 838 | 683 | 629 | 691 | 550 | 691 | 712 | 858 | 3071 | 0 |
| Array_Clone_Sort | 10,000 | 106.2μs | 39.4KB | 81.5μs | 39.5KB | 77.7μs | 39.8KB | 89.3μs | 39.8KB | 691.8μs | 39.1KB |
| Array_ForIterate | 100 | 121 | 341 | 112 | 341 | 96 | 341 | 262 | 168 | 225 | 0 |
| Array_ForIterate | 10,000 | 4412 | 341 | 4450 | 341 | 3992 | 683 | 5800 | 510 | 8054 | 0 |
| Dictionary_Add_Grow | 100 | 2021 | 7.3KB | 1538 | 7.3KB | 1404 | 7.7KB | 3817 | 7.8KB | 3025 | 0 |
| Dictionary_Add_Grow | 10,000 | 327.8μs | 216.4KB | 222.1μs | 216.4KB | 197.7μs | 216.7KB | 246.5μs | 216.7KB | 370.3μs | 629.4KB |
| Dictionary_Add_Prealloc | 100 | 1025 | 2.3KB | 733 | 2.3KB | 642 | 2.6KB | 796 | 2.6KB | 1367 | 0 |
| Dictionary_Add_Prealloc | 10,000 | 118.4μs | 33.2KB | 137.4μs | 33.2KB | 96.8μs | 33.6KB | 106.9μs | 33.6KB | 164.9μs | 197.2KB |
| Dictionary_IterateForeach | 100 | 746 | 341 | 679 | 341 | 279 | 341 | 1162 | 683 | 550 | 0 |
| Dictionary_IterateForeach | 10,000 | 66.0μs | 341 | 61.0μs | 341 | 26.4μs | 683 | 101.5μs | 683 | 47.9μs | 0 |
| Dictionary_Lookup_Hit | 100 | 854 | 341 | 792 | 341 | 550 | 683 | 883 | 341 | 1125 | 0 |
| Dictionary_Lookup_Hit | 10,000 | 73.7μs | 341 | 73.2μs | 341 | 47.0μs | 683 | 50.4μs | 683 | 100.1μs | 0 |
| Dictionary_Lookup_Miss | 100 | 662 | 341 | 583 | 341 | 412 | 341 | 588 | 341 | 771 | 0 |
| Dictionary_Lookup_Miss | 10,000 | 58.6μs | 341 | 55.5μs | 341 | 37.8μs | 683 | 41.3μs | 683 | 67.2μs | 0 |
| HashSet_Add | 100 | 2371 | 6.0KB | 2138 | 6.0KB | 1200 | 6.3KB | 3442 | 6.5KB | 2979 | 0 |
| HashSet_Add | 10,000 | 226.0μs | 63.4KB | 208.5μs | 63.4KB | 173.6μs | 63.7KB | 188.4μs | 63.7KB | 343.9μs | 502.2KB |
| HashSet_Contains_Hit | 100 | 788 | 341 | 971 | 341 | 525 | 683 | 629 | 683 | 1129 | 0 |
| HashSet_Contains_Hit | 10,000 | 71.7μs | 341 | 80.3μs | 341 | 45.3μs | 683 | 42.9μs | 683 | 103.5μs | 0 |
| List_Add_Grow | 100 | 388 | 1.3KB | 308 | 1.3KB | 308 | 1.7KB | 400 | 1.7KB | 592 | 0 |
| List_Add_Grow | 10,000 | 27.1μs | 128.5KB | 18.5μs | 128.5KB | 19.1μs | 128.9KB | 50.7μs | 129.0KB | 52.5μs | 119.8KB |
| List_Add_Prealloc | 100 | 254 | 683 | 162 | 683 | 171 | 683 | 279 | 1016 | 258 | 0 |
| List_Add_Prealloc | 10,000 | 19.7μs | 39.4KB | 13.1μs | 39.4KB | 13.2μs | 39.8KB | 23.8μs | 39.8KB | 19.1μs | 39.0KB |
| List_Contains_Miss | 100 | 15.0μs | 341 | 2183 | 341 | 1742 | 683 | 1138 | 683 | 12.3μs | 0 |
| List_Contains_Miss | 10,000 | 147.57ms | 1.6KB | 20.99ms | 2.0KB | 15.12ms | 2.4KB | 9.44ms | 1.0KB | 107.50ms | 0 |
| List_IndexGet | 100 | 150 | 341 | 121 | 341 | 112 | 683 | 271 | 683 | 275 | 0 |
| List_IndexGet | 10,000 | 6742 | 341 | 5550 | 341 | 5292 | 683 | 7212 | 683 | 20.3μs | 0 |
| List_IterateFor | 100 | 138 | 341 | 1121 | 341 | 275 | 683 | 450 | 683 | 338 | 0 |
| List_IterateFor | 10,000 | 6746 | 341 | 5333 | 341 | 5296 | 683 | 8129 | 683 | 20.9μs | 0 |
| List_IterateForeach | 100 | 242 | 341 | 246 | 341 | 129 | 341 | 350 | 510 | 375 | 0 |
| List_IterateForeach | 10,000 | 16.3μs | 341 | 19.0μs | 341 | 6771 | 683 | 15.7μs | 510 | 25.3μs | 0 |
| Queue_Stack_PushPop | 100 | 1862 | 2.7KB | 2496 | 2.7KB | 1621 | 3.0KB | 2225 | 3.0KB | 2404 | 0 |
| Queue_Stack_PushPop | 10,000 | 177.9μs | 257.0KB | 211.6μs | 257.0KB | 130.3μs | 257.3KB | 187.0μs | 257.1KB | 210.5μs | 239.8KB |
## 4. Delegates

| Benchmark | Size | net48 ns/op | net48 B/op | netcoreapp3.1 ns/op | netcoreapp3.1 B/op | net6.0 ns/op | net6.0 B/op | net7.0 ns/op | net7.0 B/op | Mono-in-Godot ns/op | Mono-in-Godot B/op |
|---|---|---|---|---|---|---|---|---|---|---|---|
| ActionInt_Invoke | 100 | 296 | 341 | 254 | 341 | 204 | 683 | 304 | 510 | 196 | 0 |
| ActionInt_Invoke | 10,000 | 22.5μs | 341 | 22.3μs | 341 | 17.8μs | 683 | 23.6μs | 510 | 16.0μs | 0 |
| ActionInt_Invoke | 1,000,000 | 2.34ms | 341 | 2.27ms | 341 | 1.82ms | 683 | 2.37ms | 510 | — | — |
| Action_Invoke | 100 | 325 | 341 | 238 | 341 | 238 | 683 | 304 | 683 | 246 | 0 |
| Action_Invoke | 10,000 | 25.8μs | 341 | 23.4μs | 341 | 20.0μs | 683 | 23.3μs | 510 | 17.9μs | 0 |
| Action_Invoke | 1,000,000 | 2.37ms | 341 | 2.19ms | 341 | 2.06ms | 683 | 2.35ms | 510 | — | — |
| ClosureCapture_Invoke | 100 | 1108 | 8.7KB | 1129 | 8.7KB | 988 | 9.0KB | 3771 | 9.2KB | 4625 | 0 |
| ClosureCapture_Invoke | 10,000 | 103.1μs | 94.0KB | 104.5μs | 94.0KB | 128.6μs | 94.3KB | 162.9μs | 94.3KB | 197.9μs | 0 |
| ClosureCapture_Invoke | 1,000,000 | 8.41ms | 174.0KB | 10.07ms | 177.9KB | 11.74ms | 174.5KB | 11.53ms | 175.7KB | — | — |
| Event_AddRemove | 100 | 3533 | 341 | 2767 | 341 | 2667 | 683 | 3329 | 683 | 4662 | 0 |
| Event_AddRemove | 10,000 | 350.2μs | 341 | 282.7μs | 341 | 336.1μs | 683 | 334.4μs | 683 | 464.2μs | 0 |
| Event_AddRemove | 1,000,000 | 35.06ms | 1.6KB | 28.55ms | 2.7KB | 26.87ms | 4.0KB | 30.11ms | 3.2KB | — | — |
| FuncInt_Invoke | 100 | 238 | 341 | 233 | 341 | 208 | 341 | 325 | 168 | 233 | 0 |
| FuncInt_Invoke | 10,000 | 20.0μs | 341 | 20.0μs | 341 | 17.8μs | 683 | 21.4μs | 510 | 20.1μs | 0 |
| FuncInt_Invoke | 1,000,000 | 2.04ms | 341 | 2.04ms | 341 | 1.82ms | 683 | 2.08ms | 510 | — | — |
| Interface_vs_DirectCall | 100 | 279 | 341 | 88 | 341 | 75 | 683 | 442 | 168 | 392 | 441 |
| Interface_vs_DirectCall | 10,000 | 24.5μs | 341 | 5208 | 341 | 4412 | 683 | 8750 | 510 | 19.1μs | 0 |
| Interface_vs_DirectCall | 1,000,000 | 2.49ms | 341 | 457.2μs | 341 | 461.3μs | 683 | 459.5μs | 510 | — | — |
| Multicast2_Invoke | 100 | 754 | 341 | 929 | 341 | 971 | 683 | 1050 | 510 | 558 | 0 |
| Multicast2_Invoke | 10,000 | 72.1μs | 341 | 91.4μs | 341 | 102.5μs | 683 | 104.8μs | 510 | 54.2μs | 0 |
| Multicast2_Invoke | 1,000,000 | 7.36ms | 390 | 9.19ms | 482 | 9.81ms | 1.5KB | 10.53ms | 815 | — | — |
| Multicast8_Invoke | 100 | 2096 | 341 | 2658 | 341 | 2975 | 683 | 3175 | 510 | 1975 | 0 |
| Multicast8_Invoke | 10,000 | 206.7μs | 341 | 281.2μs | 341 | 311.7μs | 683 | 317.2μs | 510 | 161.4μs | 0 |
| Multicast8_Invoke | 1,000,000 | 20.87ms | 1.0KB | 26.96ms | 1.3KB | 29.50ms | 2.7KB | 32.42ms | 2.4KB | — | — |
| NewDelegate_Creation | 100 | 738 | 6.3KB | 646 | 6.3KB | 646 | 6.7KB | 2600 | 6.8KB | 2683 | 0 |
| NewDelegate_Creation | 10,000 | 90.6μs | 115.0KB | 57.9μs | 115.0KB | 67.5μs | 115.3KB | 125.8μs | 115.3KB | 220.9μs | 0 |
| NewDelegate_Creation | 1,000,000 | 5.51ms | 219.5KB | 5.10ms | 219.5KB | 7.90ms | 273.8KB | 7.72ms | 15.7KB | — | — |
| StaticLambdaNoCapture | 100 | 188 | 341 | 242 | 341 | 212 | 341 | 296 | 510 | 179 | 0 |
| StaticLambdaNoCapture | 10,000 | 15.6μs | 341 | 20.0μs | 341 | 16.4μs | 683 | 18.2μs | 510 | 16.0μs | 0 |
| StaticLambdaNoCapture | 1,000,000 | 1.59ms | 341 | 2.03ms | 341 | 1.73ms | 683 | 1.59ms | 510 | — | — |
## 5. GcAlloc

| Benchmark | Size | net48 ns/op | net48 B/op | netcoreapp3.1 ns/op | netcoreapp3.1 B/op | net6.0 ns/op | net6.0 B/op | net7.0 ns/op | net7.0 B/op | Mono-in-Godot ns/op | Mono-in-Godot B/op |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Array_New_Class16 | 100 | 9696 | 65.3KB | 7129 | 65.3KB | 8729 | 65.7KB | 21.9μs | 65.7KB | 16.9μs | 0 |
| Array_New_Class16 | 10,000 | 1.09ms | 105.0KB | 828.1μs | 105.0KB | 1.22ms | 105.3KB | 1.20ms | 105.3KB | 1.89ms | 1.1KB |
| Array_New_Int128 | 100 | 4500 | 52.7KB | 1896 | 52.7KB | 2933 | 53.0KB | 4142 | 53.0KB | 3612 | 432 |
| Array_New_Int128 | 10,000 | 489.9μs | 130.2KB | 178.6μs | 130.2KB | 380.5μs | 130.5KB | 406.4μs | 130.5KB | 335.1μs | 0 |
| Boxing_Plus_Equals | 100 | 462 | 2.7KB | 300 | 2.7KB | 342 | 3.0KB | 1142 | 5.3KB | 1433 | 0 |
| Boxing_Plus_Equals | 10,000 | 40.8μs | 235.3KB | 29.9μs | 235.3KB | 30.6μs | 235.7KB | 49.5μs | 3.0KB | 136.4μs | 0 |
| Class_New_Big | 100 | 854 | 7.3KB | 571 | 7.3KB | 496 | 7.7KB | 1208 | 7.7KB | 1104 | 0 |
| Class_New_Big | 10,000 | 115.5μs | 193.3KB | 69.5μs | 193.3KB | 67.7μs | 193.7KB | 95.2μs | 193.7KB | 108.4μs | 7 |
| Class_New_Small | 100 | 450 | 3.3KB | 300 | 3.3KB | 383 | 4.0KB | 696 | 3.7KB | 750 | 0 |
| Class_New_Small | 10,000 | 55.0μs | 57.7KB | 32.3μs | 57.7KB | 42.8μs | 58.0KB | 73.0μs | 58.0KB | 115.8μs | 0 |
| Finalizable_New | 100 | 3412 | 2.7KB | 3221 | 2.7KB | 3379 | 3.0KB | 3412 | 3.0KB | 17.1μs | 0 |
| Finalizable_New | 10,000 | 407.0μs | 235.3KB | 433.2μs | 235.3KB | 645.4μs | 235.7KB | 425.7μs | 235.7KB | 1.82ms | 0 |
| GC_Collect_Gen0 | 100 | 912.9μs | 381 | 1.10ms | 381 | 5.35ms | 383 | 3.68ms | 210 | 5.21ms | 738 |
| GC_Collect_Gen0 | 10,000 | 93.44ms | 1.6KB | 108.81ms | 1.6KB | 551.97ms | 1.6KB | 356.63ms | 848 | 457.80ms | 0 |
| ListPool_Reuse8 | 100 | 6333 | 341 | 4188 | 341 | 4600 | 683 | 33.4μs | 683 | 8604 | 0 |
| ListPool_Reuse8 | 10,000 | 653.7μs | 341 | 402.5μs | 341 | 412.6μs | 683 | 402.1μs | 683 | 934.0μs | 0 |
| String_New100 | 100 | 4400 | 23.0KB | 2458 | 22.0KB | 2533 | 22.3KB | 6296 | 22.3KB | 4238 | 0 |
| String_New100 | 10,000 | 361.9μs | 224.4KB | 238.6μs | 146.1KB | 388.6μs | 146.4KB | 569.6μs | 146.4KB | 372.5μs | 0 |
| Struct_CopyAssign_Baseline | 100 | 108 | 341 | 104 | 341 | 100 | 683 | 642 | 683 | 588 | 0 |
| Struct_CopyAssign_Baseline | 10,000 | 6804 | 341 | 12.1μs | 341 | 6692 | 683 | 47.5μs | 683 | 49.4μs | 0 |
## 6. Linq

| Benchmark | Size | net48 ns/op | net48 B/op | netcoreapp3.1 ns/op | netcoreapp3.1 B/op | net6.0 ns/op | net6.0 B/op | net7.0 ns/op | net7.0 B/op | Mono-in-Godot ns/op | Mono-in-Godot B/op |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Aggregate_Max | 100 | 1108 | 341 | 654 | 341 | 596 | 341 | 704 | 683 | 925 | 0 |
| Aggregate_Max | 10,000 | 62.0μs | 341 | 50.5μs | 341 | 53.1μs | 683 | 52.9μs | 683 | 74.1μs | 0 |
| Aggregate_Max | 1,000,000 | 6.48ms | 341 | 5.19ms | 341 | 5.72ms | 683 | 5.33ms | 683 | — | — |
| Any_Miss_FullScan | 100 | 662 | 341 | 600 | 341 | 721 | 683 | 638 | 683 | 875 | 0 |
| Any_Miss_FullScan | 10,000 | 53.5μs | 341 | 46.0μs | 341 | 52.5μs | 683 | 47.2μs | 683 | 75.0μs | 0 |
| Any_Miss_FullScan | 1,000,000 | 5.42ms | 341 | 4.67ms | 341 | 5.61ms | 683 | 4.86ms | 510 | — | — |
| Count_Predicate | 100 | 733 | 341 | 596 | 341 | 738 | 341 | 625 | 683 | 925 | 0 |
| Count_Predicate | 10,000 | 61.2μs | 341 | 49.7μs | 341 | 70.9μs | 683 | 56.9μs | 683 | 79.8μs | 0 |
| Count_Predicate | 1,000,000 | 6.35ms | 341 | 5.17ms | 341 | 6.89ms | 1.1KB | 6.56ms | 532 | — | — |
| Distinct_Count | 100 | 2629 | 4.3KB | 2012 | 4.3KB | 1150 | 2.3KB | 1817 | 2.5KB | 5296 | 0 |
| Distinct_Count | 10,000 | 125.8μs | 4.3KB | 121.8μs | 4.3KB | 95.9μs | 160.3KB | 103.5μs | 160.2KB | 166.7μs | 0 |
| Distinct_Count | 1,000,000 | 14.41ms | 4.4KB | 13.16ms | 4.7KB | 8.79ms | 2.0MB | 10.14ms | 7.1MB | — | — |
| FirstOrDefault_Miss | 100 | 646 | 341 | 596 | 341 | 600 | 341 | 612 | 510 | 896 | 0 |
| FirstOrDefault_Miss | 10,000 | 52.6μs | 341 | 45.7μs | 341 | 50.5μs | 683 | 50.1μs | 510 | 74.2μs | 0 |
| FirstOrDefault_Miss | 1,000,000 | 5.83ms | 341 | 4.68ms | 341 | 5.64ms | 683 | 4.93ms | 510 | — | — |
| GroupBy_Count | 100 | 5312 | 11.0KB | 5179 | 11.0KB | 5383 | 11.3KB | 8117 | 11.3KB | 8958 | 0 |
| GroupBy_Count | 10,000 | 201.3μs | 127.0KB | 190.4μs | 127.0KB | 178.4μs | 127.3KB | 233.7μs | 127.2KB | 283.5μs | 0 |
| GroupBy_Count | 1,000,000 | 21.23ms | 1.2MB | 21.33ms | 3.4MB | 17.51ms | 973.3KB | 22.77ms | 2.3MB | — | — |
| Handwritten_Sum_Loop | 100 | 133 | 341 | 121 | 341 | 108 | 683 | 258 | 510 | 229 | 0 |
| Handwritten_Sum_Loop | 10,000 | 5012 | 341 | 4592 | 341 | 4683 | 683 | 6279 | 510 | 12.9μs | 0 |
| Handwritten_Sum_Loop | 1,000,000 | 692.3μs | 341 | 513.8μs | 341 | 540.0μs | 683 | 501.2μs | 510 | — | — |
| OrderBy_First | 100 | 3500 | 1.7KB | 1042 | 341 | 1017 | 683 | 1300 | 851 | 1858 | 229 |
| OrderBy_First | 10,000 | 568.1μs | 117.7KB | 88.0μs | 341 | 89.0μs | 683 | 95.8μs | 849 | 114.5μs | 0 |
| OrderBy_First | 1,000,000 | 86.31ms | 2.3MB | 9.20ms | 964 | 9.45ms | 2.0KB | 9.89ms | 764 | — | — |
| OrderBy_Take10 | 100 | 3842 | 1.7KB | 1679 | 1.7KB | 1621 | 2.0KB | 1938 | 1.8KB | 2262 | 0 |
| OrderBy_Take10 | 10,000 | 566.9μs | 117.7KB | 125.5μs | 117.7KB | 125.1μs | 118.0KB | 142.3μs | 118.0KB | 191.3μs | 117.3KB |
| OrderBy_Take10 | 1,000,000 | 89.18ms | 2.3MB | 14.62ms | 2.1MB | 15.46ms | 2.3MB | 13.80ms | 2.1MB | — | — |
| Select_Project_Last | 100 | 904 | 341 | 175 | 341 | 133 | 683 | 188 | 341 | 108 | 0 |
| Select_Project_Last | 10,000 | 51.5μs | 341 | 146 | 341 | 108 | 683 | 154 | 341 | 142 | 0 |
| Select_Project_Last | 1,000,000 | 5.36ms | 341 | 167 | 341 | 96 | 683 | 146 | 168 | — | — |
| ToArray_Materialize | 100 | 704 | 1.0KB | 633 | 1.0KB | 1479 | 1.3KB | 1012 | 1.0KB | 758 | 0 |
| ToArray_Materialize | 10,000 | 45.7μs | 84.1KB | 27.6μs | 52.3KB | 25.9μs | 52.9KB | 45.2μs | 52.8KB | 35.2μs | 43.6KB |
| ToArray_Materialize | 1,000,000 | 5.29ms | 215.1KB | 4.23ms | 1000.7KB | 3.71ms | 571.3KB | 3.92ms | 721.4KB | — | — |
| ToList_Materialize | 100 | 929 | 1.0KB | 558 | 1.0KB | 596 | 1.3KB | 992 | 1.2KB | 725 | 0 |
| ToList_Materialize | 10,000 | 73.9μs | 64.8KB | 24.9μs | 64.8KB | 30.9μs | 64.8KB | 43.8μs | 65.0KB | 43.2μs | 55.5KB |
| ToList_Materialize | 1,000,000 | 5.95ms | 341.7KB | 4.00ms | 341.7KB | 3.96ms | 342.0KB | 3.37ms | 342.0KB | — | — |
| WhereSelect_Chain_Sum | 100 | skip | — | skip | — | skip | — | skip | — | 1233 | 0 |
| WhereSelect_Chain_Sum | 10,000 | 43.3μs | 341 | 45.2μs | 341 | 43.0μs | 683 | 70.9μs | 510 | 53.1μs | 0 |
| Where_Filter_Count | 100 | 429 | 341 | 446 | 341 | 383 | 683 | 758 | 341 | 592 | 0 |
| Where_Filter_Count | 10,000 | 26.9μs | 341 | 29.2μs | 341 | 27.9μs | 683 | 61.4μs | 683 | 38.3μs | 0 |
| Where_Filter_Count | 1,000,000 | 2.85ms | 341 | 2.99ms | 341 | 3.42ms | 683 | 5.93ms | 683 | — | — |
## 7. Primitives

| Benchmark | Size | net48 ns/op | net48 B/op | netcoreapp3.1 ns/op | netcoreapp3.1 B/op | net6.0 ns/op | net6.0 B/op | net7.0 ns/op | net7.0 B/op | Mono-in-Godot ns/op | Mono-in-Godot B/op |
|---|---|---|---|---|---|---|---|---|---|---|---|
| DateTime_Now | 100 | 6729 | 341 | 7833 | 341 | 3471 | 683 | 3221 | 510 | 4458 | 0 |
| DateTime_Now | 10,000 | 684.4μs | 341 | 907.9μs | 341 | 355.5μs | 683 | 318.4μs | 510 | 505.5μs | 0 |
| DateTime_Now | 1,000,000 | 69.41ms | 1.6KB | 83.57ms | 1.6KB | 34.26ms | 4.8KB | 30.60ms | 2.4KB | — | — |
| Decimal_Add_Loop | 100 | 1158 | 341 | 1192 | 361 | 450 | 341 | 1342 | 510 | 2117 | 0 |
| Decimal_Add_Loop | 10,000 | 113.6μs | 341 | 108.0μs | 341 | 40.7μs | 683 | 92.0μs | 510 | 238.8μs | 0 |
| Decimal_Add_Loop | 1,000,000 | 11.27ms | 568 | 11.24ms | 1.1KB | 4.13ms | 683 | 8.45ms | 680 | — | — |
| Double_Mul_Loop | 100 | 121 | 341 | 117 | 341 | 112 | 683 | 221 | 168 | 229 | 0 |
| Double_Mul_Loop | 10,000 | 8921 | 341 | 8967 | 341 | 8904 | 683 | 10.4μs | 510 | 20.2μs | 0 |
| Double_Mul_Loop | 1,000,000 | 894.7μs | 341 | 893.6μs | 196 | 894.2μs | 683 | 910.6μs | 510 | — | — |
| Double_Parse | 100 | 7783 | 341 | 6021 | 341 | 5700 | 683 | 5754 | 510 | 13.8μs | 0 |
| Double_Parse | 10,000 | 771.0μs | 341 | 633.2μs | 341 | 577.1μs | 683 | 578.2μs | 510 | 1.66ms | 0 |
| Double_Parse | 1,000,000 | 83.92ms | 1.6KB | 64.15ms | 3.2KB | 49.65ms | 4.8KB | 49.74ms | 2.4KB | — | — |
| Enum_Parse | 100 | 28.3μs | 7.3KB | 11.2μs | 2.7KB | 8958 | 3.0KB | 8188 | 2.8KB | 69.0μs | 0 |
| Enum_Parse | 10,000 | 2.36ms | 193.3KB | 949.3μs | 235.3KB | 899.2μs | 235.7KB | 749.7μs | 235.5KB | 5.40ms | 219 |
| Enum_Parse | 1,000,000 | 255.50ms | 478.8KB | 94.62ms | 163.4KB | 77.98ms | 165.1KB | 55.85ms | 163.3KB | — | — |
| Guid_NewGuid | 100 | 6262 | 341 | 6625 | 341 | 6312 | 683 | 6271 | 510 | 11.8μs | 0 |
| Guid_NewGuid | 10,000 | 633.6μs | 341 | 660.4μs | 341 | 624.8μs | 683 | 616.6μs | 510 | 1.04ms | 0 |
| Guid_NewGuid | 1,000,000 | 64.23ms | 1.6KB | 68.82ms | 3.2KB | 67.12ms | 3.2KB | 61.68ms | 2.4KB | — | — |
| Int_Add_Loop | 100 | 142 | 341 | 258 | 341 | 342 | 646 | 267 | 683 | 154 | 0 |
| Int_Add_Loop | 10,000 | 4250 | 341 | 4271 | 341 | 4238 | 683 | 6162 | 510 | 6867 | 0 |
| Int_Add_Loop | 1,000,000 | 441.5μs | 341 | 425.4μs | 341 | 454.6μs | 683 | 426.4μs | 510 | — | — |
| Int_Boxing | 100 | 796 | 2.5KB | 1408 | 2.5KB | 1183 | 2.9KB | 1196 | 2.8KB | 796 | 0 |
| Int_Boxing | 10,000 | 77.2μs | 235.2KB | 83.5μs | 235.5KB | 79.2μs | 235.7KB | 92.9μs | 235.5KB | 123.6μs | 0 |
| Int_Boxing | 1,000,000 | 3.47ms | 210.4KB | 2.77ms | 210.4KB | 3.00ms | 210.7KB | 3.21ms | 211.1KB | — | — |
| Int_Parse | 100 | 6250 | 341 | 1746 | 341 | 1667 | 683 | 1883 | 683 | 8754 | 0 |
| Int_Parse | 10,000 | 641.2μs | 341 | 173.7μs | 341 | 162.5μs | 683 | 157.2μs | 683 | 812.2μs | 0 |
| Int_Parse | 1,000,000 | 67.90ms | 1.6KB | 18.33ms | 1.8KB | 17.19ms | 1.8KB | 15.65ms | 1.6KB | — | — |
| Int_TryParse | 100 | 6292 | 341 | 1350 | 683 | 1692 | 683 | 1708 | 510 | 6938 | 0 |
| Int_TryParse | 10,000 | 661.3μs | 341 | 169.3μs | 341 | 213.5μs | 1.0KB | 153.8μs | 510 | 838.6μs | 0 |
| Int_TryParse | 1,000,000 | 66.47ms | 1.6KB | 14.38ms | 745 | 14.53ms | 1.5KB | 15.03ms | 1.2KB | — | — |
| String_Compare_Ordinal | 100 | 254 | 341 | 300 | 341 | 275 | 683 | 350 | 341 | 704 | 0 |
| String_Compare_Ordinal | 10,000 | 20.0μs | 341 | 24.5μs | 341 | 24.5μs | 683 | 28.3μs | 683 | 43.2μs | 0 |
| String_Compare_Ordinal | 1,000,000 | 2.35ms | 341 | 2.58ms | 341 | 2.53ms | 683 | 2.71ms | 683 | — | — |
| String_Concat_Builder | 100 | 5617 | 5.3KB | 2200 | 2.3KB | 1283 | 3.0KB | 1412 | 2.7KB | 8146 | 0 |
| String_Concat_Builder | 10,000 | 884.8μs | 336.6KB | 238.2μs | 207.3KB | 186.2μs | 207.8KB | 145.3μs | 207.8KB | 884.4μs | 197.3KB |
| String_Concat_Builder | 1,000,000 | 85.97ms | 6.1MB | 32.33ms | 5.3MB | 19.59ms | 3.3MB | 21.69ms | 3.8MB | — | — |
| String_Concat_Interp | 100 | 19.5μs | 15.0KB | 8892 | 8.0KB | 5400 | 8.7KB | 6721 | 8.3KB | 22.8μs | 0 |
| String_Concat_Interp | 10,000 | 1.91ms | 23.4KB | 943.0μs | 86.4KB | 586.9μs | 86.7KB | 654.6μs | 86.7KB | 2.13ms | 0 |
| String_Concat_Interp | 1,000,000 | 203.98ms | 325.5KB | 105.09ms | 167.8KB | 62.85ms | 170.8KB | 59.80ms | 170.5KB | — | — |
| String_Concat_Plus | 100 | 6396 | 6.3KB | 3092 | 6.3KB | 2238 | 6.3KB | 2512 | 6.3KB | 9067 | 0 |
| String_Concat_Plus | 10,000 | 577.4μs | 7.0KB | 342.7μs | 114.7KB | 225.5μs | 115.0KB | 230.4μs | 115.0KB | 913.8μs | 0 |
| String_Concat_Plus | 1,000,000 | 64.71ms | 933.3KB | 39.28ms | 86.6KB | 26.81ms | 699.4KB | 22.18ms | 256.3KB | — | — |
## 8. Reflection

| Benchmark | Size | net48 ns/op | net48 B/op | netcoreapp3.1 ns/op | netcoreapp3.1 B/op | net6.0 ns/op | net6.0 B/op | net7.0 ns/op | net7.0 B/op | Mono-in-Godot ns/op | Mono-in-Godot B/op |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Activator_CreateInstance | 100 | 4658 | 3.3KB | 3854 | 3.3KB | 2146 | 3.3KB | 2067 | 3.7KB | 19.2μs | 0 |
| Activator_CreateInstance | 10,000 | 478.4μs | 57.7KB | 364.1μs | 57.7KB | 191.7μs | 58.0KB | 212.2μs | 58.0KB | 2.02ms | 0 |
| CreateDelegate_ThenInvoke | 100 | 170.2μs | 6.3KB | 36.7μs | 6.3KB | 42.3μs | 6.7KB | 46.9μs | 6.7KB | 128.4μs | 0 |
| CreateDelegate_ThenInvoke | 10,000 | 18.11ms | 627.6KB | 4.23ms | 115.0KB | 3.71ms | 115.3KB | 3.79ms | 115.3KB | 13.17ms | 0 |
| Expression_Compile | 100 | 6.30ms | 68.0KB | 5.26ms | 259.7KB | 5.57ms | 271.5KB | 6.23ms | 272.9KB | 8.25ms | 0 |
| Expression_Compile | 10,000 | 618.95ms | 1.2MB | 452.37ms | 727.9KB | 440.17ms | 725.9KB | 461.10ms | 936.1KB | 30.78s | 16.6KB |
| GetCustomAttribute | 100 | 78.2μs | 16.0KB | 20.9μs | 341 | 15.0μs | 683 | 13.8μs | 683 | 98.5μs | 0 |
| GetCustomAttribute | 10,000 | 8.26ms | 273.7KB | 2.08ms | 341 | 1.54ms | 683 | 1.35ms | 683 | 11.49ms | 0 |
| Method_Invoke_Instance | 100 | 15.2μs | 5.7KB | 12.2μs | 5.7KB | 10.5μs | 3.0KB | 4112 | 2.8KB | 54.6μs | 0 |
| Method_Invoke_Instance | 10,000 | 1.57ms | 36.7KB | 1.26ms | 36.7KB | 1.18ms | 235.7KB | 386.0μs | 235.8KB | 5.60ms | 4 |
| Method_Invoke_Static | 100 | 14.8μs | 5.7KB | 13.5μs | 5.7KB | 10.2μs | 3.0KB | 3767 | 3.0KB | 37.7μs | 262 |
| Method_Invoke_Static | 10,000 | 1.48ms | 36.7KB | 1.22ms | 36.7KB | 1.06ms | 235.7KB | 332.8μs | 235.7KB | 3.77ms | 5 |
| Property_GetValue | 100 | 9900 | 2.7KB | 7942 | 2.7KB | 6400 | 3.0KB | 2479 | 3.0KB | 1454 | 0 |
| Property_GetValue | 10,000 | 997.7μs | 235.3KB | 817.5μs | 235.3KB | 638.8μs | 235.7KB | 207.7μs | 235.7KB | 136.9μs | 0 |
| Property_GetValue_ViaGetter | 100 | 9458 | 2.7KB | 7425 | 2.7KB | 6250 | 3.0KB | 2092 | 3.0KB | 51.3μs | 0 |
| Property_GetValue_ViaGetter | 10,000 | 951.8μs | 235.3KB | 769.0μs | 235.3KB | 642.8μs | 235.7KB | 177.6μs | 235.7KB | 5.45ms | 0 |
| Property_SetValue | 100 | 15.3μs | 8.7KB | 11.7μs | 8.7KB | 13.4μs | 3.0KB | 4012 | 3.0KB | 56.7μs | 0 |
| Property_SetValue | 10,000 | 1.78ms | 94.0KB | 1.28ms | 94.0KB | 844.6μs | 235.7KB | 394.9μs | 235.7KB | 5.74ms | 158 |
| Type_GetMethod | 100 | 5042 | 341 | 5288 | 341 | 4133 | 683 | 4221 | 510 | 59.6μs | 0 |
| Type_GetMethod | 10,000 | 509.5μs | 341 | 535.0μs | 341 | 408.7μs | 683 | 400.5μs | 510 | 6.11ms | 0 |
| Type_GetProperties | 100 | 5233 | 4.0KB | 6967 | 4.0KB | 6854 | 4.3KB | 6567 | 4.3KB | 64.5μs | 0 |
| Type_GetProperties | 10,000 | 544.8μs | 136.0KB | 725.0μs | 136.0KB | 673.4μs | 136.3KB | 654.4μs | 136.3KB | 6.58ms | 0 |
## 9. Scenarios

| Benchmark | Size | net48 ns/op | net48 B/op | netcoreapp3.1 ns/op | netcoreapp3.1 B/op | net6.0 ns/op | net6.0 B/op | net7.0 ns/op | net7.0 B/op | Mono-in-Godot ns/op | Mono-in-Godot B/op |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Csv_Parse | 100 | 27.0μs | 31.3KB | 16.8μs | 19.0KB | 15.8μs | 19.3KB | 16.0μs | 19.3KB | 40.3μs | 0 |
| Csv_Parse | 10,000 | 2.76ms | 98.9KB | 2.10ms | 151.1KB | 1.72ms | 151.4KB | 2.96ms | 151.7KB | 4.17ms | 0 |
| Csv_Parse | 1,000,000 | 290.36ms | 531.5KB | 198.62ms | 200.2KB | 172.18ms | 201.1KB | 227.34ms | 205.2KB | — | — |
| Json_ManualSerialize | 100 | 58.9μs | 26.2KB | 34.3μs | 22.8KB | 20.8μs | 23.4KB | 22.4μs | 23.4KB | 79.8μs | 18.8KB |
| Json_ManualSerialize | 10,000 | 6.22ms | 977.9KB | 4.04ms | 1.1MB | 3.70ms | 1.1MB | 2.27ms | 1.1MB | 8.46ms | 333.7KB |
| Json_ManualSerialize | 1,000,000 | 649.31ms | 24.4MB | 417.66ms | 24.3MB | 296.71ms | 24.3MB | 260.50ms | 72.4MB | — | — |
| Json_ReflectionSerialize | 100 | 104.1μs | 41.8KB | 56.7μs | 41.1KB | 46.1μs | 41.7KB | 108.4μs | 41.7KB | 173.5μs | 25.1KB |
| Json_ReflectionSerialize | 10,000 | 10.24ms | 1.5MB | 5.96ms | 1.5MB | 5.38ms | 1.4MB | 7.49ms | 1.4MB | 19.63ms | 1.4MB |
| Json_ReflectionSerialize | 1,000,000 | 1.01s | 28.1MB | 643.20ms | 57.4MB | 563.62ms | 57.4MB | 736.44ms | 57.4MB | — | — |
| PrimeSieve | 100 | 204 | 341 | 167 | 341 | 400 | 3.2KB | 625 | 683 | 329 | 0 |
| PrimeSieve | 10,000 | 18.6μs | 10.1KB | 19.5μs | 10.1KB | 25.2μs | 10.5KB | 24.8μs | 10.5KB | 33.6μs | 9.6KB |
| PrimeSieve | 1,000,000 | 2.37ms | 976.9KB | 2.75ms | 976.9KB | 2.76ms | 977.3KB | 2.64ms | 977.2KB | — | — |
| SortAggregate_Pipeline | 100 | 14.7μs | 12.8KB | 19.9μs | 10.0KB | 16.2μs | 10.0KB | 31.0μs | 10.2KB | 12.3μs | 0 |
| SortAggregate_Pipeline | 10,000 | 1.46ms | 203.4KB | 1.66ms | 145.3KB | 1.34ms | 145.6KB | 1.70ms | 145.3KB | 2.10ms | 155.2KB |
| SortAggregate_Pipeline | 1,000,000 | 362.52ms | 15.7MB | 351.95ms | 15.5MB | 312.20ms | 15.5MB | 360.06ms | 16.5MB | — | — |
| Text_SplitJoin | 100 | 3560 | 6.4KB | 3600 | 6.4KB | 3380 | 8.0KB | 3420 | 8.0KB | 5833 | 0 |
| Text_SplitJoin | 10,000 | 309.5μs | 498.5KB | 300.0μs | 242.5KB | 284.7μs | 242.8KB | 217.5μs | 242.8KB | 682.3μs | 346.3KB |
| Text_SplitJoin | 1,000,000 | 96.54ms | 73.0MB | 78.82ms | 47.5MB | 85.40ms | 47.5MB | 78.34ms | 47.5MB | — | — |
| Tree_BuildWalk | 100 | 620 | 1.6KB | 6020 | 1.6KB | 2120 | 3.2KB | 880 | 3.2KB | 671 | 0 |
| Tree_BuildWalk | 10,000 | 34.9μs | 39.4KB | 33.3μs | 39.4KB | 36.0μs | 39.8KB | 37.7μs | 39.8KB | 61.9μs | 38.4KB |
| Tree_BuildWalk | 1,000,000 | 4.14ms | 3.8MB | 4.49ms | 3.8MB | 4.69ms | 3.8MB | 4.60ms | 3.8MB | — | — |
| WordFreq_Dictionary | 100 | 6412 | 30.6KB | 5921 | 30.6KB | 3412 | 30.9KB | 5175 | 30.7KB | 6562 | 25.5KB |
| WordFreq_Dictionary | 10,000 | 468.5μs | 30.6KB | 421.1μs | 30.6KB | 418.1μs | 30.8KB | 446.1μs | 30.8KB | 737.8μs | 24.9KB |
| WordFreq_Dictionary | 1,000,000 | 46.37ms | 30.7KB | 47.55ms | 30.6KB | 36.76ms | 33.2KB | 40.17ms | 32.3KB | — | — |
## 10. SpanMemory

| Benchmark | Size | net48 ns/op | net48 B/op | netcoreapp3.1 ns/op | netcoreapp3.1 B/op | net6.0 ns/op | net6.0 B/op | net7.0 ns/op | net7.0 B/op | Mono-in-Godot ns/op | Mono-in-Godot B/op |
|---|---|---|---|---|---|---|---|---|---|---|---|
| ArrayPool_RentReturn1k | 100 | skip | — | skip | — | skip | — | skip | — | 8667 | 0 |
| ArrayPool_RentReturn1k | 10,000 | — | — | — | — | — | — | — | — | 544.8μs | 0 |
| Array_Copy256 | 100 | skip | — | skip | — | skip | — | skip | — | 3954 | 0 |
| Array_Copy256 | 10,000 | — | — | — | — | — | — | — | — | 328.9μs | 0 |
| Buffer_BlockCopy256 | 100 | skip | — | skip | — | skip | — | skip | — | 2946 | 533 |
| Buffer_BlockCopy256 | 10,000 | — | — | — | — | — | — | — | — | 353.5μs | 0 |
| CharSpan_Copy128 | 100 | skip | — | skip | — | skip | — | skip | — | 4679 | 0 |
| CharSpan_Copy128 | 10,000 | — | — | — | — | — | — | — | — | 585.8μs | 0 |
| Span_Fill64 | 100 | skip | — | skip | — | skip | — | skip | — | 1554 | 0 |
| Span_Fill64 | 10,000 | — | — | — | — | — | — | — | — | 141.8μs | 0 |
| Span_IndexerLoop | 100 | skip | — | skip | — | skip | — | skip | — | 321 | 0 |
| Span_IndexerLoop | 10,000 | — | — | — | — | — | — | — | — | 18.0μs | 0 |
| Span_Reverse64 | 100 | skip | — | skip | — | skip | — | skip | — | 4525 | 0 |
| Span_Reverse64 | 10,000 | — | — | — | — | — | — | — | — | 478.7μs | 0 |
| Span_SequenceEqual256 | 100 | skip | — | skip | — | skip | — | skip | — | 2417 | 0 |
| Span_SequenceEqual256 | 10,000 | — | — | — | — | — | — | — | — | 200.9μs | 0 |
| Span_SliceCopy8 | 100 | skip | — | skip | — | skip | — | skip | — | 4712 | 425 |
| Span_SliceCopy8 | 10,000 | — | — | — | — | — | — | — | — | 426.6μs | 112 |
| StackAlloc_Fill128 | 100 | skip | — | skip | — | skip | — | skip | — | 3404 | 0 |
| StackAlloc_Fill128 | 10,000 | — | — | — | — | — | — | — | — | 282.6μs | 0 |
## 11. 瓶颈分析与优化建议

### 各类别最慢项 Top 3（最大规模，ns/调用）

- Async/TaskRun_Offload_Wait (size=10,000): 55.53ms 总耗时 ≈ 5553/元素
- Async/TaskRun_Offload_Wait (size=10,000): 13.87ms 总耗时 ≈ 1387/元素
- Async/WhenAll_64 (size=10,000): 13.07ms 总耗时 ≈ 1307/元素
- Collections/List_Contains_Miss (size=10,000): 147.57ms 总耗时 ≈ 14.8μs/元素
- Collections/List_Contains_Miss (size=10,000): 107.50ms 总耗时 ≈ 10.8μs/元素
- Collections/List_Contains_Miss (size=10,000): 20.99ms 总耗时 ≈ 2099/元素
- Delegates/Event_AddRemove (size=1,000,000): 35.06ms 总耗时 ≈ 35/元素
- Delegates/Multicast8_Invoke (size=1,000,000): 32.42ms 总耗时 ≈ 32/元素
- Delegates/Event_AddRemove (size=1,000,000): 30.11ms 总耗时 ≈ 30/元素
- GcAlloc/GC_Collect_Gen0 (size=10,000): 551.97ms 总耗时 ≈ 55.2μs/元素
- GcAlloc/GC_Collect_Gen0 (size=10,000): 457.80ms 总耗时 ≈ 45.8μs/元素
- GcAlloc/GC_Collect_Gen0 (size=10,000): 356.63ms 总耗时 ≈ 35.7μs/元素
- Linq/OrderBy_Take10 (size=1,000,000): 89.18ms 总耗时 ≈ 89/元素
- Linq/OrderBy_First (size=1,000,000): 86.31ms 总耗时 ≈ 86/元素
- Linq/GroupBy_Count (size=1,000,000): 22.77ms 总耗时 ≈ 23/元素
- Primitives/Enum_Parse (size=1,000,000): 255.50ms 总耗时 ≈ 255/元素
- Primitives/String_Concat_Interp (size=1,000,000): 203.98ms 总耗时 ≈ 204/元素
- Primitives/String_Concat_Interp (size=1,000,000): 105.09ms 总耗时 ≈ 105/元素
- Reflection/Expression_Compile (size=10,000): 30.78s 总耗时 ≈ 3.08ms/元素
- Reflection/Expression_Compile (size=10,000): 618.95ms 总耗时 ≈ 61.9μs/元素
- Reflection/Expression_Compile (size=10,000): 461.10ms 总耗时 ≈ 46.1μs/元素
- Scenarios/Json_ReflectionSerialize (size=1,000,000): 1.01s 总耗时 ≈ 1011/元素
- Scenarios/Json_ReflectionSerialize (size=1,000,000): 736.44ms 总耗时 ≈ 736/元素
- Scenarios/Json_ManualSerialize (size=1,000,000): 649.31ms 总耗时 ≈ 649/元素
- SpanMemory/CharSpan_Copy128 (size=10,000): 585.8μs 总耗时 ≈ 59/元素
- SpanMemory/ArrayPool_RentReturn1k (size=10,000): 544.8μs 总耗时 ≈ 54/元素
- SpanMemory/Span_Reverse64 (size=10,000): 478.7μs 总耗时 ≈ 48/元素

### 分配大户 Top 10（B/调用，size=10,000）

- Scenarios/Json_ReflectionSerialize: 1.5MB (162.3 B/元素)
- Scenarios/Json_ReflectionSerialize: 1.5MB (152.8 B/元素)
- Scenarios/Json_ReflectionSerialize: 1.4MB (146.1 B/元素)
- Scenarios/Json_ReflectionSerialize: 1.4MB (144.9 B/元素)
- Scenarios/Json_ReflectionSerialize: 1.4MB (142.4 B/元素)
- Reflection/Expression_Compile: 1.2MB (124.8 B/元素)
- Scenarios/Json_ManualSerialize: 1.1MB (110.6 B/元素)
- Scenarios/Json_ManualSerialize: 1.1MB (110.6 B/元素)
- Scenarios/Json_ManualSerialize: 1.1MB (110.6 B/元素)
- Scenarios/Json_ManualSerialize: 977.9KB (100.1 B/元素)

### 版本演进: net7.0 / net48 加速比（中位数，越大越好；<1 为回归）

| Benchmark | Size | net48 ns | net7.0 ns | Speedup |
|---|---|---|---|---|
| Linq/Select_Project_Last | 1,000,000 | 5.34ms | 100 | 53359.50× |
| Linq/Select_Project_Last | 10,000 | 51.3μs | 100 | 513.00× |
| Collections/List_Contains_Miss | 10,000 | 145.00ms | 9.27ms | 15.65× |
| Collections/List_Contains_Miss | 100 | 14.7μs | 1000 | 14.70× |
| Linq/OrderBy_First | 1,000,000 | 86.41ms | 9.67ms | 8.94× |
| Primitives/String_Concat_Builder | 10,000 | 1.08ms | 136.2μs | 7.90× |
| Linq/Select_Project_Last | 100 | 700 | 100 | 7.00× |
| Linq/OrderBy_Take10 | 1,000,000 | 91.55ms | 13.34ms | 6.86× |
| Reflection/GetCustomAttribute | 10,000 | 7.91ms | 1.28ms | 6.17× |
| Linq/OrderBy_First | 10,000 | 558.2μs | 94.4μs | 5.91× |
| Reflection/GetCustomAttribute | 100 | 77.0μs | 13.6μs | 5.66× |
| Delegates/Interface_vs_DirectCall | 1,000,000 | 2.47ms | 453.4μs | 5.44× |

**回归项 (net7.0 反而慢 20%+):** GcAlloc/Struct_CopyAssign_Baseline (0.14×)、GcAlloc/Struct_CopyAssign_Baseline (0.17×)、Scenarios/PrimeSieve (0.20×)、GcAlloc/Boxing_Plus_Equals (0.25×)、GcAlloc/GC_Collect_Gen0 (0.26×)、GcAlloc/GC_Collect_Gen0 (0.26×)、Delegates/ClosureCapture_Invoke (0.28×)、Delegates/NewDelegate_Creation (0.28×)

### 优化建议（基于数据的规则库）

- LINQ 链 (Where+Select+Sum) 比手写循环慢 14.1× — 热路径建议手写 for 循环
- Int 装箱每次产生 24 B 分配 — 热路径改泛型集合/避免 object 传值
- 捕获闭包 lambda 每次调用分配 10 B — 高频回调把捕获变量提为字段或用 static lambda
- List 动态扩容比预分配慢 2.1× — 已知规模时传 capacity
- TaskCompletionSource 往返 ≈ 33 ns/次 — 高频同步完成路径考虑 ValueTask / 直接回调（本项目 Godot 基准实测 TCS roundtrip 达 25μs+）
- Mono-in-Godot 列存在跳过的类目: Async — 属运行时 API 兼容性差异（如缺 System.Memory），已如实标注而非报错

### 冷启动首调成本（新进程，size=100，含 JIT/静态构造，ms）

| Benchmark | Mono-in-Godot | net48 | net6.0 | net7.0 | netcoreapp3.1 |
|---|---|---|---|---|---|
| Reflection/Activator_CreateInstance | 46.523 | 0.253 | 0.350 | 0.190 | 0.416 |
| Reflection/Expression_Compile | 45.190 | 8.223 | 24.126 | 22.315 | 20.210 |
| Reflection/GetCustomAttribute | 31.284 | 0.203 | 0.307 | 0.101 | 0.239 |
| Async/TaskRun_Offload_Wait | 10.367 | 1.524 | 0.899 | 1.132 | 0.665 |
| Scenarios/SortAggregate_Pipeline | 8.868 | 2.856 | 2.552 | 7.272 | 3.426 |
| SpanMemory/Span_SliceCopy8 | 8.286 | 0.000 | 0.000 | 0.000 | 0.000 |
| GcAlloc/GC_Collect_Gen0 | 7.064 | 1.850 | 4.531 | 3.831 | 2.256 |
| SpanMemory/ArrayPool_RentReturn1k | 6.935 | 0.000 | 0.000 | 0.000 | 0.000 |
| SpanMemory/Span_SequenceEqual256 | 5.448 | 0.000 | 0.000 | 0.000 | 0.000 |
| Scenarios/Csv_Parse | 5.254 | 1.118 | 1.161 | 0.432 | 1.307 |
| Primitives/DateTime_Now | 5.144 | 0.145 | 0.370 | 0.319 | 0.372 |
| Scenarios/Json_ManualSerialize | 4.905 | 1.684 | 1.537 | 0.929 | 1.765 |
| Reflection/Property_GetValue | 3.660 | 0.144 | 0.279 | 0.224 | 0.152 |
| SpanMemory/CharSpan_Copy128 | 3.411 | 0.000 | 0.000 | 0.000 | 0.000 |
| Primitives/Enum_Parse | 3.014 | 0.322 | 0.272 | 0.179 | 0.376 |
| Primitives/Double_Parse | 2.947 | 0.135 | 0.289 | 0.137 | 0.304 |
| Async/Await_CompletedTask | 2.889 | 1.121 | 1.503 | 0.619 | 2.274 |
| Async/ProducerConsumer_Queue | 2.705 | 6.410 | 5.840 | 3.823 | 4.887 |
| Primitives/String_Concat_Interp | 2.423 | 0.288 | 0.211 | 0.080 | 0.238 |
| Linq/Where_Filter_Count | 2.381 | 2.683 | 3.727 | 11.664 | 2.597 |
| Reflection/Method_Invoke_Instance | 1.918 | 0.222 | 0.211 | 1.388 | 0.219 |
| Scenarios/Json_ReflectionSerialize | 1.877 | 1.402 | 1.233 | 0.511 | 0.971 |
| Linq/OrderBy_First | 1.842 | 3.135 | 1.294 | 1.351 | 1.602 |
| Linq/GroupBy_Count | 1.665 | 4.036 | 2.633 | 2.242 | 3.000 |
| Async/WhenAll_8 | 1.580 | 1.130 | 1.206 | 0.540 | 0.999 |

> 注: 冷启动在同进程内逐项首调，共享 JIT 预热会带来向下的偏差，用于量级对比而非精确值。
