// 微信小游戏入口 - game.js
// 加载顺序: adapter(web polyfills) -> index.js(Godot引擎) -> 启动Engine

// === 诊断日志：写入文件（console.log 在 WeChat IDE 中可能不输出到 WeappLog）===
var _diagLog = [];
function _diag(msg) {
  var line = '[' + new Date().toISOString() + '] ' + msg;
  _diagLog.push(line);
  console.log(line);
  try {
    var fs = wx.getFileSystemManager();
    var path = wx.env.USER_DATA_PATH + '/game_diag.log';
    fs.writeFileSync(path, _diagLog.join('\n') + '\n', 'utf8');
  } catch (e) {
    // 文件写入失败不影响游戏运行
  }
}

_diag('[WeChat Boot] game.js executing');
_diag('[WeChat Boot] typeof wx = ' + typeof wx);
_diag('[WeChat Boot] typeof require = ' + typeof require);

try {
  _diag('[WeChat Boot] requiring wechat_adapter.js...');
  require('./adapter/wechat_adapter.js');
  _diag('[WeChat Boot] wechat_adapter.js loaded OK');
} catch (e) {
  _diag('[WeChat Boot] adapter load FAILED: ' + e.message);
  if (e.stack) _diag('[WeChat Boot] stack: ' + e.stack);
}

try {
  _diag('[WeChat Boot] requiring index.js...');
  require('./index.js');
  _diag('[WeChat Boot] index.js loaded OK');
} catch (e) {
  _diag('[WeChat Boot] index.js load FAILED: ' + e.message);
  if (e.stack) _diag('[WeChat Boot] stack: ' + e.stack);
}

// 设置 CDN 基础 URL
var cdnBaseUrl = globalThis._cdnBaseUrl || '';
if (cdnBaseUrl && typeof _setCdnBase === 'function') {
  _setCdnBase(cdnBaseUrl);
  console.log('[WeChat] CDN base: ' + cdnBaseUrl);
  _diag('[WeChat Boot] CDN base set: ' + cdnBaseUrl);
} else {
  console.log('[WeChat] No CDN base set (local-only mode)');
  _diag('[WeChat Boot] No CDN base set (local-only mode)');
}

// 诊断 F5 分包配置
_diag('[WeChat Boot] _dataSubpkg = ' + (globalThis._dataSubpkg || '(none)'));
_diag('[WeChat Boot] _dataInSubpkg = ' + globalThis._dataInSubpkg);
_diag('[WeChat Boot] _wasmSubpkg = ' + (globalThis._wasmSubpkg || '(none)'));
_diag('[WeChat Boot] _wasmBrInSubpkg = ' + globalThis._wasmBrInSubpkg);

console.log('[WeChat] Starting Godot 4.7 Engine...');
_diag('[WeChat Boot] Starting Godot 4.7 Engine...');

var executableName = globalThis._executableName || 'Game2048';
var fileSizesMap = globalThis._fileSizes || {};
var pckFileName = globalThis._pckFileName || (executableName + '.pck');
var pckEmbedded = globalThis._pckEmbedded === true;

console.log('[WeChat] executable: ' + executableName + ', mainPack: ' + pckFileName + ', pckEmbedded: ' + pckEmbedded);
_diag('[WeChat Boot] executable: ' + executableName + ', mainPack: ' + pckFileName + ', pckEmbedded: ' + pckEmbedded);

var GODOT_CONFIG = {
  executable: executableName,
  mainPack: pckFileName,
  args: [],
  // 直接使用适配层增强过的主 canvas（带 .style/事件/dispatchEvent + HTMLCanvasElement
  // 原型补丁）。不要用 wx.createCanvas() 裸对象（无 .style 会在 display_setup 崩溃），
  // 也避免引擎 getGodotConfig 回退去调 document.getElementsByTagName('canvas')——
  // 新版开发者工具基础库自带只读 document 桩，适配层 document polyfill 无法覆盖。
  canvas: (typeof globalThis.__godotGetMainCanvas === 'function') ? globalThis.__godotGetMainCanvas() : wx.createCanvas(),
  canvasResizePolicy: 2,
  experimentalVK: false,
  focusCanvas: true,
  gdextensionLibs: [],
  fileSizes: fileSizesMap,
  unloadAfterInit: false,
  onPrint: function () {
    console.log.apply(console, Array.prototype.slice.call(arguments));
  },
  onPrintError: function () {
    var msg = Array.prototype.join.call(arguments, ' ');
    // 过滤 WASM/微信环境下已知的非致命引擎警告
    // - mbedTLS PSA crypto: WASM 无持久化存储，PSA crypto 无法初始化，不影响游戏功能
    // - 音频驱动失败: 微信 AudioContext 是桩，已在 project.godot 中预设 Dummy driver
    if (msg.indexOf('psa crypto') !== -1 ||
        msg.indexOf('initialize_mbedtls_module') !== -1 ||
        msg.indexOf('All audio drivers failed') !== -1 ||
        msg.indexOf('audio_server.cpp') !== -1) {
      return;
    }
    console.error.apply(console, Array.prototype.slice.call(arguments));
  },
};

try {
  _diag('[WeChat Boot] typeof Engine = ' + typeof Engine);
  _diag('[WeChat Boot] pckEmbedded = ' + pckEmbedded);
  _diag('[WeChat Boot] executable = ' + executableName + ', mainPack = ' + pckFileName);

  var engine = new Engine(GODOT_CONFIG);
  _diag('[WeChat Boot] Engine instance created OK');

  // 心跳：每 3s 写一行，用于判断 JS 事件循环是否还活着
  var _hbCount = 0;
  var _hbTimer = setInterval(function () {
    _hbCount++;
    _diag('[WeChat Boot] HEARTBEAT #' + _hbCount + ' (JS alive)');
    if (_hbCount >= 20) {
      clearInterval(_hbTimer);
    }
  }, 3000);

  var initPromise;
  if (pckEmbedded) {
    _diag('[WeChat Boot] Loading embedded PCK data...');
    console.log('[WeChat] Loading embedded PCK data...');
    var pckModule = require('./pck_data.js');
    _diag('[WeChat Boot] PCK data loaded: ' + pckModule.buffer.byteLength + ' bytes');
    console.log('[WeChat] PCK data loaded: ' + pckModule.buffer.byteLength + ' bytes');
    // preloadFile(ArrayBuffer, path) 直接将 buffer 注册到 preloader.preloadedFiles
    initPromise = engine.preloadFile(pckModule.buffer, pckModule.name).then(function () {
      _diag('[WeChat Boot] PCK preloaded, starting engine...');
      console.log('[WeChat] PCK preloaded, starting engine...');
      // 手动执行 startGame 逻辑，但跳过 preloadFile(pack, pack) 避免重复 fetch
      // startGame 内部: config.update() + args加 --main-pack + init(exe) + start()
      var exe = engine.config.executable;
      var pack = engine.config.mainPack || (exe + '.pck');
      engine.config.args = ['--main-pack', pack].concat(engine.config.args);
      return engine.init(exe).then(function () {
        _diag('[WeChat Boot] engine.init() resolved, calling start()...');
        console.log('[WeChat] Engine init OK, starting...');
        // 用 try-catch 包裹 start()，捕获同步异常
        try {
          var startRet = engine.start();
          _diag('[WeChat Boot] engine.start() returned: ' + (typeof startRet));
          return startRet;
        } catch (startErr) {
          _diag('[WeChat Boot] engine.start() THREW: ' + startErr.message);
          if (startErr.stack) _diag('[WeChat Boot] start stack: ' + startErr.stack);
          throw startErr;
        }
      });
    });
  } else {
    _diag('[WeChat Boot] Calling startGame()...');
    initPromise = engine.startGame();
  }

  initPromise.then(function () {
    clearInterval(_hbTimer);
    _diag('[WeChat Boot] Game started successfully!');
    console.log('[WeChat] Game started successfully!');
  }, function (err) {
    clearInterval(_hbTimer);
    _diag('[WeChat Boot] Game failed to start: ' + err);
    console.error('[WeChat] Game failed to start:', err);
    if (err && err.stack) {
      _diag('[WeChat Boot] stack: ' + err.stack);
      console.error(err.stack);
    }
  });
} catch (e) {
  _diag('[WeChat Boot] Engine start error: ' + e.message);
  if (e.stack) _diag('[WeChat Boot] stack: ' + e.stack);
  console.error('[WeChat] Engine start error:', e);
  if (e && e.stack) console.error(e.stack);
}
