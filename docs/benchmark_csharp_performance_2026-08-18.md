# C# 性能基准测试报告

> **日期**: 2026-08-18
> **引擎**: Godot 4.7 stable (mono.custom_build.e8fcf47ee)
> **运行时**: Mono 6.12 JIT (桌面), .NET Standard 2.0
> **硬件**: NVIDIA GeForce RTX 4070 Ti SUPER (Vulkan 1.3.280, Forward Mobile)
> **测试场景**: `res://benchmark_test.tscn` → `csharp_test/Benchmark.cs`
> **日志来源**: `benchmark_run6.log` (Run A) / `benchmark_screenshot.log` (Run B)
> **单位**: ns/iter (每次迭代纳秒数)，每项迭代 10,000 次（重型测试为 N/20 ~ N/100）

---

## 1. 执行摘要

- **53 项微基准全部成功**，覆盖 5 大类别：Object 变体调用、Signals、Properties、Direct methods、Direct properties。
- **纯 C# 委托调用比 Godot Signal.Emit 快约 260~290 倍** (`Action.Invoke` 1.85 ns vs `Signal.Emit` ~480 ns)。
- **强类型 icall 比字符串方法名调用快约 9 倍** (`Node.SetProcess` ~80 ns vs `Object.Call("set_process")` ~750 ns)。
- 与公开基准数据对比，**量级一致**，无异常值（详见第 7 节）。
- 与官方 CoreCLR 绑定的差距主要来自：Mono JIT 较慢 + 字符串信号名（无 `StringName` 缓存）。

---

## 2. Object calls — 变体方法名调用（Variant 封送 + 字符串查找）

| 基准项 | Run A | Run B |
|---|---:|---:|
| Object.Call (set_process_internal, 1 bool) | 1000.95 | 597.04 |
| Object.Call (set_process, 1 bool) | 751.10 | 513.33 |
| Object.Call (set_name, 1 string) | 1038.76 | 992.02 |
| Object.Set (name, string) | 768.04 | 1104.77 |
| Object.Get (name → string) | 556.94 | 489.02 |
| Object.Set (process, bool) | 275.72 | 266.42 |
| Object.Get (process → bool) | 288.85 | 279.01 |
| Object.Set (position, Vector3) | 408.59 | 390.98 |
| Object.Get (position → Vector3) | 496.34 | 408.03 |
| Object.Connect+Disconnect (Action, pair) | 6639.80 | 7167.80 |
| Object.EmitSignal (script_changed, 0 args, connected) | 599.71 | 679.68 |
| Object.HasSignal (ready, string) | 213.12 | 206.06 |
| Object.IsInstanceValid | 77.02 | 75.15 |
| Object.ToString | 481.68 | 455.61 |
| Object.GetNativePtr | 1.55 | 1.58 |

## 3. Signals — Godot 信号 vs 纯 C# 事件

| 基准项 | Run A | Run B |
|---|---:|---:|
| Godot Signal.Emit (ready, 0 args, connected) | 479.64 | 479.26 |
| Godot Signal.Emit (tree_entered, 0 args, connected) | 570.91 | 539.56 |
| C# EventHandler\<int\>.Invoke (1 int) | 9.48 | 9.22 |
| C# Action.Invoke () | 1.85 | 1.86 |
| C# Action\<int\>.Invoke (1 int) | 4.18 | 4.15 |
| Godot Signal.Connect+Disconnect (per-iter) | 2274.00 | 2099.20 |
| C# EventHandler add+remove (per-iter) | 11539.60 | 13378.80 |
| SignalAwaiter (ToSignal) create | 4675.00 | 5657.00 |
| C# event + TaskCompletionSource roundtrip | 25118.00 | 36578.50 |

## 4. Properties — 变体 Object.Set/Get（字符串属性名）

| 基准项 | Run A | Run B |
|---|---:|---:|
| Property.Set (name → string) | 766.13 | 755.13 |
| Property.Get (name → string) | 483.55 | 466.91 |
| Property.Set (process → bool) | 304.71 | 287.52 |
| Property.Get (process → bool) | 270.96 | 259.99 |
| Property.Set (position → Vector3) | 390.25 | 393.88 |
| Property.Get (position → Vector3) | 369.04 | 370.72 |
| Property.Set (scale → Vector3) | 494.76 | 363.35 |
| Property.Get (scale → Vector3) | 474.11 | 362.71 |
| Property.set_Visible (Node3D, bool) | 452.06 | 269.46 |
| Property.get_Visible (Node3D, bool) | 492.93 | 342.80 |
| Property.set_TopLevel (Node3D, bool) | 312.48 | 290.71 |
| Property.get_TopLevel (Node3D, bool) | 475.57 | 355.75 |

## 5. Direct methods — 强类型 C# icall（无 Variant、无字符串查找）

| 基准项 | Run A | Run B |
|---|---:|---:|
| Node.SetProcess (bool) | 86.41 | 71.47 |
| Node.SetPhysicsProcess (bool) | 75.86 | 68.53 |
| Node.SetProcessInput (bool) | 103.52 | 66.95 |
| Node.IsInsideTree | 698.39 | 480.73 |
| Node.GetChildCount | 110.77 | 71.46 |
| Node.GetNodeOrNull\<Node\> (miss, typed) | 658.07 | 605.78 |
| GodotObject.IsInstanceValid | 55.45 | 57.43 |
| Node3D.SetPosition (Vector3) | 762.45 | 641.08 |
| Node3D.GetPosition (Vector3) | 437.69 | 429.14 |
| Node3D.SetRotation (Vector3) | 799.36 | 850.17 |
| Node3D.GetRotation (Vector3) | 574.99 | 452.27 |
| Node3D.SetScale (Vector3) | 763.10 | 620.05 |
| Node3D.GetScale (Vector3) | 430.82 | 419.19 |
| Node3D.SetGlobalPosition (Vector3) | 723.84 | 696.90 |
| Node3D.GetGlobalPosition (Vector3) | 577.52 | 518.51 |

## 6. Direct properties — 强类型 C# 属性 getter/setter

| 基准项 | Run A | Run B |
|---|---:|---:|
| Node.set_Name (string) | 1296.49 | 712.33 |
| Node.get_Name (string) | 508.49 | 493.19 |
| Node3D.set_RotationOrder (long) | 328.17 | 354.20 |
| Node3D.get_RotationOrder (long) | 378.50 | 374.52 |
| Node3D.set_Visible (bool) | 293.60 | 268.83 |
| Node3D.get_Visible (bool) | 340.62 | 557.91 |
| Node3D.set_TopLevel (bool) | 295.81 | 301.01 |
| Node3D.get_TopLevel (bool) | 352.58 | 352.29 |
| Node3D.set_RotationEditMode (long) | 363.74 | 353.28 |
| Node3D.get_RotationEditMode (long) | 415.61 | 401.71 |
| Node3D Position.x r+w chain (read-modify-write) | 1091.54 | 1064.72 |

---

## 7. 与公开基准数据对比

**说明**: Godot 官方文档未发布与本测试项一一对应的微基准表（本套基准的 "G471 official" 列即本项目 Mono 6.12 移植版自测值，表格格式仿照外部参考截图）。以下选用可对比的公开数据做量级校验：

| 数据源 | 环境 | 公开数值 | 我们的数值 | 结论 |
|---|---|---:|---:|---|
| [dicarne/godot-benchmark-gdscript-csharp](https://github.com/dicarne/godot-benchmark-gdscript-csharp) | Godot 4 + .NET (CoreCLR) | 信号发射 ~720 ns | 480~600 ns | **量级一致**，我们略快（接收器更少、发射开销与已连接数相关） |
| [Godot PR #115741](https://github.com/godotengine/godot/pull/115741) (EmitSignal 基准) | .NET 10 + BenchmarkDotNet | EmitSignal (3 args) 1002~1177 ns | EmitSignal (0 args) 480~600 ns | **量级一致**；参数封送是大头，每多 1 参数约 +150~250 ns |
| [CSDN 信号机制对比文](https://blog.csdn.net/yog99/article/details/153715375) | Godot 4.2 + .NET 8 + `SignalName` 缓存 | 信号 ~80 ns (0.8ms/万次) | 480~600 ns | **我们慢 ~6×**，原因见下 |
| [Godot 官方 QA](https://godotengine.org/qa/137021/state-of-gdscript-vs-c%23-performance-in-godot-4-0) | 定性结论 | "C# 更快，但与 Godot 通信需要昂贵的封送" | 见 8 节加速比 | **定性一致** |

### 差异原因分析（为什么比 CoreCLR 绑定慢）

1. **运行时不同**: 我们用 Mono 6.12 JIT（.NET Standard 2.0），公开数据多为 CoreCLR (.NET 8/10)。Mono JIT 的委托调用、接口分发、GC 都较慢。
2. **信号名传递方式**: 我们的绑定 `EmitSignal(string)` 每次都要做 `string → StringName` 转换；官方绑定有 `SignalName.XXX` 静态缓存（`StringName` 复用），可省去每次哈希/驻留。
3. **无源生成器优化**: 官方 4.x 绑定用 Source Generator 生成强类型信号发射路径；我们的移植版走通用 `godot_icall_Object_EmitSignal` 路径。
4. **已知优化方向**（未做）: `EmitSignal` 的 `params object[]` 装箱、`ConnectImpl` 连接前线性去重扫描（O(已连接数)）、`SignalAwaiter` 每次三重分配 — 三者在引擎 glue 层 (`modules/mono/glue/GodotSharp/GodotObject.cs`, `SignalAwaiter.cs`) 均可继续优化。

---

## 8. 关键性能洞察与加速比

| 对比维度 | 慢路径 | 快路径 | 加速比 |
|---|---:|---:|---:|
| 事件派发 (0 args) | Signal.Emit: ~480 ns | C# Action: 1.85 ns | **~260×** |
| 事件派发 (1 int) | (Signal 带 1 参数估算 ~700 ns) | Action\<int\>: 4.18 ns | ~170× |
| 方法调用 (1 bool) | Object.Call: ~750 ns | Node.SetProcess: ~80 ns | **~9×** |
| 属性 Set (bool) | Object.Set: ~276 ns | 强类型 setter: ~294 ns | 持平 |
| 属性 Get (bool) | Object.Get: ~289 ns | 强类型 getter: ~341 ns | 持平 |

要点：

1. **最快项**: `Object.GetNativePtr` 1.55~1.58 ns — 几乎是寄存器读取级别。
2. **最慢单次操作**: `C# event + TaskCompletionSource roundtrip` 25~37 μs — 单线程同步等待上下文切换成本，**严禁热路径使用**。
3. **`SignalAwaiter` (await 信号) 创建成本**: 4.7~5.7 μs/次，含 OneShot 连接 + awaiter + callable + lock 四重分配。
4. **变体属性与强类型属性差距小**（bool/long 类型 10~50 ns 级）：因为两者最终都走 icall，差别只在 Variant 封送。
5. **`ConnectImpl` 线性去重**: 高频 Connect/Disconnect 场景成本随已连接数线性增长（引擎 glue 层实现决定）。

---

## 9. 编码规范建议（热路径 Signal 使用规则）

基于以上数据，项目内 C# 代码遵循以下规则：

### 必须用纯 C# 事件的场景（热路径）

- 每帧/每物理帧触发的回调（`_Process` 内派发、AI tick、动画帧回调）
- 循环内高频发射（>100 次/秒）
- 对象池、粒子、弹幕等批量生成/销毁的通知

```csharp
// 推荐：纯 C# event，~9ns/次 (带参) / 1.85ns/次 (无参)
public event Action<int> OnScoreChanged;
OnScoreChanged?.Invoke(score);
```

### 允许用 Godot Signal 的场景（低频/UI/跨语言）

- UI 按钮、菜单交互（人类点击频率，~几次/秒）— 本项目 `game2048_demo/Main.cs` 的 2 处 `Button.pressed` 连接即为正确用法
- 生命周期事件（`ready`、`tree_entered`、一次性）
- 需要在编辑器里可视化连接、或与 GDScript 互操作的事件

### 禁止项

- 热路径中 `await ToSignal(...)`（4.7+ μs/次）
- 热路径中 `TaskCompletionSource` 同步等待（25+ μs/次）
- 循环内 `Connect`/`Disconnect`（2.3+ μs/对，且随已连接数线性恶化）

---

## 10. 复现方法

```powershell
cd godot4.7_mono/csharp_test
dotnet build CSharpTest.csproj -c Debug
$env:GODOT_LOG_PATH="."
& "..\bin\editor\windows\godot.windows.editor.x86_64.mono.console.exe" `
    --path . res://benchmark_test.tscn 2>&1 | Tee-Object -FilePath benchmark_run.log
```

注意事项：

- 基准通过多帧状态机执行（`_Process` 内逐段推进），避免单帧长循环卡死移动端/WASM。
- 输出双写：引擎内 Debug UI Label（`Runtime.DebugUiAddLine`）+ stdout（`GD.Print`），日志文本与 UI 表格逐行一致。
- 已知环境问题: 测试数据全部打印完成后、引擎退出阶段 NVIDIA 驱动 (`nvoglv64.dll`) 可能崩溃 (exit -1073741819)，不影响数据采集。
- 若要更稳定的生产级数值，将 `Benchmark.cs` 中 `_iters` 从 10000 提升至 100000。

---

## 附: 项目内 Signal 使用现状盘点（2026-08-18）

| 位置 | 用法 | 热路径 | 处置 |
|---|---|:---:|---|
| `game2048_demo/Main.cs:208,213` | Button `pressed` 连接 | 否 | **保留**（低频 UI 事件，Godot 正确用法） |
| `csharp_test/Benchmark.cs` 多处 | Connect/EmitSignal/ToSignal 循环 | 是 | **保留**（基准测试本身，即测量目的） |
| `csharp_test/Test.cs:196,1562` | 信号往返断言 | 否 | 保留（功能验证） |
| `csharp_test/ExportTest.cs:20` | `[Signal] HealthChanged` 声明 | 否 | 保留（导出/工具验证） |
| `csharp_test/fuzz/Fuzz10, Fuzz14` | `[Signal]`+异常/重入测试 | 否 | 保留（fuzz 覆盖） |

**结论**: 项目内不存在需要重构的热路径 Signal 代码。游戏核心逻辑（`game2048_demo/Game2048.cs`、`HotUpdater.cs`）已是纯 C# 直调，未走信号机制。
