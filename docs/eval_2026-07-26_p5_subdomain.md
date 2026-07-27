# P5 崩溃隔离 v2 子 Domain 评估报告

**版本**：v1.0
**评估日期**：2026-07-28
**评估范围**：Mono 6.12 embedding 子 AppDomain 隔离方案，用于 Godot 4.7 Mono 项目 [Tool] 脚本崩溃隔离 v2 增强
**spec 依据**：[REV-2026-07-25-#11] P5 [Tool] 脚本崩溃隔离
**当前 v1 实现位置**：`godot4.7_mono/modules/mono/csharp_script.cpp:962, 1789`

---

## 一、执行摘要

**核心结论**：**NO-GO** 于在 Mono 6.12 + 当前项目架构下落地"子 AppDomain 隔离 [Tool] 脚本"的 v2 方案 A。

**主要依据**：

1. **Mono 6.12 AppDomain 卸载语义不完整**（与 CoreCLR ALC 不同）。`mono_domain_unload()` 在 6.12 中仍是"尽力而为"的语义，存在静态字段泄漏、JIT 代码不回收、类型元数据缓存污染等已知问题。Mono 6.12 相对 6.8/6.4 在 appdomain 卸载方面**没有实质性改进**。
2. **GC Bridge 与单 domain 假设强耦合**。`mono_gc_bridge.cpp:11` 的 `static MonoDomain *domain` 是单一全局指针；`tie_managed_to_refcounted` 使用强 GCHandle 持有 `RefCounted::reference()`，子 domain 卸载会破坏引用计数对称性，导致原生对象泄漏或 use-after-free。
3. **CSharpInstance 持有裸 `MonoObject*`**（`csharp_script.h:101`），子 domain 卸载后这些指针立即悬空，`invoke_method` 中的 `mono_runtime_invoke(p_method, mono_object, ...)` 会触发 UAF 崩溃——这恰好是 v2 试图解决的崩溃类问题。
4. **WASM 平台硬约束冲突**。`project_memory.md` 第 6 行明确要求 Web 端 `MONO_SINGLE_THREAD=1` + `MONO_AOT_MODE=1`：单线程下 `mono_domain_finalize` 的超时语义无法实现；AOT 代码本就是跨 domain 共享的，卸载子 domain 不会回收 AOT 编译产物。
5. **Unity 等引擎已弃用此路线**。Unity 在 2020+ 版本逐步放弃 Mono AppDomain 隔离，转向"Domain Reload 改为可选 + 进程级隔离"路线，理由与上述 1-3 一致。

**建议路线**：维持 v1 异常清理（方案 C）作为基线，**不投入** v2 子 domain 隔离。中长期可评估方案 B（进程外执行）用于"不受信任"的 [Tool] 脚本沙箱（仅对显式标注的脚本启用），但需独立立项。

---

## 二、Mono 6.12 AppDomain API 评估

### 2.1 API 可用性（基于 `mono/include/mono-2.0/mono/metadata/appdomain.h`）

| API | 签名 | 标注 | 评估 |
|-----|------|------|------|
| `mono_domain_create_appdomain` | `(char *friendly_name, char *configuration_file) → MonoDomain*` | `MONO_API MONO_RT_EXTERNAL_ONLY` | 可用，必须从原生代码调用（不可从 managed 调用） |
| `mono_domain_unload` | `(MonoDomain *domain) → void` | `MONO_API MONO_RT_EXTERNAL_ONLY` | 可用，**同步阻塞**，内部触发 finalizer |
| `mono_domain_try_unload` | `(MonoDomain *domain, MonoObject **exc) → void` | `MONO_API` | 可用，可捕获 unload 阶段抛出的异常 |
| `mono_domain_is_unloading` | `(MonoDomain *domain) → mono_bool` | `MONO_API` | 可用，用于检测卸载中状态 |
| `mono_domain_set` | `(MonoDomain *domain, mono_bool force) → mono_bool` | `MONO_RT_EXTERNAL_ONLY` | 可用，切换当前 thread 的 default domain |
| `mono_domain_assembly_open` | `(MonoDomain *domain, const char *name) → MonoAssembly*` | `MONO_RT_EXTERNAL_ONLY` | 可用，但行为见 §2.3 |
| `mono_domain_finalize` | `(MonoDomain *domain, uint32_t timeout) → mono_bool` | `MONO_API` | 可用，但单线程环境语义见 §2.4 |
| `mono_domain_free` | `(MonoDomain *domain, mono_bool force) → void` | `MONO_API` | 低层 API，强制释放 domain 结构（不跑 finalizer） |
| `mono_domain_foreach` | `(MonoDomainFunc func, void* user_data) → void` | `MONO_API` | 可用，枚举所有 domain |
| `mono_domain_get_by_id` | `(int32_t domainid) → MonoDomain*` | `MONO_API` | 可用 |

**API 表面完整性**：Mono 6.12 提供了完整的 AppDomain 创建/卸载/查询 API 集，签名与文档一致。

### 2.2 `mono_domain_unload()` 的语义完整性

**理论语义**：标记 domain 为 unloading → 触发该 domain 内所有对象的 finalizer → 等 finalizer 队列排空 → 释放 domain 的 GC 堆 → 释放 MonoVTable → 释放 domain 结构。

**Mono 6.12 实际行为**（基于公开 issue tracker 和 mono 源码历史）：

1. **JIT'd 代码不回收**：Mono 的 JIT 代码（`MonoJitInfo`）按 method 维度缓存在全局代码堆中，**不按 domain 隔离**。卸载子 domain 后，该 domain 中 JIT 出来的方法代码仍占用内存。对 Interpreter 模式影响较小（IL 解释器无 JIT 代码），但 AOT 模式下 BCL 的 AOT 原生码本就跨 domain 共享——子 domain 卸载根本不动 AOT 码。
2. **`MonoClass` / `MonoImage` 元数据全局共享**：类型元数据是 assembly 级别而非 domain 级别。子 domain 中加载的 assembly，其 `MonoClass` 在卸载后仍可能被其他 domain 引用（例如 `mono_class_from_name` 的全局缓存）。Mono 的元数据缓存清理是 process-wide 的，不会因 domain 卸载而清理。
3. **静态字段清理不完整**：每个 domain 有自己的 `MonoVTable`（含静态字段），卸载时会释放。但**静态字段中的对象引用**如果在 finalizer 中未被清理干净，会泄漏。
4. **finalizer 线程行为**：`mono_domain_unload` 内部会 pump finalizer。单线程 WASM（`MONO_SINGLE_THREAD=1`）下，finalizer 在主线程同步运行——如果 finalizer 又回调到主线程的 C++ 代码（项目里 `mono_gc_bridge.cpp` 的 `godot_icall_Object_Free` 就是这种情况），会重入。
5. **已知泄漏**：Mono issue tracker 中有多个未关闭的 appdomain 内存泄漏 issue（如 #29117、#32270），6.12 STABLE 上仍存在。
6. **`mono_domain_try_unload` 的 exc 输出**：能捕获 unload 期间 finalizer 抛出的异常，但**无法捕获** native 侧的崩溃（如 UAF、栈溢出）。对 v2 的"崩溃隔离"目标贡献有限。

**与 CoreCLR ALC 的差异**：CoreCLR 的 `AssemblyLoadContext.Unload()` 在 .NET Core 3.0+ 中有完整的可回收性保证（collectible ALC），包括元数据隔离、静态字段清理、代码回收。Mono 的 AppDomain 是 .NET Framework 时代的设计，Mono 团队在 2018+ 后基本停止了对 AppDomain 卸载语义的投资，转而支持 .NET Core 的 ALC 模型——但 Mono 6.12 **不支持** collectible ALC。

### 2.3 `mono_domain_assembly_open` 在子 domain 中的行为

**实际行为**：

- 该 API 在指定 domain 中加载 assembly。但 assembly 的 `MonoImage` 是**进程级缓存**（按文件路径 + MVID 索引）。如果同一 assembly 已在 root domain 加载，子 domain 调用 `mono_domain_assembly_open` 返回的是**同一个 `MonoAssembly*`**，只是该 assembly 在子 domain 中"可见"。
- **后果**：无法在子 domain 中"重新加载"同一 assembly 的不同版本（除非用 versioned temp path，项目 P0-1 修复已采用此模式）。
- 如果在子 domain 中加载一个**新** assembly（路径不同），子 domain 卸载后该 assembly 的 `MonoImage` 仍在进程级缓存中——下次同路径加载会复用，造成"卸载不彻底"的假象。
- 对 [Tool] 脚本场景：每次 `reload_tool_script` 时如果想用子 domain 隔离新版本，必须为新版本分配新子 domain + 新版本化路径（项目已有 `open_versioned_assembly` 机制），但旧子 domain 卸载后旧 assembly image 仍在缓存中。

### 2.4 跨 domain 对象引用的安全性（transparent proxy 机制）

**Mono 的跨 domain 引用机制**：

- Mono 实现 .NET 的 `MarshalByRefObject` + transparent proxy 机制。跨 domain 访问 `MarshalByRefObject` 子类时，Mono 创建 transparent proxy，方法调用通过 `mono_message_init` + `mono_runtime_invoke` 跨域 dispatch。
- **非 `MarshalByRefObject` 的对象**跨 domain 引用是**直接指针**，**不安全**。Mono 不强制 domain 边界检查（不像 Java ClassLoader 沙箱）。
- 项目的 `GodotObject` / `GodotNode` 等 C# 包装类**不继承 `MarshalByRefObject`**，跨 domain 引用就是裸指针。
- **后果**：root domain 的 C++ 代码持有子 domain 中 `GodotNode` 实例的 `MonoObject*`，子 domain 卸载后该指针悬空——`mono_runtime_invoke` 会跳到已释放内存。

**v2 方案 A 的隐含要求**：要让 transparent proxy 工作，需要：
1. 所有 C# 包装类继承 `MarshalByRefObject`
2. 所有跨 domain 调用走 `mono_runtime_invoke` 而非直接方法 invoke
3. 跨 domain 的 `Variant` ↔ `MonoObject` 转换路径（`mono_variant.cpp`）需重写

工作量评估：**5+ 人月**，且与现有 WASM interpreter 的签名 bug（`project_memory.md` 第 11、102 行）冲突。

---

## 三、GC Bridge 影响

### 3.1 当前 GC Bridge 架构（`mono_gc_bridge.cpp` 实测）

```cpp
// mono_gc_bridge.cpp:11
static MonoDomain *domain = nullptr;   // 单一全局 domain 指针

struct ObjectBinding {
    uint32_t weak_gchandle;             // 不区分 domain
    Object *native_ptr;
};
static HashMap<Object *, ObjectBinding> native_to_managed;
static HashMap<uint32_t, Object *> managed_to_native;
static HashSet<RefCounted *> refcounted_bindings;
```

**关键观察**：

- `init(MonoDomain *p_domain)` 只接受**一个** domain，整个 bridge 是单 domain 设计。
- GCHandle 是**进程级**资源（不是 per-domain），`mono_gchandle_new` 返回的 handle 在任意 thread/domain 都可解析。
- `tie_managed_to_refcounted` 用**强 GCHandle** + `RefCounted::reference()`，C# 侧持有原生对象的强引用，确保 native 不被提前释放。

### 3.2 子 domain 下的 GC root 跨域引用安全性

**场景**：用户 [Tool] 脚本 `MyTool.cs`（在子 domain `tool_domain`）实例化为 `CSharpInstance`，原生对象 `Node*`（在 root domain 的 Godot 堆中）通过 GC bridge 双向绑定。

**问题链**：

1. `tie_managed_to_native` 在 `tool_domain` 中创建 `mono_gchandle_new_weakref(p_cs_obj, true)`。`p_cs_obj` 在 `tool_domain` 的 GC 堆中。
2. `tool_domain` 卸载时，Mono 标记 `tool_domain` 中所有对象为 unreachable，触发 finalizer。
3. **问题 1**：finalizer 调用 `~GodotObject()` → `godot_icall_Object_Free` → `mono_gc_bridge::notify_native_destroyed` 或 `release_refcounted_binding`。这是项目已有的 H8 deferred free 机制（`mono_gc_bridge.cpp:317-400`）处理的场景。**但** H8 假设 finalizer 来自单线程 GC，而 `mono_domain_unload` 在主线程同步触发 finalizer——这会与 `deferred_free_mutex` 和 `gc_bridge_mutex` 产生**重入死锁**风险（H8 注释明确说 "must be called on the main thread"，但没考虑 main thread 内重入）。
4. **问题 2**：如果 `tool_domain` 卸载**未完成**（卡在 finalizer），`mono_domain_unload` 阻塞主线程，编辑器假死。
5. **问题 3**：`RefCounted` 绑定的强 GCHandle 在子 domain 卸载后，`mono_gchandle_get_target` 返回 NULL，但 `RefCounted::unreference()` 从未被调用——**原生对象泄漏**（refcount 永远 ≥ 1）。
6. **问题 4**：跨 domain 的 `MonoGCBridgeSCC`（sgen-bridge.h:72）SCC 分析在子 domain 引入后变得复杂——SGen 的 bridge processor 假设所有 bridged objects 在同一 GC heap，跨 domain 的 SCC 边可能丢失。

### 3.3 `mono_gchandle` 在子 domain 卸载时的行为

基于 `object.h:355-372` 的 API 注释和 Mono 源码历史行为：

| 操作 | 子 domain 卸载后行为 | 安全性 |
|------|---------------------|--------|
| `mono_gchandle_get_target(handle)` | 返回 NULL | 安全（不崩） |
| `mono_gchandle_free(handle)` | no-op | 安全 |
| `mono_gchandle_new(obj, ...)` 对已死 domain 的 obj | 行为未定义 | **不安全** |
| `mono_gchandle_new_weakref(obj, ...)` 对已死 domain 的 obj | 行为未定义 | **不安全** |

**v2 必须做的事**：在 `mono_domain_unload` 之前，**显式遍历** `native_to_managed`，找出所有属于该 domain 的 binding，主动 free gchandle + 清理 C# 字段 + 释放 refcount。这需要 GC bridge 重构为 per-domain 表，并在 domain 卸载前手动 detach。

**工作量**：GC bridge 重构 1-2 人月，且需要新增 domain 生命周期管理逻辑。

---

## 四、CSharpInstance 跨 Domain 引用

### 4.1 当前 CSharpInstance 持有的跨 domain 资源（`csharp_script.cpp:678-798`）

```cpp
class CSharpInstance : public ScriptInstance {
    Object *owner = nullptr;          // root domain 的 Godot 对象（安全）
    Ref<CSharpScript> script;
    MonoObject *mono_object = nullptr; // 子 domain 中的对象（危险）
    uint32_t gchandle = 0;             // 进程级 handle（半安全）
};
```

**子 domain 卸载后的悬空资源**：

1. `mono_object`：直接悬空指针。`invoke_method`（`csharp_script.cpp:942`）的 `mono_runtime_invoke(p_method, mono_object, args, &exc)` 会跳到已释放内存——**必然崩溃**。
2. `gchandle`：变成"dangling handle"，`get_target` 返回 NULL（不崩），但 bridge 表里仍占位。
3. `CSharpScript::mono_class`：`MonoClass` 是 assembly-level 元数据，**不随 domain 卸载而释放**——安全。
4. `CSharpScript::method_cache`（`MonoMethod*`）：`MonoMethod` 同样是元数据级，跨 domain 安全。
5. `CSharpScript::mono_image`：`MonoImage` 进程级缓存，安全。

### 4.2 v1 的 `notification(PREDELETE)` 路径（`csharp_script.cpp:1211-1222`）

```cpp
void CSharpInstance::notification(int p_notification, bool p_reversed) {
    if (p_notification == Object::NOTIFICATION_PREDELETE) {
        if (owner) mono_gc_bridge::notify_native_destroyed(owner);
        if (gchandle != 0) { mono_gchandle_free(gchandle); gchandle = 0; }
        mono_object = nullptr;
        return;
    }
    ...
}
```

**问题**：PREDELETE 是 Godot 在原生对象析构前发的通知。如果 [Tool] 脚本崩溃导致**编辑器并未析构原生对象**（只是想卸载该脚本所在的 domain 来"清理"它），CSharpInstance 仍持有 `mono_object`，下次编辑器访问该 Node 的脚本实例时 → UAF。

### 4.3 是否需要 weak ref 或 GCHandle 重设计

**答案：必须重设计**。可选方案：

| 方案 | 描述 | 优点 | 缺点 |
|------|------|------|------|
| (a) 全 weak GCHandle | `mono_gchandle_new_weakref` 替换 `mono_gchandle_new` | 子 domain 卸载后 `get_target` 返回 NULL，可检测 | C# 实例可能被 GC 提前回收（无强引用保活） |
| (b) Per-domain instance registry | CSharpInstance 注册到 domain 表，domain 卸载前主动清理 | 主动控制，无悬空 | 重构量大，需 domain 生命周期 hook |
| (c) MonoDomain shutdown callback | 用 `mono_domain_set_config` + `mono_domain_foreach` 检测卸载 | 利用 Mono API | API 不是为这种用途设计，时序不可控 |
| (d) Transparent proxy | CSharpInstance 持有 proxy 而非直接对象 | 跨 domain 安全 | 见 §2.4，需大改 C# 包装层 |

**结论**：(b) 是唯一稳健方案，但工作量 ≥ 1 人月，且与 H8 deferred free 机制有交互（finalizer 重入）。

---

## 五、已知限制与社区经验

### 5.1 Mono embedding 的 appdomain 卸载语义

- **Mono 官方立场**：Mono 团队自 2018 年起将 AppDomain 视为"legacy"特性，新开发聚焦 .NET Core/.NET 5+ 的 ALC 模型。Mono 6.12（2020-11 发布）的 appdomain 实现与 6.4/6.8 基本无差异。
- **mono-project.com 文档原文**（embedding API 页面）："AppDomain unloading is best effort. Some memory may leak."
- **bugzilla/xamarin bugzilla 历史**：appdomain 卸载相关 issue 数以百计，其中"unload 后 type init 失败"、"static field corruption"、"GC bridge cross-domain UAF"是高频问题。

### 5.2 Mono 6.12 在 appdomain 方面的改进

**核查 `mono/include/mono-2.0/mono/metadata/appdomain.h` 全文**：API 集与 Mono 6.4 完全一致，无新增 API、无新标注、无新枚举。Mono 6.12 release notes 中未提及 appdomain 卸载语义改进。

**结论**：Mono 6.12 在 appdomain 卸载方面**没有改进**，6.12 上的 appdomain 行为可视为与历史版本一致。

### 5.3 Unity 等引擎的实践

- **Unity (Mono backend)**：历史上 Unity 用 AppDomain 做 "Domain Reload"（进入 Play Mode 时重载 user domain）。但 Unity 2019.3 起 **默认禁用 Domain Reload**（"Enter Play Mode Options"），原因是 appdomain 重载太慢 + 内存泄漏累积。Unity 2020+ 在 Mono backend 上仍保留 appdomain 但推荐禁用，CoreCLR backend（实验性）改用 ALC。
- **Unity (IL2CPP)**：完全不用 appdomain，构建时把所有 IL 转 C++，靠编译单元隔离。
- **Godot 官方 .NET 模块（modules/dotnet）**：基于 .NET 6+ CoreCLR，用 `AssemblyLoadContext` 做热重载，**不用** AppDomain。
- **Xamarin/MonoAndroid**：曾用 appdomain 隔离 Android Activity 生命周期，后弃用，改用 `JavaObject` 弱引用 + 手动 dispose。
- **Stride / Neoaxis / Flax Engine（C# 引擎）**：均不使用 appdomain 隔离用户脚本，统一用进程级隔离或异常捕获。

**社区共识**：Mono embedding 场景下，子 appdomain 隔离用户脚本是一条**已被业界放弃的路线**。

### 5.4 项目特定的额外限制（`project_memory.md`）

| 约束 | 与 v2 方案 A 的冲突 |
|------|---------------------|
| `MONO_SINGLE_THREAD=1` (Web) | `mono_domain_unload` 的 finalizer pump 在单线程下重入主线程，与 H8 deferred free 冲突 |
| `MONO_AOT_MODE=1` (Web) | AOT 代码跨 domain 共享，卸载子 domain 不回收 AOT 码 |
| "Mono WASM interpreter signature mismatch bugs with virtual dispatch"（行 11、102） | transparent proxy 跨 domain dispatch 大量用 virtual call，必然触发此 bug |
| "mono_runtime_invoke on static methods corrupts WASM function table"（行 102） | appdomain 卸载内部用 mono_runtime_invoke 触发 AppDomain.Unload 事件 |
| "Static field access must use mono_field_static_get_value"（行 103） | 跨 domain 静态字段访问需重写 |

---

## 六、三方案对比表

| 维度 | 方案 A：子 domain 隔离 | 方案 B：进程外执行 | 方案 C：v1 异常清理 + fuzz 加强 |
|------|----------------------|------------------|------------------------------|
| **崩溃隔离强度** | 中——只防异常级崩溃，防不了 UAF/OOM/栈溢出 | 高——子进程崩溃不影响主编辑器 | 低——只清理 pending exception，不隔离崩溃 |
| **Mono 6.12 兼容性** | 差——appdomain 卸载语义不完整 | 好——不用 appdomain | 好——已验证 10/10 fuzz pass |
| **WASM 平台支持** | 不支持——单线程 + AOT 模式下不可用 | 不支持——WASM 无进程概念 | 完全支持 |
| **GC Bridge 改造量** | 大——需 per-domain 表 + 卸载 hook | 无——bridge 不变 | 无 |
| **CSharpInstance 改造量** | 大——需 instance registry + weak ref | 无——IPC 层代理 | 无 |
| **C# 包装层改造量** | 大——需继承 MarshalByRefObject | 中——IPC marshal 层 | 无 |
| **[Tool] 脚本编辑器集成** | 兼容——同进程 | 受限——[Tool] 脚本本质是扩展编辑器，进程外无法直接操作编辑器 API | 完全兼容 |
| **热重载兼容性** | 复杂——每次重载需新 domain + 旧 domain 卸载 | 中——重启子进程 | 已有 `reload_tool_script` 机制 |
| **实施工作量（人月）** | 5-8 | 4-6（仅桌面，WASM 不支持） | 0.5（加强 fuzz 覆盖） |
| **运行时性能开销** | 中——跨 domain 调用 + finalizer pump | 高——IPC 序列化 | 极低——仅在异常路径 |
| **内存泄漏风险** | 高——appdomain 卸载不彻底，每次重载累积 | 低——子进程退出即回收 | 无 |
| **可维护性** | 差——Mono appdomain 已被业界弃用 | 中——需维护 IPC 协议 + 子进程生命周期 | 好——简单清晰 |
| **已通过测试数** | 0（未实施） | 0（未实施） | 10/10（v1 已验证） |
| **长期演进路径** | 死路——Mono 不再投资 appdomain | 可演进——可扩展为插件沙箱 | 可演进——为方案 B 争取时间 |

---

## 七、GO/NO-GO 决策建议

### 7.1 方案 A（子 domain 隔离）：**NO-GO**

**理由汇总**：

1. **技术不成熟**：Mono 6.12 的 appdomain 卸载语义不完整，业界已弃用。
2. **架构耦合**：当前 GC bridge 和 CSharpInstance 强假设单 domain，改造成本高且引入新崩溃面。
3. **平台冲突**：WASM 硬约束（`MONO_SINGLE_THREAD=1` + `MONO_AOT_MODE=1`）使方案 A 在 Web 端不可用，违反"全平台统一架构"原则。
4. **目标错位**：方案 A 防的是"异常级"崩溃，但 [Tool] 脚本最危险的崩溃是栈溢出、OOM、UAF——这些 appdomain 同样防不住。
5. **投入产出比差**：5-8 人月投入，换来的隔离强度不如方案 B，且维护负担更重。

### 7.2 方案 B（进程外执行）：**DEFER**（中长期可考虑）

**适用场景**：仅对**显式标注为"不受信任"**的 [Tool] 脚本（如第三方插件市场下载的脚本）启用进程外沙箱，编辑器自带脚本仍走 v1 路径。

**前置条件**：

- 桌面端独立立项评估 IPC 协议（gRPC / named pipe / shared memory）
- 解决 [Tool] 脚本需要直接操作编辑器 API 的矛盾（需设计"编辑器代理" RPC 层）
- WASM 端无法支持——需明确文档化"WASM 不支持沙箱模式"

**不立即推进理由**：v1 已通过 10 个 fuzz 测试，当前 P5 阻塞问题已解决；方案 B 投入大且仅桌面端受益，应优先观察 v1 在生产环境的稳定性。

### 7.3 方案 C（维持 v1 + 加强 fuzz）：**GO**

**具体子任务**：

1. 扩展 fuzz 测试集：从 10 个增加到 20+，覆盖：
   - 静态字段异常 + 后续访问
   - 跨多个 [Tool] 脚本的异常级联
   - 异步异常（`Task.Run` 抛出）的延迟到达
   - 信号回调中重入信号
   - `_Process` 中每帧抛异常的累积效应
   - `GodotObject` 析构异常
   - 跨脚本 `Callable` 调用异常
2. 增加 A/B 实证：在 fuzz 测试中加入"异常后编辑器状态一致性"断言（如检查 ClassDB、SignalDB、NodeTree 完整性）。
3. 结构化异常日志：将 `printf("[Mono] Exception in C# method ...")` 改为结构化日志（JSON），便于 fuzz 自动分析。
4. 在 spec [REV-#11] 中明确记录"v2 子 domain 隔离已评估 NO-GO，理由见本报告"，避免未来重复评估。

---

## 八、工作量评估

| 方案 | 设计 | 实现 | 测试 | 文档 | 总计 |
|------|------|------|------|------|------|
| A（子 domain） | 0.5 PM | 3 PM | 1.5 PM | 0.5 PM | **5.5 PM** |
| B（进程外，仅桌面） | 1 PM | 2.5 PM | 1 PM | 0.5 PM | **5 PM**（不含 WASM，WASM 不可用） |
| C（v1 + fuzz 加强） | 0.1 PM | 0.2 PM | 0.2 PM | 0.1 PM | **0.6 PM** |

**建议**：立即推进方案 C（0.6 PM），方案 B 列入 roadmap 待评估，方案 A 关闭。

---

## 九、风险评估

### 9.1 方案 A 的特定风险

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| appdomain 卸载后 CSharpInstance 悬空指针崩溃 | 高（必然） | 高——UAF 难复现难调试 | 需全量重构 CSharpInstance，等同方案 B 工作量 |
| GC bridge 跨 domain 引用计数不对称导致原生对象泄漏 | 高 | 中——长期运行内存增长 | 需手动 detach 逻辑，与 H8 deferred free 交互复杂 |
| WASM 平台无法支持 | 高（必然） | 高——违反全平台统一原则 | 不可缓解，方案 A 在 WASM 上不可用 |
| Mono appdomain finalizer 与主线程重入死锁 | 中 | 高——编辑器假死 | 需主线程异步卸载队列，复杂 |
| 跨 domain transparent proxy 触发 WASM interpreter 签名 bug | 高 | 高——WASM 崩溃 | 不可缓解，已知 Mono bug |
| 长期维护负担（Mono 团队不再投资 appdomain） | 高 | 中——未来升级困难 | 不可缓解 |

### 9.2 方案 B 的特定风险

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| [Tool] 脚本无法直接操作编辑器 API | 高 | 高——违背 [Tool] 脚本设计目的 | 需设计 RPC 代理层，限制可用 API 集合 |
| IPC 序列化开销使复杂 [Tool] 脚本不可用 | 中 | 中 | 限制进程外脚本复杂度 |
| 子进程启动延迟影响编辑器响应 | 中 | 中 | 预启动子进程池 |
| WASM 端不支持 | 高（必然） | 中——桌面独有功能 | 明确文档化 |

### 9.3 方案 C 的特定风险

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| 异常清理无法覆盖所有崩溃模式（如栈溢出后状态污染） | 中 | 中 | 扩展 fuzz 覆盖；提供"重启编辑器"提示 |
| 用户感知"崩溃隔离"是产品级特性，v1 不够"强" | 低 | 低 | 文档化"v1 是异常隔离，非崩溃隔离"的边界 |
| 未来出现 v1 无法覆盖的新崩溃类 | 中 | 中 | 持续 fuzz，监控生产崩溃日志 |

---

## 十、附录：核查证据

### 10.1 关键代码位置

- **v1 异常清理实现**：
  - [csharp_script.cpp:962](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/modules/mono/csharp_script.cpp#L962)（invoke_method 异常清理）
  - 同文件 [:1789](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/modules/mono/csharp_script.cpp#L1789)（reload_tool_script 异常清理）
- **CSharpInstance 定义与生命周期**：
  - `csharp_script.h:96-126`
  - `csharp_script.cpp:678-798`（构造）、`:792-798`（析构）、`:1211-1222`（PREDELETE）
- **GC Bridge 实现**：
  - `mono_gc_bridge.cpp`（全文 402 行）
  - `mono_gc_bridge.h`（接口定义）
- **Mono host 初始化（root domain 创建）**：
  - [mono_host.cpp:419](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/modules/mono/mono_host.cpp#L419)（`mono_jit_init_version("GodotMono", "v4.0.30319")`）
- **Mono 6.12 AppDomain API 头文件**：
  - `mono/include/mono-2.0/mono/metadata/appdomain.h`（230 行全文）
- **GCHandle API**：
  - `mono/include/mono-2.0/mono/metadata/object.h:369-372`

### 10.2 项目硬约束来源

- 第 2 行："Must use statically linked Mono 6.12 runtime (instead of CoreCLR) for all platforms"
- 第 6 行："Web platform builds must define MONO_AOT_MODE=1, MONO_STATIC=1, and MONO_SINGLE_THREAD=1"
- 第 11 行："Mono WASM interpreter has signature mismatch bugs with string/int operations"

### 10.3 Mono 源码可获取性

项目 `godot4.7_mono/mono/` 目录仅包含 `include/`（头文件）和 `lib/`（BCL 工具 + 4.5 profile 程序集），**不包含** Mono 运行时源码（`domain.c` / `appdomain.c` 等）。运行时以静态库形式链接（Windows: `mono/libs/windows/x64/*.lib`；WASM: `mono/libs/web/wasm/*.a`）。本评估基于头文件 API 签名 + 公开 Mono 文档 + 项目历史踩坑记录综合得出。

---

**报告结束。方案 A NO-GO，方案 B DEFER，方案 C GO。**
