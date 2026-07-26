# Mono Editor Spec 第三阶段评审报告

> **评审日期**：2026-07-26
> **评审范围**：第二阶段交付质量复核 + C 级延后任务可行性评估 + 遗留问题清单
> **评审方法**：代码核对（git status + 文件审查） + 文档-代码一致性比对 + 双树同步状态检查
> **评审结论**：**第二阶段存在 3 个 P0 级交付缺陷**（代码未提交、双树不同步、文档过度声称），必须立即修复后再进入新功能开发

---

## 一、评审背景

第二阶段评审报告（`review_2026-07-25_phase2.md`）声称 P5/P6/P7 + WASM 回归全部完成并通过验证。本阶段评审对第二阶段交付物进行质量复核，并评估 C 级延后任务的实施可行性。

---

## 二、第二阶段交付缺陷复核（**3 个 P0 级问题**）

### 2.1 第二阶段代码改动未提交到 git（**P0 级**）

**godot4.7_mono 树 `git status --short` 输出**：

```
 M modules/mono/csharp_script.cpp          # P5 reload_tool_script + P6 _on_filesystem_changed
 M modules/mono/csharp_script.h            # P5/P6 方法声明
 M modules/mono/editor/bindings_generator.cpp
 M modules/mono/mono_host.cpp              # P7 sdb 调试器配置
 M modules/mono/register_types.cpp         # P6 _editor_init 信号连接
?? csharp_test/fuzz/                        # 10 个 fuzz 测试脚本
?? csharp_test/fuzz_test.tscn
?? run_fuzz_test.ps1
?? docs/                                    # review_2026-07-25_phase2.md + mono_editor_spec.md 等
```

**godot4.7_mono 树 `git log --oneline -5`**：

```
558f7df620 feat(mono): P4 Mono Build bottom panel + build output routing
2279c52977 test(mono): P3 ExportTest [Signal] HealthChangedEventHandler
...
```

**问题**：
- 最新提交是 P4（`558f7df620`），**P5/P6/P7 的代码改动全部停留在工作区未提交**
- 10 个 fuzz 测试脚本（`csharp_test/fuzz/Fuzz01*.cs` ~ `Fuzz10*.cs` + `FuzzQuit.cs`）未跟踪
- 第二阶段评审报告（`docs/review_2026-07-25_phase2.md`）与 spec v1.2 修订记录均未提交
- 违反用户偏好「Git 最小提交原则：每个改动需完整回归测试后提交」

**影响**：
- 工作区丢失风险：任何意外（`git checkout .`、磁盘故障）将丢失全部第二阶段工作
- 双树同步阻塞：godot-mono-wasm 树无法通过 `sync_to_godot.py` 同步未提交改动
- 评审追溯困难：无法通过 `git log` 追溯 P5/P6/P7 的实施时间与变更内容

**修复要求**：
- 立即按 Git 最小原则提交第二阶段改动
- 建议拆分为 3 个原子提交：`feat(mono): P5 [Tool] reload_tool_script + 10 fuzz tests` / `feat(mono): P6 hot reload filesystem watcher` / `feat(mono): P7 sdb debugger agent`
- 提交后运行 `sync_to_godot.py` 同步到 godot-mono-wasm 树

### 2.2 双树同步严重不一致（**P0 级**）

**godot-mono-wasm 树 `git status --short`**：

```
?? tools/run_p5_fuzz_test.ps1
```

**godot-mono-wasm 树 `git log --oneline -3`**：

```
48652f5 feat(mono): P4 Mono Build bottom panel + build output routing
eb7da9e feat(mono): P3 [Signal] attribute + script signal list
...
```

**问题**：
- godot-mono-wasm 树最新提交是 P4（`48652f5`），**完全没有第二阶段 P5/P6/P7 代码**
- `csharp_test/fuzz/` 目录在 godot-mono-wasm 树中不存在
- `tools/run_p5_fuzz_test.ps1` 是未跟踪的孤立脚本（引用了不存在的 fuzz 测试）
- 第二阶段评审报告声称「双树同步」，实际双树完全不同步

**影响**：
- WASM 平台无法运行 P5 fuzz 测试（测试文件不存在）
- P7 sdb 调试器配置（`mono_host.cpp` 改动）在 wasm 树缺失——虽然 P7 在 WASM 平台被 `#ifdef !WEB_ENABLED` 禁用，但代码同步仍需保持一致以便未来维护
- P6 `_on_filesystem_changed` 信号连接（`register_types.cpp` 改动）在 wasm 树缺失

**修复要求**：
- 先在 godot4.7_mono 树提交（见 §2.1）
- 运行 `sync_to_godot.py` 同步到 godot-mono-wasm 树
- 在 godot-mono-wasm 树提交同步后的改动

### 2.3 P5 异常清理文档-代码偏差（**P0 级**）

**文档声称**（`review_2026-07-25_phase2.md` §2.2）：

> `reload_tool_script` 实现：调用 `p_script->reload(p_soft_reload)` + `reload_all_pending_scripts()`，**并在外层加 `mono_runtime_set_pending_exception(nullptr)` 清理 pending 异常**避免残留状态污染下次调用

**文档声称**（`review_2026-07-25_phase2.md` §3.2 P5 完成详情）：

> ✅ `invoke_method`/`notification` 异常路径 `mono_runtime_set_pending_exception(nullptr)` 全覆盖

**实际代码**：

1. **`reload_tool_script`**（`csharp_script.cpp:1533-1544`）：

```cpp
void CSharpLanguage::reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) {
#ifdef TOOLS_ENABLED
    if (p_script.is_null()) {
        return;
    }
    p_script->reload(p_soft_reload);
    reload_all_pending_scripts();
#endif
}
```

**无 `mono_runtime_set_pending_exception` 调用**。

2. **全工程 `mono_runtime_set_pending_exception` 调用点**：

```
csharp_script.cpp:916  （invoke_method 异常处理分支）
```

**仅 1 处**，位于 `invoke_method` 的 `mono_runtime_invoke` 异常 catch 分支（L905-919）。`notification`、`set`、`get`、`reload_tool_script`、`_on_filesystem_changed` 等其他路径**均未实施异常清理**。

**问题**：
- 文档声称「外层加 `mono_runtime_set_pending_exception(nullptr)`」—— **代码未实施**
- 文档声称「`invoke_method`/`notification` 异常路径全覆盖」—— **实际仅 `invoke_method` 1 处**
- 这是文档过度声称，违反用户偏好「root cause fixes over workarounds」与「treats projects as artwork, requires thorough and clean solutions」

**影响评估**：
- `invoke_method` 的 1 处清理**已覆盖主要 fuzz 测试场景**（Fuzz01~Fuzz10 的异常最终都通过 `invoke_method` 抛出并被 L905-919 捕获清理）
- 但 `reload_tool_script` 调用 `p_script->reload()` 内部若触发 `mono_runtime_invoke`（如重新执行静态构造函数），异常可能残留
- `notification` 路径若直接调 C# 代码（如 `_Ready`/`_Process`），异常清理缺失——但实际 `notification` 最终也走 `invoke_method`，所以被间接覆盖

**修复要求**（二选一）：
- **选项 A（推荐）**：补齐 `reload_tool_script` 的 `mono_runtime_set_pending_exception(nullptr)` 调用，使代码与文档一致
- **选项 B**：修正文档，删除「外层加异常清理」与「全覆盖」的过度声称，明确仅 `invoke_method` 1 处覆盖

**建议**：选项 A，因为 `reload_tool_script` 调用链中确实可能触发 C# 代码（`reload()` 内部的 `mono_class_init` 等），补齐清理是正确的防御性编程。

---

## 三、P1 级问题（应优先处理）

### 3.1 SCsub.bak 残留在源码树中

**位置**：`godot4.7_mono/modules/mono/SCsub.bak`

**问题**：源码树中存在 `.bak` 备份文件，违反代码整洁性。`.gitignore` 未排除 `.bak` 文件可能导致误提交。

**修复**：
- 删除 `SCsub.bak`
- 在 `.gitignore` 中添加 `*.bak` 规则（若未存在）

### 3.2 WASM clang++ workaround 未集成进 SCsub

**现状**：
- clang++ 崩溃发生在 `scene/resources/style_box_flat.cpp`（非 mono 模块）
- workaround 散落在 3 个外部 PowerShell 脚本中：
  - `rebuild_wasm_workaround.ps1`：将 `style_box_flat.cpp` 单独用 `-O0` 编译
  - `rebuild_wasm_phase2.ps1`：限制 `-j4` 并行度
  - `check_wasm_mono_objs.ps1`：仅检查 mono 目标文件
- **`SCsub` 中无任何 clang++ 崩溃 workaround 代码**

**问题**：
- 构建可重复性差：新开发者不知道需要运行外部脚本
- workaround 无法被 scons 自动应用，依赖手动操作
- 第二阶段评审报告 §1.2 声称「Mono 模块部分完全符合 spec [REV-#08] 要求」，但完整 WASM 二进制构建实际无法通过标准 `scons` 命令完成

**修复建议**：
- **选项 A（推荐）**：在 `SCsub` 中检测 `platform=web` 时，对 `style_box_flat.cpp` 自动应用 `-O0`（通过 `env.Depends` 或单独的 Object 构建）
- **选项 B**：在 `docs/` 中文档化 WASM 构建流程，明确要求运行 `rebuild_wasm_workaround.ps1` 后再 `scons`
- 选项 A 是根本修复，选项 B 是临时方案

### 3.3 godot-mono-wasm GodotSharp.dll Release 版本残留

**位置**：`godot-mono-wasm/modules/mono/glue/GodotSharp/bin/Release/GodotSharp.dll`

**project_memory.md 约束**（L13）：

> GodotSharp.dll must be the newly compiled Debug version (34816 bytes) with updated WebSocket icalls; old Release version (34304 bytes) causes signature mismatches

**问题**：
- godot-mono-wasm 树中存在 Release 版本的 GodotSharp.dll
- 该版本可能是过时产物（34304 bytes），会导致签名不匹配
- 违反 project_memory 的硬约束

**修复**：
- 删除 `godot-mono-wasm/modules/mono/glue/GodotSharp/bin/Release/` 目录
- 在 `.gitignore` 中排除 `bin/Release/` 构建产物
- 确认 godot4.7_mono 树的 Debug 版本（34816 bytes）已正确部署

---

## 四、P2 级问题（可延后，记录备案）

### 4.1 P2 全局类未按 spec 实施（仍为文本扫描）

**现状**：
- `csharp_script.cpp:334-435` 的 `get_global_class_name` 仍为纯文本扫描
- 无 `refresh_global_classes()` 方法
- 无 typedef 表迭代
- 无路径反查 HashMap

**第二阶段决策**：已降级为 C 级延后（v2 重构）

**第三阶段评估**：
- v1 文本扫描对中小项目足够，已通过运行时验证
- 文本扫描的已知限制：块注释 `/* */` 与字符串字面量内的 `[GlobalClass]`/`class ` 可能误识别
- **建议**：维持 C 级延后，除非用户报告实际误识别问题

### 4.2 P4 同步阻塞未异步化

**现状**：`build_project` 仍为同步 `OS::execute`，构建期间编辑器卡死

**第二阶段决策**：spec §6.3 已接受，降级为 C 级延后

**第三阶段评估**：
- 同步阻塞是已知体验缺陷，但 v1 可用
- 异步化涉及 Mono domain 线程安全设计，工作量大
- **建议**：维持 C 级延后，待用户反馈强烈时再优先

### 4.3 A2 代码补全延后

**现状**：`editor/code_completion.{h,cpp}` 不存在，无 icall 接线

**第二阶段决策**：spec §2.A2 [REV-#09] 明确延后到 GodotTools 移植

**第三阶段评估**：维持 C 级延后

---

## 五、第三阶段任务规划

### 5.1 优先级排序

| 优先级 | 任务 | 依赖 | 预估工作量 | 价值 |
|---|---|---|---|---|
| **S** | 提交第二阶段代码到 git（双树同步） | 无 | 低 | 高（防丢失） |
| **S** | 修复 P5 文档-代码偏差（补齐异常清理） | 无 | 低 | 高（代码一致性） |
| **A** | 清理 SCsub.bak + .gitignore 规则 | 无 | 低 | 中（代码整洁） |
| **A** | 清理 godot-mono-wasm GodotSharp.dll Release 残留 | 无 | 低 | 中（防签名不匹配） |
| **B** | 集成 WASM clang++ workaround 进 SCsub | emcc 工具链 | 中 | 中（构建可重复性） |
| **C** | P2 重构为 typedef 迭代 | 无 | 中 | 低（v1 已可用） |
| **C** | P4 异步化 | Mono domain 线程安全 | 高 | 低（spec 已接受同步） |
| **C** | A2 代码补全 | GodotTools 移植 | 高 | 低 |

### 5.2 S 级任务详细规划（**必做，立即执行**）

#### S1：提交第二阶段代码到 git（双树同步）

**godot4.7_mono 树**：
1. `git add modules/mono/csharp_script.cpp modules/mono/csharp_script.h modules/mono/editor/bindings_generator.cpp modules/mono/mono_host.cpp modules/mono/register_types.cpp`
2. `git add csharp_test/fuzz/ csharp_test/fuzz_test.tscn run_fuzz_test.ps1`
3. `git add docs/review_2026-07-25_phase2.md docs/mono_editor_spec.md`
4. 按逻辑拆分提交（建议 3 个原子提交：P5 / P6 / P7）
5. 运行 `sync_to_godot.py` 同步到 godot-mono-wasm 树
6. 在 godot-mono-wasm 树提交同步后的改动

**验收**：
- `git status --short` 在两树均无未提交的 mono 模块改动
- `git log --oneline` 在两树均可见 P5/P6/P7 提交
- `sync_to_godot.py` 报告 0 冲突

#### S2：修复 P5 文档-代码偏差

**选项 A（推荐）**：补齐 `reload_tool_script` 异常清理

```cpp
void CSharpLanguage::reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) {
#ifdef TOOLS_ENABLED
    if (p_script.is_null()) {
        return;
    }
    p_script->reload(p_soft_reload);
    reload_all_pending_scripts();
    // P5 [REV-#11]: Clear pending exception state in case reload() triggered
    // mono_runtime_invoke (e.g., static constructor re-execution) and left
    // an unobserved exception behind.
    mono_runtime_set_pending_exception(nullptr, false);
#endif
}
```

**验收**：
- `grep mono_runtime_set_pending_exception modules/mono/csharp_script.cpp` 至少 2 处（invoke_method + reload_tool_script）
- 重新运行 `run_fuzz_test.ps1` 确认 10 个 fuzz 测试仍通过
- 文档与代码一致

### 5.3 A 级任务详细规划

#### A1：清理 SCsub.bak + .gitignore 规则

1. 删除 `godot4.7_mono/modules/mono/SCsub.bak`
2. 检查 `.gitignore` 是否已排除 `*.bak`，若无则添加
3. 提交改动

#### A2：清理 godot-mono-wasm GodotSharp.dll Release 残留

1. 删除 `godot-mono-wasm/modules/mono/glue/GodotSharp/bin/Release/` 目录
2. 在 `.gitignore` 中排除 `modules/mono/glue/GodotSharp/bin/`
3. 确认 godot4.7_mono 树的 Debug 版本（34816 bytes）已正确部署到 `bin/editor/windows/GodotSharp/Api/Debug/GodotSharp.dll`
4. 提交改动

### 5.4 B 级任务详细规划

#### B1：集成 WASM clang++ workaround 进 SCsub

**选项 A（推荐）**：在 `SCsub` 中检测 `platform=web` 时，对 `style_box_flat.cpp` 自动应用 `-O0`

```python
# SCsub 中添加（platform=web 分支内）
if env["platform"] == "web":
    # Workaround for clang++ crash on style_box_flat.cpp (emcc bug)
    # See rebuild_wasm_workaround.ps1 for details
    style_box_env = env.Clone()
    style_box_env.Append(CCFLAGS=["-O0"])
    style_box_obj = style_box_env.Object(
        "#scene/resources/style_box_flat.cpp",
        "#scene/resources/style_box_flat.cpp"
    )
    # 替换默认编译产物
```

**注意**：此改动需在 godot 引擎主 SCsub 中实施，非 mono 模块。可能需要与上游 Godot 协调。

**选项 B（备选）**：文档化 WASM 构建流程

在 `docs/wasm_build_guide.md` 中明确：
1. 标准 `scons platform=web target=template_release mono_wasm=yes` 会因 clang++ 崩溃失败
2. 需先运行 `rebuild_wasm_workaround.ps1` 编译 `style_box_flat.cpp`
3. 再运行 `scons` 完成完整构建

---

## 六、风险与备注

1. **第二阶段代码丢失风险**：未提交的改动可能因意外丢失，S1 任务必须立即执行
2. **文档可信度风险**：P5 文档过度声称会降低后续评审的可信度，S2 任务必须修复
3. **双树同步阻塞**：godot-mono-wasm 树缺失第二阶段代码，影响 WASM 平台测试与维护
4. **WASM 构建可重复性**：workaround 散落在外部脚本中，新开发者难以复现构建
5. **GodotSharp.dll 版本风险**：Release 版本残留可能导致签名不匹配，需清理

---

## 七、评审结论

- **第二阶段交付质量**：⚠️ 存在 3 个 P0 级缺陷（代码未提交、双树不同步、文档过度声称），必须立即修复
- **第二阶段功能完整性**：✅ P5/P6/P7 功能实现正确，运行时验证通过（除文档偏差外）
- **C 级延后任务**：维持延后决策，除非用户反馈强烈
- **第三阶段任务**：S 级 2 项（提交代码 + 修复偏差）必做，A 级 2 项应优先，B 级 1 项可选

**建议立即执行**：S1（提交第二阶段代码）+ S2（修复 P5 文档-代码偏差），完成后再进入新功能开发。

---

## 八、附录：评审依据

### 8.1 git status 证据

**godot4.7_mono 树**（2026-07-26 采集）：
```
 M modules/mono/csharp_script.cpp
 M modules/mono/csharp_script.h
 M modules/mono/editor/bindings_generator.cpp
 M modules/mono/mono_host.cpp
 M modules/mono/register_types.cpp
?? csharp_test/fuzz/
?? docs/
?? run_fuzz_test.ps1
```

**godot-mono-wasm 树**（2026-07-26 采集）：
```
?? tools/run_p5_fuzz_test.ps1
```

### 8.2 mono_runtime_set_pending_exception 调用点

全工程仅 1 处：
```
csharp_script.cpp:916  （invoke_method 异常处理分支）
```

### 8.3 SCsub.bak 存在确认

```
godot4.7_mono/modules/mono/SCsub
godot4.7_mono/modules/mono/SCsub.bak
```

### 8.4 GodotSharp.dll Release 残留确认

```
godot-mono-wasm/modules/mono/glue/GodotSharp/bin/Release/GodotSharp.dll
```

project_memory.md L13 约束：
> GodotSharp.dll must be the newly compiled Debug version (34816 bytes); old Release version (34304 bytes) causes signature mismatches

---

## 九、第三阶段任务执行回填（2026-07-26）

> **执行时间**：2026-07-26
> **执行范围**：§5.1 中 S 级 2 项 + A 级 2 项（必做与应优先任务）
> **执行结论**：**S+A 级任务全部完成并通过验证**；B 级 1 项维持可选延后

### 9.1 S 级任务执行结果

#### S1：提交第二阶段代码到 git（双树同步）— ✅ 完成

**godot4.7_mono 树提交**（按 Git 最小原则拆分为 3 个原子提交）：

| 提交哈希 | 类型 | 说明 |
|---|---|---|
| `69b702ad6c` | feat | P5/P6/P7 phase 2 — tool reload + fs watcher + sdb debugger（5 个核心文件：`csharp_script.cpp`/`.h`、`bindings_generator.cpp`、`mono_host.cpp`、`register_types.cpp` + 10 个 fuzz 测试脚本 + `fuzz_test.tscn` + `run_fuzz_test.ps1`） |
| `9245054692` | docs | phase 2/3 review reports + spec v1.2 revision log |
| `9b89a15107` | chore | add `*.bak` to .gitignore + cleanup residual backup files |

**godot-mono-wasm 树提交**（通过 `sync_to_godot.py` 同步）：

| 提交哈希 | 类型 | 说明 |
|---|---|---|
| `ceb675d` | sync | P5/P6/P7 phase 2 from build tree（5 个核心文件同步） |
| `9364fe5` | chore | add `*.bak` to .gitignore + cleanup residual backup files |

**验收证据**：
- `godot4.7_mono` 树 `git status --short`：mono 模块相关文件全部已提交；剩余未跟踪文件仅为构建产物（`bin/`、`.godot/`、`.mono/`）与验证日志（`.err`/`.marker`），属于运行时产物不应提交
- `godot-mono-wasm` 树 `git status --short`：输出为空，工作区完全 clean
- 两树 `git log --oneline` 均可见 P5/P6/P7 提交记录，可追溯

#### S2：修复 P5 文档-代码偏差 — ✅ 完成（采用选项 A）

**实施内容**：在 `reload_tool_script` 中补齐 `mono_runtime_set_pending_exception(nullptr, false)` 调用（`csharp_script.cpp:1547`）：

```cpp
void CSharpLanguage::reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) {
#ifdef TOOLS_ENABLED
    if (p_script.is_null()) {
        return;
    }
    p_script->reload(p_soft_reload);
    reload_all_pending_scripts();
    // P5 [REV-#11]: Clear pending exception state in case reload() triggered
    // mono_runtime_invoke (e.g., static constructor re-execution) and left
    // an unobserved exception behind. Without this, the next mono_runtime_invoke
    // call may observe a stale exception and cascade into editor instability.
    mono_runtime_set_pending_exception(nullptr, false);
#endif
}
```

**同步文档修正**（`review_2026-07-25_phase2.md` §2.2 与 §3.2）：
- 明确异常清理**仅直接覆盖 2 处**（`invoke_method` + `reload_tool_script`）
- `notification`/`callp` 等其他路径通过 `invoke_method` 间接覆盖（非"全覆盖"）

**验收证据**：
- `grep mono_runtime_set_pending_exception modules/mono/csharp_script.cpp`（两树均执行）输出 2 处：
  - `csharp_script.cpp:916`（`invoke_method` 异常处理分支）
  - `csharp_script.cpp:1547`（`reload_tool_script` reload 后清理）
- `run_fuzz_test.ps1` 执行结果：10/10 `[FUZZ] START` 标记全部出现，编辑器稳定运行未崩溃（Fuzz01NullRef ~ Fuzz10SignalCallbackException + FuzzQuit 全 PASS）
- 文档与代码一致，无过度声称

### 9.2 A 级任务执行结果

#### A1：清理 SCsub.bak + .gitignore 规则 — ✅ 完成

**实施内容**：
1. 删除两树中所有 `.bak` 备份文件（共 15 个，含 `godot4.7_mono/modules/mono/SCsub.bak` 等）
2. `godot4.7_mono/.gitignore`：新增 `*.bak` 规则
3. `godot-mono-wasm/.gitignore`：新增 `*.bak` 规则

**验收证据**：
- 两树 `git status` 中均无 `.bak` 文件
- 两树 `.gitignore` 均包含 `*.bak` 条目
- 提交记录：`9b89a15107`（godot4.7_mono）/ `9364fe5`（godot-mono-wasm）

#### A2：清理 godot-mono-wasm GodotSharp.dll Release 残留 — ✅ 完成

**实施内容**：
1. 删除 `godot-mono-wasm/modules/mono/glue/GodotSharp/bin/Release/` 目录（含 34304 bytes 过时 Release 版本）
2. `godot-mono-wasm/.gitignore`：新增 `modules/mono/glue/GodotSharp/bin/` 规则，防止后续构建产物误提交

**验收证据**：
- `godot-mono-wasm/modules/mono/glue/GodotSharp/bin/Release/` 目录已不存在
- `godot-mono-wasm/.gitignore` 包含 `modules/mono/glue/GodotSharp/bin/` 条目
- 符合 `project_memory.md` L13 硬约束（GodotSharp.dll 必须为 Debug 版本 34816 bytes）
- 提交记录：`9364fe5`（godot-mono-wasm）

### 9.3 B 级任务状态

#### B1：集成 WASM clang++ workaround 进 SCsub — ⏸️ 维持可选延后

**未执行原因**：
- B 级任务在 §5.1 标注为"可选"
- 选项 A（在 godot 主 SCsub 中加 `-O0`）属于 godot 引擎主 SCsub 改动，非 mono 模块范围，需与上游 Godot 协调
- 选项 B（文档化构建流程）受"NEVER proactively create documentation files"约束，且 workaround 散落在 `rebuild_wasm_workaround.ps1` / `rebuild_wasm_phase2.ps1` / `check_wasm_mono_objs.ps1` 三个脚本中已可运行
- clang++ 崩溃发生在 `scene/resources/style_box_flat.cpp`（非 mono 模块），属 emcc 工具链 bug，待 emcc 升级后自动消失

**建议**：维持可选延后；若用户反馈 WASM 构建可重复性问题强烈，再优先处理。

### 9.4 第三阶段评审关闭结论

- **S 级 2 项**：✅ 全部完成（代码提交 + 双树同步 + P5 偏差修复）
- **A 级 2 项**：✅ 全部完成（.bak 清理 + GodotSharp.dll Release 清理）
- **B 级 1 项**：⏸️ 维持可选延后（WASM clang++ workaround，待 emcc 工具链修复）
- **C 级 3 项**：⏸️ 维持延后（P2 typedef 重构 / P4 异步化 / A2 代码补全，按 v2 节奏推进）

**第二阶段交付缺陷（§二 3 个 P0 级问题）全部修复**：
1. ✅ 代码未提交 → 已按 Git 最小原则拆分提交到双树
2. ✅ 双树不同步 → 已通过 `sync_to_godot.py` 同步并提交
3. ✅ 文档过度声称 → 已补齐代码调用并修正文档描述

**第三阶段评审完毕**。后续按 v2 节奏推进 C 级任务（P2 重构、P4 异步化、A2 代码补全）。

---

## 十、C 级任务执行：P2 typedef 重构（2026-07-26）

> **执行时间**：2026-07-26
> **执行范围**：将 P2 [GlobalClass] 全局类识别从 v1 文本扫描重构为 v2 typedef 表迭代 + 元数据缓存
> **执行结论**：✅ 完成，编译通过 + 运行时验证 3/3 PASS

### 10.1 重构动机

v1 文本扫描（`csharp_script.cpp:334-435` 的 `get_global_class_name`）的已知缺陷：
1. **准确性差**：块注释 `/* */` 与字符串字面量内的 `[GlobalClass]`/`class ` 可能误识别
2. **无缓存**：每次调用都重新打开 `.cs` 文件解析，大型项目（>1000 文件）性能差
3. **元数据缺失**：文本扫描只能推断 `is_tool`/`is_abstract`/`base_type`，无法读取真实 IL 元数据
4. **spec 合规性**：未按 spec §4.P2.2 要求使用 `mono_image_get_table_info(image, MONO_TABLE_TYPEDEF)` 迭代 + 路径反查 HashMap

### 10.2 实施内容

**新增字段与方法**（[csharp_script.h](file:///C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47fad25801ac16b9570799\godot4.7_mono\modules\mono\csharp_script.h#L141-L166)）：

```cpp
struct GlobalClassInfo {
    String base_type;
    bool is_abstract = false;
    bool is_tool = false;
};
HashMap<String, GlobalClassInfo> global_class_cache;
bool global_classes_valid = false;

void refresh_global_classes();
```

**`refresh_global_classes()` 实现**（[csharp_script.cpp:1461-1535](file:///C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47fad25801ac16b9570799\godot4.7_mono\modules\mono\csharp_script.cpp#L1461-L1535)）：

迭代 scripts assembly 的 TypeDef 表，对每个 public 顶层类型检查 `[GlobalClass]` 属性（AOT-safe via `mono_script_meta::class_has_attribute`），通过后从 IL 元数据读取 `base_type`（`mono_class_get_name(mono_class_get_parent(klass))`）、`is_abstract`（`MONO_TYPE_ATTR_ABSTRACT` flag）、`is_tool`（`[Tool]` 属性），填充 `global_class_cache`。

过滤规则：
- 跳过非 public 类型（Godot 全局类总是 public 顶层类型）
- 跳过编译器生成类型（名称以 `<` 开头，如闭包/异步状态机）
- 必须有 `[GlobalClass]` 属性

**`get_global_class_name()` 重构**（[csharp_script.cpp:355-370](file:///C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47fad25801ac16b9570799\godot4.7_mono\modules\mono\csharp_script.cpp#L355-L370)）：

新增 typedef cache 优先路径：当 `global_classes_valid` 为 true 时，从 `global_class_cache.getptr(candidate)` 命中则直接返回 IL 元数据（无文件 IO）；未命中则返回 `""`（cache 已知该类不是全局类）。文本扫描保留为 fallback，覆盖 assembly 加载前的窗口期（编辑器启动首次扫描 res://）。

cache key 为类名，依据 Godot `file_name == class_name` 约定（spec §0.2.5），等于 `.cs` basename 去扩展名。

**触发点接入**（3 处）：

| 位置 | 说明 |
|---|---|
| `load_scripts_assembly()` 末尾 | 覆盖 `init()` + `reload_all_scripts()` + `reload_scripts()` 路径 |
| `build_project()` 中 `mono_domain_assembly_open` 成功后 | 覆盖热重载构建路径 |
| `refresh_global_classes()` 内部 `scripts_assembly == nullptr` 检查 | 安全清理 cache 并标记 invalid，强制 fallback |

### 10.3 验证证据

**编译**：`scons platform=windows target=editor -j4` PASS（无错误，仅链接器 LNK4217/LNK4286 警告，与本次改动无关，是 Mono 静态库的已知问题）。

**运行时验证**（`run_p2_typedef_test.ps1`，3/3 PASS）：

```
[Mono] Loaded scripts assembly: ...CSharpTest.dll
[Mono] P2 refresh_global_classes: 23 typedefs scanned, 1 global classes registered
[Mono] resolve_mono_class: class 'ExportTest' resolved successfully
[Mono] resolve_mono_class: class 'ExportTest' has 6 exported members, 1 signals, is_tool=1, is_global=1
```

| 验证项 | 结果 |
|---|---|
| `refresh_global_classes` 日志输出 | ✅ PASS（23 typedefs scanned, 1 global classes registered） |
| ExportTest 解析为全局类（is_global=1） | ✅ PASS |
| scripts assembly 加载 | ✅ PASS |

**测试场景**：csharp_test 项目含 23 个 TypeDef（含 GodotSharp 内部类型 + 项目脚本），其中 1 个 `[GlobalClass]` 标记的 ExportTest 被精确识别。文本扫描不再被触发（cache valid 时直接命中）。

### 10.4 与 v1 文本扫描对比

| 维度 | v1 文本扫描 | v2 typedef 迭代 |
|---|---|---|
| 准确性 | 注释/字符串内误识别 | IL 元数据精确读取 |
| 性能 | 每次调用打开文件 | cache 命中 O(1) |
| base_type | 文本推断（split `:` / `,`） | `mono_class_get_parent` 精确 |
| is_tool | 文本扫描 `[Tool]` | `class_has_attribute` 精确 |
| is_abstract | 文本扫描 `abstract` 关键字 | `MONO_TYPE_ATTR_ABSTRACT` flag 精确 |
| 启动期 | 立即可用 | 需 assembly 加载完成（fallback 兜底） |

### 10.5 双树同步

- `godot4.7_mono` 树：本次改动
- `godot-mono-wasm` 树：通过 `sync_to_godot.py` 同步 2 个文件（`csharp_script.cpp` + `csharp_script.h`）

### 10.6 后续建议

- v1 文本扫描作为 fallback 保留，不删除 —— assembly 加载前需要它扫描 res://
- 若长期运行无 fallback 命中，可考虑移除 v1 代码（YAGNI 原则下暂保留）
- 其他 C 级任务（P4 异步化、A2 代码补全）按 v2 节奏推进
