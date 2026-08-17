# v3 SourceGenerators PoC（W5 垂直切片）

**日期**：2026-08-18
**来源计划**：`docs/v2_收尾计划.md` §五 W5
**评估依据**：`docs/eval_2026-07-26_v3_sourcegenerators.md`（决策 GO，阶段 1 = GlobalClass 注册 SG 化）

---

## 1. 目标与范围

垂直切片只打通**一条完整链路**（评估 §6.2 阶段 1 的骨架部分）：

1. `GodotSharp.SourceGenerators` 项目骨架（netstandard2.0 + `IsRoslynComponent` + `Microsoft.CodeAnalysis.CSharp 4.7.0`）
2. 单个 `[GlobalClass]` 注册表生成器（程序集级，非 per-class）
3. C++ 侧消费链路打通（`refresh_global_classes` 优先读 SG 注册表）
4. 验证"脚本类改 partial"迁移成本

**不做**（防范围膨胀）：Export/Signal/Tool 生成器、per-class `__Register` 方法、icon 的实际消费、csproj 集成自动化。

## 2. 交付物

| 文件 | 角色 |
|------|------|
| [glue/GodotSharp.SourceGenerators/GodotSharp.SourceGenerators.csproj](../modules/mono/glue/GodotSharp.SourceGenerators/GodotSharp.SourceGenerators.csproj) | SG 项目骨架（输出 `bin/GodotSharp/SourceGenerators/`） |
| [glue/GodotSharp.SourceGenerators/GlobalClassGenerator.cs](../modules/mono/glue/GodotSharp.SourceGenerators/GlobalClassGenerator.cs) | `[GlobalClass]` 程序集级注册表生成器 |
| [glue/GodotSharp/ScriptRegistry.cs](../modules/mono/glue/GodotSharp/ScriptRegistry.cs) | 运行时 push 入口（C# → icall） |
| [glue/GodotSharp/GodotBridge.cs](../modules/mono/glue/GodotSharp/GodotBridge.cs) | 新增 `godot_icall_ScriptRegistry_RegisterGlobalClass` 声明 |
| [modules/mono/mono_icalls.cpp](../modules/mono/mono_icalls.cpp) | icall 实现 + 注册 |
| [modules/mono/csharp_script.h](../modules/mono/csharp_script.h) / [.cpp](../modules/mono/csharp_script.cpp) | `sg_global_class_cache` 表 + `sg_register_global_class()` + `refresh_global_classes()` SG 分支 |

## 3. 链路设计（评估 §5.3 方式 A：C# 主动 push）

```
dotnet build（用户程序集）
  └─ Roslyn 加载 GodotSharp.SourceGenerators.dll
       └─ GlobalClassGenerator 对每个 [GlobalClass] 类收集元数据
            （simple name / base simple name / [Tool] / IsAbstract / IconPath）
       └─ 生成 GodotGlobalClassRegistry.g.cs：
            ModuleInitializerAttribute polyfill（netstandard2.0 无此属性，
            Roslyn 按全名识别，internal polyfill 可用；CS0436 已 pragma 压制）
            <Module> .cctor → __GlobalClassRegistry.__Register()

引擎运行时（主线程，refresh_global_classes）
  └─ mono_runtime_class_init(<Module>)          ← 纯运行时 C API，
       │                                           无 mono_runtime_invoke 托管
       │                                           签名 marshalling → WASM 安全
       └─ C# module cctor → ScriptRegistry.RegisterGlobalClass(...)
            └─ icall: godot_icall_ScriptRegistry_RegisterGlobalClass
                 （int 标志位而非 bool —— WASM interpreter icall 惯例）
                 └─ CSharpLanguage::sg_register_global_class()
                      └─ sg_global_class_cache 填充 + sg_registry_populated = true

  └─ refresh_global_classes：sg_registry_populated ?
       ├─ 是 → global_class_cache = sg_global_class_cache（0 次 TypeDef 扫描）
       └─ 否 → 回退现有 TypeDef 反射扫描（pre-SG 程序集零影响）
```

**时序安全性**：`mono_runtime_class_init` 幂等——非 SG 程序集的 `<Module>` cctor 为空操作，`sg_registry_populated` 保持 false，反射扫描兜底。`godot_register_icalls()` 在 `mono_host` 初始化时执行，先于任何用户程序集加载，icall 目标必定已注册。

**AOT 友好性**：注册表类型为 `Dictionary`（C++ 侧 HashMap）+ 无 `dynamic`/`Emit`/反射构造泛型，符合评估 §5.3 约束。

## 4. partial 迁移成本验证（重要发现）

评估 §2.1 称"脚本类必须 `partial`（SG 才能拼 partial class）"。**本 PoC 推翻该约束对阶段 1 的适用性**：

- **程序集级注册表方案（本 PoC）**：生成代码全部位于 SG 自产文件（module initializer），**不触碰用户类声明**。用户类**无需加 `partial`**，零迁移成本。
- **per-class `__Register` 方案（评估 §2.4 目标形态，阶段 2 Export 成员注册才需要）**：SG 生成 `partial class MyClass` 追加 `__Register` 方法——此阶段才要求用户类 `partial`，与评估 §4.1 任务 10 的 0.3 人天机械改动估算一致。

**结论**：阶段 1（GlobalClass，最高 ROI 项）可零成本落地；`partial` 改造可推迟到阶段 2 启动时统一执行。

## 5. 已知限制（PoC 级别）

1. **source_path 为空**：SG 注册表不含源文件路径，桌面端 `.pdb` 反查链路在 SG 路径下不执行 → `get_global_class_name()` 回退"文件名==类名"约定（与 WASM 路径同策略）。若影响桌面编辑器体验，阶段 2 可让 SG 直接携带 source 路径（GeneratorSyntaxContext 里可拿 `Location.SourceTree.FilePath`）。
2. **iconPath 未消费**：icall 签名含 `iconPath`（对齐评估 §6.2 API 设计），C++ 侧 `GlobalClassInfo` 暂无 icon 字段，先接收后忽略。
3. **键为 simple name**：与现有 typedef 扫描（`mono_class_get_name`，无命名空间）一致；命名空间下同名类冲突是既有限制，非本 PoC 引入。
4. **ModuleInitializer polyfill 的 CS0436**：目标框架若自带该属性（net5+）会产生类型冲突警告，生成代码已 `#pragma warning disable`。本仓库目标恒为 netstandard2.0，实际不触发。
5. **csproj 消费接线**：游戏 csproj 需加 `<ProjectAnalyzer Reference>`/Analyzer 引用指向 SG 输出，此项在开发机验证时接线（见 §6）。

## 6. 开发机验证步骤（构建环境恢复后执行）

```powershell
# 1. 构建 SG 项目（产出 bin/GodotSharp/SourceGenerators/GodotSharp.SourceGenerators.dll）
dotnet build modules/mono/glue/GodotSharp.SourceGenerators

# 2. csharp_test.csproj 添加（验证接线）：
#    <ItemGroup>
#      <ProjectReference Include="..\modules\mono\glue\GodotSharp.SourceGenerators\GodotSharp.SourceGenerators.csproj"
#                        OutputItemType="Analyzer" ReferenceOutputAssembly="false" />
#    </ItemGroup>
dotnet build csharp_test

# 3. 重建引擎（editor 配置）并启动，观察日志：
#    [Mono] P2 refresh_global_classes: SG registry path, N global classes registered (0 typedefs scanned)
#    —— 出现该行 = 生成器产出被 C++ 成功消费（M-W5 验收）
run_csharp_test.ps1   # 全局类注册 23 场景回归必须 PASS（反射扫描与 SG 路径行为一致）
```

**回归判据**：`refresh_global_classes` SG 路径下 `mono_class_get` / `class_has_attribute` 调用次数为 0（日志 "0 typedefs scanned"）；全局类行为（Add Node 对话框、`get_global_class_name`）与反射路径无差异。

## 7. 结论

- 里程碑 M-W5 前半（PROJECT_STATUS 无过期待办）已达成，见 [PROJECT_STATUS.md](PROJECT_STATUS.md)。
- 里程碑 M-W5 后半（SG PoC 生成器产出被 C++ 成功消费）：代码链路完整，运行时验证待开发机（§6 步骤 3 的日志行为验收判据）。
- 阶段 1 全量落地（含 icon 消费、source_path、csproj 自动接线）留待 v3 正式启动，按评估 §6.2 分四个阶段推进。
