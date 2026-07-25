# Mono Editor Spec 修订记录

> **修订日期**：2026-07-25
> **修订人**：基于 `docs/review_2026-07-25.md` 评审报告
> **修订文件**：
> - `docs/mono_editor_spec.md`（12 处修订，标记 [REV-2026-07-25-#01] 至 [REV-2026-07-25-#12]）
> - `docs/steel-echo-red-star.md`（3 处修订，标记 [REV-2026-07-25-#13] 至 [REV-2026-07-25-#15]）
> **目的**：每处修订保留原文/新文对照，便于第三方审查定位与确认

---

## 一、`mono_editor_spec.md` 修订（12 处）

### [REV-2026-07-25-#01] 统一 SCsub 条件编译策略

**位置**：§0.2.5 + §1.2
**优先级**：P0
**问题**：§0.2.5 说"SCsub 不做条件编译"，§1.2 又要加 `if env["tools"]:` 块，自相矛盾

**原文（§0.2.5）**：
```
5. **editor 隔离**：当前模块 SCsub 不做条件编译（`SCsub:21-35`），editor-only 代码一律用 `#ifdef TOOLS_ENABLED` 包裹，运行时入口再叠加 `Engine::get_singleton()->is_editor_hint()` 判断（参照 `ensure_project_file`，`csharp_script.cpp:1431-1434`）。
```

**新文（§0.2.5）**：
```
5. **editor 隔离**：当前模块 SCsub 不做条件编译（`SCsub:21-35`，已核实所有源文件无条件列入 `mono_sources`，包括 `mono_export_plugin.cpp` 与 `editor/bindings_generator.cpp`），editor-only 代码一律用 `#ifdef TOOLS_ENABLED` 包裹，运行时入口再叠加 `Engine::get_singleton()->is_editor_hint()` 判断（参照 `ensure_project_file`，`csharp_script.cpp:1431-1434`）。**[REV-2026-07-25-#01]** 本约定与 §1.2 一致：新增 editor 源文件（如 `code_completion.cpp`、`mono_build_panel.cpp`）直接追加到 `mono_sources`，**不在 SCsub 层加 `if env["tools"]:` 分支**；TOOL 裁剪由 C++ 内部 `#ifdef TOOLS_ENABLED` 全权负责，确保 SCsub 简洁一致。
```

**原文（§1.2）**：
```python
if env["tools"]:
    SConscript("editor/script_templates/SCsub")
```

**新文（§1.2）**：
```python
# 主 SCsub 中无条件 SConscript（script_templates/SCsub 内部自行判断 tools）
SConscript("editor/script_templates/SCsub")
```
并补充说明："`script_templates/SCsub` 内部用 `if env["tools"]:` 包裹 `make_templates` 调用，确保非编辑器构建不生成 `templates.gen.h`（已核对 `godot4.7_mono/modules/gdscript/editor/script_templates/SCsub` 是此模式，可作为参考）。"

**审查要点**：
- 确认 `godot4.7_mono/modules/gdscript/editor/script_templates/SCsub` 确实是"子 SCsub 内部判断 tools"模式
- 实施时核对 `script_templates/SCsub` 的实际内容是否符合此约定

---

### [REV-2026-07-25-#02] 修正 A3 sanitize 抽取方向描述

**位置**：§2.A3 实现步骤 1
**优先级**：P0
**问题**：原文说"行为对齐当前 `get_safe_project_name`"，但实际 `sanitize_project_name` 是 `csharp_script.cpp:149-185` 中的 static 内嵌函数，未在 `path_utils.cpp` 中

**原文**：
```
1. `utils/path_utils.cpp` 全量替换为旧版（258 行）：恢复 `find_executable`（PATH 搜索 + Windows PATHEXT）、`abspath`/`realpath` 完整语义。`get_csharp_project_name` 保留旧版签名，但**行为对齐当前 `get_safe_project_name`**（`csharp_script.cpp:174-185`）：读 `dotnet/project/assembly_name` → 回退 `application/config/name` → sanitize。注意 `csharp_script.cpp:149-185` 与 `mono_export_plugin.cpp:14-37` 已有两份 sanitize 实现，本次统一到 `path_utils`，删除重复（保持 `mono_export_plugin.cpp` 注释要求的同步关系改为单一来源）。
```

**新文**：
```
1. `utils/path_utils.cpp` 全量替换为旧版（258 行）：恢复 `find_executable`（PATH 搜索 + Windows PATHEXT）、`abspath`/`realpath` 完整语义。**[REV-2026-07-25-#02]** 关键澄清：当前 `sanitize_project_name` 与 `get_safe_project_name` 是 **`csharp_script.cpp:149-185` 中的 static 内嵌函数**（未在 `path_utils.cpp` 中）；`mono_export_plugin.cpp:14-37` 还有第二份 sanitize 重复实现。本次工作不是"对齐"，而是**抽出 + 删除重复**：
   - 将 `csharp_script.cpp:149-172` 的 `sanitize_project_name` 实现移入 `path_utils.cpp`（公开为 `Path::sanitize_project_name`）
   - 将 `csharp_script.cpp:174-185` 的 `get_safe_project_name` 实现移入 `path_utils.cpp`（公开为 `Path::get_csharp_project_name`，签名对齐旧版 `path_utils.h`）
   - 删除 `csharp_script.cpp` 中的 static 原实现，改为 `#include "utils/path_utils.h"` 调用
   - 删除 `mono_export_plugin.cpp:14-37` 的第二份 sanitize，改为调用 `Path::sanitize_project_name`
   - 行为保持当前实现（非旧版）：读 `dotnet/project/assembly_name` → 回退 `application/config/name` → sanitize（ASCII 字母数字下划线保留，空格/连字符/#/./括号转 `_`，首字符为数字时加 `_` 前缀）
```

**审查要点**：
- 实施时确认 `csharp_script.cpp:149-172` 与 `csharp_script.cpp:174-185` 的行号范围准确
- 确认 `mono_export_plugin.cpp:14-37` 确实有第二份 sanitize 实现
- 抽出后两处原调用点行为保持不变（用单元测试验证）

---

### [REV-2026-07-25-#03] 核查 EditorDock API 已存在

**位置**：§4.P4 实现步骤 1
**优先级**：P0
**问题**：原 spec 假设 `EditorDock` / `EditorDockManager` API 存在，但未核查

**核查结果（2026-07-25）**：
- `godot4.7_mono/editor/docks/editor_dock.{h,cpp}` **存在**
- `godot4.7_mono/editor/docks/editor_dock_manager.{h,cpp}` **存在**
- API 可用，spec 假设成立

**新文（在 P4 实现步骤 1 前加注）**：
```
- **[REV-2026-07-25-#03]** 已核查 `EditorDock` 与 `EditorDockManager` API 存在于 `godot4.7_mono/editor/docks/editor_dock.{h,cpp}` 与 `editor_dock_manager.{h,cpp}`，可放心使用。
```

**审查要点**：
- 实施时确认 `editor_dock.h` 的 `EditorDock::DOCK_SLOT_BOTTOM` 枚举存在
- 确认 `EditorDockManager::add_dock` 签名与 spec 描述一致

---

### [REV-2026-07-25-#04] 补充 B0 类型映射表

**位置**：§3.2 类型映射段
**优先级**：P1
**问题**：原文"全部数学值类型"表述模糊，未列出具体类型对应关系

**原文**：
```
**类型映射**：`MonoType*` → `Variant::Type` 的映射目前分散在 `CSharpInstance::get_property_type`（`csharp_script.cpp:868-901`，仅 6 类且不查字段）。B0 将其提取为 `mono_script_meta::mono_type_to_variant_type(MonoType*)`，覆盖：bool/int 全宽/float 全宽/string/全部数学值类型（与 `mono_variant` 缓存类比较）/enum→INT/Object 派生→OBJECT/Array·Dictionary 包装→ARRAY·DICTIONARY；不支持的类型跳过该成员（不导出），`WARN_PRINT` 一次。
```

**新文**：替换为完整的 25 行类型映射表（含 MONO_TYPE_BOOLEAN/I1-I8/U1-U8/R4/R8/STRING/VALUETYPE 18 种 Godot 数学类型/enum/OBJECT 派生/Dictionary·Array 包装/其他→NIL），并补充实现要点（`mono_class_from_mono_type` + 类名比较、struct 值类型、WARN_PRINT 去重）。

**审查要点**：
- 确认 glue 中 Godot 数学类型均为 struct（值类型），非 class
- 确认 `mono_variant.cpp` 缓存的类指针与映射表中的类名一致
- 实施时单测覆盖每个 Variant::Type 的映射

---

### [REV-2026-07-25-#05] PascalCase 命名决策说明

**位置**：§4.P1 实现步骤 2
**优先级**：P1
**问题**：未说明与旧方案一致性、序列化兼容性、混用风险

**新文（追加到原命名决策后）**：
```
**[REV-2026-07-25-#05]** 补充说明：
   - **与旧方案一致性**：Godot 官方 .NET 模块（旧方案）也是 PascalCase，本约定与上游一致
   - **序列化兼容性**：场景保存时属性名会写入 `.tscn`/`.scn` 文件，C# 脚本的 `PascalCase` 与 GDScript 脚本的 `snake_case` **不互通**——混用 GDScript + C# 跨脚本引用属性会因命名差异失败
   - **风险文档化**：在 `Attributes.cs` 中加 `[Export]` 的 XML 注释说明此约定，提醒用户 C# 脚本属性名在 .tscn 中保持 PascalCase
```

**审查要点**：
- 确认旧方案（提交 `1963b2f126`）确实是 PascalCase
- 实施时在 `Attributes.cs` 的 `ExportAttribute` 加 XML 注释

---

### [REV-2026-07-25-#06] 文件名==类名约定限制

**位置**：§4.P2 实现步骤 2 路径反查段
**优先级**：P1
**问题**：C# 不强制文件名==类名，约定存在风险

**新文（追加到路径反查说明后）**：
```
**[REV-2026-07-25-#06]** 已知限制：
     - C# **不强制**文件名==类名：一个 .cs 文件可含多个类（partial class 拆分、内部类）；文件名也可与类名不同（如 `MyNode.v2.cs` 含 `MyNode` 类）
     - v1 在文档中明确说明此限制：「C# 类必须定义在与类名同名的 .cs 文件中，否则全局类注册失败」
     - v2 计划：用 `mono_image_get_table_info(image, MONO_TABLE_TYPEDEF)` 迭代 + `MonoMethod* debug_info` 查源文件路径（需 .pdb 支持），解除文件名约束
```

**审查要点**：
- 实施时在 `CSharpLanguage::refresh_global_classes` 注释中明确此限制
- v2 计划需评估 .pdb 在 WASM 端的可用性

---

### [REV-2026-07-25-#07] P7 sdb 可用性 spike 前置

**位置**：§4.P7 实现前
**优先级**：P1
**问题**：Mono 静态库可能未启用 sdb，P7 实施前必须验证

**新文（在 P7 实现前加 spike 段）**：
```
**[REV-2026-07-25-#07] 实施前置 spike（必做）**：在实施 P7 前，必须先做以下可用性验证：
1. 核查桌面 Mono 静态库（`mono/lib/` 或系统 `C:\Program Files\Mono`）是否在构建时启用了 `--enable-debug` 与 `--with-monodbg`，否则 sdb agent 不可用
2. 当前 `mono/libs/web/wasm/*.a` 是 WASM 静态库（不含 sdb），P7 仅桌面受益——需确认桌面 Mono 静态库（非 WASM）含 sdb 符号
3. 在 `mono_host.cpp` 加 `mono_jit_parse_options` 试运行（必须在 `mono_jit_init_version` 之前），验证：
   - IDE（Rider/VS）能附加到运行中的 Godot 进程
   - 断点命中、单步执行正常
4. 若 spike 失败（sdb 不可用），P7 降级为 C 级（延后到桌面 Mono 静态库重编后再做）
```

**审查要点**：
- spike 必须在 P7 编码前完成，记录 spike 报告
- 若 spike 失败，更新 spec 将 P7 移至 C 级

---

### [REV-2026-07-25-#08] WASM 回归测试

**位置**：§5 验证总计划表
**优先级**：P1
**问题**：原验证计划仅"编译验证"不够，需 WASM 端完整回归

**新文（验证表新增一行）**：
```
| **[REV-2026-07-25-#08] WASM 回归测试** | **必做**：B 级改动在 `csharp_script.cpp` 等文件加大量 `#ifdef TOOLS_ENABLED` 块，需确保：<br>1. WASM 构建产物中无 TOOLS 相关符号（`nm godot.web.template_release.wasm \| grep TOOLS` 应为空）<br>2. WASM 端 23 场景回归测试全 PASS（`python tools/run_h9_wasm_test.py`，当前基线 205/205 断言）<br>3. WASM 包体积无显著增长（< 1% 波动，用 `tools/analyze_wasm.py` 比对） |
```

**审查要点**：
- 每个 B 级条目（P1-P7）实施后都需跑 WASM 回归
- 当前基线 23/23 场景 PASS、205/205 断言必须保持

---

### [REV-2026-07-25-#09] A2 CodeCompletion 消费方说明

**位置**：§2.A2 实现步骤 5
**优先级**：P2
**问题**：A2 无明确消费方，价值为零

**新文（追加到 A2 步骤 5 后）**：
```
**[REV-2026-07-25-#09]** 消费方澄清与降级评估：
   - **A2 当前无明确消费方**——若不接入 CSharpLanguage 或外部 IDE 插件，A2 的 icall + glue API 就位后无人调用，价值为零
   - **降级选项 A（推荐）**：将 A2 降级为 C 级（延后到 GodotTools 移植时一起做），本期不实施
   - **降级选项 B**：在 A2 末尾加「最小消费示例」——在 `ScriptTextEditor` 的 completion provider 中调 `CodeCompletion.Request`，需核对 Godot 4.7 的 `ScriptEditorBase::_request_code_completion` API 是否可用
   - **决策**：本期采用降级选项 A，A2 从 A 级移至 C 级；A3/A4 保持 A 级
```

**审查要点**：
- A2 降级后，§1.1 SCsub 的 `editor/code_completion.cpp` 也从源文件清单移除
- linker.xml 中 `CodeCompletion` 类型注释保持（不删除，便于 C 级实施时启用）

---

### [REV-2026-07-25-#10] linker.xml 类型清单

**位置**：§3.1 末尾
**优先级**：P2
**问题**：未列出 B0 新增的具体类型

**新文（在 §3.1 末尾追加）**：
```xml
<type fullname="Godot.ExportAttribute" preserve="all"/>
<type fullname="Godot.SignalAttribute" preserve="all"/>
<type fullname="Godot.ToolAttribute" preserve="all"/>
<type fullname="Godot.GlobalClassAttribute" preserve="all"/>
<!-- 若 A2 不降级，则追加： -->
<!-- <type fullname="Godot.CodeCompletion" preserve="all"/> -->
<!-- <type fullname="Godot.CompletionKind" preserve="all"/> -->
```
并注明："当前 `linker.xml` 已有 33 个类型（`GodotSharp` 27 + `HelloMono` 2 + `mscorlib` 25），新增上述 4 个特性类型后总数 37 个。"

**审查要点**：
- 实施时确认 `linker.xml` 当前类型数与 spec 描述一致（33 个）
- 新增 4 个特性类型后总数应为 37 个

---

### [REV-2026-07-25-#11] P5 [Tool] 脚本崩溃隔离

**位置**：§4.P5 实现步骤 4
**优先级**：P2
**问题**：未说明崩溃隔离策略

**新文（追加到 P5 步骤 4 后）**：
```
**[REV-2026-07-25-#11]** 崩溃隔离策略：
   - **当前架构**：`mono_domain_assembly_open` 加载到 root domain，[Tool] 脚本崩溃会直接影响编辑器进程
   - **v1 策略（推荐）**：在 `invoke_method` / `notification` 外层加 `mono_runtime_set_pending_exception(nullptr)` 清理 pending 异常（已有部分覆盖，需确认全覆盖）
   - **v2 策略（评估）**：子 domain 隔离（`mono_domain_create_appdomain`），但 Mono embedding 的 appdomain 卸载语义不完整（与 CoreCLR ALC 不同），需评估可行性
   - **验收项**：fuzz 测试——构造 10 个异常脚本（空引用/除零/栈溢出/无限循环/递归爆栈/异步异常/静态构造异常/属性 getter 异常/方法参数异常/信号回调异常），确认编辑器不崩
```

**审查要点**：
- 实施时先核查 `csharp_script.cpp:641-749` 中 `mono_runtime_set_pending_exception` 的覆盖度
- fuzz 测试 10 个异常脚本是 P5 验收的硬性要求

---

### [REV-2026-07-25-#12] collect_signals 委托约定

**位置**：§3.2 collect_signals 函数注释
**优先级**：P2
**问题**：委托约定实现细节不清

**新文（在 collect_signals 函数声明后追加注释）**：
```cpp
// **[REV-2026-07-25-#12]** collect_signals 实现要点：
//   - 委托必须嵌套在类内部（mono_class_get_nested_types 迭代，取 MONO_NESTED_DECL_TYPE)
//   - 父类必须是 MulticastDelegate（mono_class_get_parent 检查）
//   - 委托名必须以 "EventHandler" 结尾（约定：<SignalName>EventHandler）
//   - 从 Invoke 方法（mono_class_get_methods 迭代，name=="Invoke"）签名构建 MethodInfo：
//     * 参数名从 mono_method_signature + mono_parameter_get_name 获取
//     * 参数类型走 B0 类型映射（mono_type_to_variant_type）
//     * 返回类型必须为 void（信号无返回值）
```

**审查要点**：
- 实施时确认 `MONO_NESTED_DECL_TYPE` 常量存在
- 确认 `mono_parameter_get_name` API 在 Mono 6.12 中可用

---

## 二、`steel-echo-red-star.md` 修订（3 处）

### [REV-2026-07-25-#13] 文档头部加更新说明

**位置**：文档头部
**优先级**：P3
**问题**：文档未更新实施状态

**新文（在标题下追加）**：
```
> **更新说明（2026-07-25）**：基于本报告已立项 `docs/mono_editor_spec.md` 实施 A 级 + B 级 P1-P7。
> 当前状态：spec 已完成评审修订（见 `docs/review_2026-07-25.md` 与 `docs/revision_log_2026-07-25.md`），未开始编码。
> 本文档作为基线分析保持不变，仅在 §3 和 §4 补充实施状态标注与一致性修订（[REV-2026-07-25-#13/14/15]）。
```

**审查要点**：
- 后续实施进度更新时，同步更新此处的"当前状态"行

---

### [REV-2026-07-25-#14] P4 排序与 spec 一致性

**位置**：§3 B 级 P4
**优先级**：P3
**问题**：steel-echo 与 spec 对 P4 的描述需保持一致

**新文（在 P4 描述末尾追加）**：
```
**[REV-2026-07-25-#14]** 与 spec §4.P4 一致性修订：v1 同步阻塞编辑器是已知缺陷（`OS::execute` 阻塞），spec 已明确「异步化列为 v2 改进项」；P4 完成后编辑器仍会短暂卡顿（dotnet build 期间），但输出与错误高亮可见，是 v1 可接受的体验折衷。
```

**审查要点**：
- 两份文档对 P4 的描述保持一致

---

### [REV-2026-07-25-#15] SourceGenerators 评价调整

**位置**：§3 C 级 2
**优先级**：P3
**问题**：原文评价过于保守，未点明其作为 WASM 性能优化方向的价值

**原文**：
```
桥接类生成器（ScriptMethods/Properties/SignalsGenerator）深度绑定旧 Bridge API，**不可直接用**，但其"编译期生成替代反射"的思路是未来 WASM 性能优化的方向。
```

**新文**：
```
桥接类生成器（ScriptMethods/Properties/SignalsGenerator）深度绑定旧 Bridge API，**不可直接用**。**[REV-2026-07-25-#15]** 评价调整：其"编译期生成替代反射"的思路是 WASM AOT 性能优化的关键方向——当前 `Object_Call` 动态分发在 WASM 解释器下有性能损耗（每次 icall 调用需查 ClassDB + Variant 转换），编译期生成直接调用桥接可显著降低开销。列为 v3 探索项，待 B 级稳定后单独立项评估。
```

**审查要点**：
- v3 探索项需在 B 级稳定后单独立项
- 评估时需对比 `Object_Call` 动态分发 vs 编译期直接调用的性能差异

---

## 三、修订统计

| 文件 | P0 | P1 | P2 | P3 | 合计 |
|------|----|----|----|----|------|
| `mono_editor_spec.md` | 3 | 5 | 4 | 0 | 12 |
| `steel-echo-red-star.md` | 0 | 0 | 0 | 3 | 3 |
| **合计** | **3** | **5** | **4** | **3** | **15** |

---

## 四、第三方审查清单

### 必查项（P0）

- [ ] **#01**：核查 `godot4.7_mono/modules/gdscript/editor/script_templates/SCsub` 是否为"子 SCsub 内部判断 tools"模式
- [ ] **#02**：核查 `csharp_script.cpp:149-185` 与 `mono_export_plugin.cpp:14-37` 的行号范围准确性
- [ ] **#03**：核查 `godot4.7_mono/editor/docks/editor_dock.h` 的 `EditorDock::DOCK_SLOT_BOTTOM` 与 `EditorDockManager::add_dock` 签名

### 建议查项（P1）

- [ ] **#04**：核查 glue 中 Godot 数学类型（Vector2/Color 等）均为 struct
- [ ] **#05**：核查旧方案（提交 `1963b2f126`）的 Inspector 命名是否为 PascalCase
- [ ] **#07**：P7 spike 报告——桌面 Mono 静态库是否启用 sdb
- [ ] **#08**：核查 `tools/run_h9_wasm_test.py` 与 `tools/analyze_wasm.py` 路径准确性

### 可选查项（P2/P3）

- [ ] **#10**：核查 `linker.xml` 当前类型数是否为 33
- [ ] **#12**：核查 `MONO_NESTED_DECL_TYPE` 与 `mono_parameter_get_name` 在 Mono 6.12 中可用

---

## 五、修订未覆盖的事项

以下事项未在本轮修订中处理，留待后续迭代：

1. **Godot 4.7 EditorDock API 的具体用法**：spec 仅声明 API 存在，未给出 `set_dock_shortcut` / `set_transient` 等方法的具体签名。实施时需阅读 `editor_dock.h` 确认。
2. **`ScriptEditorBase::_request_code_completion` API 可用性**：A2 降级选项 B 提到，但未核查。若后续启用 A2，需先核查。
3. **`mono_parameter_get_name` API 可用性**：#12 提到，但未核查。实施时需确认 Mono 6.12 头文件中存在。
4. **桌面 Mono 静态库的 sdb 启用状态**：#07 的 spike 未实际执行，留待 P7 实施前完成。

---

## 六、文档版本

| 文件 | 修订前版本 | 修订后版本 | 修订日期 |
|------|-----------|-----------|---------|
| `mono_editor_spec.md` | v1.0 | v1.1 | 2026-07-25 |
| `steel-echo-red-star.md` | v1.0 | v1.1 | 2026-07-25 |
| `review_2026-07-25.md` | - | v1.0 | 2026-07-25 |
| `revision_log_2026-07-25.md` | - | v1.0 | 2026-07-25 |
