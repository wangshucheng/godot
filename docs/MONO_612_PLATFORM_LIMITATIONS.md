# Godot 4.7 + Mono 6.12 平台限制与功能禁用清单

> 生效范围：本仓库 `4.7-mono` 分支（Mono 6.12.0.200 + Godot 4.7 移植）。
> 代码层 Guard API 命名空间：`Godot.Platform.*`。
> 每条限制在代码层都有显式禁用检查（`NotSupportedException` / `PlatformNotSupportedException`），
> 异常消息附带本相对路径，保证误用即报错，而不是静默死锁 / 崩溃。

---

## 总览

| # | 平台 | 禁用功能 | 触发代码层检查 | 若绕过检查的后果 |
|---|---|---|---|---|
| 1 | Windows / Linux / macOS 桌面 | **同步等待异步任务**：`Task.Wait()`、`Task<T>.Result`、`task.GetAwaiter().GetResult()`、`obj.ToSignal().GetAwaiter().GetResult()`（在 Godot 主线程且 `GodotSynchronizationContext` 已安装时） | `GodotSynchronizationContext.Wait(...)` 重写 / `SignalAwaiter.GetResult()` | **主线程死锁** — continuation 需要泵送消息队列才会执行，但调用方已阻塞。 |
| 2 | Web / WASM（包括微信小游戏） | **C# async/await 及任何 `async Task` / `async void` 状态机** — 含 BCL 基准的 Await_TaskYield、Await_Delay、Await_WhenAll、Await_MutexSemaphore | 入口推荐调用 `Platform.ThrowIfAsyncForbiddenOnWeb()` | **sgen GC 断言 abort** — async 状态机对象毒化托管堆，首次强制 GC 在 `sgen-scan-object.h:91` crash，进程不可恢复。 |
| 3 | Web / WASM（包括微信小游戏） | **`System.Linq.Expressions.Expression.Compile()` 及 IL 动态生成** — `ILGenerator.Emit*`、`DynamicMethod`、运行时代码编译 | 入口推荐调用 `Platform.ThrowIfExpressionCompileForbiddenOnWeb()` | **`PlatformNotSupportedException` / icall signature mismatch** — Mono WASM 解释器未实现 IL 发射运行时，BCL 直接抛或在内部调用时挂死。 |

---

## #1 桌面：禁止同步等待异步任务（SyncContext 死锁）

### 触发条件
- Godot 主线程（即已安装 `GodotSynchronizationContext` 的线程）上
- 对一个尚未完成的 `Task` 或 `SignalAwaiter` 调用了阻塞等待 API：
  ```csharp
  var task = SomeAsyncWork();
  task.Wait();                         // ❌
  var r = LongTask().Result;           // ❌
  var sig = obj.ToSignal(timer, "timeout").GetAwaiter().GetResult();  // ❌
  ```

### 根因
`GodotSynchronizationContext` 的延续（continuation）**以 `Post` 形式排入 `_pending` 队列，只有 Godot 主线程在每帧 `Pump()` 时才会被调度执行**。同步等待会阻塞主线程本身，从而永远无法泵送 → 任务永远不能完成 → 死锁。

基准验证见：`report_wasm_20260820.md` 第 6 节 Async 死锁分析（24/24 warm 中 Await_TaskYield 曾 2 处 skip，现已确认是 SyncContext 同步等待所致，并加代码层禁用）。

### 代码层禁用检查（自动生效，无需用户调用）
1. `GodotSynchronizationContext.Wait(IntPtr[] waitHandles, bool waitAll, int millisecondsTimeout)` 被重写，**任何通过 `WaitHandle.WaitAll/WaitAny` 或 `Task.Wait()` 走到底层 `SynchronizationContext.Wait` 的同步等待都会抛**：
   ```
   System.NotSupportedException: Synchronous blocking waits (Task.Wait / .Result /
   GetAwaiter().GetResult()) are FORBIDDEN on the Godot main thread when
   GodotSynchronizationContext is installed. Use 'await' instead.
   See: docs/MONO_612_PLATFORM_LIMITATIONS.md #1
   ```
2. `SignalAwaiter.GetResult()` 在 `!IsCompleted` 时直接抛相同异常（用户写 `GetAwaiter().GetResult()` 未完成的信号）。

### 替代方案（✅ 正确写法）

```csharp
// ✅ 全部改用 async + await
public override async void _Ready() {
    await Task.Delay(1000);
    var r = await LongTask();
    var args = await obj.ToSignal(timer, "timeout");
    GD.Print("done");
}

// ✅ 如果必须在同步回调里"等一下"，改用 Godot 信号 + 帧轮询，不用 Task
private double _deadline;
public override void _Process(double dt) {
    if (_deadline == 0) _deadline = Time.GetTicksMsec() + 1000;
    if (Time.GetTicksMsec() >= _deadline) {
        DoTheThing();
        SetProcess(false);
    }
}
```

---

## #2 WASM：禁止 async/await（sgen GC 崩溃）

### 触发条件
- 目标平台：Web / WASM / 微信小游戏（`OSPlatform.Web`，`RuntimeFlags.WebPlatform | SingleThreaded`）
- 用户代码中任何 `async` 方法（`async void _Ready()`、`async Task<X> Load()` 等），即使是纯 CPU 计算无 IO 的 `await Task.Yield()`。

### 根因
Mono 6.12 WASM 解释器 + sgen GC 的 ABI 缺陷：**async/await 编译器生成的显示类（display class）状态机对象在逃逸分析和栈/堆转换中会在 sgen 灰色对象扫描链上写入非托管指针**。首次强制 `GC.Collect()` 扫描该对象时，`sgen-scan-object.h` line 91 断言 `!QUEUE_HAS_NEXT(...)` 失败并 `abort()`，浏览器 tab 进程直接崩。

基准数据：WASM 全部 Async 类目 24/24 结构化 skip，任何 1 条放行都会在"强制 GC 阶段"（见 `CSharpBenchRunner.cs` 的 Warmup 后 GC.Collect）被触发崩溃。

### 代码层禁用检查（用户在 async 入口第一行显式调用）
```csharp
// 在你任何可能跑 WASM 的 async 方法顶部加：
public override async void _Ready() {
    Platform.ThrowIfAsyncForbiddenOnWeb();   // ← WASM 命中即抛，桌面无副作用
    // ... 其余桌面逻辑
}
```
抛出的异常：
```
System.PlatformNotSupportedException: C# async/await is FORBIDDEN on
Mono 6.12 WASM (Web / WeChat minigame). Async state machine objects poison
the sgen GC heap and trigger an unrecoverable abort at the next GC.Collect.
Rewrite as synchronous code + frame-based polling.
See: docs/MONO_612_PLATFORM_LIMITATIONS.md #2
```

⚠️ **注意**：代码层无法 100% 自动拦截（async 状态机是编译器生成的），**必须由你自己在 async 方法开头调用 `ThrowIfAsyncForbiddenOnWeb()`**。推荐在项目的 `Main.cs / GameRoot._Ready()` 调用一次以作全局 fail-fast 提醒。

### 替代方案（✅ WASM 下正确写法）

```csharp
// ❌ 原写法：async/await
public override async void _Ready() {
    await Task.Delay(1000);
    SpawnEnemy();
}

// ✅ 替代：帧轮询 + 计数器 / Godot 定时器
private double _spawnAt = 0;
public override void _Ready() {
    _spawnAt = Time.GetTicksMsec() + 1000;
}
public override void _Process(double dt) {
    if (_spawnAt > 0 && Time.GetTicksMsec() >= _spawnAt) {
        SpawnEnemy();
        _spawnAt = 0;
    }
}

// ✅ 或：用 Godot 的 GetTree().CreateTimer + ToSignal（SignalAwaiter 同步版？不行，也别同步等）
//    → 直接用 C++ Callable + 信号回调
public override void _Ready() {
    var timer = GetTree().CreateTimer(1.0);
    timer.Timeout += () => { SpawnEnemy(); };
}
```

---

## #3 WASM：禁止 System.Linq.Expressions 动态编译（Expression.Compile / ILGenerator.Emit）

### 触发条件
- 目标平台：Web / WASM / 微信小游戏
- 调用 `Expression<Func<int>> e = () => 42; e.Compile();`
- 或任何 `System.Reflection.Emit.*`、`DynamicMethod`、`ILGenerator.Emit(OpCodes.*)`。

### 根因
Mono 6.12 WASM 解释器**未实现运行时 IL 发射（runtime IL emit）的解释执行后端**。`Expression.Compile()` 内部走 `System.Reflection.Emit.DynamicMethod` → `ILGenerator`，在 WASM 上要么当场抛 `PlatformNotSupportedException`，要么在 `mono_compile_method` 阶段 signature mismatch 挂死。

基准数据：`Linq_Expression_Compile`、`Reflection_Emit_Sqrt` 在 WASM 端被结构化 skip。

### 代码层禁用检查（用户在 Compile 前显式调用）
```csharp
Platform.ThrowIfExpressionCompileForbiddenOnWeb();   // WASM 命中即抛，桌面无副作用
var compiled = expr.Compile();
```
抛出的异常：
```
System.PlatformNotSupportedException:
System.Linq.Expressions.Expression.Compile() / System.Reflection.Emit is
FORBIDDEN on Mono 6.12 WASM. The interpreter has no IL emit backend. Use
pre-compiled delegates, Func<T> fields, or source generators instead.
See: docs/MONO_612_PLATFORM_LIMITATIONS.md #3
```

### 替代方案（✅ WASM 下正确写法）

| ❌ 原写法 | ✅ 替代 |
|---|---|
| `expr.Compile()()` | 把委托写成 `static readonly Func<int> _f = () => 42;` 存静态字段，运行时直接调用。 |
| `DynamicMethod` 运行时生成包装器 | 用 Roslyn Source Generator 在编译期生成包装（已在 `eval_2026-07-26_v3_sourcegenerators.md` 评估通过）。 |
| 数据驱动的 IL 字节码生成器 | 改用解释器模式：查表 + `switch` dispatch，不生成新方法。 |

---

## 推荐：项目入口全局 Fail-Fast 模板

在你的主场景根节点 `_Ready()` 加一次全局检查，WASM 用户开页面**第一秒**就能看到异常，而不是跑到某个深层 async 方法才崩：

```csharp
using Godot;

public partial class GameRoot : Node {
    public override void _Ready() {
        // —— 全局平台限制 fail-fast（WASM 下立即抛异常并提示，桌面无副作用）——
        Platform.ThrowIfAsyncForbiddenOnWeb();
        Platform.ThrowIfExpressionCompileForbiddenOnWeb();
        // —— 其余启动代码 ——
        GD.Print("platform checks passed");
    }
}
```

桌面端的限制（#1 同步等待）已在 `GodotSynchronizationContext.Wait()` 重写里自动生效，不需要额外入口调用。
