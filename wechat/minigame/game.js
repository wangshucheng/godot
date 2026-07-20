// 微信小游戏入口 - game.js
// 加载顺序: adapter(web polyfills) -> index.js(Godot引擎) -> 启动Engine

// === 早期诊断日志（验证 game.js 真的被执行）===
console.log('[WeChat Boot] === game.js execution started ===');
console.log('[WeChat Boot] timestamp: ' + Date.now());

// === 加载 adapter（含 try-catch 捕获同步错误）===
try {
  console.log('[WeChat Boot] requiring adapter/wechat_adapter.js ...');
  require('./adapter/wechat_adapter.js');
  console.log('[WeChat Boot] adapter loaded OK');
} catch (e) {
  console.error('[WeChat Boot] adapter require FAILED:', e);
  if (e && e.stack) console.error(e.stack);
  throw e;
}

// === 加载 Godot 引擎 index.js ===
try {
  console.log('[WeChat Boot] requiring index.js ...');
  require('./index.js');
  console.log('[WeChat Boot] index.js loaded OK');
} catch (e) {
  console.error('[WeChat Boot] index.js require FAILED:', e);
  if (e && e.stack) console.error(e.stack);
  throw e;
}

// === 检查 Engine 全局是否定义 ===
console.log('[WeChat Boot] typeof Engine = ' + typeof Engine);
if (typeof Engine !== 'function') {
  console.error('[WeChat Boot] FATAL: Engine is not defined after index.js load!');
}

// 设置 CDN 基础 URL
var cdnBaseUrl = globalThis._cdnBaseUrl || '';
if (cdnBaseUrl && typeof _setCdnBase === 'function') {
  _setCdnBase(cdnBaseUrl);
  console.log('[WeChat] CDN base: ' + cdnBaseUrl);
} else {
  console.log('[WeChat] No CDN base set (local-only mode)');
}

console.log('[WeChat] Starting Godot 4.7 Engine...');

var executableName = globalThis._executableName || 'Game2048';
var fileSizesMap = globalThis._fileSizes || {};
var pckFileName = globalThis._pckFileName || (executableName + '.pck');
var pckEmbedded = globalThis._pckEmbedded === true;

console.log('[WeChat] executable: ' + executableName + ', mainPack: ' + pckFileName + ', pckEmbedded: ' + pckEmbedded);

var GODOT_CONFIG = {
  executable: executableName,
  mainPack: pckFileName,
  args: [],
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
    console.error.apply(console, Array.prototype.slice.call(arguments));
  },
};

try {
  var engine = new Engine(GODOT_CONFIG);

  var initPromise;
  if (pckEmbedded) {
    console.log('[WeChat] Loading embedded PCK data...');
    var pckModule = require('./pck_data.js');
    console.log('[WeChat] PCK data loaded: ' + pckModule.buffer.byteLength + ' bytes');
    // preloadFile(ArrayBuffer, path) 直接将 buffer 注册到 preloader.preloadedFiles
    initPromise = engine.preloadFile(pckModule.buffer, pckModule.name).then(function () {
      console.log('[WeChat] PCK preloaded, starting engine...');
      // 手动执行 startGame 逻辑，但跳过 preloadFile(pack, pack) 避免重复 fetch
      // startGame 内部: config.update() + args加 --main-pack + init(exe) + start()
      var exe = engine.config.executable;
      var pack = engine.config.mainPack || (exe + '.pck');
      engine.config.args = ['--main-pack', pack].concat(engine.config.args);
      return engine.init(exe).then(function () {
        return engine.start();
      });
    });
  } else {
    initPromise = engine.startGame();
  }

  initPromise.then(function () {
    console.log('[WeChat] Game started successfully!');
  }, function (err) {
    console.error('[WeChat] Game failed to start:', err);
    if (err && err.stack) console.error(err.stack);
  });
} catch (e) {
  console.error('[WeChat] Engine start error:', e);
  if (e && e.stack) console.error(e.stack);
}
