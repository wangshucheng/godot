# Mono Editor 项目状态总览

> **最后更新**：2026-07-26
> **当前里程碑**：B 级 P1-P7 全部完成 + H9 WASM 运行时修复完成 + P2 v2 typedef 重构完成
> **下一节奏**：v2 推进 P4 异步化 / A2 代码补全（待 GodotTools 移植）

---

## 一、项目结构

### 双树布局

| 树 | 路径 | 角色 | 远端 |
|----|------|------|------|
| 构建树 | `godot4.7_mono/` | 完整 Godot 4.7 源码 + Mono 模块改动 + csharp_test 测试项目 | `origin/4.7-mono`（领先 60 提交未推送） |
| 源码树 | `godot-mono-wasm/` | Mono 模块 + WASM 工具链的镜像副本（仅 `modules/mono/` + `tools/` + `docs/`） | 无远端（纯本地） |

### 同步机制

- 构建树 → 源码树：通过 `godot-mono-wasm/tools/sync_to_godot.py` 单向同步
- 同步范围：`modules/mono/*` + `tools/*` + 评审报告（按需）
- **不同步**：`docs/review_*.md`（评审报告仅存在于构建树，源码树不镜像）

---

## 二、已完成工作

### A 级（共享基础设施）— 全部完成

| 任务 | 提交 | 说明 |
|------|------|------|
| A1 脚本模板 | `2c945b9` 之前 | C# 脚本创建模板 |
| A2 代码补全 | — | **降级为 C 级**（spec [REV-#09]），等 GodotTools 移植 |
| A3 命名工具 | 已完成 | `utils/naming_utils.cpp` + `string_utils.cpp` |
| A4 SemVer 解析 | 已完成 | `editor/semver.cpp`（P4 消费 dotnet --version） |
| B0 共享基础设施 | 已完成 | `utils/mono_script_metadata.cpp`（[Export]/[Signal]/[Tool]/[GlobalClass] 读取） |

### B 级（按当前架构重写）— 全部完成

| 任务 | 提交 | 状态 |
|------|------|------|
| P1 [Export] + Inspector | `d6c74d1` | ✅ |
| P2 [GlobalClass] v1 | `af08236` | ✅ |
| P3 [Signal] 脚本信号 | `eb7da9e` | ✅ |
| P4 Mono Build 面板 | `48652f5` | ✅（同步阻塞，v2 异步化列为改进项） |
| P5 [Tool] 脚本 + 崩溃隔离 | `69b702a` | ✅ |
| P6 热重载文件监视 | `69b702a` | ✅ |
| P7 sdb 调试器最小集 | `69b702a` | ✅ |

### C 级（v2 节奏）

| 任务 | 提交 | 状态 |
|------|------|------|
| P2 typedef 重构 | `3547fcff27` | ✅ 已完成（2026-07-26） |
| P4 异步化 | — | ⏸️ 未开始 |
| A2 代码补全 | — | ⏸️ 等 GodotTools 移植 |

### H9 WASM 运行时修复 — 全部完成

| 提交 | 范围 |
|------|------|
| `a292f7a050`（构建树） | 5 个问题修复（AOT 表/Facades/probe/cache/编码） |
| `801b1ad`（源码树） | 镜像同步 |

**验证**：
- Web 解释器构建：WASM 46.6 MB，含 `INTERP_LLVMONLY` 字符串
- H9 23 场景测试：23/23 PASS，205/205 断言
- 微信小游戏 AOT 构建：67.13 MB WASM（18.09 MB ZIP）

---

## 三、v2 / v3 待办清单

### v2 节奏（高优先级）

| 任务 | 优先级 | 前置依赖 | 来源 |
|------|--------|---------|------|
| P4 异步化 | 高 | 无 | phase3 评审 §4 + spec §4.P4 |
| P5 .pdb 反查路径 | 中 | WASM 端 .pdb 可用性评估 | spec §4.P2.2 [REV-#06] |
| P5 崩溃隔离 v2 子 domain 评估 | 中 | Mono embedding appdomain 卸载语义评估 | spec [REV-#11] |
| P7 sdb spike 报告补全 | 低 | P7 已实施，spike 报告可能未补 | spec [REV-#07] |

### v2 可选延后

| 任务 | 优先级 | 前置依赖 | 来源 |
|------|--------|---------|------|
| B1 WASM clang++ workaround 集成到 SCsub | 可选 | emcc 工具链修复 | phase3 评审 §B1 |
| P2 全局类 pdb 反查（解除文件名==类名约束） | 可选 | .pdb 在 WASM 端可用 | spec §4.P2.2 |

### v3 探索项

| 任务 | 优先级 | 前置依赖 | 来源 |
|------|--------|---------|------|
| SourceGenerators 评估（编译期生成替代反射） | 探索 | B 级稳定 | spec [REV-#15] |

### 永久延后

| 任务 | 决策 | 来源 |
|------|------|------|
| A2 代码补全 | 等 GodotTools 移植时一起做 | spec [REV-#09] |

---

## 四、关键文档索引

| 文档 | 路径 | 用途 |
|------|------|------|
| 实施规范 | `docs/mono_editor_spec.md` | A/B/C 级任务实施规范（v1.2） |
| 基线分析 | `docs/steel-echo-red-star.md` | 项目立项前的基线代码分析 |
| 评审记录 | `docs/review_2026-07-25.md` | spec v1.0 评审（15 处修订） |
| 修订记录 | `docs/revision_log_2026-07-25.md` | spec v1.1 修订详情（[REV-#01] 至 [REV-#15]） |
| 第二阶段评审 | `docs/review_2026-07-25_phase2.md` | P5/P6/P7 + WASM 回归评审 |
| 第三阶段评审 | `docs/review_2026-07-26_phase3.md` | C 级延后任务可行性评估 |
| 终审报告 | `docs/review_2026-07-26_final.md` | P0/P1 修复评审 |
| H9 WASM 修复 | `docs/review_2026-07-26_h9_wasm.md` | H9 5 个问题根因与修复方案 |
| 测试审查 | `docs/csharp_test_review.md` | csharp_test 项目测试覆盖审查 |
| 修复轮次 | `docs/review_2026-07-26_fix_round.md` | 多轮修复追踪 |

---

## 五、构建与测试入口

### 构建

| 平台 | 脚本 | 说明 |
|------|------|------|
| Windows Editor | `scons platform=windows target=editor -j8` | 桌面编辑器构建 |
| Web Interpreter | `godot-mono-wasm/tools/build_web_interpreter.ps1` | WASM 解释器模式 |
| Web AOT | `godot-mono-wasm/tools/build_web_aot.ps1` | WASM AOT 模式 |
| 微信小游戏 | `godot-mono-wasm/tools/build_web_wechat.ps1` | 微信小游戏 AOT + 体积优化 |

### 测试

| 测试 | 脚本 | 说明 |
|------|------|------|
| C# 回归测试 | `run_csharp_test.ps1` | csharp_test 项目 23 场景 |
| Fuzz 测试 | `run_fuzz_test.ps1` | P5 崩溃隔离验收（10 异常脚本） |
| H9 WASM 测试 | `godot-mono-wasm/tools/run_h9_wasm_test.py` | 23 场景 205 断言 |
| Web 测试 | `run_web_test.py` | Web 平台运行时测试 |

### 两树同步

```bash
# 构建树修改后，同步到源码树
python godot-mono-wasm/tools/sync_to_godot.py
```

---

## 六、版本节奏

- **v1**（已完成）：A 级 + B 级 P1-P7
- **v1.1**（已完成）：spec 评审修订（15 处 [REV-#01] 至 [REV-#15]）
- **v1.2**（已完成）：P2 v2 typedef 重构 + H9 WASM 修复
- **v2**（进行中）：P4 异步化 + P5 .pdb 反查 + P5 崩溃隔离 v2 评估
- **v3**（探索）：SourceGenerators 评估
