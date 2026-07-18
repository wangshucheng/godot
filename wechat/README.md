# 微信小游戏工作流（2048 验证案例）

本文档说明如何用 **2048 小游戏** 验证从 Godot 4.7 Mono 项目到微信小游戏的完整工作流。

## 一、工作流总览

```
godot4_7_mono/
├── bin/windows/godot.windows.editor.x86_64.exe   ← 编辑器（构建产物 1）
├── templates/web/godot.web.template_release.wasm32.nothreads.zip  ← 导出模板（构建产物 2）
├── test_game_2048/                                ← 2048 游戏源码
│   ├── project.godot
│   ├── Main.gd
│   ├── Main.tscn
│   └── export_presets.cfg
├── exports/web_2048/                              ← Web 导出包（构建产物 3）
│   ├── Game2048.html
│   ├── Game2048.js
│   ├── Game2048.wasm                              ← 走 CDN（48 MB，超过 4 MB 主包限制）
│   ├── Game2048.pck
│   └── godot.web.template_release.wasm32.nothreads.data  ← 走 CDN（19 MB）
└── wechat/
    ├── adapter/                                   ← 微信适配层源码
    │   ├── game.js
    │   └── wechat_adapter.js                      ← 13 个 polyfill 模块
    ├── convert_to_wechat.py                       ← Web → 微信 转换脚本
    ├── build/                                     ← 微信小游戏产物（构建产物 4）
    │   ├── game.js                                ← 主入口
    │   ├── game.json                              ← 小游戏配置
    │   ├── project.config.json                    ← 开发者工具配置
    │   ├── index.js                               ← Godot 引擎 + 5 个修补
    │   ├── index.pck                              ← 2048 游戏资源包
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
| 编辑器 | `bin/windows/godot.windows.editor.x86_64.exe` | 开发游戏、导出 PCK |
| 模板 | `templates/web/godot.web.template_release.wasm32.nothreads.zip` | Web 导出依赖（含 .data 修复） |
| Web 导出 | `exports/web_2048/` | 标准 Web 运行包（HTML5） |
| 微信小游戏 | `wechat/build/` | 微信开发者工具导入目录 |

## 二、前置准备

### 1. 安装微信开发者工具

下载地址：https://developers.weixin.qq.com/miniprogram/dev/devtools/download.html

- 选择 **稳定版 Stable Build**（Windows 64 位）
- 安装完成后用微信扫码登录

### 2. 注册小游戏 AppID（可选）

- 测试阶段可直接使用 `touristappid`（游客模式，已在 `project.config.json` 配置）
- 正式发布需在 https://mp.weixin.qq.com 注册小游戏账号获取真实 AppID

### 3. 准备本地 CDN 服务器

由于 `.wasm`（48 MB）和 `.data`（19 MB）超过微信主包 4 MB 限制，必须走 CDN。测试时用本地 HTTP 服务器模拟：

```powershell
# 在 godot4_7_mono 目录下执行
cd "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono"

# 启动静态文件服务器，监听 8000 端口
python -m http.server 8000
```

服务器会以 `godot4_7_mono/` 为根目录。CDN 文件路径为：
- `http://localhost:8000/exports/web_2048/Game2048.wasm`
- `http://localhost:8000/exports/web_2048/godot.web.template_release.wasm32.nothreads.data`

> 转换脚本默认使用 `--cdn-url http://localhost:8000/exports/web_2048/`，因此需要把 `exports/web_2048/` 目录作为 CDN 根目录，或者启动服务器时直接 `cd exports/web_2048` 然后 `python -m http.server 8000`。

## 三、导入微信开发者工具

### 1. 打开微信开发者工具

- 选择「小游戏」分类
- 点击「导入项目」

### 2. 配置导入参数

| 字段 | 值 |
|------|---|
| 项目目录 | `C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\build` |
| AppID | `touristappid`（已配置，导入后自动读取） |
| 项目名称 | `2048-minigame`（自动读取） |

### 3. 启动本地 CDN 服务器

在另一个终端窗口运行（保持运行）：

```powershell
cd "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\exports\web_2048"
python -m http.server 8000
```

### 4. 点击「编译」按钮

观察开发者工具的「Console」面板，预期输出：

```
[WeChat] Loading package from cache: .../Game2048.wasm     ← 首次无缓存，走 CDN
[WeChat] Downloading from CDN: http://localhost:8000/Game2048.wasm
[WeChat] WASM instantiated successfully
[WeChat] Loading package from cache: .../godot.web...data
[WeChat] Downloading from CDN: http://localhost:8000/godot.web...data
Godot Engine v4.3.stable.mono.official
```

## 四、预期运行效果

### 操作方式

- **键盘方向键**：上下左右移动方块
- **触屏滑动**：在游戏区域内滑动（移动端/微信开发者工具模拟器）
- **R 键**：重新开始游戏

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
& "$projectRoot\bin\windows\godot.windows.editor.x86_64.exe" `
    --headless `
    --path "$projectRoot\test_game_2048" `
    --export-release "Web" `
    "$projectRoot\exports\web_2048\Game2048.html"
```

### 步骤 2：重新转换为微信小游戏

```powershell
$projectRoot = "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono"
python "$projectRoot\wechat\convert_to_wechat.py" `
    --source "$projectRoot\exports\web_2048" `
    --output "$projectRoot\wechat\build" `
    --cdn-url "http://localhost:8000/"
```

### 步骤 3：在微信开发者工具中重新编译

- 点击「编译」按钮（或 Ctrl+B）
- 如有缓存问题，先「清缓存 → 全部清除」再编译

## 六、常见问题排查

### Q1：`WXWebAssembly is not defined`

**原因**：开发者工具版本过低，或 AppID 类型不是小游戏。

**解决**：
- 升级开发者工具到最新稳定版
- 确认 `project.config.json` 中 `compileType` 为 `"miniGame"`
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

**原因**：适配层触摸事件未正确转发到 Godot。

**解决**：
- 确认 `wechat_adapter.js` 模块 6（TouchEvents）已注册 `wx.onTouchStart/Move/End/Cancel`
- 检查 `Main.gd` 中 `_gui_input` 是否处理 `InputEventScreenDrag`
- 开发者工具中「模拟 → 触摸」模式

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

## 七、部署到生产环境

### 1. 上传 CDN 文件

将以下文件上传到 CDN（推荐腾讯云 COS / 阿里云 OSS）：
- `exports/web_2048/Game2048.wasm` (48 MB)
- `exports/web_2048/godot.web.template_release.wasm32.nothreads.data` (19 MB)

### 2. 重新转换时指定真实 CDN

```powershell
python wechat\convert_to_wechat.py `
    --source exports\web_2048 `
    --output wechat\build `
    --cdn-url "https://your-cdn.com/2048/"
```

### 3. 上传小游戏代码

- 在微信开发者工具中点击「上传」
- 填写版本号（如 `1.0.0`）和备注
- 在微信公众平台提交审核

### 4. 微信小游戏包体限制

| 类型 | 大小限制 | 当前 2048 项目 |
|------|---------|----------------|
| 主包 | 4 MB | ✅ 约 516 KB（仅 index.js + index.pck） |
| 分包 | 单包 4 MB，总计 20 MB | 未使用 |
| CDN | 无限制 | ✅ .wasm + .data 走 CDN |

## 八、参考文档

- [Godot 4 Web 导出文档](https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_web.html)
- [微信小游戏开发文档](https://developers.weixin.qq.com/minigame/dev/guide/)
- [WXWebAssembly API](https://developers.weixin.qq.com/minigame/dev/api/base/wxa/WXWebAssembly.html)
- [微信小游戏分包加载](https://developers.weixin.qq.com/minigame/dev/guide/base-ability/subPackage.html)

## 九、构建日志

详细构建日志见：`godot-mono-port/docs/BUILD_LOG.md`

主要里程碑：
1. ✅ Windows 编辑器构建完成
2. ✅ Web 导出模板构建完成（含 .data 文件修复）
3. ✅ 2048 游戏项目创建
4. ✅ 微信适配层实现（13 个 polyfill 模块）
5. ✅ 微信转换脚本实现（5 个修补点）
6. ✅ 2048 Web 导出包生成
7. ✅ 微信小游戏产物生成
8. ⏳ 微信开发者工具实际运行验证（待用户执行）
