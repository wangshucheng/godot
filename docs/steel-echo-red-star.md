# modules/mono 深度对比：当前实现 vs 提交 1963b2f（上游官方 .NET 模块）

> **更新说明（2026-07-25）**：基于本报告已立项 `docs/mono_editor_spec.md` 实施 A 级 + B 级 P1-P7。
> 当前状态：spec 已完成评审修订（见 `docs/review_2026-07-25.md` 与 `docs/revision_log_2026-07-25.md`），未开始编码。
> 本文档作为基线分析保持不变，仅在 §3 和 §4 补充实施状态标注与一致性修订（[REV-2026-07-25-#13/14/15]）。

## 0. 对比基线与总体结论

- **旧方案**（`1963b2f126`，接近上游 Godot master）：官方 .NET 模块，403 个文件。运行时是 **CoreCLR**（经 hostfxr 动态加载；`mono_gd/` 等命名是历史遗留），绑定靠 Roslyn 源生成器 + 函数表互操作，编辑器功能由 C# 插件 **GodotTools**（72 文件）+ **Godot.NET.Sdk**（150 文件，含 SourceGenerators）实现。这是"完整桌面 .NET 开发体验"方案。
- **当前方案**：面向**微信小游戏 / WASM** 彻底重写的轻量模块，C++ 约 7600 行 + glue C# 约 6.8 万行（其中 6.5 万行是生成的 `GeneratedBindings.cs`）。直接**嵌入 Mono 6.12 (SGen)**，icall + 反射驱动，无 CoreCLR、无 hostfxr、无源生成器。AOT 在引擎构建期完成（`mono/aot-cache/*.o` 链进 WASM）。
- **一句话结论**：两者不是同一模块的两个版本，而是两套架构。旧方案的**运行时部分无法照搬**（与 embedded Mono / WASM AOT 根本冲突），但旧方案的 **editor 层大量功能与运行时弱耦合**，可以按"反射/icall 路线"选择性移植。下面按可借鉴程度分 A/B/C/D 四级。

---

## 1. 架构对比总表

| 维度 | 旧方案 (1963b2f) | 当前方案 |
|---|---|---|
| 运行时 | CoreCLR via hostfxr（`mono_gd/gd_mono.cpp`），移动 AOT 回退 libmonosgen | 嵌入式 Mono 6.12（`mono_host.cpp`，`mono_jit_init_version`），桌面 JIT / WASM 解释器 / WASM 混合 AOT 四模式 |
| 互操作 | 双向函数表（`glue/runtime_interop.cpp` 150+ 函数 ↔ 托管 `ManagedCallbacks` 40+ 回调），不透明互操作结构体（`interop_types.h`） | ~180 个 icall（`mono_icalls.cpp` 3008 行）+ 反射调用 + `mono_variant` 双向转换 |
| 托管绑定 | 每方法直接 icall（`NativeCalls.cs`），源生成器生成桥接 | 全部经 `Object_Call` 动态分发（`GeneratedBindings.cs`），22 个手写包装类 |
| 脚本系统 | 完整：导出属性/信号/全局类/tool 脚本/RPC/调试器断点堆栈/热重载（ALC 卸载） | 骨架：加载/实例/9 个虚方法桥接/属性三级查找/程序集重开式热重载；`get_property_list`、`get_script_signal_list`、`validate`、`debug_*` 全是空实现（已核实 `csharp_script.h:50,91,144,157`） |
| 编辑器 | GodotTools（构建面板/IDE 集成/导出插件/热重载监视）+ Godot.NET.Sdk + 代码补全 + 脚本模板 | 仅：2 个脚本模板、自动 .csproj/.sln、后台 `dotnet build`、`MonoExportPlugin`（96 行）、命令行 `--generate-csharp-bindings` |
| 导出 | GodotTools C# 导出插件：`dotnet publish` 按 RID 逐平台发布（含 NativeAOT、lipo、xcframework） | 引擎构建期 AOT（`tools/aot_compile.py` → `mono/aot-cache/*.o`），导出插件只把 dll 打进 PCK |
| 构建 | `build_assemblies.py` 构建 3 个 C# sln（Debug/Release Api 等） | SCsub 不构建任何 C#；GodotSharp.dll 由 `glue/GodotSharp` 项目单独编译，产物在 `build_godotsharp/` |
| 目标平台 | 桌面 + Android/iOS（bionic 选项） | Windows/Linux/macOS JIT + **Web/微信小游戏**（WASM，禁用 WASMEH，`SUPPORT_LONGJMP`） |

## 2. 关键差异详解

### 2.1 运行时托管
- 旧：`GDMono` 经 `hostfxr_initialize_for_runtime_config` / `load_assembly_and_get_function_pointer` 加载 `GodotPlugins.dll`；导出版走 `coreclr_initialize` 自包含路径。热重载依赖**可收集 AssemblyLoadContext**（`PluginLoadContext`）。
- 当前：`MonoHost::initialize()` 用传统 embedding API；WASM 下 BCL 在 MEMFS（`--embed-file` 白名单 4 个 dll），程序集从 PCK 解包；热重载用 `mono_assembly_close` 再重开（修复 L7 缓存问题）——**Mono embedding 没有 ALC 卸载能力**，这是旧方案热重载架构无法移植的根本原因。

### 2.2 互操作与绑定
- 旧绑定是编译期确定的直接调用，性能好、类型安全，但依赖 SourceGenerators 生成桥接代码和 `NativeFuncs` DllImport 解析。
- 当前绑定是运行期 `Call()` 动态分发，配合大量「WASM-safe 全局指针模型」规避解释器 bug（不跨 icall 边界传指针/字符串、`mono_field_get_value_object` 防栈溢出等）。这套约束是旧方案没有的，**任何移植代码都必须遵守同样的 icall 边界纪律**。

### 2.3 脚本系统缺口（editor 体验的核心短板）
当前 `csharp_script.h` 已核实为空实现的关键接口：
- `get_property_list` → Inspector 不显示任何 C# 脚本属性（无 [Export]）
- `get_script_signal_list` → 无脚本自定义信号
- `validate` 恒 true → 无诊断
- `debug_*` 全空 → 无断点/堆栈
- `handles_global_class_type` false、`is_tool` 恒 false、`get_base_script` 空 → 无全局类、无 [Tool]、无基脚本继承

### 2.4 导出
旧方案导出即 `dotnet publish`，完全不适合 WASM/微信小游戏；当前「AOT 属于引擎二进制、导出只打包 dll」才是该 fork 的正确形态。**导出体系不需要借鉴旧方案**（仅桌面导出可借其配置项思路）。

---

## 3. 可保留/借鉴清单（按可行性分级）

### A 级：几乎可直接搬（与运行时零耦合，纯 C++/资源文件）
1. **`editor/script_templates/`（10 个文件）** — Node/default、Object/empty、CharacterBody2D/3D、EditorPlugin、EditorScript 等模板，当前只有 2 个，直接复制即可扩充。
2. **`editor/code_completion.{h,cpp}`** — IDE 代码补全（节点路径、信号、动画名、输入动作等），本身是纯 C++、从 GDScript 补全移植而来，只依赖引擎 ClassDB，与 CoreCLR 无关；可接入当前 `CSharpLanguage`。
3. **utils 三件**：`naming_utils`（snake→Pascal/camel，当前绑定生成器正好需要）、`string_utils`（`is_csharp_keyword` 等）、`path_utils` 完整版（当前被阉割到 61 行，`find_executable` 是空实现——恢复它即可找回 dotnet 查找能力）。
4. **`editor/semver.{h,cpp}`、`utils/macos_utils`** — 工具代码，按需取。
5. **GodotTools 的 `DotNetFinder` / dotnet 查找逻辑**（C# 代码，思路可移植到 C++ 的 `find_executable`）。

### B 级：需按当前架构重写，但价值最高（editor 功能主体）
这些都是旧方案证明过"该做什么"的功能，实现路线换成反射/icall。**按「价值 ÷ 成本 + 依赖关系」的优先级排序如下（P1 最高）：**

1. **P1 — [Export] 属性 + Inspector 显示**：缺口最大——当前 Inspector 里 C# 脚本一个属性都看不到（`get_property_list` 空实现），脚本完全不可配置。glue 侧已有 `Reflection.cs`（ClassDB 反射）和属性三级查找，补上 `get_property_list`/`_get_property_list` 的反射实现（读 C# 字段/属性的 `[Export]` 特性）即可解锁。它建立的「反射读特性 → 属性列表缓存」基础设施是 P2/P3 的共用底座，必须最先做。
2. **P2 — 全局类（`global class`）与脚本类图标**：让 C# 类出现在「添加节点/创建资源」对话框，是脚本系统"像个正式语言"的标志。`update_script_class_info` 对应旧 `ScriptManagerBridge_UpdateScriptClassInfo`，只需反射读类名/基类实现 `handles_global_class_type`，成本低、见效快。
3. **P3 — 脚本信号 [Signal]**：实际游戏逻辑连线必需。`Callable` 桥（`mono_callable`）已存在，主要工作是 `get_script_signal_list` + 信号缓存（旧方案 `event_signals` 缓存结构可参考），可复用 P1 的反射设施，故排其后。
4. **P4 — 构建面板（MSBuildPanel 等价物）**：构建已在后台自动跑（`build_project()`），缺的是输出与诊断可见性；脚本复杂度上来后看不到编译错误是硬痛点。旧 GodotTools 的 `Build/MSBuildPanel/BuildOutputView/BuildProblemsView` 可简化重写为编辑器底部面板插件；`GodotTools.BuildLogger` 的结构化诊断思路可直接借鉴。完全独立于 P1–P3，可穿插做。**[REV-2026-07-25-#14]** 与 spec §4.P4 一致性修订：v1 同步阻塞编辑器是已知缺陷（`OS::execute` 阻塞），spec 已明确「异步化列为 v2 改进项」；P4 完成后编辑器仍会短暂卡顿（dotnet build 期间），但输出与错误高亮可见，是 v1 可接受的体验折衷。
5. **P5 — [Tool] 脚本**：`is_tool` 读 `[Tool]` 特性即可，但编辑器内执行用户 C# 代码风险高（脚本崩溃会拖累编辑器稳定性），且依赖程序集重载足够可靠，适合在 P1–P3 稳固后做。
6. **P6 — 热重载文件监视**：当前保存已触发自动构建+重载，监视（旧 `HotReloadAssemblyWatcher` 思路嫁接到 `reload_all_scripts()`）主要覆盖外部 IDE 改文件的场景，增量价值有限；实现便宜，可顺手做。
7. **P7 — 调试器最小集（断点+堆栈）**：需接 `mono_debugger_agent` 软调试代理，成本最高，且 WASM 端基本不可行（仅桌面受益）。旧方案也只完成了断点/堆栈帧（locals/globals 本来就是 TODO），放最后。

### C 级：大工程，需先评估收益
1. **GodotTools 完整移植**（IDE 集成：VS/VSCode/Rider、MessagingServer TCP 协议、外部编辑器打开）：依赖 **GodotSharpEditor API**——当前绑定生成器刻意跳过编辑器类。需要先开启编辑器类绑定生成（走 `Call()` 动态分发现在已可行），再重写 GodotTools 的 `Internals/` icall 层到当前 `Godot.Bridge`。代码量数千行 C#，且对微信小游戏目标收益有限。
2. **Godot.NET.Sdk / SourceGenerators**：Sdk.props/targets 的 Configuration 映射（Debug/ExportDebug/ExportRelease）和引用校验可借鉴进当前 .csproj 生成逻辑；生成器中仅 `ScriptPathAttributeGenerator` 等少数与运行时无关的可直接用，桥接类生成器（ScriptMethods/Properties/SignalsGenerator）深度绑定旧 Bridge API，**不可直接用**。**[REV-2026-07-25-#15]** 评价调整：其"编译期生成替代反射"的思路是 WASM AOT 性能优化的关键方向——当前 `Object_Call` 动态分发在 WASM 解释器下有性能损耗（每次 icall 调用需查 ClassDB + Variant 转换），编译期生成直接调用桥接可显著降低开销。列为 v3 探索项，待 B 级稳定后单独立项评估。
3. **导出预设的 C# 专属选项**（`dotnet/include_scripts_content`、`embed_build_outputs` 等）：可作为当前 `MonoExportPlugin` 的增强项参考。

### D 级：不可借鉴 / 不宜借鉴（架构冲突）
1. **CoreCLR/hostfxr 托管层**（`gd_mono`、`hostfxr_resolver`、`thirdparty/hostfxr.h`、`coreclr_delegates.h`）：与 embedded Mono + WASM AOT 根本冲突。
2. **`ManagedCallbacks` 双向函数表 + `interop_types.h` 不透明结构体**：整套互操作范式不同，当前 icall 体系已自成一体且针对 WASM 解释器 bug 做了大量规避。
3. **ALC 热重载**（`PluginLoadContext`）：Mono embedding 无此能力。
4. **`dotnet publish` 导出流程、Android/iOS targets、bionic/lipo/xcframework 处理**：与微信小游戏目标无关；桌面导出也用不上（当前桌面走 JIT + PCK 内 dll）。
5. **`glue/GodotSharp` 旧版 C# 核心库**（NativeInterop/Marshaling/Bridge）：与当前手写包装类体系冲突，不可混用。

---

## 4. 建议路径

按「A 级全拿 → B 级按优先级逐个实现 → C 级单独立项评估」推进：

1. 第一步（低风险）：搬入 A 级 1–4（模板、code_completion、utils、semver），恢复 `find_executable`。
2. 第二步（核心体验）：B 级 P1–P3（[Export]/Inspector → 全局类 → 脚本信号），这三项让 C# 脚本在编辑器里"可用"，且共享同一套反射设施。
3. 第三步：P4（构建面板，可穿插）→ P5（[Tool]）→ P6（热重载监视）。
4. 第四步（可选）：P7 调试器（仅桌面）、C 级 2 的 csproj 增强。
5. GodotTools 完整移植（C 级 1）不建议在当前阶段做——投入产出比低，且微信小游戏为主要目标。

**移植纪律**：所有新增互操作必须遵守当前 WASM-safe 约束（不跨 icall 边界传裸指针/字符串、值类型用 `mono_field_get_value_object`、跳过未覆写虚方法等），桌面与 WASM 双端验证。

---

## 5. 后续工作选项

- 仅交付本分析报告，不改代码；
- 或在报告基础上实施上述移植（可全做或只做某一级）。
