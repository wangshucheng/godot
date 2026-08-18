# GodotTools 移植量评估（IDE 消息协议 + MSBuild 协作 + 热重载）

> **评估日期**：2026-08-18
> **评估目的**：v2 收尾计划 W4-D5 交付物——为 v3 决策提供上游 GodotTools（`modules/mono/GodotTools`，官方 C# 编辑器工具集）向本 fork 移植的工作量与优先级输入
> **参照输入**：`mono_editor_spec.md` §A2（[REV-2026-07-25-#09]）、`eval_2026-07-26_v3_sourcegenerators.md`、`v2_收尾计划.md` §四（W4 范围红线：语义级补全依赖 GodotTools/LSP，本期不做）
> **结论速览**：**P1 最小可用（IDE 消息服务器 + 构建诊断解析）≈ 2-3 人周，GO**；全量移植（含 ScriptManagerBridge 完整热重载）6-9 人周，**建议留 v3 分阶段**，热重载桥因 GC bridge 耦合列为高风险项

---

## 一、评估范围与现状基线

上游 GodotTools 是 Godot 官方 `modules/mono` 内的 C# 编辑器工具集（自身是一个 netstandard/net6 多项目工程），核心含四大块：

| 上游组件 | 职责 |
|----------|------|
| **IdeMessaging / MessageServer** | 引擎内 TCP 消息服务器，与 IDE 插件（VS/VSCode/Rider 官方 Godot 插件）双向通信 |
| **MSBuild Connector + BuildInfo** | `dotnet build` 编排、诊断（错误/警告）结构化解析、构建输出面板 |
| **GodotSharpTools（ProjectUtils）** | 基于 Microsoft.Build 的 .sln/.csproj 结构化读写（增删项、多目标框架） |
| **ScriptManagerBridge** | 热重载协作：脚本拓扑排序、实例状态恢复、程序集切换通知 |

### 本 fork 已具备的对应能力（移植基线）

| 能力 | 本 fork 实现 | 位置 | 与上游差距 |
|------|-------------|------|-----------|
| .csproj/.sln 生成 | **字符串模板拼接**，单 csproj 固定 netstandard2.0，GodotSharp 引用写死 Debug 目录 HintPath | `csharp_script.cpp:2160-2256`（`ensure_project_file`） | 上游用 MSBuildWorkspace 结构化操作，支持多 TFM/增删 Item |
| 构建执行 | `dotnet build <csproj>` 经 `OS::execute` 同步执行；**W3 已异步化**（worker 线程 + 主线程邮箱轮询 + 失败降级 + 请求合并） | `csharp_script.cpp:2259`（`build_project`）/ `2450`（`build_project_async`） | 上游 BuildInfo 支持多配置（Debug/Export*）、取消、增量状态机 |
| 构建输出面板 | MonoBuildPanel 底部 dock，`append_output`/`set_status` 路由，headless 降级 printf | `editor/mono_build_panel.cpp` | 上游输出行可点击跳转（诊断定位）——本 fork 明确列为"后续"（spec P4 v2 项） |
| 热重载 | `open_versioned_assembly`（版本化 dll 副本，避免 dotnet 重写占用）+ `reload_all_pending_scripts`（全脚本重载） | `csharp_script.cpp:1449 / 1715` | 上游 ScriptManagerBridge 做拓扑排序与实例字段恢复；本 fork 是"整程序集全量重载"，实例状态不保留 |
| 外部 IDE 触发构建 | 文件监视去抖：external IDE「写+stat+改名」连续触发被 build_pending 合并 | `csharp_script.cpp:1947-1950` | 仅被动响应，无主动通知 IDE（构建结果/运行状态） |
| IDE 消息协议 | **无**（仅通用 `text_editor/external/use_external_editor` 设置） | `editor_settings.cpp:876` | **完全缺失**——IDE 插件无法与本引擎通信 |
| 调试器挂接 | `dotnet/debugger/enabled` + `port 55555` 全局配置（sdb attach 入口） | `csharp_script.cpp:1653-1654` | 上游有完整 debugger agent 协作；本 fork 仅配置位 |

---

## 二、重点分析（一）：IDE 消息协议

### 2.1 上游架构

```
IDE 插件 (VSCode/VS/Rider Godot add-in)
     │  TCP localhost:<port>，自定义帧协议
     ▼
MessageServer（引擎内，GodotTools C# 侧）
     │  GodotObject 消息总线 / EditorPath 通知
     ▼
编辑器（打开文件、播放/停止、场景跳转、脚本创建）
```

- **传输**：TCP localhost，帧 = `id(content-length) content` 文本协议（非 HTTP），启动时握手交换 capabilities
- **典型消息**：`OpenFile`（带光标行列）、`Play`/`Stop`（IDE 工具栏驱动编辑器运行）、`GodotVersion`、`SendEditorThemeSettings`（IDE 侧主题跟随）
- **引擎 → IDE**：构建启动/完成通知、诊断结果推送、运行日志转发
- **价值**：把"编辑代码（IDE）"与"运行/调试（编辑器）"双向打通——这是上游 C# 体验的核心粘合层

### 2.2 移植要点与工作量

| 子项 | 内容 | 估算 |
|------|------|------|
| MessageServer | TCP 监听 + 帧编解码 + 握手 + 多客户端管理。可从上游 `IdeMessaging/` 直接抄，代码量小（~1.5k 行），但依赖上游 GodotSharp 的 `GodotObject/Node` API 面 | 1-1.5 周 |
| 消息命令集（引擎侧） | OpenFile/Play/Stop/ReloadProject 等命令落到本 fork 编辑器 API | 0.5-1 周 |
| IDE 插件侧 | 最小 VSCode 扩展（打开文件 + 触发播放）。**可改造上游 vscode 插件而非新写** | 1-2 周（可选，非引擎仓库范围） |

### 2.3 风险

1. **GodotSharp API 面不匹配（最大风险）**：上游 MessageServer 是 C# 写的，依赖官方 `GodotSharp`（Node/Timer/Thread/StreamPeer 等）。本 fork GodotSharp 是自研精简面（仅 GodotObject/GodotNode/Attributes 等手写子集，见 `glue/GodotSharp/*.cs`），移植需**先补齐 C# 侧网络 + 线程 + 文件监视的绑定**，或改为 **C++ 侧实现**（`StreamPeerTCP` 在引擎内现成）——推荐后者，绕开 C# API 面缺口。
2. **WASM 约束**：浏览器无 TCP 监听。消息服务器属 TOOLS 编辑器能力，桌面/服务器编辑器专用，WASM 导出不受影响（需 `#ifdef TOOLS_ENABLED && !WEB_ENABLED` 门控）。
3. **帧协议兼容性**：若目标是复用上游现成 IDE 插件，必须逐字节兼容上游帧格式与握手序列；若自研插件，可简化协议但失去"开箱即用 VSCode/Rider"生态。

---

## 三、重点分析（二）：MSBuild 协作

### 3.1 现状：够用但脆

- csproj 为**一次性字符串模板**（`csharp_script.cpp:2186`）：文件已存在则跳过，**不会随项目演进更新**（新增 .cs 靠 SDK 隐式包含，尚可；但引用/GodotSharp 路径变化、多目标、Analyzer 配置无法演化）
- `.sln` GUID 由项目名 md5 派生（L2218-2222），非真实确定性 GUID——多 IDE 打开重生成时可能漂移
- 构建输出是**纯文本行流**，错误/警告不结构化——"诊断解析可点击跳转"正是 v2 计划明确延后的 spec P4 v2 项
- 已有优势：W3 异步化 + 请求合并 + 降级路径已就位（`build_project_async`），MSBuild 协作的"执行层"不弱于上游；差距集中在**生成层（结构化项目操作）**与**消费层（诊断结构化）**

### 3.2 移植要点与工作量

| 子项 | 内容 | 估算 | 优先级 |
|------|------|------|--------|
| 诊断结构化解析 | 解析 `dotnet build` stdout 的 CSxxxx/warning 行 → `MSBuildDiagnostic{file,line,col,severity,code,message}` → MonoBuildPanel 可点击跳转（双击打开 ScriptEditor 定位行列） | 0.5-1 周 | **P1（先做）** |
| 构建通知推送 IDE | 构建开始/结束/失败事件经消息服务器转发（依赖 §二） | ~2 天（随消息服务器附带） | P1 |
| ProjectUtils 结构化项目操作 | 上游 `GodotSharpTools` 用 Microsoft.Build 读改写 csproj（增删 Compile Item、多 TFM、条件组）。本 fork 当前无此需求（隐式包含已覆盖） | 1.5-2 周 | P2（有需求再上） |
| 多配置构建（Export Debug/Release） | 上游 BuildInfo 按 export preset 切 `-c Release` 等 | 1 周 | P2（随导出链路演进） |

**注**：诊断解析不需要移植上游 MSBuild Connector 全套——上游用 MSBuild 二进制日志/自定义 logger，本 fork 直接正则解析 `dotnet build` 标准输出即可达到 90% 效果（CS 编译器输出格式稳定），成本远低于上游方案。

---

## 四、重点分析（三）：热重载协作（ScriptManagerBridge）

### 4.1 现状与差距

本 fork 热重载链：`build_pending → build_project(_async) → open_versioned_assembly（版本化 dll）→ reload_all_pending_scripts（全量）`。上游 ScriptManagerBridge 额外做：

- **脚本依赖拓扑排序**：基类先于派生类重载，避免中间态类型缺失
- **实例状态恢复**：重载时把旧实例字段值迁移到新类型实例（`GetScriptInstancePropertyValues` → 替换 → 回填）
- **选择性重载**：仅重载受影响脚本，未变脚本不动

### 4.2 工作量与风险

- 估算 **2-3 人周**
- **高风险**：P5 子 domain 评估（`eval_2026-07-26_p5_subdomain.md`）已确认 GC bridge 与全局 domain 指针强耦合——ScriptManagerBridge 的实例替换同样操作 CSharpInstance 裸 `MonoObject*`，与本 fork W3 硬约束（"Mono 重载必须回主线程"）叠加，出错面大
- csharp_test 场景 11（HotReload）当前 PASS，基线可用；**收益/风险比不佳，建议 v3 观望**（除非用户实际反馈"改代码丢状态"成为痛点）

---

## 五、移植量汇总与 v3 建议

### 5.1 汇总表

| 阶段 | 内容 | 工作量 | 依赖 | 建议 |
|------|------|--------|------|------|
| **P1-a** | MSBuild 诊断结构化 + 面板可点击跳转 | 0.5-1 周 | 无 | **v3 早期做**（v2 spec P4 遗留项，独立可交付） |
| **P1-b** | 消息服务器（C++ 实现，兼容上游帧协议） | 1-1.5 周 | 引擎内 StreamPeerTCP | **v3 核心**（IDE 集成地基） |
| **P1-c** | 引擎侧命令集 + 构建事件推送 | 0.5-1 周 | P1-b | 随 P1-b |
| P2-a | IDE 插件（改上游 VSCode 插件） | 1-2 周 | P1-b/c 帧兼容 | 按需（内部可先用自研轻量插件） |
| P2-b | ProjectUtils 结构化项目操作 | 1.5-2 周 | 无 | 触发条件：出现多 TFM/Analyzer 需求 |
| P3 | ScriptManagerBridge 完整热重载 | 2-3 周 | 无 | **观望**（高风险，基线已可用） |

**P1 合计 ≈ 2-3 人周**（单人、含联调）；全量 6-9 人周。

### 5.2 决策建议

1. **v3 立项只含 P1**：诊断跳转 + 消息服务器。两者把"C# 开发体验"从"编辑器内看文本日志"升级为"IDE 协作"，是上游 GodotTools 价值密度最高的 30%。
2. **消息服务器走 C++ 侧**（`modules/mono/editor/` 新增 `ide_message_server.{h,cpp}`，StreamPeerTCP + 帧解析），避免被自研 GodotSharp 精简 API 面拖住；帧格式对齐上游以保留复用其 IDE 插件的可能。
3. **热重载桥不立项**，与 P5 子 domain NO-GO 决策同一逻辑：耦合深、基线可用、无用户痛点驱动。
4. 与 SG（SourceGenerators）路线正交：v3 若同时推进 SG 全量，`ScriptRegistry`（W5 PoC）注册表可由消息服务器暴露给 IDE 做全局类补全数据源（远期加分项，不纳入估算）。

---

*本评估基于 2026-08-18 双树状态（源树 HEAD `9325c71`）。上游代码量按 Godot 4.x `modules/mono/GodotTools` 公开仓库经验值估算，落地前建议对目标上游 tag 做一次精确 diff 复核。*
