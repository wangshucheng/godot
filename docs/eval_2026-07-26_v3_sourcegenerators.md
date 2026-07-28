# v3 SourceGenerators 可行性评估报告

- 评估日期：2026-07-26（补记于 2026-07-28）
- 评估目标：在 Mono 6.12 embedding 架构下，用 Roslyn Source Generators 取代 `mono_script_meta::collect_*` 运行时反射扫描
- 评估范围：技术可行性 / 当前架构分析 / 收益 / 工作量 / 架构冲突 / GO-NO-GO 决策
- 评估对象：
  - `modules/mono/utils/mono_script_metadata.{h,cpp}`
  - `modules/mono/csharp_script.cpp`（`resolve_mono_class`、`refresh_global_classes`）
  - `modules/mono/glue/GodotSharp/{Attributes.cs, GodotBridge.cs, GodotSharp.csproj}`
  - `csharp_test/CSharpTest.csproj`
- 工具链实测：`dotnet --version` → **7.0.401**

---

## 1. 摘要

**结论：GO（分阶段，优先级 P2）**。

- **技术上可行**：Source Generators 是 Roslyn 编译期特性，与运行时（Mono 6.12 vs CoreCLR）无关。当前 `CSharpTest.csproj` / `GodotSharp.csproj` 均为 SDK 风格 + `netstandard2.0` + `LangVersion=latest`，dotnet 7.0.401 SDK 完整支持 `IIncrementalGenerator`，前置条件已全部满足。
- **架构上契合**：当前 `mono_script_meta` 用 `mono_custom_attrs_from_member` + 字符串比对 attribute 类名的方式，与 SG 生成注册表的目标形态一一对应，可以无损替换。
- **收益明确但非关键路径**：启动性能、WASM AOT 元数据占用、类型安全均有改善，但当前反射扫描已是 AOT-safe（不实例化 attribute 对象），所以收益是"锦上添花"而非"雪中送炭"。建议作为 v3 优化项推进，不阻塞当前 P0~P5 主线。
- **工作量**：约 1.5~2 人周（含 SG 项目搭骨架 + 4 个 attribute 生成器 + C++ 侧注册表接入 + 测试）。

---

## 2. 技术调研

### 2.1 Source Generators 运行原理与运行时的关系

Source Generators 是 Roslyn 在 `csc` 编译阶段调用的 `ISourceGenerator` / `IIncrementalGenerator` 钩子，在编译期产出的 `.g.cs` 文件会作为同一编译单元的输入参与编译。其执行依赖的是 **构建工具链（dotnet build + Roslyn）**，与运行时是 Mono 6.12、CoreCLR、还是 WebAssembly interpreter 无关。

关键前置条件（**全部已满足**）：

| 条件 | 要求 | 实测 | 状态 |
|------|------|------|------|
| SDK 风格 .csproj | 是 | `CSharpTest.csproj` / `GodotSharp.csproj` 均 `Microsoft.NET.Sdk` | OK |
| TargetFramework | `netstandard2.0`+ 或 `net6.0`+ | `netstandard2.0` | OK |
| LangVersion | `latest` 或 `9.0`+ | `latest` | OK |
| dotnet SDK | ≥ 6.0（推荐 7.0+ 用 `IIncrementalGenerator`） | 7.0.401 | OK |
| 编译器入口 | 走 `dotnet build`（Roslyn），非 `mcs` | `csproj` SDK 风格 → 默认走 Roslyn | OK |
| 脚本类修饰符 | 必须 `partial`（SG 才能拼 partial class） | 当前 C# 类**非 partial** | **需改造** |

### 2.2 SG 项目自身要求

新建 `GodotSharp.SourceGenerators` 项目：

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>netstandard2.0</TargetFramework>
    <LangVersion>latest</LangVersion>
    <IsRoslynComponent>true</IsRoslynComponent>
  </PropertyGroup>
  <ItemGroup>
    <PackageReference Include="Microsoft.CodeAnalysis.CSharp" Version="4.7.0" PrivateAssets="all" />
  </ItemGroup>
</Project>
```

引用方式（在 `CSharpTest.csproj` 中）：

```xml
<ItemGroup>
  <ProjectReference Include="..\modules\mono\glue\GodotSharp.SourceGenerators\GodotSharp.SourceGenerators.csproj"
                    OutputItemType="Analyzer" ReferenceOutputAssembly="false" />
</ItemGroup>
```

**SG 程序集不进运行时**（`ReferenceOutputAssembly="false"` + `OutputItemType="Analyzer"`），不会污染 Mono 6.12 运行时加载链。**与 Mono 运行时无任何耦合**，仅作为编译期 analyzer 加载。

### 2.3 当前反射架构详析

`mono_script_metadata.cpp` 提供以下 5 个函数，全部走 `mono_custom_attrs_from_*` + 字符串比对 attribute 简单类名（**AOT-safe，不实例化 attribute 对象**）：

| 函数 | 输入 | 行为 | 调用方 |
|------|------|------|--------|
| `has_attribute` | `MonoCustomAttrInfo*`, 名字 | 遍历 `attrs[i].ctor` → `mono_method_get_class` → 比对 `mono_class_get_name` | 内部 |
| `class_has_attribute` | `MonoClass*`, 名字 | 上述的类级封装 | `resolve_mono_class`、`refresh_global_classes` |
| `mono_type_to_variant_type` | `MonoType*` | 硬编码 switch + 类名/命名空间字符串匹配 | `collect_*` |
| `collect_exported_members` | `MonoClass*` | 自底向上遍历类层次（停在 `Godot.Object`），收集 `[Export]` 字段/属性，按名称去重 | `resolve_mono_class` |
| `collect_signals` | `MonoClass*` | 遍历嵌套类型，筛 `MulticastDelegate` + `[Signal]` + 名称 `EventHandler` 后缀，从 `Invoke` 签名构 `MethodInfo` | `resolve_mono_class` |

调用点（`csharp_script.cpp`）：

- **`resolve_mono_class`（行 503-614）**：类首次解析时调用 `collect_exported_members` → 填 `exported_properties`、调用 `collect_signals` → 填 `signal_cache`、调用 `class_has_attribute("ToolAttribute"/"GlobalClassAttribute")` → 填 `is_tool_class`/`is_global_class`。每个脚本类**懒加载一次**。
- **`refresh_global_classes`（行 1671-1770）**：编辑器启动时遍历 `scripts_assembly` 的 TypeDef 表（`mono_image_get_table_rows` + `mono_class_get`），对每个 public 类调用 `class_has_attribute("GlobalClassAttribute")`，命中则登记 `global_class_cache`。**全量扫描**。

### 2.4 SG 替换后的目标形态

SG 在每个被 `[Export]`/`[Signal]`/`[GlobalClass]`/`[Tool]` 标记的类旁生成 `MyClass.Register.cs`，包含一个静态注册函数：

```csharp
partial class MyClass
{
    [global::Godot.ScriptRegistration]
    internal static void __Register(global::Godot.ScriptRegistry reg)
    {
        reg.AddExport("Speed", global::Godot.Variant.Type.Int);
        reg.AddExport("Color", global::Godot.Variant.Type.Color);
        reg.AddSignal("Hit", new[] { ("target", global::Godot.Variant.Type.Object) });
        reg.MarkTool();
        reg.MarkGlobalClass(iconPath: null);
    }
}
```

C++ 侧通过 icall 在 `resolve_mono_class` 中调用该方法，拿到注册表后填本地数据结构。`collect_*` 全部废弃，`refresh_global_classes` 改为查程序集级注册表（每个程序集有一个 `[module: GodotScriptRegistry]` 触发的入口）。

---

## 3. 收益评估

### 3.1 启动性能

| 场景 | 当前 | SG 后 | 提升 |
|------|------|-------|------|
| `resolve_mono_class`（单类首次解析） | 遍历类层次字段+属性，每个成员 `mono_custom_attrs_from_field/property` + 字符串比对；遍历嵌套类型查 `[Signal]` delegate | 一次 icall + 拉一张静态表 | 单类节省 ~毫秒级（成员数 × attribute 查询成本） |
| `refresh_global_classes`（编辑器启动） | 全量扫 TypeDef 表 + 每个 public 类调 `class_has_attribute`（一次 `mono_custom_attrs_from_class` + 字符串比对） | 程序集级注册表一次性返回所有 `[GlobalClass]` 类型清单 | 显著，类数量 N 越大越明显（O(N) attribute 查询 → O(1) 表查询） |

注意：当前 `collect_*` 是懒加载，对启动路径影响有限；`refresh_global_classes` 是启动期同步全量扫描，是真正可量化的性能点。**建议把 `refresh_global_classes` 作为 SG 化的第一优先目标**。

### 3.2 WASM AOT 友好性

当前实现已经做了 AOT-safe 设计（`mono_script_metadata.h` 注释明确写了"never instantiating attribute objects"），所以 SG 化的 AOT 增益是"减少元数据依赖"而非"修复 AOT 不兼容"：

- **可去除的元数据**：`mono_custom_attrs_from_field/property/class` 在 full AOT 下需要保留 attribute 的 ctor 元数据与字段 metadata 供运行时反射查询。SG 化后这些元数据不再被运行时引用，linker 可裁剪。
- **可去除的字符串比较**：当前用 `strcmp(cname, "ExportAttribute")` 等字符串硬编码，SG 化后这些字符串常量从 native 侧消失。
- **WASM 包体收益预估**：小（几十 KB 量级），主要收益是缩短 AOT 编译期 attribute 元数据保留判定，不显著。

### 3.3 类型安全

`mono_type_to_variant_type` 当前用字符串比对映射类型（如 `Vector2` 必须在 `Godot` 命名空间且类名匹配）。这种设计有两个明显脆弱点：

- 添加新 Godot math 类型（如 `Vector4I`）需改 C++ switch；
- 用户自定义同名类型（`MyLib.Vector2`）会被错误映射（当前用 namespace 过滤缓解，但仍依赖字符串）。

SG 化后用 Roslyn `ITypeSymbol` + `INamedTypeSymbol` 比较类型同一性，编译期精确解析，**完全消除字符串比对**。新增 Godot 类型时只需改 SG 的类型映射表（C# 代码），无需重编 native。

### 3.4 收益小结

| 维度 | 收益评级 | 说明 |
|------|----------|------|
| 启动性能 | 中 | `refresh_global_classes` 明显，`collect_*` 微弱（懒加载） |
| WASM AOT | 低~中 | 当前已 AOT-safe，SG 化主要是减少元数据保留，包体收益小 |
| 类型安全 | 高 | 彻底消除字符串比对，编译期检查类型映射 |
| 代码可维护性 | 高 | 注册逻辑从 C++ 字符串扫描迁到 C# Roslyn 符号查询，更易调试 |
| 与上游对齐 | 中 | 与 Godot 4.7 官方 `modules/dotnet` SG 架构对齐，便于后续 backport |

---

## 4. 工作量估算

### 4.1 任务分解

| # | 任务 | 涉及层 | 工作量（人天） |
|---|------|--------|----------------|
| 1 | 新建 `GodotSharp.SourceGenerators` csproj + 添加到 solution | 构建 | 0.2 |
| 2 | 实现 `ScriptRegistry` 运行时承载类（C# 侧） | C# | 0.5 |
| 3 | 在 `GodotBridge.cs` 新增 icall：`godot_icall_ScriptRegistry_Invoke(string classFullname)` | C#/C++ | 0.5 |
| 4 | SG-1：`GlobalClassAttribute` 生成器（程序集级注册表） | C# (SG) | 1.0 |
| 5 | SG-2：`ExportAttribute` 生成器（成员级注册，含类型映射） | C# (SG) | 1.5 |
| 6 | SG-3：`SignalAttribute` 生成器（delegate Invoke 签名解析） | C# (SG) | 1.0 |
| 7 | SG-4：`ToolAttribute` 生成器（与 GlobalClass 合并即可） | C# (SG) | 0.2 |
| 8 | C++ 侧 `resolve_mono_class` 改造：调 icall 拉表替代 `collect_*` | C++ | 1.0 |
| 9 | C++ 侧 `refresh_global_classes` 改造：查程序集级注册表替代 TypeDef 扫描 | C++ | 0.8 |
| 10 | 现有 C# 脚本类改为 `partial`（机械改动） | C# | 0.3 |
| 11 | 测试：`csharp_test` 全套回归 + WASM AOT 编译验证 | 测试 | 1.5 |
| 12 | 旧 `mono_script_metadata.cpp` 中 `collect_*` 标记 deprecated / 删除 | C++ | 0.3 |
| 13 | 文档更新（架构图、迁移指南） | 文档 | 0.5 |

**合计：约 9.3 人天 ≈ 1.9 人周**（含测试与文档，不含未知风险 buffer）。

### 4.2 风险点

- **partial 改造的传染性**：当前所有脚本类需改为 `partial`。如果存在第三方代码生成器（如 ProtoBuf、MessagePack）也已要求 partial，需确认无冲突；当前项目内只有 4 个 attribute，无其他生成器，风险低。
- **`IIncrementalGenerator` 与 `ISourceGenerator` 选择**：dotnet 7 推荐 `IIncrementalGenerator`（增量、缓存友好），但 API 学习曲线略陡。若时间紧可先用 `ISourceGenerator`，后续升级。
- **`mono_type_to_variant_type` 的 SG 复刻**：当前 C++ 实现涵盖约 16 个 math 类型 + enum + Object + collection wrapper。SG 复刻时需逐项覆盖并加单元测试，避免遗漏。
- **icall 签名设计**：`ScriptRegistry` 在 C# 侧是托管对象，C++ 通过 `mono_runtime_invoke` 调用其方法。但项目已有 `godot_icall_RegisterSyncContext(object instance)` 的先例（注释里提到 `mono_runtime_invoke` 在 WASM interpreter 下有签名 mismatch），**需谨慎设计调用路径**，避免重蹈覆辙。建议改成传 `IntPtr` + 数据结构扁平化（如直接返回 `string[]` 表）规避 `mono_runtime_invoke` 复杂对象传递。

---

## 5. 架构冲突分析

### 5.1 与当前 attribute 体系的冲突

**结论：无冲突，attribute 体系保留**。

当前 4 个 attribute（`Export`/`Signal`/`Tool`/`GlobalClass`）定义在 `Attributes.cs`，SG 也以它们作为 `ForAttributeWithMetadataName` 的标记锚点。**SG 不替换 attribute，只替换 attribute 的"读取者"**——从 `mono_script_metadata.cpp`（C++ 反射扫描）迁到 `GodotSharp.SourceGenerators`（C# 编译期符号查询）。

唯一改造是脚本类需加 `partial` 修饰符，与 attribute 无关。

### 5.2 SDK 风格 .csproj

**结论：已具备，无冲突**。

`CSharpTest.csproj` 和 `GodotSharp.csproj` 均为 `Microsoft.NET.Sdk` 风格 + `netstandard2.0` + `LangVersion=latest`，是 SG 的标准前置条件。无需迁移 csproj 格式。

### 5.3 WASM AOT 兼容性

**结论：兼容，且更友好**。

- SG 生成的代码是普通 C# 代码，AOT 编译器（Mono full AOT / interpreter）正常处理。
- SG 化后运行时不再调用 `mono_custom_attrs_from_*`，减少对 attribute 元数据的运行时反射依赖，linker 可裁剪更多元数据。
- **唯一需注意**：`ScriptRegistry` 在 C# 侧承载注册表，其类型设计必须 AOT-friendly（避免 `dynamic`、`Emit`、反射构造泛型集合等）。建议用 `Dictionary<string, Action<ScriptRegistry>>` + 闭包静态注册，AOT 编译期可确定性解析。
- 现有 `godot_icall_RegisterSyncContext(object instance)` 注释提到 WASM interpreter 下 `mono_runtime_invoke` 签名 mismatch。SG 注册入口若走 `mono_runtime_invoke`，**可能踩同样坑**。建议设计成：C# 侧通过 `[ModuleInitializer]`（net5+，netstandard2.0 需 polyfill）或显式 icall 主动 push 注册表到 C++，而非 C++ 主动 invoke C#。

### 5.4 与 `GodotBridge.cs` P/Invoke 体系

**结论：需扩展 icall 接口，无根本冲突**。

`GodotBridge.cs` 当前是纯 P/Invoke icall 集合。SG 化需要新增一个反向调用：C++ → C# 拉注册表。两种实现方式：

- **方式 A（推荐）**：C# 侧在程序集加载后（`Runtime.Initialize` 或 `[ModuleInitializer]`）主动通过新 icall `godot_icall_ScriptRegistry_Register(...)` 把所有类的元数据一次性 push 到 C++ 侧的全局表。C++ `resolve_mono_class` / `refresh_global_classes` 改为查 C++ 侧的表。**完全规避 `mono_runtime_invoke`**。
- **方式 B**：C++ 通过 `mono_runtime_invoke` 主动调 C# 静态方法 `ScriptRegistry.Get(classFullname)` 拉表。**有 WASM interpreter 风险**。

### 5.5 与 `GodotSharp.cs` 顶层绑定

**结论：需新增 `ScriptRegistry` 类与若干 icall，不影响现有 icall**。

`GodotSharp.cs` 当前不含 ScriptRegistry 概念。SG 化后需在该目录新增：

- `ScriptRegistry.cs`：注册表承载类
- `ScriptRegistrationAttribute.cs`：标记 SG 生成的 `__Register` 方法

这些是新增项，不修改 `GodotBridge.cs` 现有 icall。

---

## 6. 决策与计划

### 6.1 决策：**GO**（分阶段推进，P2 优先级）

理由：

1. 技术前置条件已全部满足（SDK 风格 csproj + dotnet 7.0.401 + netstandard2.0）。
2. 与现有 attribute 体系无冲突，改造范围可控（脚本类加 `partial`、新增 SG 项目、改造两个 C++ 函数）。
3. 收益明确：`refresh_global_classes` 启动性能、类型安全、与上游对齐。
4. 工作量 1.9 人周，性价比可接受。

**不作为 P0/P1 推进**的理由：当前反射扫描已是 AOT-safe 且能正常工作，没有阻塞主线的问题。SG 化是优化项，应在当前主线（P5/P7 等）收尾后推进。

### 6.2 分阶段实施计划

#### 阶段 1：GlobalClass 注册 SG 化（最高 ROI）

- **目标**：`refresh_global_classes` 不再扫 TypeDef 表。
- **任务**：
  1. 新建 `GodotSharp.SourceGenerators` csproj
  2. 实现 `GlobalClassAttribute` 生成器，生成程序集级 `[ModuleInitializer]` 注册代码
  3. 新增 icall `godot_icall_ScriptRegistry_RegisterGlobalClass(string classFullname, string baseType, bool isTool, bool isAbstract, string iconPath)`
  4. C++ 侧 `refresh_global_classes` 改为查 C++ 侧全局表
- **验收**：编辑器启动时 `refresh_global_classes` 不再调 `mono_class_get` / `class_has_attribute`；csharp_test 全局类注册测试通过。
- **工作量**：约 3 人天。

#### 阶段 2：Export 成员注册 SG 化（最复杂）

- **目标**：`resolve_mono_class` 不再调 `collect_exported_members`。
- **任务**：
  1. SG 实现 `ExportAttribute` 生成器，生成 `__Register` partial 方法
  2. 在 SG 中复刻 `mono_type_to_variant_type` 的类型映射（基于 Roslyn `ITypeSymbol`）
  3. 新增 icall：`godot_icall_ScriptRegistry_AddExport(string classFullname, string memberName, int variantType)`
  4. C++ `resolve_mono_class` 改为查 C++ 侧表
- **验收**：Inspector 显示与反射版完全一致；WASM AOT 编译通过。
- **工作量**：约 3.5 人天。

#### 阶段 3：Signal + Tool 注册 SG 化

- **目标**：`collect_signals` 与 `class_has_attribute("ToolAttribute")` 全部废弃。
- **任务**：
  1. SG 实现 `SignalAttribute` 生成器，从 delegate `Invoke` 签名提取参数类型
  2. SG 实现 `ToolAttribute` 生成器（合并到 GlobalClass 注册流程）
  3. C++ 侧移除 `collect_signals` / `class_has_attribute` 调用
- **验收**：信号面板、Tool 脚本行为与反射版一致。
- **工作量**：约 2 人天。

#### 阶段 4：清理与对齐

- **任务**：
  1. 删除 `mono_script_metadata.cpp` 中 `collect_*` / `class_has_attribute` / `has_attribute`（保留 `mono_type_to_variant_type` 给非 SG 路径兜底，或一并删除）
  2. 更新 `mono_editor_spec.md` / `PROJECT_STATUS.md`
  3. 评估与官方 `modules/dotnet` SG 代码风格的差异，决定是否对齐命名
- **工作量**：约 1 人天。

### 6.3 回退策略

每阶段独立合入，任一阶段出现 WASM AOT 编译失败或 icall `mono_runtime_invoke` 兼容问题，可回退到反射扫描路径：

- 保留 `mono_script_metadata.cpp` 的 `collect_*` 函数（仅在阶段 4 才删）
- C++ 侧通过宏 `USE_SOURCE_GENERATORS` 控制走 SG 表还是反射扫描
- 阶段 1-3 任一阶段失败不影响其他阶段（GlobalClass / Export / Signal 三个生成器彼此独立）

### 6.4 关键决策点

| 决策 | 选择 | 理由 |
|------|------|------|
| SG API | `IIncrementalGenerator` | dotnet 7 原生支持，缓存友好，未来 backport 上游更顺 |
| 注册方向 | C# → C++ push（方式 A） | 规避 `mono_runtime_invoke` 在 WASM interpreter 的签名 mismatch |
| 注册时机 | `[ModuleInitializer]` 或 `Runtime.Initialize` | 程序集加载后立即注册，C++ 侧查询时表已就绪 |
| 数据传递 | 扁平化 icall（string/int 参数） | 避免 `mono_runtime_invoke` 复杂对象序列化 |
| 旧代码处理 | 阶段 4 统一删除 | 保留回退路径直到全量验证 |

---

## 7. 附录

### 7.1 相关文件清单

- 反射扫描层：`modules/mono/utils/mono_script_metadata.{h,cpp}`
- 调用方：`modules/mono/csharp_script.cpp`（`resolve_mono_class` 行 503-614，`refresh_global_classes` 行 1671-1770）
- Attribute 定义：`modules/mono/glue/GodotSharp/Attributes.cs`
- icall 顶层：`modules/mono/glue/GodotSharp/GodotBridge.cs`
- 构建配置：`modules/mono/glue/GodotSharp/GodotSharp.csproj`、`csharp_test/CSharpTest.csproj`

### 7.2 dotnet SDK 实测

```
$ dotnet --version
7.0.401
```

满足 Source Generators（含 `IIncrementalGenerator`）的全部 SDK 要求。

### 7.3 上游参考

Godot 4.7 官方 `modules/dotnet` 模块用 Source Generators 生成注册代码（`Godot.SourceGenerators` 项目），其架构与本评估目标形态高度一致。本评估的实施可参考上游命名约定与生成代码结构，但需注意上游运行时为 CoreCLR，本项目为 Mono 6.12，icall 路径与运行时假设不能直接照搬。
