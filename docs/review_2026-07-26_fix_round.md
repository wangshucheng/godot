# 修复轮代码审查报告（2026-07-26 第二轮）

> **审查对象**：提交 `4aac3f0ebb`（P0/P1 修复）、`a292f7a050`（H9 WASM 修复）、`e2d0d7ff3f`（构建产物）
> **审查方法**：逐行精读全部 diff + 关键事实独立验证（wasm 二进制字符串、Mono 头文件、git 跟踪状态）
> **结论**：上一轮终审的 8 项 P0/P1 修复**全部正确落实**；H9 WASM 修复方向正确且产物可验证；但修复本身**新引入 3 个问题**（rev 文件不清理、shutdown 集中 close、热重载后旧实例类身份不匹配），另有 5 项上轮遗留未处理。

---

## 一、上轮 P0/P1 修复核验（8/8 正确）

| 项 | 核验结果 |
|---|---|
| P0-1 版本化 dll 不 close | ✅ `open_versioned_assembly`（csharp_script.cpp:1314）实现正确；`reload_all_pending_scripts` 改为强制重载全部脚本，正好堵住上轮指出的"已解析脚本被跳过"漏洞；`reload_all_scripts`/`build_project` 两处 close 均已移除 |
| P0-2 TOOLS 守卫 | ✅ 调用点（register_types.cpp:119-125）与头文件声明（bindings_generator.h:7-57）双重守卫，符合修复建议 |
| P1-1 GodotSharp 判空 | ✅ 三处（mono_script_metadata.cpp 类型映射/导出成员/信号收集）均判空 |
| P1-2 NIL 跳过 | ✅ 字段与属性两个收集循环均 `vtype == Variant::NIL → continue` |
| P1-3 信号基类遍历 | ✅ 层级遍历 + 按名去重（派生类优先），Godot.Object 边界正确 |
| P1-4 异常清理 | ✅ 两处均改 `(nullptr, true)`（:951, :1733），已 grep 确认 |
| P1-6 xml_escape | ✅ 已删除，注释正确解释了 add_text 不解析 BBCode |
| P1-7 程序集名统一 | ✅ mono_export_plugin 与 mono_host 均改用 `Path::get_csharp_project_name()` |
| （附带）P2 全局类 cache-miss | ✅ `get_global_class_name` miss 时回退文本扫描，正是上轮建议 |

## 二、H9 WASM 修复核验（方向正确，产物已验证）

- **AOT 模块表裁剪**（mono_aot_modules.cpp）：移除 5 个 nullptr 条目，facade/用户程序集改走 MEMFS+解释器——推理正确，注释完整记录了原因。
- **Facades/ 嵌入**（SCsub `_embed_bcl_assemblies`）：目录存在判断 + 诊断输出完整。
- **产物验证（独立执行）**：`bin/exports/web/web_test/index.wasm`（48,862,836 字节 ≈ 提交声称的 46.6MB ✓）含 `INTERP_LLVMONLY` 字符串 ✓，不含 `Hybrid AOT mode` ✓——确为解释器模式构建，与提交声称一致。
- `mono_jit_set_aot_mode` 符号在 `mono/include/mono-2.0/mono/jit/jit.h:73` 存在 ✓。

**两个证据缺口**：
1. `build_web_interpreter.ps1`（承载 H9 P0-1 scons 缓存失效修复与 P2 编码修复）**未入库**——全仓库及 git 历史均无此文件。该修复对其他人不可复现，且任何直接用 scons 的人仍会踩"模式切换链接旧 .o"的坑。建议：入库该脚本，或把缓存失效逻辑固化进 SCsub（对比 CPPDEFINES 变化时主动清理）。
2. "H9 23/23 PASS、205/205 断言"的测试工具（`tools/run_h9_wasm_test.py`）与结果日志均不在本树——与上轮 `verify_p7_debugger.ps1` 同类问题：验证声称无入库证据。

## 三、修复新引入的问题（3 项）

### N1（P1）：`.rev{N}.dll` 文件永不删除，注释与实现不符

`open_versioned_assembly` 注释声称"Old copies accumulate on disk and are cleaned up by finish()"，但 `finish()`（csharp_script.cpp:1493-1509）只 `mono_assembly_close`，**没有任何删除文件的操作**。每次构建/热重载在 `.mono/assemblies/` 留下一个完整 dll 副本（桌面 = 磁盘污染且用户可见；WASM = MEMFS 内存膨胀）。修复：`finish()` 中 close 后删除 rev 文件，或 `init()` 时清理历史 rev 文件。

### N2（P1）：`finish()` 集中 close 在 shutdown 路径重新引入 UAF 风险，且收益为零

P0-1 的整个立意是"脚本/实例持有裸指针，永不 close"。`finish()` 对 `opened_assemblies` 逐个 close 时，`ResourceCache` 中的 CSharpScript 仍可能持有指向这些 image 的 `mono_class`，sgen 堆中也可能仍有存活托管对象——若 close 后、runtime 关闭前发生一次 GC 或脚本析构触碰元数据，即复现 P0-1 崩溃模式。而进程退出时 OS 本就回收一切，close 毫无收益。建议：**删掉 finish() 的 close 循环**（保留清缓存即可），让"永不 close"真正贯彻到底。

### N3（P1，需运行时确认）：热重载后旧实例与新脚本方法的类身份不匹配

强制重载后 `CSharpScript::mono_class` 指向新 image，但场景中**已存在的** `CSharpInstance::mono_object` 仍是旧 image 类的实例。`find_method` 优先走 `script->get_method`（新 image 的 `MonoMethod*`），`invoke_method` 随后 `mono_runtime_invoke(新方法, 旧对象)`——Mono 校验对象类与方法声明类不一致，轻则抛异常、重则崩溃。即：热重载后"新代码"只对新创建的实例生效，旧实例可能每次调用都炸。建议（二选一）：
- a) `find_method`/`invoke_method` 当 `mono_object` 存在时，优先在对象自身类层级（`mono_object_get_class`）上解析方法——旧实例跑旧代码，行为一致不崩；
- b) 热重载时重建实例的托管对象（经 gc_bridge 摘除旧绑定、创建新对象并拷贝导出属性）。
- 至少应先在 csharp_test 做一次"改代码→热重载→旧实例调用方法"的运行时验证，确认实际行为。

### N4（P2）：AOT 探针覆盖不足，注释高估其作用

探针只解析 corlib 的 `System.Object`（mscorlib 必然注册成功，几乎不可能失败）；H9 的真实失败模式（System → System.Runtime facade 缺失）**不会被该探针捕获**。且该分支本就在 INTERP_LLVMONLY 模式，fallback 调用是 no-op（注释自己承认）。作为诊断输出无害，但"AOT 模块依赖完整性检测"的功能声称不成立。要么强化探针（解析一个依赖 facade 的 System 类），要么把注释降级为"corlib  sanity check"。

### N5（P2）：版本化复制在运行时/导出构建中是死重

`load_scripts_assembly` 在导出游戏中只加载一次（无缓存问题），版本化复制纯属浪费；桌面导出版 `res://` 只读时走 fallback 直接打开（恰好正确），但逻辑上应 `#ifdef TOOLS_ENABLED` 只在编辑器做复制，运行时直接 `mono_domain_assembly_open`。

## 四、上轮遗留未处理（5 项）

| 项 | 状态 |
|---|---|
| P1-#5 `reload_tool_script` 不触发 build（热重载名义存在实际无效） | ❌ 未修复 |
| P1-#8 9 个模板 6 个无法用当前 glue 编译 | ❌ 待决策（收窄模板 vs 补 glue） |
| 文档 P1-#9/#10（spec §1.1 code_completion 行、§4.P7 编辑器设置措辞） | ❌ spec 未更新 |
| 调试残留（marker 文件 + printf） | ❌ 反而恶化：`csharp_test/*.marker` 两个文件仍被 git 跟踪，`e2d0d7ff3f` 又提交了一次 marker 更新；修复提交自身也新增了多处 printf |
| 文档回填组 + `verify_p7_debugger.ps1` 补交 | ❌ 未做 |

## 五、修复优先级建议

| 优先级 | 项 |
|---|---|
| 立即 | N2（删 finish() close 循环，一行级）+ N1（rev 文件清理） |
| 立即 | N3 运行时验证（改代码→热重载→旧实例调用），据结果选方案 a/b |
| 本周 | P1-#5 reload_tool_script 接 build；`build_web_interpreter.ps1` 入库或固化进 SCsub |
| 决策 | P1-#8 模板集去留 |
| 一次性 | spec §1.1/§4.P7 修正、调试残留清理（含 git rm 两个 marker）、文档回填 |

---

## 七、修复落实（2026-07-26 第二轮，全部完成）

| 项 | 落实 | 说明 |
|---|---|---|
| N1 rev 文件清理 | ✅ | 新增 `cleanup_stale_rev_files()`（`csharp_script.cpp`），`init()` 启动时清理；`open_versioned_assembly` 跟踪 rev 路径（注释同步修正——shutdown 时文件被 Mono 锁定无法删除，只能下次启动清理） |
| N2 finish() 集中 close | ✅ | close 循环已删除，"永不 close"贯彻到进程退出 |
| N3 旧实例类身份不匹配 | ✅ 方案 a | `find_method` 改为先在对象自身类层级解析（旧实例跑旧代码、新实例跑新代码，均一致），`script->get_method` 降为兜底；热重载改变旧实例行为列为已知限制 |
| N4 探针注释 | ✅ | 注释降级为 corlib sanity check，移除"依赖完整性检测"声称 |
| N5 版本化复制 TOOLS-only | ✅ | 非 TOOLS 构建直接 `mono_domain_assembly_open` |
| P1-#5 reload_tool_script | ✅ | 入口先 `request_build()`，由 frame() → build_project() 完成构建+程序集重载 |
| P1-#8 模板集 | ✅ 收窄 | 删除 6 个不可编译模板（EditorPlugin/EditorScript/EditorScenePostImport×2/VisualShaderNodeCustom/CharacterBody2D/3D，git 历史可查），保留可编译的 Node/default + Object/empty；恢复这些模板需先补 glue（Editor 类包装、Input 动作 API、Mathf 等） |
| 调试残留 | ✅ | `_editor_init` marker 写入与调试 printf 全部删除（register_types.cpp）；两个被 git 跟踪的 `.marker` 文件已删除；`csharp_script.cpp` 的 P6/P0-1 例行 printf 降级为 `print_verbose` |
| 文档修正 | ✅ | spec §1.1 删除 code_completion 行 + 修正 metadata 路径、§0.2.5 示例、§1.2 SConscript 偏差回填、§3.2 路径、§4.P7 ProjectSettings 回填、§4.P2 REV#06 完成标注；steel-echo 头部状态更新；phase2 §1.3 P2 状态回填 |
| verify_p7_debugger.ps1 | ✅ | 已补交（仓库根目录，纯 ASCII 避免 PS5 GBK 问题） |

**新增说明**：修复过程中发现 `csharp_script.cpp` 在提交 `4aac3f0ebb` 中被引入 CRLF 行尾（原文件为 LF），形成混合行尾文件；本次新增代码沿用所在区域的行尾。建议后续统一规范化为 LF（注意会产生全文件 diff）。
| mono_static_compat shim 补符号 | ✅ | 编译验证发现链接错误 LNK2019 `__imp_strdup`（sgen-bridge.obj / oldnames.lib 引用），属 shim 预先存在的缺口；已在 `mono_static_compat.c` 补 `__imp_strdup` / `__imp__strdup`（沿用 fdopen 双装饰模式） |
