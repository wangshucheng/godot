# H9 WASM 运行时失败评审报告

**日期**: 2026-07-26
**评审范围**: Web 解释器模式构建 + H9 23 场景 WASM 运行时验证
**结论**: 失败，根因为 5 个独立问题叠加，其中 2 个 P0、2 个 P1、1 个 P2

---

## 一、失败现象

H9 测试 (`run_h9_wasm_test.py`) exit code 3（完全没跑），WASM 启动后立即 abort：

```
[Mono] AOT: Full AOT runtime initialized (no JIT compiler).
[Mono] AOT: Registering Full AOT modules...
[Mono] AOT:   Registered: mscorlib
[Mono] AOT:   Registered: System
[Mono] AOT:   Registered: System.Core
[Mono] AOT:   WARNING: Module 'System.Runtime' info is NULL (not linked or not AOT-compiled).
[Mono] AOT:   WARNING: Module 'System.Collections' info is NULL (not linked or not AOT-compiled).
[Mono] AOT:   WARNING: Module 'System.Threading.Tasks' info is NULL (not linked or not AOT-compiled).
[Mono] AOT:   WARNING: Module 'GodotSharp' info is NULL (not linked or not AOT-compiled).
[Mono] AOT:   WARNING: Module 'ProjectScripts' info is NULL (not linked or not AOT-compiled).
[Mono] Failed to load AOT module 'System' while running in aot-only mode because a dependency cannot be found or it is out of date.
[error] Aborted()
```

---

## 二、根因分析（第一性原理）

通过 5 层证据链定位，确认是 **5 个独立问题叠加** 导致：

### 问题 1（P0）：scons 缓存导致模式切换无效

**证据**：
1. Web 解释器构建日志显示 `[Mono] WASM platform: Interpreter mode configured`（[SCsub:558](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/modules/mono/SCsub#L558)）
2. 但 WASM 二进制中包含 `Hybrid AOT mode: AOT + Interpreter fallback` 字符串（来自 [mono_host.cpp:316](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/modules/mono/mono_host.cpp#L316)，仅在 `MONO_AOT_MODE + MONO_INTERP_MODE` 同时定义时编译）
3. 当前 `mono_host.web.template_release.wasm32.nothreads.o`（11:54:32）中 **不包含** "Hybrid AOT" 字符串
4. 当前 `mono_aot.web.template_release.wasm32.nothreads.o`（11:54:32）中 **不包含** "Full AOT runtime initialized" 字符串
5. 8 个 Mono 静态库中 **都不包含** 这些字符串

**根因**：scons 的 `platform=web -c` 命令没有清理 `bin/templates/web/` 下的 WASM 产物。后续构建检测到 WASM 文件已存在且时间戳较新，**跳过了重新链接步骤**。实际部署到 web_test 的 WASM 是之前微信构建（AOT 模式）的残留产物。

**修复建议**：
- 切换 AOT/解释器模式时，必须手动删除 `bin/templates/web/` 和 `bin/obj/modules/mono/` 目录
- 或在 SCsub 中添加模式切换的缓存失效逻辑（对比 CPPDEFINES 变化）

### 问题 2（P0）：AOT 模块表不完整

**证据**：[mono_aot_modules.cpp:29-39](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/modules/mono/mono_aot_modules.cpp#L29-L39)

```cpp
static const AotModuleEntry aot_module_table[] = {
    {"mscorlib", &mono_aot_module_mscorlib_info},
    {"System", &mono_aot_module_System_info},
    {"System.Core", &mono_aot_module_System_Core_info},
    {"System.Runtime", nullptr},           // ← 未 AOT 编译
    {"System.Collections", nullptr},        // ← 未 AOT 编译
    {"System.Threading.Tasks", nullptr},    // ← 未 AOT 编译
    {"GodotSharp", nullptr},                // ← 走解释器
    {"ProjectScripts", nullptr},            // ← 走解释器
    {nullptr, nullptr}
};
```

`System` 模块 AOT 注册成功，但运行时加载 `System` 时其依赖 `System.Runtime`（.NET Core 风格的 facade 拆分）未注册，触发 `Failed to load AOT module 'System'... a dependency cannot be found`。

**根因**：BCL WASM 目录（`BCL_WASM_DIR`）是 Mono illinker 测试样本，包含 netstandard2.0 facade 拆分的 `System.Runtime.dll` / `System.Collections.dll` / `System.Threading.Tasks.dll`，但 AOT 工作流（[SCsub:282](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/modules/mono/SCsub#L282)）只 AOT 编译了 `mscorlib/System/System.Core` 三个，facade 程序集未 AOT 编译。

**修复建议**：
- **方案 A（推荐）**：在 `_run_aot_workflow` 的 `dll_path_map` 中添加 facade 程序集，让 AOT 编译器处理它们
- **方案 B**：从 BCL 目录中移除 facade DLL，强制 Mono 回退到 mscorlib 内的 Type Forwarding 实现

### 问题 3（P1）：load_godotsharp 错误恢复路径不完整

**证据**：[mono_host.cpp:302-345](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/modules/mono/mono_host.cpp#L302-L345)

`Hybrid AOT` 分支的注释写道：
> GodotSharp.dll load failure ("dependency cannot be found") is expected because System.Runtime/System.Collections/etc are not AOT-compiled. The load_godotsharp() call handles this gracefully (returns false, mono_host continues without managed bindings).

但实际运行时日志显示：在 `load_godotsharp()` 被调用之前（日志行 38-41），`mono_class_init(Object)` 触发了 `System` 模块加载，进而触发 abort。错误恢复路径从未被执行。

**根因**：`mono_class_init` 在 [mono_host.cpp:411](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/modules/mono/mono_host.cpp#L411) `mono_variant::cache_mono_corlib_classes()` 中被调用，此时 `load_godotsharp()` 还未执行。AOT 模块缺失导致的 abort 发生在错误恢复之前。

**修复建议**：在 `mono_aot_register_modules()` 之后添加一次 `mono_class_init` 探针，提前检测 AOT 模块依赖完整性，失败时降级为纯解释器模式（`MONO_AOT_MODE_INTERP_LLVMONLY` 不强制 AOT 检查）。

### 问题 4（P1）：BCL 嵌入策略与 AOT 模块表不一致

**证据**：
- BCL 嵌入白名单（[SCsub:65-70](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/modules/mono/SCsub#L65-L70)）：`mscorlib / System / System.Core / netstandard`
- AOT 模块表：`mscorlib / System / System.Core`（前 3 个有 info，后 5 个 nullptr）
- 运行时 MONO_PATH（[mono_host.cpp:177](file:///C:/Users/Administrator/AppData/Roaming/TRAE%20SOLO%20CN/ModularData/ai-agent/work-mode-projects/6a47fad25801ac16b9570799/godot4.7_mono/modules/mono/mono_host.cpp#L177)）：`lib/mono/4.5:.mono/assemblies:lib/mono/4.5/Facades`
- 运行时日志：`'/lib/mono/4.5/Facades' in MONO_PATH doesn't exist or has wrong permissions.`

BCL 嵌入只放了 4 个核心 DLL 到 `lib/mono/4.5/`，但 MONO_PATH 还引用了 `lib/mono/4.5/Facades` 子目录（运行时不存在）。当 Mono 尝试解析 `System.Runtime` facade 时，MEMFS 中找不到，回退到 AOT 模块表（也是 nullptr），最终 abort。

**根因**：BCL 嵌入策略只考虑了 `mscorlib/System/System.Core/netstandard`，但 netstandard2.0 的 facade 拆分要求 `System.Runtime` 等作为独立 DLL 存在。

**修复建议**：
- 在 `_BCL_WASM_REQUIRED_DLLS` 中添加 `System.Runtime.dll / System.Collections.dll / System.Threading.Tasks.dll` 等 facade DLL
- 或在 `_embed_bcl_assemblies` 中额外嵌入整个 `Facades/` 子目录

### 问题 5（P2）：Web 解释器构建脚本编码问题

**证据**：`build_web_interpreter.ps1` 执行失败：

```
Unexpected token 'MB' in expression or statement.
Missing closing ')' in expression.
```

脚本中包含中文注释（"Mono 静态库"），PowerShell 5 默认用 GBK 解析，导致中文乱码引发语法错误。

**修复建议**：
- 将脚本保存为 UTF-8 with BOM 编码
- 或移除中文注释，改用英文
- 推荐使用 pwsh（PowerShell 7）替代 powershell 5

---

## 三、问题优先级与修复顺序

| 优先级 | 问题 | 修复影响 | 建议时机 |
|--------|------|----------|----------|
| **P0** | 问题 1：scons 缓存导致模式切换无效 | 阻塞 Web 解释器模式验证 | 立即（手动删除 bin/ 即可验证） |
| **P0** | 问题 2：AOT 模块表不完整 | 阻塞 Hybrid AOT 模式运行 | 本轮 |
| **P1** | 问题 3：load_godotsharp 错误恢复路径不完整 | 运行时鲁棒性 | 本轮 |
| **P1** | 问题 4：BCL 嵌入策略与 AOT 模块表不一致 | 阻塞 facade 解析 | 本轮 |
| **P2** | 问题 5：构建脚本编码问题 | 阻塞自动化构建 | 后续 |

---

## 四、与本次 P0/P1 修复的关系

本次终审报告中的 P0/P1 修复（P0-1 版本化 DLL、P0-2 TOOLS 守卫、P1-#1 GodotSharp 判空、P1-#2 NIL 跳过、P1-#3 collect_signals 基类遍历、P1-#4 异常清理、P1-#6 xml_escape、P1-#7 程序集名统一）**全部不涉及**：
- AOT 模块表（`mono_aot_modules.cpp`）
- BCL 嵌入策略（`SCsub` 的 `_embed_bcl_assemblies`）
- WASM 链接流程
- Mono 运行时初始化路径（`mono_host.cpp` 的 `#if defined(MONO_AOT_MODE)` 分支）

**结论**：H9 WASM 运行时失败是 **已有的 AOT/BCL 配置问题**，不是本次 P0/P1 修复引入的回归。本次修复的影响范围仅限 editor/tools 运行时（P1-#3/#6）和导出链路（P1-#7），不影响 WASM 导出模板的运行时行为。

---

## 五、验证建议

### 5.1 立即可执行的最小验证（手动清理 bin/）

```powershell
# 1. 彻底清理 bin/obj 和 bin/templates
Remove-Item -Recurse -Force "godot4.7_mono\bin\obj\modules\mono"
Remove-Item -Recurse -Force "godot4.7_mono\bin\templates\web"

# 2. 重新构建 Web 解释器模式
# （使用 build_web_interpreter.ps1 修复编码后，或直接 scons 命令）

# 3. 同步 WASM + JS 到 web_test
# 4. 重新跑 H9 测试
```

### 5.2 AOT 模块表修复后的完整验证

修复问题 2/3/4 后，H9 应能进入场景测试阶段。此时需验证：
- 23 个场景的 PASS/FAIL 结果
- `System.Runtime` / `System.Collections` 等 facade 解析成功
- `GodotSharp.dll` / `ProjectScripts.dll` 通过解释器加载成功

---

## 六、附录：证据链

### A.1 WASM 二进制字符串分析

```
[FOUND] [Mono] Hybrid AOT mode: AOT + Interpreter fallback (INTERP_LLVMONLY, deferred registration)
[FOUND] [Mono] AOT: Full AOT runtime initialized (no JIT compiler).
[FOUND] [Mono] AOT: Registering Full AOT modules...
[NOT FOUND] [Mono] Setting AOT mode to INTERP_LLVMONLY...
[NOT FOUND] [Mono] JIT runtime initialized
```

→ WASM 是 Hybrid AOT 模式编译，不是解释器模式。

### A.2 .o 文件字符串分析

| 文件 | "Full AOT runtime" | "Hybrid AOT" | "INTERP_LLVMONLY" | 时间戳 |
|------|-------------------|--------------|-------------------|--------|
| `mono_aot.web.template_release.wasm32.nothreads.o` | ❌ | ❌ | ❌ | 11:54:32 |
| `mono_host.web.template_release.wasm32.nothreads.o` | ❌ | ❌ | ❌ | 11:54:31 |
| `mono_aot_modules.web.template_release.wasm32.nothreads.o` | ❌ | ❌ | ❌ | 11:54:33 |

→ 当前 .o 文件是解释器模式编译（不含 AOT 字符串），但 WASM 是 AOT 模式 → scons 链接了旧的 WASM 而非重新链接。

### A.3 Mono 静态库字符串分析

8 个静态库（`libmonosgen-2.0.a` 等）中均不包含 "Hybrid AOT mode" / "Full AOT runtime initialized" 字符串 → 这些字符串只来自 Godot 源码，不来自 Mono 库。
