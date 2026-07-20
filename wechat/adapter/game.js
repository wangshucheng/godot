// 微信小游戏入口 - game.js
// 加载顺序: adapter(web polyfills) -> index.js(Godot引擎) -> 启动Engine
require('./adapter/wechat_adapter.js');
require('./index.js');

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
