# 微信小游戏工作流（2048 验证案例）

本文档说明如何用 **2048 小游戏** 验证从 Godot 4.7 Mono 项目到微信小游戏的完整工作流。

## 一、工作流总览

```
godot4_7_mono/
├── bin/windows/godot.windows.editor.dev.x86_64.exe ← 编辑器（构建产物 1，dev 构建）
├── templates/web/godot.web.template_release.wasm32.nothreads.zip  ← 导出模板（构建产物 2）
├── test_game_2048/                                ← 2048 游戏源码（C#）
│   ├── project.godot
│   ├── Main.cs                                    ← 游戏主逻辑（C#，非 GDScript）
│   ├── Main.tscn
│   └── export_presets.cfg
├── exports/web_2048/                              ← Web 导出包（构建产物 3）
│   ├── Game2048.html
│   ├── Game2048.js
│   ├── Game2048.wasm                              ← 48 MB，走分包或 CDN
│   ├── Game2048.pck
│   └── godot.web.template_release.wasm32.nothreads.data  ← 19 MB，走分包或 CDN
└── wechat/
    ├── adapter/                                   ← 微信适配层源码
    │   ├── game.js
    │   └── wechat_adapter.js                      ← polyfill 模块（WA/FS/Canvas/Touch/Keyboard 等）
    ├── convert_to_wechat.py                       ← Web → 微信 转换脚本
    ├── build/                                     ← 微信小游戏产物（构建产物 4）
    │   ├── game.js                                ← 主入口
    │   ├── game.json                              ← 小游戏配置（含 subpackages）
    │   ├── project.config.json                    ← 开发者工具配置（appid + packOptions.include）
    │   ├── project.private.config.json            ← 私有配置（优先级高于 public）
    │   ├── index.js                               ← Godot 引擎 + 5 处修补
    │   ├── index.pck                              ← 2048 游戏资源包
    │   ├── wasm_pkg/Game2048.wasm.br              ← 分包：压缩后的 WASM（默认模式）
    │   ├── data_pkg/*.dat                         ← 分包：原始二进制 .data
    │   ├── Game2048.audio.worklet.js
    │   ├── Game2048.audio.position.worklet.js
    │   └── adapter/
    │       ├── game.js
    │       └── wechat_adapter.js
    └── README.md                                  ← 本文件
```

### 四层产物关系

| 层级 | 路径 | 用途 |
|------|------|------|
| 编辑器 | `bin/windows/godot.windows.editor.dev.x86_64.exe` | 开发游戏、导出 PCK（dev 构建） |
| 模板 | `templates/web/godot.web.template_release.wasm32.nothreads.zip` | Web 导出依赖（含 .data 修复） |
| Web 导出 | `exports/web_2048/` | 标准 Web 运行包（HTML5） |
| 微信小游戏 | `wechat/build/` | 微信开发者工具导入目录 |

### 加载策略（两种模式）

| 模式 | 命令行 | .wasm/.data 来源 | 适用场景 |
|------|--------|------------------|---------|
| **分包模式（默认）** | `--no-cdn-only`（默认） | `wasm_pkg/Game2048.wasm.br` + `data_pkg/*.dat` | 自包含，手机预览无需外部 CDN |
| CDN 模式 | `--cdn-only --cdn-url <URL>` | CDN 下载并缓存到 `USER_DATA_PATH` | 主包极小，但需 CDN 在线且手机可达 |

## 二、前置准备

### 1. 安装微信开发者工具

下载地址：https://developers.weixin.qq.com/miniprogram/dev/devtools/download.html

- 选择 **稳定版 Stable Build**（Windows 64 位）
- 安装完成后用微信扫码登录

### 2. 准备小游戏 AppID

⚠️ **必须使用真实 AppID，不能用 `touristappid` 游客模式**。

- DevTools 2.02.2607222+ 对游客 AppID 的处理存在回归：项目类型识别错误，导致 `game.json` 中的 `subpackages` 配置被忽略，所有文件被打成名为 `__FULL__` 的主包，超过 4 MB 限制时报 `subpackage __FULL__ source size exceed max limit 4096KB`。
- 当前 `project.config.json` 已配置真实 AppID `wxc07c26935264a5e5`，导入时直接读取，无需手动填写。
- 若要切换 AppID，需同时在 `project.config.json` 与 `project.private.config.json` 中修改（私有配置优先级更高，DevTools 自动生成时可能用错误默认值覆盖）。

### 3. 准备本地 CDN 服务器（仅 `--cdn-only` 模式需要）

默认分包模式（`--no-cdn-only`）下，`.wasm.br` 和 `.data` 已经在分包内，**不需要 CDN 服务器**，手机预览自包含。

仅当使用 `--cdn-only` 模式时才需要本地 CDN 服务器模拟：

```powershell
# 在 exports/web_2048 目录下执行
cd "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\exports\web_2048"

# 启动静态文件服务器，监听 8000 端口
python -m http.server 8000
```

CDN 文件路径为：
- `http://localhost:8000/Game2048.wasm`
- `http://localhost:8000/godot.web.template_release.wasm32.nothreads.data`

> 转换脚本 `--cdn-url` 仅在 `--cdn-only` 模式下生效；分包模式下传入 `--cdn-url` 会被忽略并打印 `[INFO] --no-cdn-only mode: ignoring --cdn-url=...`。

## 三、导入微信开发者工具

### 1. 打开微信开发者工具

- 选择「小游戏」分类
- 点击「导入项目」

### 2. 配置导入参数

| 字段 | 值 |
|------|---|
| 项目目录 | `C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\build` |
| AppID | `wxc07c26935264a5e5`（已配置在 `project.config.json`，导入后自动读取；不要使用 `touristappid`） |
| 项目名称 | `2048-minigame`（自动读取） |

### 3. 启动本地 CDN 服务器（仅 `--cdn-only` 模式需要）

默认分包模式跳过此步。仅当 `wechat/build/` 是用 `--cdn-only` 模式生成时，才需要在另一终端运行：

```powershell
cd "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\exports\web_2048"
python -m http.server 8000
```

### 4. 点击「编译」按钮

观察开发者工具的「Console」面板，预期输出（分包模式）：

```
[WeChat Adapter] USER_DATA_PATH = wxfile://...
[WeChat Adapter] WA ctx: WXWebAssembly=yes, native=yes   ← 或 native=no（视上下文）
[WeChat Adapter] WebAssembly polyfill installed (path + buffer support)
[WeChat] Subpackage loaded: wasm_pkg, res=...
[WeChat] WASM in subpackage: wasm_pkg/Game2048.wasm.br
[WeChat] Subpackage loaded: data_pkg, res=...
[WeChat fetch F5] Strategy 1 OK: ... bytes (... chunks)  ← .data 通过 fd-based 读取
[WeChat Adapter] All polyfill modules loaded successfully.
Godot Engine v4.7... — Mono runtime initialized (static linking): 6.12.0.206
[2048] _Ready started (dynamic build)
[2048] UI built OK
[2048] Game initialized
```

## 四、预期运行效果

### 操作方式

- **键盘方向键**：上下左右移动方块（仅 PC 微信客户端预览生效，DevTools 模拟器不响应物理键盘，见下文「键盘输入限制」）
- **触屏滑动**：在游戏区域内滑动（移动端真机 / DevTools 模拟器触摸模式）
- **R 键 / Enter 键**：游戏结束后重启

### 键盘输入限制（重要）

| 测试环境 | 物理键盘方向键 | 触摸滑动 |
|---------|---------------|---------|
| DevTools 模拟器 | ❌ 不响应（框架拦截事件） | ✅ |
| PC 微信客户端预览 | ✅ 通过 `wx.onKeyDown` 双路径生效 | ✅ |
| 手机真机预览 | N/A | ✅ |

DevTools 模拟器的物理键盘事件被框架层完全拦截，无法到达游戏上下文，`wx.onKeyDown` 在模拟器中是 stub 函数。这是平台限制，不是适配层 bug。**测试键盘功能请用 PC 微信客户端的「预览」功能**（扫码登录后在手机微信中打开）。

适配层实现了双路径键盘监听以覆盖 PC 客户端：
1. `wx.onKeyDown`（PC 客户端原生 API，模拟器为 stub）
2. `window.addEventListener('keydown')` + `document.addEventListener` + canvas 三级 DOM 监听（兜底）

### 游戏功能

- 4×4 网格，初始 2 个方块（2 或 4）
- 同方向滑动合并相同数字
- 合并后生成新方块，分数累加
- 出现 2048 即胜利（可继续）
- 无法移动时显示「Game Over」面板，点击重启

### 分数持久化

- 使用 `ConfigFile` 存储到 `user://2048.cfg`
- 微信环境下通过 `wx.setStorageSync` 适配层自动转存

## 五、完整重新构建工作流

如果修改了 2048 游戏代码或适配层，按以下步骤重建：

### 步骤 1：重新导出 Web 包

```powershell
$projectRoot = "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono"
& "$projectRoot\bin\windows\godot.windows.editor.dev.x86_64.exe" `
    --headless `
    --path "$projectRoot\test_game_2048" `
    --export-release "Web" `
    "$projectRoot\exports\web_2048\Game2048.html"
```

### 步骤 2：重新转换为微信小游戏

默认分包模式（推荐，自包含、手机预览无需 CDN）：

```powershell
$projectRoot = "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono"
python "$projectRoot\wechat\convert_to_wechat.py" `
    --source "$projectRoot\exports\web_2048" `
    --output "$projectRoot\wechat\build"
# 注意：默认 --no-cdn-only，--cdn-url 即便传入也会被忽略
```

CDN 模式（主包极小，但需 CDN 在线）：

```powershell
python "$projectRoot\wechat\convert_to_wechat.py" `
    --source "$projectRoot\exports\web_2048" `
    --output "$projectRoot\wechat\build" `
    --cdn-only `
    --cdn-url "http://localhost:8000/"
```

### 步骤 3：在微信开发者工具中重新编译

- 点击「编译」按钮（或 Ctrl+B）
- 如有缓存问题，先「清缓存 → 全部清除」再编译
- 修改 `wechat/adapter/wechat_adapter.js` 后，需重新运行步骤 2 同步到 `wechat/build/adapter/`，或手动 `Copy-Item` 覆盖

## 六、常见问题排查

### Q1：`WXWebAssembly is not defined` 或 `subpackage __FULL__ source size exceed max limit 4096KB`

**原因**：开发者工具版本过低、AppID 类型不对、或 `compileType` 配置错误导致项目类型识别失败。

**解决**：
- 升级开发者工具到最新稳定版（实测 2.02.2607232+ 可用）
- **必须使用真实 AppID**（`wxc07c26935264a5e5`），不能用 `touristappid` 游客模式 —— 游客模式会触发 DevTools 项目类型识别回归，`subpackages` 配置被忽略，所有文件打成 `__FULL__` 主包超限
- 确认 `project.config.json` 中 `compileType` 为 `"minigame"`（全小写，官方合法值）
- 确认 `project.private.config.json` 中 `compileType` 也为 `"minigame"`（私有配置优先级高于公共配置，DevTools 自动生成时可能用错误默认值覆盖）
- 确认 `project.config.json` 的 `packOptions.include` 包含 `wasm_pkg` 和 `data_pkg` 两个 folder，防止打包时被过滤
- 确认导入时选择的是「小游戏」分类

### Q2：CDN 下载失败（404 / 超时）

**原因**：本地 HTTP 服务器未启动，或端口/路径不对。

**解决**：
- 确认 `python -m http.server 8000` 正在运行
- 浏览器访问 `http://localhost:8000/Game2048.wasm` 验证可下载
- 检查 `wechat/build/index.js` 中 `_cdnBase` 变量是否为 `http://localhost:8000/`
- 微信开发者工具中：「详情 → 本地设置 → 不校验合法域名」必须勾选

### Q3：白屏，无任何输出

**原因**：`adapter/wechat_adapter.js` 加载失败，或 `index.js` 修补失败。

**解决**：
- 检查 `wechat/build/game.js` 是否包含 `require('./adapter/wechat_adapter.js')`
- 检查 `wechat/build/index.js` 是否包含 `=== BEGIN WeChat Patch` 标记
- 在 Console 中输入 `typeof WebAssembly` 应返回 `"object"`
- 输入 `typeof wx` 应返回 `"object"`

### Q4：触摸无响应

**原因**：适配层触摸事件未正确转发到 Godot，或游戏未处理 `InputEventScreenDrag`。

**解决**：
- 确认 `wechat_adapter.js` 的 TouchEvents 模块已注册 `wx.onTouchStart/Move/End/Cancel`
- 2048 游戏使用 `Input.IsActionJustPressed("ui_up/down/left/right")`，依赖适配层把触摸滑动转换为 `ui_up/down/left/right` 输入动作（见 `Main.cs` 的 `_Process`）
- 开发者工具中「模拟 → 触摸」模式可模拟滑动

### Q5：CDN 缓存不生效，每次都重新下载

**原因**：`wx.env.USER_DATA_PATH` 路径变化，或 `wx.getFileSystemManager().accessSync` 异常未捕获。

**解决**：
- 检查 Console 中 `[WeChat] Loading package from cache` 是否出现
- 确认 `wx.getFileSystemManager().writeFile` 成功回调触发
- 在 `wechat_adapter.js` 的 `_resolveFilePath` 函数中加 `console.log` 调试

### Q6：内存不足 / WASM 加载失败

**原因**：微信小游戏环境内存上限较低（iOS 约 1GB，Android 因设备而异）。

**解决**：
- 重新构建 Web 模板时启用 LTO：`scons p=web target=template_release lto=thin`
- 重新构建时关闭 SIMD（如目标设备不支持）：`scons p=web wasm_simd=no`
- 减小 `initial_memory`：`scons p=web initial_memory=64`

### Q7：DevTools 模拟器键盘方向键不响应

**原因**：DevTools 模拟器框架层完全拦截物理键盘事件，`wx.onKeyDown` 在模拟器中是 stub 函数，无法到达游戏上下文。这是平台限制，不是适配层 bug。

**解决**：
- 测试键盘功能请用 **PC 微信客户端预览**（开发者工具点「预览」按钮 → 手机微信打开 → 在 PC 微信客户端中扫码登录 → 即可用物理键盘操作）
- DevTools 模拟器内只能用「模拟 → 触摸」模式滑动测试
- 适配层已实现 `wx.onKeyDown` + `window/document/canvas` 三级 DOM 监听双路径，PC 客户端两条路径都生效

### Q8：`invalid CIL image` 或 mscorlib.dll 加载失败

**原因**：DevTools `readFile({offset, length})` 存在 offset bug —— 忽略 offset 参数，所有 chunk 都返回文件开头的数据，导致组装后的 .data 损坏，mscorlib.dll 的 MZ 头丢失。

**解决**：
- 适配层已实现 offset bug 检测（比较 chunk[0] 和 chunk[1] 的前 4 字节，相同则抛错降级到 base64）
- 优先使用 fd-based 顺序读取（`openSync` + `readSync`，规避 offset bug）
- 真机环境通常无此 bug，仅 DevTools 2.02.2607232 受影响

## 七、部署到生产环境

### 1. 选择部署模式

| 模式 | 命令 | 包体 | 适用 |
|------|------|------|------|
| 分包模式 | 默认（`--no-cdn-only`） | 主包 ~531KB + `wasm_pkg` 6.9MB + `data_pkg` 19.4MB | 推荐首发，自包含 |
| CDN 模式 | `--cdn-only --cdn-url <URL>` | 主包 ~531KB，大文件走 CDN | 包体最小，需 CDN 在线 |

### 2. CDN 模式：上传大文件到 CDN

将以下文件上传到 CDN（推荐腾讯云 COS / 阿里云 OSS）：
- `exports/web_2048/Game2048.wasm` (48 MB)
- `exports/web_2048/godot.web.template_release.wasm32.nothreads.data` (19 MB)

### 3. CDN 模式：重新转换时指定真实 CDN

```powershell
python wechat\convert_to_wechat.py `
    --source exports\web_2048 `
    --output wechat\build `
    --cdn-only `
    --cdn-url "https://your-cdn.com/2048/"
```

> 转换脚本会用 `json.dumps` 转义 CDN URL 后注入 `index.js` 顶部的 `globalThis._cdnBaseUrl`，并做 fail-fast 校验（若未写入则抛 `[S6] CDN URL 未正确写入产物 index.js`）。发布前确认产物 `index.js` 顶部 `_cdnBaseUrl` 是生产地址。

### 4. 上传小游戏代码

- 在微信开发者工具中点击「上传」
- 填写版本号（如 `1.0.0`）和备注
- 在微信公众平台提交审核

### 5. 微信小游戏包体限制

| 类型 | 大小限制 | 当前 2048 项目 |
|------|---------|----------------|
| 主包 | 4 MB | ✅ 约 531 KB（index.js + index.pck + adapter） |
| 分包 | 单包 20 MB，总计 20 MB | ✅ `wasm_pkg`（~6.9 MB .wasm.br）+ `data_pkg`（~19.4 MB .dat） |
| CDN | 无限制 | 可选：`--cdn-only` 模式下 .wasm/.data 走 CDN |

> ⚠️ **关键配置**：`project.config.json` 与 `project.private.config.json` 的 `compileType` 必须为 `"minigame"`（全小写官方合法值）。若误用 `"game"` 或 `"miniGame"`（驼峰式），微信服务端无法识别项目为小游戏，`game.json` 中的 `subpackages` 配置失效，所有文件会被打成名为 `__FULL__` 的主包，超过 4 MB 限制时报 `subpackage __FULL__ source size exceed max limit 4096KB`。

> ⚠️ **AppID 必须真实**：`touristappid` 游客模式在 DevTools 2.02.2607222+ 会触发项目类型识别回归，导致与 `compileType` 错误相同的 `__FULL__` 主包超限症状。当前已配置真实 AppID `wxc07c26935264a5e5`。

## 八、参考文档

- [Godot 4 Web 导出文档](https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_web.html)
- [微信小游戏开发文档](https://developers.weixin.qq.com/minigame/dev/guide/)
- [WXWebAssembly API](https://developers.weixin.qq.com/minigame/dev/api/base/wxa/WXWebAssembly.html)
- [微信小游戏分包加载](https://developers.weixin.qq.com/minigame/dev/guide/base-ability/subPackage.html)

## 九、构建日志与里程碑

详细构建日志见：`godot-mono-port/docs/BUILD_LOG.md`。P0 修复明细见 `REVIEW_REPORT.md` 附录 A。

主要里程碑：
1. ✅ Windows 编辑器构建完成（dev 构建，含 Mono 6.12 静态链接）
2. ✅ Web 导出模板构建完成（含 .data 文件修复、WASM m2n cookie 表扩展）
3. ✅ 2048 游戏项目创建（C# Main.cs）
4. ✅ 微信适配层实现（WA/FS/Canvas/Touch/Keyboard 等 polyfill 模块）
5. ✅ 微信转换脚本实现（5 处修补 + CDN fail-fast 校验 + 分包/CDN 双模式）
6. ✅ 2048 Web 导出包生成
7. ✅ 微信小游戏产物生成（分包模式自包含）
8. ✅ DevTools 模拟器 + PC 微信客户端预览验证通过（触摸 + 键盘）
9. ✅ 诊断日志清理（2026-07-26：移除 KeyDiag/Canvas/Dispatch/MZ 位置/rAF 计数等开发期诊断，保留 offset bug 检测等正确性检查）
