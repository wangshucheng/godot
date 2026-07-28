# Spike: 约束 #14 消除资格评估

- **日期**：2026-07-29
- **前置**：Phase 0.1 delegate probe WASM 验证完成 + Phase 0.2 CSharpNotifyDispatch 桌面回归 PASS
- **目标**：基于 Phase 0.1/0.2 验证结论，评估约束 #14（C# 初始化逻辑必须从 _Ready 延迟到 _Process 首帧）是否可以正式消除

---

## 一、约束 #14 的历史根因

### 1.1 约束声明（原 project_memory）

> C# initialization logic must be moved from _Ready to _Process first frame execution to avoid mono_runtime_invoke issues

### 1.2 历史代码证据（Test.cs:21-25）

```csharp
public override void _Ready()
{
    // Empty: avoids Mono WASM _Ready signature mismatch.
    // All init happens in _Process state 0.
}
```

`_Ready()` 故意留空，所有初始化延迟到 `_Process` 状态 0 完成。注释暗示 `_Ready` 阶段调用 icall 会触发"WASM signature mismatch"。

### 1.3 历史假设（已被 Phase 0.1 推翻）

旧假设：`mono_runtime_invoke` 在 WASM `_Ready` 阶段不可用（与 GC bridge 或 sync context 未初始化相关）

---

## 二、Phase 0.1/0.2 验证事实

### 2.1 Phase 0.1 DelegateProbe 实测（WASM pure interpreter, 2026-07-29）

DelegateProbe 在 `_Ready()` 中执行了三个 icall 调用：

```csharp
public override void _Ready()
{
    GD.Print("[PROBE] START DelegateProbe");
    _delegate = new Action(OnDelegateInvoked);
    _delegate();  // Test 1: C# direct — PASS

    Runtime.TestRegisterDelegateProbe(_delegate);  // icall #1 — PASS
    int invokeResult = Runtime.TestInvokeDelegateViaMRI();  // icall #2 — PASS
    int fptrResult = Runtime.TestInvokeDelegateViaFtnPtr();  // icall #3 — CRASH (expected)
}
```

**关键事实**：
- Test 1 (C# direct delegate): **PASS** — C# 侧 _Ready 阶段可执行任意代码
- Test 2 (C++ mono_runtime_invoke): **PASS** — C++ → C# delegate.Invoke 路径在 _Ready 阶段完全可用
- Test 3 (C++ mono_compile_method + ftn_ptr): CRASH — interpreter 限制，但 MRI 路径不需要 ftn_ptr

### 2.2 Phase 0.2 CSharpNotifyDispatch 实测（桌面 JIT, 2026-07-29）

新通知派发表走 `_Ready` 通知 ID (Node::NOTIFICATION_READY = 13) 路径：

```cpp
void CSharpInstance::notification(int p_notification, bool p_reversed) {
    const NotifySpec *spec = csharp_notify_find_spec(p_notification);
    if (spec) {
        NotifyEntry &entry = notify_dispatch_.get_entry(...);
        if (!entry.resolved) resolve_notify_entry(idx);
        if (entry.method) invoke_cached_notify(idx, p_notification);  // 走 MRI
    }
}
```

DelegateProbe 桌面日志（2026-07-29）显示 `_Ready` 通知直接调用 `_Ready()` 方法：
```
[Mono] notification(id=13) for 'DelegateProbe'
[Mono] notification: calling _Ready for 'DelegateProbe'
[PROBE] START DelegateProbe
[PROBE] Test1 C# direct invoke: PASS (count=1)
[PROBE] Test2 C++ mono_runtime_invoke(delegate.Invoke): PASS (count=1)
[PROBE] Test3 C++ function pointer invoke: PASS (count=1)
[PROBE] DONE DelegateProbe (pass=3/3)
```

`_Ready` 触发两次（Godot 通知流），两次都 PASS — 证明 _Ready 阶段调用 icall 完全可用。

---

## 三、根因再分析

### 3.1 约束 #14 的真实根因（推测）

Phase 0.1 已证明 `mono_runtime_invoke` 本身在 _Ready 阶段可用。约束 #14 的真实根因可能是以下之一：

| # | 假设 | 证据 | 状态 |
|---|------|------|------|
| A | GC bridge 在 _Ready 阶段未初始化，导致 mono_object 不可达 | DelegateProbe 创建了 `_delegate = new Action(OnDelegateInvoked)` 并 PASS，说明 GC 可跟踪对象 | **已排除** |
| B | GodotSynchronizationContext.Install() 在 _Ready 阶段破坏函数表 | Runtime.cs:18 显式跳过 Web 平台 Install() 调用；DelegateProbe 在 WASM 也 PASS | **已排除** |
| C | SceneTree 在 _Ready 阶段未完全建立，导致 GetTree() 等调用失败 | 未在 Phase 0.1 探针中测试 GetTree() | **未验证** |
| D | 历史 WASM 构建有 bug，导致 _Ready 阶段 icall 不稳定；后续 H9 修复后已自然消除 | H9 WASM fix (2026-07-26) + Phase 0.2 dispatch rewrite 后未重现 | **高可能性** |

### 3.2 假设 D 的支持证据

1. **H9 修复（2026-07-26）**：mono-threads-wasm.c EM_ASM_INT 读取 TOTAL_STACK → 改用 emscripten_stack_get_base/end，修复 WASM 栈大小计算
2. **H9 修复（2026-07-26）**：GodotSynchronizationContext.Install() 在 Web 平台跳过
3. **Phase 0.2（2026-07-29）**：notification() 路径重写，_Ready 走 notify_dispatch_ MRI 路径
4. **DelegateProbe WASM（2026-07-29）**：_Ready 阶段 3 个 icall 调用全部 PASS（Test 3 ftn_ptr 崩溃是预期）

综合推断：**约束 #14 是历史遗留假设，已被 H9 修复 + Phase 0.2 重构后自然消除**。

---

## 四、消除约束 #14 的方案

### 4.1 方案 A（推荐）：将 Test.cs 初始化逻辑前移到 _Ready

**改动范围**：仅 `csharp_test/Test.cs`，无引擎代码改动

```csharp
public override void _Ready()
{
    // 之前：Empty (constraint #14 workaround)
    // 现在：直接初始化（Phase 0.1 验证 _Ready 阶段 icall 可用）
    _frameCount = 1;
    _physicsCount = 0;
    _waitFrames = 0;
    _isWeb = Runtime.TestIsWebPlatform();
    Runtime.TestResetCounters();
    Runtime.DebugUiInit();
    Runtime.DebugUiClear();
    Runtime.DebugUiAddLine("=== Godot 4.7 C# Workflow Tests ===");
    // ... 其他初始化代码从 _Process state 0 前移
    _state = 1;  // 直接进入 state 1，跳过 state 0
}

public override void _Process(double delta)
{
    _frameCount++;
    // 移除 state 0，直接从 state 1 开始
    if (_state == 1) { ... }
}
```

**风险评估**：
- 低风险：仅测试代码改动，引擎代码不动
- 验证简单：跑 22 H9 场景 + 21 Fuzz 回归即可
- 若失败：可立即回滚（git revert）

### 4.2 方案 B（保守）：保持 Test.cs 不变，仅文档化约束已过时

**改动范围**：仅文档（project_memory.md + Test.cs 注释）

```csharp
public override void _Ready()
{
    // [Constraint #14 historically required this to be empty]
    // Phase 0.1 (2026-07-29) verified icalls work in _Ready on WASM.
    // Kept empty for backward compatibility; can be migrated to _Ready
    // when ready (see docs/spike_2026-07-29_constraint_14_eligibility.md).
}
```

---

## 五、决策

**建议采用方案 A**（Test.cs 初始化前移），但需要先完成 WASM interpreter 回归（H9 22 场景）确认 Phase 0.2 代码在 WASM 下无回归后再实施。

### 5.1 实施步骤（方案 A）

1. **前置条件**：WASM interpreter 重编完成 + H9 22 场景 PASS（进行中）
2. **代码改动**：Test.cs `_Ready()` 前移初始化逻辑，`_Process` state 0 移除
3. **桌面回归**：跑 22 H9 场景 + 21 Fuzz（确保无回归）
4. **WASM 回归**：跑 H9 22 场景（确保 WASM 也 PASS）
5. **更新约束声明**：project_memory.md 中约束 #14 标记为"已消除（2026-07-29）"
6. **commit**：单个 commit 包含 Test.cs + project_memory.md 改动

### 5.2 验收标准

- 桌面 + WASM 双平台 22 H9 场景全 PASS
- 桌面 + WASM 双平台 21 Fuzz 全 PASS
- DelegateProbe 桌面 + WASM 双平台 3/3 tests PASS（仅 Test 3 ftn_ptr 在 WASM 预期 CRASH）

---

## 六、相关文件

| 文件 | 角色 |
|------|------|
| [csharp_test/Test.cs](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/csharp_test/Test.cs) (L21-25) | 约束 #14 历史代码 |
| [csharp_test/DelegateProbe.cs](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/csharp_test/DelegateProbe.cs) | Phase 0.1 验证探针（_Ready 阶段调用 icall PASS） |
| [docs/spike_2026-07-28_phase0_delegate_probe.md](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/docs/spike_2026-07-28_phase0_delegate_probe.md) | Phase 0.1 验证报告 |
| [docs/spike_2026-07-28_phase0.2_notify_dispatch.md](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/docs/spike_2026-07-28_phase0.2_notify_dispatch.md) | Phase 0.2 架构设计 |

---

## 七、mono_host 初始化序列审计（2026-07-29 增补）

为彻底排除"约束 #14 消除后是否还需将 GC/sync-context init 前移到 mono_host"的疑虑，对 mono_host 初始化代码路径做了一次完整审计。

### 7.1 初始化时序证据

`MonoHost::initialize()` 在 `MODULE_INITIALIZATION_LEVEL_SERVERS` 阶段执行（`register_types.cpp:62-73`），远早于 SceneTree 创建（`main.cpp:4358`）和 `_Ready` 通知派发。

`MonoHost::initialize()` ([mono_host.cpp](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/modules/mono/mono_host.cpp#L446-L496)) 的关键步骤顺序：

| # | 行 | 步骤 |
|---|----|------|
| 1 | 100-442 | BCL/程序集路径、JIT/AOT/Interpreter 选择、`mono_jit_init_version` 创建 root domain |
| 2 | 453 | `mono_bridge::init(domain)` |
| 3 | **454** | **`mono_gc_bridge::init(domain)`** ← GC bridge 初始化 |
| 4 | 455 | `mono_variant::cache_mono_corlib_classes()` |
| 5 | 457 | `register_internal_calls()` → `godot_register_icalls()` ← icall 注册 |
| 6 | 465 | `load_corlib()` |
| 7 | 470 | `load_godotsharp()` ← GodotSharp.dll 加载 |
| 8 | **475-489** | **`mono_runtime_invoke(Runtime.Initialize)`** ← 托管侧初始化 |
| 9 | 491 | `cache_sync_context_method()` |
| 10 | 493 | `is_initialized = true` |

### 7.2 Runtime.Initialize() 调用链

`Runtime.Initialize()` ([Runtime.cs:9-21](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/modules/mono/glue/GodotSharp/Runtime.cs#L9-L21)) 调用 `Platform.Initialize()`，后者在非 Web 平台调用 `GodotSynchronizationContext.Install()`：

```
MonoHost::initialize() [mono_host.cpp:482]
  └─ mono_runtime_invoke(Runtime.Initialize)
       └─ Runtime.Initialize() [Runtime.cs:13]
            └─ Platform.Initialize() [Platform.cs:50]
                 └─ GodotSynchronizationContext.Install() [Platform.cs:65]  ← 非 Web 才调用
```

### 7.3 关键发现：原假设已不成立

| 原 Phase 0.1 假设 | 当前代码状态 | 结论 |
|-------------------|------------|------|
| GC bridge init timing 问题（SceneTree 未完全建立） | GC bridge init 已在 [mono_host.cpp:454](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/modules/mono/mono_host.cpp#L454)（SERVERS 级别，pre-SceneTree） | **已排除** |
| GodotSynchronizationContext.Install() 在 _Ready 阶段破坏函数表 | Install() 已通过 Runtime.Initialize() 间接在 [mono_host.cpp:482](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/modules/mono/mono_host.cpp#L482)（SERVERS 级别）调用（非 Web） | **已排除** |
| SceneTree 未完全建立 | 节点必须在树中才会收到 _Ready（语义前提） | **假设无效** |

### 7.4 唯一未前移项：Web 平台的 Install()

Web (WASM) 平台在 [Platform.cs:64](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/modules/mono/glue/GodotSharp/Platform.cs#L64) 显式跳过 `Install()`：

```csharp
// Install sync context only on non-Web platforms.
if ((_flags & RuntimeFlags.WebPlatform) == 0) {
    GodotSynchronizationContext.Install();
}
```

**跳过原因**：规避 WASM 解释器 `mono_runtime_invoke` 调用静态方法时的签名 mismatch bug（[Runtime.cs:14-17](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/modules/mono/glue/GodotSharp/Runtime.cs#L14-L17) 注释）。

**关键**：此跳过**不是 SceneTree 依赖问题**——`Install()` 本身只做 `new GodotSynchronizationContext()` + `SynchronizationContext.SetSynchronizationContext()`，不依赖任何 Godot native 对象。

### 7.5 "post-assembly-load 但 pre-SceneTree" 钩子窗口

[mono_host.cpp:475-491](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/modules/mono/mono_host.cpp#L475-L491) 即此窗口：在 `load_godotsharp()` 之后、`is_initialized = true` 之前。当前已用于 `Runtime.Initialize()` 调用 + `cache_sync_context_method()`。若未来需要为 Web 平台强制前移 `Install()`，可在此窗口内通过实例方法路径（`PumpInstance`，见 [GodotSynchronizationContext.cs:63-65](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/modules/mono/glue/GodotSharp/GodotSynchronizationContext.cs#L63-L65)）绕过静态方法签名 bug。

### 7.6 可前移性评估

| Init 项 | 当前位置 | 在 mono_host 启动阶段？ | 依赖 SceneTree？ | 是否需要前移 |
|---------|---------|----------------------|-----------------|------------|
| `mono_gc_bridge::init` | `mono_host.cpp:454` | 是 | 否 | **已就位** |
| `mono_bridge::init` | `mono_host.cpp:453` | 是 | 否 | **已就位** |
| icall 注册 | `mono_host.cpp:457` | 是 | 否 | **已就位** |
| `Runtime.Initialize()` | `mono_host.cpp:482` | 是 | 否 | **已就位** |
| `GodotSynchronizationContext.Install()` (非 Web) | `Platform.cs:65`（经 Runtime.Initialize） | 是 | 否 | **已就位** |
| `GodotSynchronizationContext.Install()` (Web/WASM) | 被跳过 | N/A | 否（不依赖 SceneTree） | 阻挡因素是 WASM 解释器 bug，非 SceneTree 依赖 |

### 7.7 结论

约束 #14 消除所需的"GC/sync-context init 前移到 mono_host" **早已在当前代码中完成**（除 Web 平台 sync-context Install 因 WASM 解释器 bug 而跳过）。原 Phase 0.1 假设中"GC bridge init timing / sync-context Install 在 _Ready 阶段破坏函数表"对当前代码不成立。

**因此，方案 A（Test.cs 初始化前移）即是根因修复**：移除已被 H9 + Phase 0.2 + 已就位的 mono_host init 共同消除的历史遗留 workaround。无需额外的引擎代码改动。

约束 #14 的最终消除仍依赖 WASM interpreter H9 22 场景回归 PASS（WASM 重编进行中），以确保 Phase 0.2 dispatch 改写在 WASM 下无回归。
