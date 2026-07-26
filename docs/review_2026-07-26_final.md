# Mono Editor 实现全面代码审查报告（第四阶段·终审）

> **审查日期**：2026-07-26
> **审查范围**：A 级 + B0 + B 级 P1–P7 全部实现代码（`029d405e9d~1..HEAD`，约 +1384 行）+ docs/ 全部 6 份文档
> **审查方法**：6 个独立分区并行审查（B0 元数据读取器 / csharp_script 主改动 / P4 面板+register_types / P7+TOOLS 跨切面 / A 级条目 / 文档-代码一致性），全部结论基于实际代码核对；两个 P0 由主审查员亲自复核确认
> **审查结论**：**2 个 P0 必须立即修复**（热重载 UAF、非 TOOLS 构建链接失败），10 个 P1，若干 P2 优化项。前三阶段评审的"验证通过"声称部分无法追溯（详见 §3）。

---

## 一、P0 级问题（必修，已亲自复核确认）

### P0-1 `mono_assembly_close` 后全链路 use-after-free —— 热重载即崩溃源

**位置**：`modules/mono/csharp_script.cpp:1625`（`reload_all_scripts`）、`:1992`（`build_project`）

`mono_assembly_close` 在 root domain 直接释放 scripts assembly 的 image（Mono 6.12 不检查 GC 堆中存活对象）。close 之后四类引用全部悬空，代码中无任何清理：

1. **`CSharpScript::mono_class` / `method_cache`**：`reload_all_pending_scripts()`（`:1464-1480`）的过滤条件是 `!mono_class_valid || !mono_class`——close 前成功 resolve 的脚本两个条件都不满足，**全部被跳过**，继续持有已释放的 `MonoClass*`。后续 `has_method()`/`get_method()`/`can_instantiate()` → 崩溃。`reload_all_scripts()`（`:1617-1629`）close+重开后甚至没有调用 `reload_all_pending_scripts()`。
2. **`CSharpInstance::mono_object`**：托管对象仍在 sgen 堆中，`vtable->klass` 指向已释放元数据。任何 `mono_object_get_class()`（find_method/set/get）= UAF；更致命的是**下一次 sgen GC 扫描堆时读 vtable 本身就会崩**，与 C++ 侧是否触碰无关。仓库中已有多份无法符号化的崩溃转储（`csharp_test/p2_typedef_test.err` 等 6 个 `.err`，HEAD 提交信息亦承认 "add crash dump"），与该崩溃模式吻合。
3. **`mono_gc_bridge` 绑定表**：close 后 gchandle 仍钉住旧对象；新建 CSharpInstance 走"复用已绑定托管对象"分支会把旧 image 的对象复活给新脚本。
4. **`method_cache` 中的 `MonoMethod*`**：随 image 一并释放。

**修复建议（方案 A，推荐）**：**永不 close**。热重载时把新 dll 复制为带版本号的临时路径（如 `ProjectScripts.rev{N}.dll`）再 `mono_domain_assembly_open`，绕开 Mono 按路径缓存旧 image 的机制（即 L7 注释描述的问题），旧 image 泄漏但安全——这是旧 Godot mono 与多数 embedding 项目的做法。方案 B（close 前全量失效脚本/实例/gc bridge）只能降低概率不能根除（sgen 堆残留对象仍在下次 GC 爆炸）。

### P0-2 非 TOOLS 构建（export template / WASM）链接失败

**位置**：`modules/mono/register_types.cpp:118-120`

`MODULE_INITIALIZATION_LEVEL_SCENE` 分支**无条件**调用 `BindingsGenerator::handle_cmdline_args(...)`，但 `editor/bindings_generator.cpp` 本次被整体 `#ifdef TOOLS_ENABLED` 包裹（提交 `69b702ad6c` 引入），非 TOOLS 构建下该翻译单元为空 → undefined symbol，链接失败。`bindings_generator.h:26` 的声明无守卫，所以编译能过、链接才炸，WASM 恰好中招。

**修复**：调用点加 `#ifdef TOOLS_ENABLED`；同时给头文件声明加同样守卫，让此类问题暴露在编译期。

---

## 二、P1 级问题（功能错误，10 项）

### 代码（8 项）

1. **GodotSharp.dll 加载失败时空指针解引用崩溃** — `mono_script_metadata.cpp:172-174, 199-201`：`load_godotsharp()` 失败是非致命的（host 照常初始化），此时 `get_godotsharp_assembly()` 返回 nullptr，`mono_assembly_get_image(nullptr)` 直接崩。修复：两处判空，members 返回空即可。
2. **不支持类型的 `[Export]` 成员未跳过** — `mono_script_metadata.cpp:240-249, 286-296` + `csharp_script.cpp:551-558`：类型映射返回 `Variant::NIL` 的成员仍进入属性列表（带 STORAGE|EDITOR），自定义 struct/数组等在 Inspector 产生坏条目。修复：`vtype == Variant::NIL` 时 `continue`。
3. **`collect_signals` 不遍历基类** — `mono_script_metadata.cpp:310-384`：只扫当前类嵌套类型，继承的 C# 基类声明的信号不出现在信号面板。修复：与 exported members 一样沿 `mono_class_get_parent` 向上走（按信号名去重）。
4. **`mono_runtime_set_pending_exception(nullptr, false)` 是空操作** — `csharp_script.cpp:942, 1653`：Mono 6.12 语义为"无 pending 或 overwrite==true 才写入"，传 `(nullptr, false)` 在恰恰需要清理的场景下直接返回、不清除。REV-#11 验收实际未达成。修复：改 `(nullptr, true)`。
5. **`reload_tool_script` 不触发构建，热重载对新代码无实际效果** — `csharp_script.cpp:1639-1655`：`reload()` 只在已加载的旧程序集中重新解析同名类，不经过 `dotnet build` 永远不会生效。修复：先 `request_build()` 走 frame() 消费路径，或文档明示该入口仅重建缓存。
6. **构建面板转义错误，输出显示乱码** — `mono_build_panel.cpp:70`：`line.xml_escape(true)` + `add_text()`。已核实 `add_text` 不解析 BBCode 也不解码实体，转义后的 `&lt;` 等原样显示（C# 错误输出大量含 `<>`/`'`）。修复：删掉 `xml_escape`，直接 `add_text(line)`。
7. **程序集名解析三处不一致，默认配置下导出链路断裂** — `path_utils.cpp:264-273`（assembly_name → app name → sanitize）vs `mono_export_plugin.cpp:19-21`（只读 assembly_name，无回退）vs `mono_host.cpp:149-151`（回退**硬编码 `"CSharpTest"`**）。默认项目未设置 `dotnet/project/assembly_name` 时三方错位，导出的游戏加载不到任何脚本；现有测试全部通过仅因 `csharp_test/project.godot:19` 手工设置了该值。修复：三处统一为 `Path::get_csharp_project_name()`。
8. **9 个脚本模板中 6 个无法用当前 glue 编译** — EditorPlugin/EditorScript/EditorScenePostImport 类在 glue 中**根本不存在**；VisualShaderNodeCustom/CharacterBody2D/3D 模板引用了 glue 缺失的 API（`Input.IsActionJustPressed`、`Mathf`、`Node3D.Transform`、泛型 `Array<T>`、嵌套枚举）。仅 `Node/default.cs` 与 `Object/empty.cs` 可编译。决策项：收窄模板集到 2 个，或补齐 glue。

### 文档（2 项）

9. **spec §1.1 仍列 `editor/code_completion.cpp`** — REV#09 已将 A2 降级不实施，revision_log 明确"计划从清单移除"但从未执行；SCsub 实际无此行。spec 自相矛盾。
10. **spec §4.P7 配置来源与实现不符** — spec 写"编辑器设置"，实现用 `ProjectSettings`（`mono_host.cpp:272-274`）。语义不同（ProjectSettings 进 project.godot 和版本库）；因代码块整体 TOOLS+非 WEB 守卫，无实际泄漏后果，但 spec 必须修正或代码改读 EditorSettings。

---

## 三、验证声称的可信度问题

- **`verify_p7_debugger.ps1` 从未存在**：spec v1.2 修订记录、phase2 评审两处声称"全 PASS"，但全仓库及 git 历史均无此脚本（只有 `csharp_test/p7_verify.err` 日志文件）。P7 代码本身经与 Mono 上游源码交叉核对**是正确的**（调用顺序、字符串生命周期、端口校验、守卫均无误），但"验证已通过"无脚本级证据。建议补交脚本或修正措辞。
- **A3 单元测试真实存在且数量吻合**：`modules/mono/tests/test_naming_utils.h`（10 用例/35 断言）+ `test_string_utils.h`（14 用例/82 断言）= 24 用例/117 断言，与文档一致；但"`scons tests=yes` 构建并运行通过"未被任何审查实际执行过。
- 其余验证产物（`p6_verify.log`、fuzz 10+1 脚本、`run_fuzz_test.ps1`、`run_p2_typedef_test.ps1`）均实际存在。

---

## 四、P2 级问题（优化项/整洁性，按主题分组）

### 调试残留（应清理后提交）

- `register_types.cpp:38-45, 61-67, 72-84`：`_editor_init` 每次编辑器启动向 CWD 写两个 `.marker` 文件（用哪个项目打开就污染哪个目录；`csharp_test/csharp_test_p6_connect_result.marker` 已被误提交入库）；`:108-113` 的 `#else` 分支 printf 会出现在**所有导出模板（含 WASM）**的 stdout。
- `csharp_script.cpp:1576, 1595-1612`：P6 路径每次 filesystem_changed 刷 4 行 printf。建议全部删除或改 `print_verbose`。

### 死代码与纪律

- `editor/semver.{h,cpp}`：已入 SCsub 但**零消费方**（SCsub 注释声称"P4 消费"不实），且全文无 `TOOLS_ENABLED` 包裹——死代码进入 WASM/模板构建。
- `utils/naming_utils.{h,cpp}`：零消费方，头文件注释声称被 bindings_generator/make_template 使用**不实**（bindings_generator.cpp:103-107 明确注释"故意不替换"）。同样无 TOOLS 包裹。
- `mono_script_metadata.cpp:126-141`：7 个数学类型映射分支在当前 glue 中无对应类型（死分支）；且 `"AABB"` 与 `mono_variant.cpp:92` 的 `"Aabb"` 命名不一致。

### 功能完善（非阻塞）

- 导出的枚举成员缺 `PROPERTY_HINT_ENUM`（Inspector 显示数字框而非下拉）。
- 类型映射缺 `MONO_TYPE_SZARRAY`（`byte[]`→PackedByteArray 等）与 `MONO_TYPE_GENERICINST`。
- `[Export]` get-only 属性被收录（set 必失败）应跳过或告警；特性名比较未校验 `Godot` 命名空间（用户自定义同名特性会误判）。
- 信号参数名是占位符 `argN`——正确 API 是 `mono_method_get_param_names`（不是 spec 写的 `mono_parameter_get_name`）。
- 信号名未做 PascalCase→snake_case（与官方 Godot C# 约定和跨语言连接不一致）。
- `get_property_list` 缺 `PROPERTY_USAGE_SCRIPT_VARIABLE`（Inspector 脚本变量分组/本地化）。
- `resolve_mono_class` 直达路径（`instance_create`）失败时不清 `exported_members`/`signal_cache` 等缓存（`csharp_script.cpp:463-467`）。
- `get_global_class_name` cache-valid-miss 直接返回空——源码新增 `[GlobalClass]` 未 build 时新类长期从 Add Node 消失；建议 miss 时回退文本扫描。
- `get_property_default_value` 返回类型零值而非 C# 字段初始值（Inspector「还原默认值」行为偏差，建议注释注明）。
- P6：500ms 冷却丢尾沿；任何文件（非 .cs）变更都触发 `dotnet build`。
- 面板：构建按钮无进行中守卫（阻塞结束后排队点击会触发重复构建）；失败路径不清空输出（重复失败时无限累积）。
- `make_template` 对类名多做一次 `to_pascal_case`（`csharp_script.cpp:1668`），偏离上游，用户输入 `myNode` 被静默改写为 `Mynode`。
- `mono_host.cpp:279`：`atoi` 解析环境变量端口，溢出为 UB；建议 `strtol` + endptr。
- sdb 协议无认证：开启期间任何本地进程可 attach 执行代码，建议在文档中明示。

### 文档回填（一次性处理）

- `docs/steel-echo-red-star.md:3-5` 头部仍写"未开始编码"——严重过时。
- spec §3.2/§1.1：`mono_script_metadata` 实际在 `utils/` 下，两处路径需补前缀。
- spec §1.2（REV#01）要求"无条件 SConscript"，实际 `SCsub:580` 用 `if env.editor_build:`——代码内已留偏差说明，文档未回填。
- phase2 评审 §1.3/§2.1 仍记 P2 为"文本扫描 v1，typedef 延后"，与 phase3 §10 矛盾，未回填。
- 行号漂移：异常清理 916→942、1547→1653；SCsub:21-35→21-40；建议文档改引函数名而非行号。
- spec §4.P2 REV#06 附注的"v2 计划：typedef 迭代"已完成，措辞需改为已完成。
- `csharp_test/project.godot.bak` 又被编辑器重新生成（已被 .gitignore 覆盖，无危害，知晓即可）。

---

## 五、确认无问题的方面（审查覆盖但通过的项）

- B0 内存管理：3 处 `mono_custom_attrs_from_*` 均配对 `mono_custom_attrs_free`，无泄漏；`[Export]`/`ExportAttribute` 后缀问题不存在（元数据始终是完整类名）。
- `ExportedMember` 裸指针当前**安全但脆弱**（立即转 `PropertyInfo` 值拷贝，未长期持有）——建议补注释，真正的悬空在 P0-1。
- P2 typedef 迭代本身正确（rid 从 1 开始、`<Module>` 过滤、public 位检查、`mono_class_get` 错误处理均无误）；全局类 add/remove 配对由 EditorFileSystem 统一驱动，语言层无泄漏。
- EditorDock API 使用、面板生命周期与所有权、P6 信号连接的双向自动清理、TOOLS 守卫（除 P0-2 外）全部正确；`mono_build_panel` 非 TOOLS 构建为空翻译单元 ✓。
- P7 核心实现与 Mono 6.12 上游源码交叉核对无误（`mono_jit_parse_options` 在 init 前、options 被 Mono 内部 strdup、suspend=n 合理）。
- 值类型纪律：`get()` 用 `mono_field_get_value_object` ✓；`set()` 先 unbox ✓。
- A3 sanitize 抽出已完成（csharp_script/mono_export_plugin 旧实现确已删除）；`escape_csharp_keyword` 接入参数名转义（单调用点合理）；`templates.gen.h` 被 `.gitignore` 正确排除且非陈旧。

---

## 六、修复优先级建议

| 优先级 | 项 | 工作量 |
|---|---|---|
| **立即** | P0-2 TOOLS 守卫（一行）+ P0-1 热重载改"版本化 dll 不 close"（小改动，同时消除 L7 陈旧 IL 与 UAF） | 低 |
| **立即** | P1-#7 程序集名三处统一（防默认配置导出断链） | 低 |
| **本周** | P1-#1/2/3/4/6（判空、NIL 跳过、信号继承、异常清理签名、删 xml_escape）+ 调试残留清理（含已入库 marker 文件） | 低-中 |
| **决策** | P1-#8 模板集：收窄到 2 个可用模板，或立项补 glue | 需决策 |
| **下周** | P1-#5 reload_tool_script 接 build；P2 功能完善组（枚举 hint、SZARRAY、信号参数名/命名约定） | 中 |
| **一次性** | 文档回填组 + spec §1.1/§4.P7 修正 + P7 验证脚本补交 | 低 |
