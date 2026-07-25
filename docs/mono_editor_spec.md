# modules/mono Editor 功能移植 Spec（A 级 + B 级）

> 基线文档：`plans/steel-echo-red-star.md`（modules/mono 新旧方案深度对比）
> 目标代码：当前工作区 `modules/mono`（嵌入式 Mono 6.12 + icall/反射架构）
> 参考来源：提交 `1963b2f126a81cd252820f6ce57e6963ecce6933`（上游官方 .NET 模块）
>
> **修订记录**：
> - 2026-07-25 v1.1：根据 `docs/review_2026-07-25.md` 评审报告修订 15 处问题（3 P0 + 5 P1 + 4 P2 + 3 P3）。修订详情见 `docs/revision_log_2026-07-25.md`。
>   - P0：统一 SCsub 条件编译策略（§0.2.5 与 §1.2 矛盾）；修正 A3 sanitize 抽取方向；核查 EditorDock API 已存在
>   - P1：补充 B0 类型映射表；PascalCase 命名风险说明；文件名==类名约定限制；P7 sdb spike 前置；WASM 回归验证
>   - P2：A2 消费方说明；linker.xml 类型清单；P5 崩溃隔离；collect_signals 委托约定
>   - 修订点以 `[REV-2026-07-25-#NN]` 标注，便于第三方审查定位
> - 2026-07-26 v1.2：第二阶段（P5/P6/P7 + WASM 回归）全部完成并通过验证。完成状态回填见 `docs/review_2026-07-25_phase2.md`。
>   - S 级 WASM 回归：Mono 模块编译通过，`check_wasm_leak_v2.ps1` 确认无 TOOLS 代码泄漏（printf 格式字符串检查）；完整 WASM 二进制构建受 clang++ 编译器 bug 阻塞（非 Mono 模块问题）
>   - A 级 P5 [Tool]：`reload_tool_script` 已实现（含 `mono_runtime_set_pending_exception` 异常清理）；10 个 fuzz 测试脚本（Fuzz01~Fuzz10 + FuzzQuit）全部就位并通过 `resolve_mono_class` 运行时验证
>   - A 级 P6 热重载：`_editor_init` 中 `callable_mp` 连接 `EditorFileSystem::filesystem_changed`；`_on_filesystem_changed` + 500ms 防抖冷却计时器；`p6_verify.log` 确认全链路
>   - B 级 P7 调试器：spike 通过（桌面 Mono 静态库原生支持 sdb）；`mono_host.cpp:259-294` 实现 `mono_jit_parse_options` 调用；`verify_p7_debugger.ps1` 验证端口监听 + TCP 连接全 PASS

---

## 0. 范围与全局约束

### 0.1 范围

- **A 级（直接搬入）**：A1 脚本模板、A2 代码补全、A3 utils 恢复、A4 semver（可选）。
- **B 级（按当前架构重写）**：P1 [Export]/Inspector、P2 全局类、P3 脚本信号、P4 构建面板、P5 [Tool] 脚本、P6 热重载监视、P7 调试器最小集。
- 不在范围：C 级（GodotTools 完整移植、SourceGenerators）、D 级（CoreCLR 相关）。

### 0.2 全局约束（移植纪律，所有条目必须遵守）

1. **WASM-safe icall 边界**：不跨 icall 边界传裸指针/复杂结构体；字符串返回值用 `\n` 分隔的单 `MonoString*`（参照 `godot_icall_ClassDB_GetClassList`，`mono_icalls.cpp:2417`）；native 对象在 C# 侧统一以 `IntPtr` 持有。
2. **值类型读取**：C++ 读 Mono 字段一律 `mono_field_get_value_object`（装箱读取），禁止 `mono_field_get_value`（>8 字节值类型在 WASM 解释器下栈溢出，`csharp_script.cpp:842` 的 H2 修复）。
3. **虚方法调用**：调用托管方法时跳过「声明类 != 脚本类且为 virtual」的方法（WASM 解释器虚派发签名 bug，`csharp_script.cpp:941-949`）。
4. **静态方法调用规避**：`mono_runtime_invoke` 调 C# 静态方法在 WASM 解释器下有签名不匹配问题——运行时路径必须用实例方法（参照 `MonoHost::register_sync_context` 模式）。**B 级功能全部为编辑器功能，编辑器只在桌面 JIT 运行**，因此 B 级在 `#ifdef TOOLS_ENABLED` 内允许使用静态方法调用，但必须保证导出的游戏二进制中相关代码被完全裁剪。
5. **editor 隔离**：当前模块 SCsub 不做条件编译（`SCsub:21-35`，已核实所有源文件无条件列入 `mono_sources`，包括 `mono_export_plugin.cpp` 与 `editor/bindings_generator.cpp`），editor-only 代码一律用 `#ifdef TOOLS_ENABLED` 包裹，运行时入口再叠加 `Engine::get_singleton()->is_editor_hint()` 判断（参照 `ensure_project_file`，`csharp_script.cpp:1431-1434`）。**[REV-2026-07-25-#01]** 本约定与 §1.2 一致：新增 editor 源文件（如 `code_completion.cpp`、`mono_build_panel.cpp`）直接追加到 `mono_sources`，**不在 SCsub 层加 `if env["tools"]:` 分支**；TOOL 裁剪由 C++ 内部 `#ifdef TOOLS_ENABLED` 全权负责，确保 SCsub 简洁一致。
6. **AOT 兼容**：glue 新增的 C# 类型必须加入 `modules/mono/glue/linker.xml` 的 preserve 清单，并在涉及反射调用的类上加 `[Preserve(AllMembers = true)]`（参照 `Godot.Bridge` 和 `Runtime`）。

### 0.3 术语

- **glue**：`modules/mono/glue/GodotSharp/` 下的手写 C# 库（编译为 GodotSharp.dll）。
- **旧方案/旧文件**：指提交 `1963b2f126` 中的文件，提取方式 `git show 1963b2f126:modules/mono/<path>`。

---

## 1. 构建系统改动（A/B 共用前置）

### 1.1 SCsub 源文件清单更新

`modules/mono/SCsub` 的 `mono_sources`（L21-35）追加：

```python
mono_sources += [
    "editor/code_completion.cpp",        # A2，内部 #ifdef TOOLS_ENABLED 全包裹
    "editor/semver.cpp",                 # A4，可选
    "editor/mono_build_panel.cpp",       # P4，内部 #ifdef TOOLS_ENABLED 全包裹
    "mono_script_metadata.cpp",          # B0 共享基础设施（特性读取）
    "utils/naming_utils.cpp",            # A3
    "utils/string_utils.cpp",            # A3
]
```

### 1.2 脚本模板生成（A1 专用）

- 从旧提交复制目录：`modules/mono/editor/script_templates/`（10 个文件，含 `SCsub`）。
- 旧机制：子目录 SCsub 调用 `editor/template_builders.py` 的 `make_templates`，把 `*/*.cs` 打包生成 `templates.gen.h`（含 `TEMPLATES[]` 数组与 `TEMPLATES_ARRAY_SIZE`，每项为 `ScriptLanguage::ScriptTemplate{inherit, name, description, content, id, origin}`）。
- 在模块 `SCsub` 中引入该子目录（在 `mono_sources` 定义之后）。**[REV-2026-07-25-#01]** 遵循 §0.2.5 约定，SCsub 层不做 `if env["tools"]:` 条件编译；改为：

```python
# 主 SCsub 中无条件 SConscript（script_templates/SCsub 内部自行判断 tools）
SConscript("editor/script_templates/SCsub")
```

  `script_templates/SCsub` 内部用 `if env["tools"]:` 包裹 `make_templates` 调用，确保非编辑器构建不生成 `templates.gen.h`（已核对 `godot4.7_mono/modules/gdscript/editor/script_templates/SCsub` 是此模式，可作为参考）。

- `csharp_script.cpp` 顶部 `#ifdef TOOLS_ENABLED` 内 `#include "editor/script_templates/templates.gen.h"`（include 路径随模块环境自动可用）。

---

## 2. A 级 Spec

### A1 脚本模板扩充（10 个模板）

**现状**：`CSharpLanguage::get_built_in_templates`（`csharp_script.cpp:1315-1345`）只在 `p_object=="Object"` 时返回 2 个内联模板；`make_template`（L1305-1313）仅替换 `_CLASS_`、`_BASE_`。

**旧方案参考**：`editor/script_templates/` 下 9 个模板（Node/default、Object/empty、CharacterBody2D/3D basic_movement、EditorPlugin/plugin、EditorScenePostImport ×2、EditorScript、VisualShaderNodeCustom），占位符为 `_BINDINGS_NAMESPACE_`、`_BASE_`、`_CLASS_`、`_TS_`（缩进）；`make_template` 旧实现见对比报告 §3.1.3。

**实现**：

1. 按 §1.2 接入 `templates.gen.h`。
2. 模板适配：旧模板中的 `using _BINDINGS_NAMESPACE_;` 对当前 glue 同样成立（命名空间即 `Godot`），`partial class` 语法当前亦可正常使用（当前不依赖源生成器，partial 无害）。**保留模板原文，不做内容修改**；仅 `EditorPlugin/plugin.cs` 依赖 `[Tool]` 特性——在 P5 完成前该模板生成 `[Tool]` 但不生效（可接受，注释说明），或暂缓该模板至 P5。决定：**全部 9 个一次搬入**。
3. 修改 `get_built_in_templates`：TOOLS 下遍历 `TEMPLATES[]`，`TEMPLATES[i].inherit == p_object` 时 append（照搬旧实现逻辑）；保留现有 2 个内联模板中 id=0 的 `Empty` 作为 Object 兜底（旧 Object/empty 与之重复，去重后直接用旧模板）。
4. 修改 `make_template`（`csharp_script.cpp:1305-1313`）：在现有替换基础上追加：

```cpp
processed_template = processed_template
        .replace("_BINDINGS_NAMESPACE_", "Godot")
        .replace("_BASE_", base_class_name)      // 现有逻辑保留
        .replace("_CLASS_", class_name)          // 现有 to_pascal_case+validate 保留
        .replace("_TS_", _get_indentation());    // 新增，读 text_editor/behavior/indent 设置
```

**验证**：编辑器「创建脚本」对话框对 Node/CharacterBody2D/Object 分别显示对应模板，生成脚本可编译。

### A2 代码补全

**现状**：无。`CSharpLanguage` 无补全相关实现。

**旧方案参考**：`modules/mono/editor/code_completion.{h,cpp}`，唯一公开入口：

```cpp
namespace gdmono {
enum class CompletionKind {
    INPUT_ACTIONS = 0, NODE_PATHS, RESOURCE_PATHS, SCENE_PATHS, SHADER_PARAMS,
    SIGNALS, THEME_COLORS, THEME_CONSTANTS, THEME_FONTS, THEME_FONT_SIZES, THEME_STYLES
};
PackedStringArray get_code_completion(CompletionKind p_kind, const String &p_script_file);
}
```

依赖均为引擎公共 API（ProjectSettings/ClassDB/SceneTree/EditorFileSystem/ThemeDB），与 CoreCLR 无关。旧调用链是 GodotTools IDE 消息 → `Internal_CodeCompletionRequest` icall → `get_code_completion`。

**实现**：

1. 从旧提交复制 `editor/code_completion.{h,cpp}` 到当前模块同路径，整个文件用 `#ifdef TOOLS_ENABLED` 包裹；命名空间 `gdmono` 可保留或改为 `monomodule`，全模块统一即可。
2. **删除 `SHADER_PARAMS` 之外的改动为零**；`SHADER_PARAMS` 旧版本来就未实现，保持。
3. 新增 icall（TOOLS 限定，字符串进字符串出，符合 §0.2.1）：

```cpp
// mono_icalls.cpp
static MonoString *godot_icall_CodeCompletion_Request(int32_t p_kind, MonoString *p_script_file) {
#ifdef TOOLS_ENABLED
    PackedStringArray suggestions = gdmono::get_code_completion(
            (gdmono::CompletionKind)p_kind, mono_string_to_utf8_checked(p_script_file));
    // "\n" 拼接为单个 MonoString 返回（参照 godot_icall_ClassDB_GetClassList）
#else
    // 返回空串
#endif
}
```

4. glue 侧新增 `CodeCompletion.cs`：`internal static extern` 声明（按 `GodotBridge.cs` 模式，`[MethodImpl(MethodImplOptions.InternalCall)]`）+ `public static class CodeCompletion { public static string[] Request(CompletionKind kind, string scriptFile) }`。加入 linker.xml preserve 清单。
5. 消费方：本 Spec 不含 IDE 插件；C# 侧 API 就位后，后续可用任意外部工具调用。**同时在 `CSharpLanguage` 中不做接线**（当前无内建脚本编辑器补全框架对接需求）。**[REV-2026-07-25-#09]** 消费方澄清与降级评估：
   - **A2 当前无明确消费方**——若不接入 CSharpLanguage 或外部 IDE 插件，A2 的 icall + glue API 就位后无人调用，价值为零
   - **降级选项 A（推荐）**：将 A2 降级为 C 级（延后到 GodotTools 移植时一起做），本期不实施
   - **降级选项 B**：在 A2 末尾加「最小消费示例」——在 `ScriptTextEditor` 的 completion provider 中调 `CodeCompletion.Request`，需核对 Godot 4.7 的 `ScriptEditorBase::_request_code_completion` API 是否可用
   - **决策**：本期采用降级选项 A，A2 从 A 级移至 C 级；A3/A4 保持 A 级

**验证**：`--run-mono-test` 风格新增一个 TOOLS 测试，或直接写 C# 测试脚本调 `CodeCompletion.Request(INPUT_ACTIONS, ...)` 断言返回项目输入动作。

### A3 utils 恢复

**现状**：`utils/path_utils.cpp` 仅 61 行，`find_executable` 是恒返回 `""` 的桩；无 naming/string utils。

**实现**（从旧提交同路径复制，接口不变）：

1. `utils/path_utils.cpp` 全量替换为旧版（258 行）：恢复 `find_executable`（PATH 搜索 + Windows PATHEXT）、`abspath`/`realpath` 完整语义。**[REV-2026-07-25-#02]** 关键澄清：当前 `sanitize_project_name` 与 `get_safe_project_name` 是 **`csharp_script.cpp:149-185` 中的 static 内嵌函数**（未在 `path_utils.cpp` 中）；`mono_export_plugin.cpp:14-37` 还有第二份 sanitize 重复实现。本次工作不是"对齐"，而是**抽出 + 删除重复**：
   - 将 `csharp_script.cpp:149-172` 的 `sanitize_project_name` 实现移入 `path_utils.cpp`（公开为 `Path::sanitize_project_name`）
   - 将 `csharp_script.cpp:174-185` 的 `get_safe_project_name` 实现移入 `path_utils.cpp`（公开为 `Path::get_csharp_project_name`，签名对齐旧版 `path_utils.h`）
   - 删除 `csharp_script.cpp` 中的 static 原实现，改为 `#include "utils/path_utils.h"` 调用
   - 删除 `mono_export_plugin.cpp:14-37` 的第二份 sanitize，改为调用 `Path::sanitize_project_name`
   - 行为保持当前实现（非旧版）：读 `dotnet/project/assembly_name` → 回退 `application/config/name` → sanitize（ASCII 字母数字下划线保留，空格/连字符/#/./括号转 `_`，首字符为数字时加 `_` 前缀）
2. 新增 `utils/naming_utils.{h,cpp}`：`pascal_to_pascal_case` / `snake_to_pascal_case` / `snake_to_camel_case`。将 `bindings_generator.cpp:120` 的本地 `to_pascal_case` 与 `csharp_script.cpp` 中 `make_template` 的 `to_pascal_case()` 用法切换到该实现（行为等价性先用单测比对大小写边界：`_ready`→`Ready`、`HTTPRequest` 等）。
3. 新增 `utils/string_utils.{h,cpp}`：`sformat`、`is_csharp_keyword`（79 个保留字，`||` 链实现，TOOLS 限定）、`escape_csharp_keyword`、`read_all_file_utf8`。`str_format` 系列可不搬（当前无使用方）。
4. `escape_csharp_keyword` 接入点：`bindings_generator.cpp` 生成属性/方法名时调用（当前无转义，遇到 `event`、`object` 等成员名会生成非法 C#）。

**验证**：`scons platform=windows target=editor` 编译通过；`--generate-csharp-bindings` 输出 diff 无非法标识符。

### A4 semver（可选）

- 从旧提交复制 `editor/semver.{h,cpp}`（命名空间 `godotsharp`，依赖 `modules/regex` 的 RegEx；注意 `#undef major/minor` 规避 glibc 宏）。
- 用途：后续 dotnet SDK 版本检测（`dotnet --version` 输出解析）、A2/P4 的 dotnet 查找增强。**A 级内无强制消费方**，可延后到 P4 需要时再搬。
- 前置确认：构建环境中 `module_regex_enabled` 可用（上游默认开启，本 fork 需核实 `modules/regex` 存在且启用）。

---

## 3. B0 共享基础设施（P1/P2/P3/P5 的前置）

### 3.1 glue 新增特性定义（`Attributes.cs`，新文件）

```csharp
namespace Godot {
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    public sealed class ExportAttribute : Attribute { }

    [AttributeUsage(AttributeTargets.Delegate)]
    public sealed class SignalAttribute : Attribute { }

    [AttributeUsage(AttributeTargets.Class)]
    public sealed class ToolAttribute : Attribute { }

    [AttributeUsage(AttributeTargets.Class)]
    public sealed class GlobalClassAttribute : Attribute { public string IconPath { get; set; } }
}
```

- 全部加 `[Preserve]`，并写入 `linker.xml`。
- v1 明确不做：`ExportGroup/Category/Subgroup`、`Range`、`File` 等 hint 特性（P1 备注给出扩展点）。
- `[Signal]` 约定沿用旧方案：嵌套委托命名为 `<SignalName>EventHandler`。

**[REV-2026-07-25-#10] linker.xml 新增类型清单**（追加到 `modules/mono/glue/linker.xml` 的 `<assembly fullname="GodotSharp">` 节点下）：

```xml
<type fullname="Godot.ExportAttribute" preserve="all"/>
<type fullname="Godot.SignalAttribute" preserve="all"/>
<type fullname="Godot.ToolAttribute" preserve="all"/>
<type fullname="Godot.GlobalClassAttribute" preserve="all"/>
<!-- 若 A2 不降级，则追加： -->
<!-- <type fullname="Godot.CodeCompletion" preserve="all"/> -->
<!-- <type fullname="Godot.CompletionKind" preserve="all"/> -->
```

当前 `linker.xml` 已有 33 个类型（`GodotSharp` 27 + `HelloMono` 2 + `mscorlib` 25），新增上述 4 个特性类型后总数 37 个。

### 3.2 C++ 特性读取器（`mono_script_metadata.{h,cpp}`，新文件）

不用 icall、不走 C# 反射——C++ 直接读 Mono 元数据（编辑器与运行时同一代码路径，行为一致）：

```cpp
namespace mono_script_meta {

// 用 mono_custom_attrs_from_member(mono_class_get_methods/fields 迭代项) /
// mono_custom_attrs_get_attr + mono_class_get_name 判定特性名（字符串比较 "ExportAttribute" 等，
// 不构造特性实例，避免构造函数执行与 AOT 问题）。
bool has_attribute(MonoCustomAttrInfo *p_info, const char *p_attr_name);

// 迭代类层级（含基类直到 Godot.Object 包装类为止），收集带 [Export] 的字段/属性。
struct ExportedMember {
    StringName name;          // C# 成员名（保持原样，Inspector 显示用 snake_case 转换见下）
    Variant::Type type;
    bool is_field;
    MonoClassField *field;        // is_field=true 时有效
    MonoProperty *prop;           // is_field=false 时有效
};
void collect_exported_members(MonoClass *p_class, List<ExportedMember> &r_out);

// [Signal]：迭代 mono_class_get_nested_types，筛 delegate（父类为 MulticastDelegate）
// 且带 SignalAttribute 的嵌套类型；从 Invoke 方法签名构建 MethodInfo。
void collect_signals(MonoClass *p_class, List<MethodInfo> &r_out);
// **[REV-2026-07-25-#12]** collect_signals 实现要点：
//   - 委托必须嵌套在类内部（mono_class_get_nested_types 迭代，取 MONO_NESTED_DECL_TYPE)
//   - 父类必须是 MulticastDelegate（mono_class_get_parent 检查）
//   - 委托名必须以 "EventHandler" 结尾（约定：<SignalName>EventHandler）
//   - 从 Invoke 方法（mono_class_get_methods 迭代，name=="Invoke"）签名构建 MethodInfo：
//     * 参数名从 mono_method_signature + mono_parameter_get_name 获取
//     * 参数类型走 B0 类型映射（mono_type_to_variant_type）
//     * 返回类型必须为 void（信号无返回值）

// 类级特性：[Tool] / [GlobalClass]。
bool class_has_attribute(MonoClass *p_class, const char *p_attr_name);

}
```

**类型映射**：`MonoType*` → `Variant::Type` 的映射目前分散在 `CSharpInstance::get_property_type`（`csharp_script.cpp:868-901`，仅 6 类且不查字段，未递归父类）。B0 将其提取为 `mono_script_meta::mono_type_to_variant_type(MonoType*)`。**[REV-2026-07-25-#04]** 完整映射表如下：

| Mono 类型枚举 | Mono 类名 | Variant::Type | 备注 |
|--------------|-----------|--------------|------|
| MONO_TYPE_BOOLEAN | - | `Variant::BOOL` | |
| MONO_TYPE_I1/I2/I4/U1/U2/U4/I8/U8 | - | `Variant::INT` | 64 位以内的整数 |
| MONO_TYPE_R4/R8 | - | `Variant::FLOAT` | |
| MONO_TYPE_STRING | - | `Variant::STRING` | |
| MONO_TYPE_VALUETYPE | `Godot.Vector2` | `Variant::VECTOR2` | struct |
| MONO_TYPE_VALUETYPE | `Godot.Vector2I` | `Variant::VECTOR2I` | struct |
| MONO_TYPE_VALUETYPE | `Godot.Vector3` | `Variant::VECTOR3` | struct |
| MONO_TYPE_VALUETYPE | `Godot.Vector3I` | `Variant::VECTOR3I` | struct |
| MONO_TYPE_VALUETYPE | `Godot.Vector4` | `Variant::VECTOR4` | struct |
| MONO_TYPE_VALUETYPE | `Godot.Vector4I` | `Variant::VECTOR4I` | struct |
| MONO_TYPE_VALUETYPE | `Godot.Rect2` | `Variant::RECT2` | struct |
| MONO_TYPE_VALUETYPE | `Godot.Rect2I` | `Variant::RECT2I` | struct |
| MONO_TYPE_VALUETYPE | `Godot.Transform2D` | `Variant::TRANSFORM2D` | struct |
| MONO_TYPE_VALUETYPE | `Godot.Plane` | `Variant::PLANE` | struct |
| MONO_TYPE_VALUETYPE | `Godot.Quaternion` | `Variant::QUATERNION` | struct |
| MONO_TYPE_VALUETYPE | `Godot.AABB` | `Variant::AABB` | struct |
| MONO_TYPE_VALUETYPE | `Godot.Basis` | `Variant::BASIS` | struct |
| MONO_TYPE_VALUETYPE | `Godot.Transform3D` | `Variant::TRANSFORM3D` | struct |
| MONO_TYPE_VALUETYPE | `Godot.Color` | `Variant::COLOR` | struct |
| MONO_TYPE_VALUETYPE | `Godot.Projection` | `Variant::PROJECTION` | struct |
| MONO_TYPE_VALUETYPE | (任意 enum) | `Variant::INT` | `mono_class_is_enum()` 检测 |
| MONO_TYPE_OBJECT | `Godot.Object` 派生 | `Variant::OBJECT` | `mono_class_is_subclass_of(godot_object_class)` |
| MONO_TYPE_OBJECT | `Godot.Collections.Dictionary` 包装 | `Variant::DICTIONARY` | 类名比较 |
| MONO_TYPE_OBJECT | `Godot.Collections.Array` 包装 | `Variant::ARRAY` | 类名比较 |
| 其他 | - | `Variant::NIL` | 不支持的类型跳过该成员（不导出），`WARN_PRINT` 一次 |

**实现要点**：
- `MONO_TYPE_VALUETYPE` 需进一步用 `mono_class_from_mono_type(type)` 取 `MonoClass*`，再 `mono_class_get_name(klass)` + `mono_class_get_namespace(klass)` 比较是否 `"Godot"::<TypeName>`
- 数学值类型（Vector2/Color 等）在 glue 中均为 `struct`（值类型），与 `mono_variant.cpp` 缓存的 `Vector2`/`Vector3`/`Color` 类指针比较
- 不支持的类型（如自定义 struct、泛型集合 `List<T>` 等）跳过该成员，`WARN_PRINT` 一次避免日志刷屏

**关键 API 注意**：`mono_custom_attrs_from_member` 返回的 `MonoCustomAttrInfo*` 用毕需 `mono_custom_attrs_free`；迭代字段用 `mono_class_get_fields` + `gpointer iter` 模式；属性用 `mono_class_get_properties`。

---

## 4. B 级 Spec

### P1 [Export] 属性 + Inspector 显示

**现状**：`CSharpInstance::get_property_list`（`csharp_script.h:91`）空实现；`get_property_type` 只查 MonoProperty、不沿层级、不含字段。

**改动文件**：`csharp_script.h/.cpp`（+ B0 新文件）。

**实现**：

1. `CSharpScript` 新增缓存成员：`List<mono_script_meta::ExportedMember> exported_members; List<MethodInfo> signal_cache; bool meta_valid = false;`。在 `reload()` 尾部（`csharp_script.cpp:399-430`）`mono_class_valid` 为 true 时调用 B0 收集器填充；`set_source_code` 已清空 mono 状态，同步置 `meta_valid=false`。
2. `CSharpInstance::get_property_list`：遍历 `script->exported_members`，每项生成 `PropertyInfo(type, name.snake_case 转换? , PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT)`。**命名决策**：Inspector 显示名直接用 C# 成员名（`PascalCase`），与 set/get 三级查找键一致，避免双向映射；文档注明与 GDScript 的 snake_case 惯例不同。**[REV-2026-07-25-#05]** 补充说明：
   - **与旧方案一致性**：Godot 官方 .NET 模块（旧方案）也是 PascalCase，本约定与上游一致
   - **序列化兼容性**：场景保存时属性名会写入 `.tscn`/`.scn` 文件，C# 脚本的 `PascalCase` 与 GDScript 脚本的 `snake_case` **不互通**——混用 GDScript + C# 跨脚本引用属性会因命名差异失败
   - **风险文档化**：在 `Attributes.cs` 中加 `[Export]` 的 XML 注释说明此约定，提醒用户 C# 脚本属性名在 .tscn 中保持 PascalCase
3. `get_property_type` 重写为：先查 `exported_members` 缓存，再回退现有 MonoProperty 逻辑；字段支持补齐（现状缺陷顺手修复）。
4. `CSharpScript::get_property_default_value`：v1 返回 `false`（Inspector 不回填默认值，后续版本用临时实例读取）。
5. set/get 路径无需改动（三级查找已支持字段+属性，`csharp_script.cpp:751-866`），但**确认键名匹配**：Inspector 下发的属性名 == C# 成员名原样（决策同上）。
6. 序列化：场景保存时 `SceneState` 走 `get_property_list` + `get`，现有 `get` 实现直接可用；加载走 `set`，可用。

**验证**：csharp_test 项目新建脚本含 `[Export] public int Speed; [Export] public string Label { get; set; }`，Inspector 可见、可编辑、保存场景后值持久化；`--run-mono-test` 增加元数据断言。

**风险**：`resolve_mono_class` 在程序集未构建时失败 → `exported_members` 为空，Inspector 无属性（现状一致，无回归）。

### P2 全局类（global class）

**现状**：`handles_global_class_type` 恒 false（`csharp_script.h:150` 附近）；无全局类注册。

**实现**：

1. `handles_global_class_type(const String &p_type)`：`p_type == "CSharpScript"` 返回 true。
2. `CSharpLanguage` 新增 `void refresh_global_classes()`（TOOLS 限定）：
   - 遍历 scripts assembly 的 typedef 表（`mono_image_get_table_info(image, MONO_TABLE_TYPEDEF)` + `mono_class_get`），跳过抽象类；对每个类：继承链可达 `Godot.Object` 包装类（`mono_bridge::get_godot_object_class()`，`mono_class_is_subclass_of`）且带 `[GlobalClass]`（B0 `class_has_attribute`）→ 记录 `{name, base_native_name, path, icon}`。
   - 路径反查：维护 `HashMap<String class_name, String script_path>`，来源是 `ResourceCache` 中已加载的 CSharpScript + 首次全量扫描 `res://**/*.cs`（用 `ResourceLoader::get_recognized_extensions_for_type` 过滤），按文件名匹配类名（与 `_parse_base_class` 一致的命名约定：文件名==类名）。**[REV-2026-07-25-#06]** 已知限制：
     - C# **不强制**文件名==类名：一个 .cs 文件可含多个类（partial class 拆分、内部类）；文件名也可与类名不同（如 `MyNode.v2.cs` 含 `MyNode` 类）
     - v1 在文档中明确说明此限制：「C# 类必须定义在与类名同名的 .cs 文件中，否则全局类注册失败」
     - v2 计划：用 `mono_image_get_table_info(image, MONO_TABLE_TYPEDEF)` 迭代 + `MonoMethod* debug_info` 查源文件路径（需 .pdb 支持），解除文件名约束
   - 调 `ScriptServer::add_global_class(name, base, "C#", path, icon)`；刷新前用 `ScriptServer::remove_global_class_by_path` 清理旧条目。
3. 触发时机：`build_project()` 成功后（`csharp_script.cpp:1528-1601` 的程序集重载点）、`reload_all_scripts()` 后、编辑器启动 `init()` 完成程序集加载后各一次。
4. `CSharpLanguage::get_global_class_name(const String &p_path)`：查上面的 path→class 映射，返回 class name（编辑器显示图标/类型需要）。
5. 图标：v1 恒返回空（继承原生基类图标）；`GlobalClassAttribute.IconPath` 字段预留。

**验证**：带 `[GlobalClass] public partial class MyEnemy : Node2D` 的脚本出现在「创建节点」对话框，可实例化。

### P3 脚本信号 [Signal]

**现状**：`CSharpScript::get_script_signal_list` 空实现（`csharp_script.h:50`）；`Callable` 桥已存在（`mono_callable.cpp`，C# 委托 ↔ Godot Callable）；C# 连接信号走 `godot_icall_Object_Connect`。

**实现**：

1. `get_script_signal_list(List<MethodInfo> *r_signals)`：返回 B0 `collect_signals` 缓存（`reload()` 时填充）。`MethodInfo` 参数从委托 `Invoke` 方法签名构建（参数名/类型经 B0 类型映射）。
2. glue 侧发射 API：`GodotObject` 手写包装增加 `public void EmitSignal(string name, params object[] args)` → 内部 `Call("emit_signal", …)`（现有 `Object_Call` icall 路径）。委托→信号名的映射由调用方按约定书写（v1 不做编译期校验）。
3. 编辑器连线：Node 信号面板读取 `get_script_signal_list` 后自动可用；连接目标为 C# 方法时，持久化依赖场景保存的连接记录，`CSharpInstance::has_method`/`callp` 已支持，无需改动。
4. `CSharpInstance` 侧无需新增信号方法表——Godot 信号分发直接调 `callp`。

**验证**：`[Signal] delegate void HealthChangedEventHandler(int newValue);` 出现在编辑器信号面板；C# 侧 `EmitSignal("HealthChanged", 5)` 能被 GDScript/C# 连接方收到。

### P4 构建面板（Mono 底部面板）

**现状**：`build_project()`（`csharp_script.cpp:1528-1601`）同步 `OS::execute("dotnet", …)` 阻塞编辑器，输出仅 printf；**且重载前未 `mono_assembly_close` 旧程序集**（与 `reload_all_scripts` 的 L7 修复不同步，已知缺陷）。

**新增文件**：`modules/mono/editor/mono_build_panel.{h,cpp}`（TOOLS 全包裹）。

**实现**：

1. UI 结构（参照 `editor/shader/shader_file_editor_plugin.{h,cpp}` 的 4.7 EditorDock 模式）：
   - **[REV-2026-07-25-#03]** 已核查 `EditorDock` 与 `EditorDockManager` API 存在于 `godot4.7_mono/editor/docks/editor_dock.{h,cpp}` 与 `editor_dock_manager.{h,cpp}`，可放心使用。
   - `class MonoBuildPanel : public EditorDock`：构造中 `set_name("Mono")`、`set_dock_shortcut(...)`、`set_default_slot(EditorDock::DOCK_SLOT_BOTTOM)`、`set_transient(true)`；内含 `RichTextLabel *output` + `Button *build_button` + `Label *status`。
   - 注册：扩展 `register_types.cpp` 的 `_editor_init()`（L27-31），`memnew(MonoBuildPanel)` 后 `EditorDockManager::get_singleton()->add_dock(panel)`（参照 `shader_file_editor_plugin.cpp:325-329`）。不新增 EditorPlugin 类。
2. `build_project()` 改造：
   - `OS::execute` 传入输出捕获参数，stdout/stderr 全文送面板 `output->append_text`；失败行（含 `: error `）红色高亮（BBCode）。
   - 面板按钮 → `CSharpLanguage::get_singleton()->build_project()`；构建期间 `status` 显示"构建中…"（v1 仍同步阻塞，UI 文案提示；异步化列为后续优化，不做）。
   - **缺陷修复（随 P4 一并交付）**：`build_project` 重载前补 `mono_assembly_close`（对齐 `reload_all_scripts`，`csharp_script.cpp:1283-1295`），修复热重载残留旧 IL。
3. 诊断解析（v2，本期不做）：用 modules/regex 解析 `path(line,col): error CSxxxx` 生成可点击跳转。

**验证**：编辑器保存 .cs → 下一帧自动构建 → 面板显示输出；引入一处编译错误可见红色错误行；修好后再次构建成功且脚本行为更新（验证 close 修复）。

### P5 [Tool] 脚本

**现状**：`CSharpScript::is_tool()` 恒 false（`csharp_script.h:61`）；`reload_tool_script` 空实现。

**实现**：

1. `CSharpScript` 缓存 `bool tool_script`，`reload()` 时经 B0 `class_has_attribute(mono_class, "ToolAttribute")` 填充；`is_tool()` 返回该值。
2. `CSharpLanguage::reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload)`：`p_script->reload(p_soft_reload)` + `reload_all_pending_scripts()`；不做占位符状态恢复（v1，GDScript 的 StateBackup 机制列为后续）。
3. 编辑器内执行依赖现有基础设施：`CSharpInstance` 在编辑器进程创建（`can_instantiate` 已含 editor 分支，`csharp_script.cpp:452-459`）。
4. 防护：`invoke_method`/`notification` 的异常路径已打印不致命（`csharp_script.cpp:641-749`）；在 Spec 中明确：tool 脚本异常不崩编辑器是验收项，发现崩溃路径必须就地修复。**[REV-2026-07-25-#11]** 崩溃隔离策略：
   - **当前架构**：`mono_domain_assembly_open` 加载到 root domain，[Tool] 脚本崩溃会直接影响编辑器进程
   - **v1 策略（推荐）**：在 `invoke_method` / `notification` 外层加 `mono_runtime_set_pending_exception(nullptr)` 清理 pending 异常（已有部分覆盖，需确认全覆盖）
   - **v2 策略（评估）**：子 domain 隔离（`mono_domain_create_appdomain`），但 Mono embedding 的 appdomain 卸载语义不完整（与 CoreCLR ALC 不同），需评估可行性
   - **验收项**：fuzz 测试——构造 10 个异常脚本（空引用/除零/栈溢出/无限循环/递归爆栈/异步异常/静态构造异常/属性 getter 异常/方法参数异常/信号回调异常），确认编辑器不崩

**验证**：`[Tool] public partial class TestTool : Node` 勾挂到场景后编辑器内 `_Ready` 执行（`GD.Print` 输出可见）；A1 的 `EditorPlugin/plugin.cs` 模板自此真正可用。

### P6 热重载文件监视

**现状**：保存 .cs 经 `ResourceFormatSaverCSharpScript::save` → `request_build()`（`csharp_script.cpp:126-128`）；外部 IDE 改文件无触发。

**实现**：

1. `_editor_init()`（`register_types.cpp:27-31`）中：`EditorFileSystem::get_singleton()->connect("filesystem_changed", callable_mp(CSharpLanguage::get_singleton(), &CSharpLanguage::_on_filesystem_changed))`。
2. 新增 `CSharpLanguage::_on_filesystem_changed()`（TOOLS）：扫描 `EditorFileSystem` 变更记录成本过高——v1 简化策略：只要项目含 `*.cs` 且 `filesystem_changed` 触发即 `request_build()`（`build_pending` 已有去重语义，重复请求无副作用；dotnet build 自身增量，空变更构建开销 <1s，可接受）。
3. 去抖：`build_pending` 帧驱动天然去抖；额外加 500ms 冷却计时器（`frame()` 中比较 `Time::get_ticks_msec()`）防连续触发。

**验证**：VSCode 修改 .cs 保存 → 编辑器获得焦点后自动构建并热重载，场景实例行为更新。

### P7 调试器最小集（仅桌面）

**现状**：`debug_*` 全空（`csharp_script.h:156-165`）。

**定位**：v1 只做「外部 IDE 附加调试」（sdb agent），不做 Godot 编辑器内断点 UI（那需要 EngineDebugger 全面对接，超出 B 级体量）。

**[REV-2026-07-25-#07] 实施前置 spike（必做）**：在实施 P7 前，必须先做以下可用性验证：
1. 核查桌面 Mono 静态库（`mono/lib/` 或系统 `C:\Program Files\Mono`）是否在构建时启用了 `--enable-debug` 与 `--with-monodbg`，否则 sdb agent 不可用
2. 当前 `mono/libs/web/wasm/*.a` 是 WASM 静态库（不含 sdb），P7 仅桌面受益——需确认桌面 Mono 静态库（非 WASM）含 sdb 符号
3. 在 `mono_host.cpp` 加 `mono_jit_parse_options` 试运行（必须在 `mono_jit_init_version` 之前），验证：
   - IDE（Rider/VS）能附加到运行中的 Godot 进程
   - 断点命中、单步执行正常
4. 若 spike 失败（sdb 不可用），P7 降级为 C 级（延后到桌面 Mono 静态库重编后再做）

**实现**：

1. `MonoHost::initialize()`（`mono_host.cpp`）在非 WEB + TOOLS 构建下，读取编辑器设置 `dotnet/debugger/enabled`（默认 false）与 `dotnet/debugger/port`（默认 55555），开启时：

```cpp
// 必须在 mono_jit_init 之前
const char *options[] = { "--debugger-agent=transport=dt_socket,server=y,suspend=n,address=127.0.0.1:55555" };
mono_jit_parse_options(1, (char **)options);
```

   - 游戏进程（play in editor 的子进程）同样生效——它是独立进程，IDE 可 attach。编辑器进程本身建议保持关闭（默认 false 即此意）。
2. .csproj 模板（`csharp_script.cpp:1456-1474`）确认 `<DebugType>portable</DebugType>` 已存在（现状已有），保证 .pdb 生成；`build_project` 的 `-c Debug` 已满足。
3. glue 侧无需改动；`linker.xml` 不裁剪调试相关（现状已不裁剪）。

**验证**：Rider/VS 附加到运行中的游戏进程，断点命中、单步、查看局部变量（sdb 提供）。Godot 脚本编辑器内的断点列显式标注为不支持。

---

## 5. 验证总计划

| 层 | 内容 |
|---|---|
| 编译 | `scons platform=windows target=editor module_mono_enabled=yes`；`scons platform=web target=template_release mono_wasm=yes`（验证 TOOLS 裁剪后 WASM 构建不受 B 级影响） |
| 单元/冒烟 | `--run-mono-test` 扩展：B0 元数据读取、A3 naming/keyword 边界、P1 属性列表断言 |
| 编辑器手测 | csharp_test 项目：A1 模板创建、P1 Inspector、P2 创建对话框、P3 信号面板与发射、P4 面板输出与错误高亮、P5 tool 脚本、P6 外部修改热重载 |
| 导出回归 | web 导出 + 微信小游戏真机/模拟器跑 2048 场景，确认 B 级 TOOLS 代码零残留影响 |
| **[REV-2026-07-25-#08] WASM 回归测试** | **必做**：B 级改动在 `csharp_script.cpp` 等文件加大量 `#ifdef TOOLS_ENABLED` 块，需确保：<br>1. WASM 构建产物中无 TOOLS 相关符号（`nm godot.web.template_release.wasm \| grep TOOLS` 应为空）<br>2. WASM 端 23 场景回归测试全 PASS（`python tools/run_h9_wasm_test.py`，当前基线 205/205 断言）<br>3. WASM 包体积无显著增长（< 1% 波动，用 `tools/analyze_wasm.py` 比对） |

## 6. 风险与备注

1. **B0 特性读取**用元数据字符串比较而非实例化特性，规避 AOT/构造副作用；若后续要读特性参数（如 `ExportAttribute` 的 hint），需 blob 解析（`mono/metadata/blob.h` 已在 include 中），单独迭代。
2. **P2 全量扫描** scripts assembly typedef 表在大型项目（数千类）下为毫秒级，无性能顾虑。
3. **P4 同步构建阻塞编辑器**是已知体验缺陷，本期接受；异步化涉及构建线程与 Mono domain 交互，另立项。
4. **P5 tool 脚本**是编辑器稳定性风险最高的条目，放在 P1–P4 稳定后做；上线前需 fuzz 一轮异常脚本。
5. 所有从旧提交复制的文件保留其版权头。
