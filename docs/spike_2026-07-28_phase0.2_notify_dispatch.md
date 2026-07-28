# Phase 0.2: CSharpNotifyDispatch 架构设计

- **日期**：2026-07-29
- **前置**：Phase 0.1 delegate probe 验证完成（见 [spike_2026-07-28_phase0_delegate_probe.md](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/docs/spike_2026-07-28_phase0_delegate_probe.md)）
- **目标**：基于 Phase 0.1 验证结论，设计 C++ 侧的通知派发表，消除每次通知的字符串查找开销，并为 JIT 平台提供函数指针直调能力

---

## 一、设计动机

### 1.1 现状（每次通知的开销）

`CSharpInstance::notification(int p_notification, bool p_reversed)` 在 [csharp_script.cpp:1246](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/modules/mono/csharp_script.cpp#L1246) 当前实现：

1. **`find_method(name, arg_count)`**：遍历类继承链 `mono_class_get_method_from_name`（首次查找走字符串匹配，后续走 `CSharpScript::method_cache` HashMap）
2. **`mono_method_get_class(m)` 检查**：判断方法是否被脚本类重写（WASM 跳过未重写方法的 bug workaround）
3. **`invoke_method(m, args, ...)`**：内部调用 `mono_runtime_invoke`，每次都设置 frame、marshaling args

**问题**：
- 通知 ID → 方法名映射在 `notification()` 中每次重新做（虽有 `notif_map[]` 静态表，但循环查找）
- "is overridden" 判定每次重复（对同一实例多次通知同一 ID 时冗余）
- JIT 平台无法利用 `mono_compile_method` 一次解析函数指针、多次直接调用的优势

### 1.2 Phase 0.1 验证带来的机会

| 平台 | `mono_compile_method` 路径 | `mono_runtime_invoke` 路径 |
|------|---------------------------|---------------------------|
| 桌面 Editor（JIT） | ✅ PASS（直接函数指针） | ✅ PASS |
| WASM interpreter | ❌ CRASH（interpreter thunk 不可直调） | ✅ PASS |
| WASM AOT（待验证） | 待定（AOT 编译后应为真实 WASM 表项） | ✅ PASS |

**核心策略**：JIT 平台缓存函数指针直调；WASM interpreter/AOT 平台走 `mono_runtime_invoke`，但消除其他冗余。

---

## 二、架构设计

### 2.1 类结构

```
CSharpInstance
  └── CSharpNotifyDispatch notify_dispatch_  // 新增成员
        ├── entries_[N]  // 8 个常用通知 ID 的缓存项
        └── ResolveState state_
```

### 2.2 Entry 结构（每个通知 ID 一项）

```cpp
struct NotifyEntry {
    // 共享字段
    MonoMethod *method = nullptr;        // 解析后的方法（nullptr 表示未定义该方法）
    MonoClass  *declaring_class = nullptr; // 用于 "is overridden" 判定
    bool        resolved = false;          // 是否已解析（避免重复 find_method）

    // JIT 平台独有：直接函数指针（一次解析，多次调用）
#if defined(JIT_ENABLED)
    void       *ftn_ptr = nullptr;        // mono_compile_method 返回值
    bool        ftn_ptr_resolved = false;
#endif
};
```

### 2.3 通知 ID 索引表

```cpp
// 静态映射：notification ID -> entry index
// 只缓存高频、固定的通知；其他走 fallback 路径
static constexpr int NOTIFY_ENTRY_READY        = 0;
static constexpr int NOTIFY_ENTRY_ENTER_TREE   = 1;
static constexpr int NOTIFY_ENTRY_EXIT_TREE    = 2;
static constexpr int NOTIFY_ENTRY_PROCESS      = 3;
static constexpr int NOTIFY_ENTRY_PHYSICS_PROC = 4;
static constexpr int NOTIFY_ENTRY_NOTIFICATION = 5; // _Notification(int)
static constexpr int NOTIFY_ENTRY_TOSTRING     = 6; // _ToString()
static constexpr int NOTIFY_ENTRY_COUNT        = 7;

struct NotifySpec {
    int notification;
    int entry_index;
    const char *cs_method_name;
    int arg_count;
    // arg_count == 1 时如何获取参数
    enum class ArgProvider {
        NONE,
        DELTA_PROCESS,      // node->get_process_delta_time()
        DELTA_PHYSICS,      // node->get_physics_process_delta_time()
        NOTIFICATION_ID,    // p_notification 本身
    } arg_provider;
};

static const NotifySpec NOTIFY_SPECS[] = {
    {Node::NOTIFICATION_READY,          NOTIFY_ENTRY_READY,        "_Ready",          0, ArgProvider::NONE},
    {Node::NOTIFICATION_ENTER_TREE,     NOTIFY_ENTRY_ENTER_TREE,   "_EnterTree",      0, ArgProvider::NONE},
    {Node::NOTIFICATION_EXIT_TREE,      NOTIFY_ENTRY_EXIT_TREE,    "_ExitTree",       0, ArgProvider::NONE},
    {Node::NOTIFICATION_PROCESS,        NOTIFY_ENTRY_PROCESS,      "_Process",        1, ArgProvider::DELTA_PROCESS},
    {Node::NOTIFICATION_PHYSICS_PROCESS,NOTIFY_ENTRY_PHYSICS_PROC, "_PhysicsProcess", 1, ArgProvider::DELTA_PHYSICS},
    {-1, -1, "_Notification",  1, ArgProvider::NOTIFICATION_ID},   // 特殊：所有通知都调
    {-1, -1, "_ToString",     0, ArgProvider::NONE},
};
```

### 2.4 调用路径

```cpp
void CSharpInstance::notification(int p_notification, bool p_reversed) {
    // PREDELETE 路径保持不变（清理 gchandle + gc bridge）
    if (p_notification == Object::NOTIFICATION_PREDELETE) { ... return; }
    if (!mono_object) return;

    // 查找通知 ID 对应的 entry
    const NotifySpec *spec = find_notify_spec(p_notification);
    if (spec) {
        NotifyEntry &entry = notify_dispatch_.entries_[spec->entry_index];
        if (!entry.resolved) {
            // 首次：解析方法 + 判定 overridden + (JIT) 编译函数指针
            resolve_notify_entry(entry, spec);
        }
        if (entry.method) {
            // 执行
            invoke_cached_notify(entry, spec, p_notification);
        }
    }

    // _Notification(int) 与 _ToString() 通用 fallback 路径（不变）
    ...
}
```

### 2.5 平台分支

```cpp
inline void CSharpInstance::invoke_cached_notify(
    const NotifyEntry &entry, const NotifySpec *spec, int p_notification) {

    // 检查 overridden（WASM bug workaround 保留）
    if (script.is_valid() && script->mono_class &&
        entry.declaring_class != script->mono_class) {
        return;  // 未重写，跳过
    }

    // 准备参数
    Variant arg;
    const Variant *args[1] = { &arg };
    if (spec->arg_provider == ArgProvider::DELTA_PROCESS) {
        arg = (double)Object::cast_to<Node>(owner)->get_process_delta_time();
    } else if (spec->arg_provider == ArgProvider::DELTA_PHYSICS) {
        arg = (double)Object::cast_to<Node>(owner)->get_physics_process_delta_time();
    }

#if defined(JIT_ENABLED)
    // JIT 平台：优先走函数指针直调（绕过 mono_runtime_invoke 反射开销）
    if (entry.ftn_ptr_resolved && entry.ftn_ptr) {
        typedef void (*InvokeFn)(MonoObject *, void **);  // 简化签名
        // 注意：带参数的方法需要正确的 calling convention
        // 实际实现按 arg_count 分派
        if (spec->arg_count == 0) {
            ((void (*)(MonoObject *))entry.ftn_ptr)(mono_object);
        } else if (spec->arg_count == 1) {
            // double 参数需通过 box 或直接传 ptr
            // （细节在实现时按 Mono 调用约定处理）
            ...
        }
        return;
    }
#endif

    // Fallback：mono_runtime_invoke（WASM interpreter 唯一路径，JIT 失败时也走此路径）
    MonoObject *exc = nullptr;
    void *invoke_args[1] = { nullptr };
    if (spec->arg_count == 1) {
        // double → unbox 后传指针
        MonoObject *boxed = mono_object_box(mono_domain_get(), &arg.operator double(), ...);
        invoke_args[0] = mono_object_unbox(boxed);
    }
    mono_runtime_invoke(entry.method, mono_object, spec->arg_count ? invoke_args : nullptr, &exc);
    if (exc) {
        mono_runtime_set_pending_exception(nullptr, true);
    }
}
```

### 2.6 生命周期管理

- **`CSharpInstance` 构造时**：`notify_dispatch_` 默认初始化（所有 entry 未 resolved）
- **首次 `notification()` 触发**：惰性解析对应 entry
- **热重载（P0-1 N3 fix）**：调用 `notify_dispatch_.clear()` 重置所有 entry（已存在的实例 object 可能引用旧 image 的 method/ftn_ptr）
- **`~CSharpInstance` 析构**：无需特殊处理（不持有 ownership）

### 2.7 WASM interpreter 适配

- 不启用 `JIT_ENABLED` 宏，`NotifyEntry` 不含 `ftn_ptr` 字段（节省内存）
- 始终走 `mono_runtime_invoke` 路径
- **仍能获益**：消除每次的字符串查找 + `mono_method_get_class` 调用

---

## 三、实现拆分

| # | 任务 | 文件 | 估算 |
|---|------|------|------|
| 1 | 新增 `csharp_notify_dispatch.h/cpp` | `modules/mono/` | 简单 |
| 2 | `CSharpInstance` 添加 `notify_dispatch_` 成员 | `csharp_script.h` | 简单 |
| 3 | 改造 `notification()` 使用 dispatch 表 | `csharp_script.cpp:1246` | 中等 |
| 4 | 热重载 hook：`notify_dispatch_.clear()` | `csharp_script.cpp` 热重载路径 | 简单 |
| 5 | 桌面 Editor 编译验证 | scons platform=windows | 简单 |
| 6 | Delegate probe 回归（确保通知路径正常） | 运行 DelegateProbe | 简单 |
| 7 | WASM interpreter 构建验证 | scons platform=web | 中等 |
| 8 | 23 场景 + 21 Fuzz 全回归 | H9/Fuzz scripts | 中等 |
| 9 | （可选）WASM AOT 验证 `mono_compile_method` 行为 | scons platform=web mono_aot=yes | 中等 |

---

## 四、风险评估与缓解

| 风险 | 缓解 |
|------|------|
| JIT 函数指针调用绕过异常捕获，C# 抛异常会传播到 C++ | 探针已验证 OnDelegateInvoked 不抛异常；若脚本逻辑抛异常，原 `mono_runtime_invoke` 会捕获并返回 exc，函数指针路径不安全。**决策：JIT 默认仍走 `mono_runtime_invoke`，仅在 profiler 标注的热点（如 _Process）启用 ftn_ptr** |
| 函数指针调用约定可能因 Mono 版本变化 | 限定桌面 Editor 使用；WASM 不受影响 |
| 热重载后旧 `ftn_ptr` 失效导致崩溃 | `clear()` 显式重置，且 entry 在每次 `notification()` 调用时检查 `entry.declaring_class == script->mono_class` |
| `mono_runtime_invoke` fallback 性能不如原实现 | 原实现也是 `mono_runtime_invoke`，性能持平；新增开销仅为 entry 查找（O(1) 数组索引） |

---

## 五、验收标准

1. **桌面 Editor**：23 场景 + 21 Fuzz 全 PASS（无回归）
2. **WASM interpreter**：H9 23 场景 + 21 Fuzz 全 PASS
3. **性能基准**：`_Process` 通知路径，桌面 JIT 模式下吞吐提升 ≥ 20%（可选，仅 ftn_ptr 启用时）
4. **DelegateProbe**：5/5 markers PASS（验证现有 delegate 路径未被破坏）
5. **代码质量**：无新增内存泄漏（valgrind 或 ASAN 验证）

---

## 六、决策点

### 6.1 是否实现 JIT ftn_ptr 路径？

**选项 A**：实现双路径（JIT ftn_ptr + WASM MRI），最大化性能
- 优势：JIT 平台性能最优
- 风险：调用约定维护成本，异常处理差异

**选项 B**：仅实现 MRI 路径，但消除字符串查找 + overridden 判定冗余
- 优势：简单可靠，所有平台统一
- 风险：性能提升有限（仅节省查找开销）

**建议**：先实现 **选项 B**（消除冗余），稳定后按 profiler 数据决定是否启用 **选项 A**。

### 6.2 是否先实现 WASM AOT 验证？

WASM AOT 构建已在后台启动（约 30-60 分钟）。完成后跑 delegate probe：
- 若 Test 3 PASS → AOT 生产构建可走 ftn_ptr（性能更优）
- 若 Test 3 FAIL → 双路径架构定型，AOT 也走 MRI

---

## 七、相关文件

| 文件 | 改动类型 |
|------|---------|
| `modules/mono/csharp_notify_dispatch.h` | 新增 |
| `modules/mono/csharp_notify_dispatch.cpp` | 新增 |
| `modules/mono/csharp_script.h` | 修改（添加成员） |
| `modules/mono/csharp_script.cpp` | 修改（重写 `notification()`） |
| `modules/mono/SCsub` | 修改（添加新源文件） |

---

## 八、下一步

待用户确认本架构设计后：
1. 实现 Phase 0.2 任务 1-4（核心代码）
2. 编译验证桌面 Editor
3. 跑 DelegateProbe 回归
4. 跑 23 场景 + 21 Fuzz 全回归
5. （并行）AOT 构建完成后跑 delegate probe
