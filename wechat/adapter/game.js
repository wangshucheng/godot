// 微信小游戏入口 - game.js
// 加载顺序: adapter(web polyfills) -> index.js(Godot引擎) -> 初始化Engine
//
// 设计原则：
// 1. 启动状态用微信原生wx.showLoading提示，不抢占canvas（避免WebGL不可见）
// 2. 错误用wx.showModal显式展示给用户，不静默失败
// 3. 日志只输出到console，不写文件（性能考虑）

(function () {
  'use strict';

  var DEBUG = globalThis.__WECHAT_DEBUG__ === true;

  function log() {
    if (DEBUG) {
      var args = Array.prototype.slice.call(arguments);
      args.unshift('[Boot]');
      console.log.apply(console, args);
    }
  }

  function showLoading(title) {
    try {
      wx.showLoading({ title: title || '加载中...', mask: true });
    } catch (e) {}
  }

  function hideLoading() {
    try { wx.hideLoading(); } catch (e) {}
  }

  function showFatalError(title, message) {
    hideLoading();
    console.error('[Boot FATAL] ' + title + ': ' + message);
    try {
      wx.showModal({
        title: title,
        content: message,
        showCancel: false,
        confirmText: '确定',
      });
    } catch (e) {}
  }

  // 更新启动状态（暴露给index.js中的Godot初始化流程使用）
  globalThis.__updateBootStatus = showLoading;

  showLoading('加载适配层...');
  log('game.js start, wx=' + typeof wx);

  try {
    require('./adapter/wechat_adapter.js');
    log('adapter loaded');
  } catch (e) {
    showFatalError('适配层加载失败', e.message + (e.stack ? '\n' + e.stack : ''));
    return;
  }

  showLoading('加载引擎...');

  try {
    require('./index.js');
    log('index.js loaded');
  } catch (e) {
    showFatalError('引擎加载失败', e.message + (e.stack ? '\n' + e.stack : ''));
    return;
  }

  // 设置CDN
  var cdnBaseUrl = globalThis._cdnBaseUrl || '';
  if (cdnBaseUrl && typeof globalThis._setCdnBase === 'function') {
    globalThis._setCdnBase(cdnBaseUrl);
    log('CDN: ' + cdnBaseUrl);
  }

  var executableName = globalThis._executableName || 'Game';
  var fileSizesMap = globalThis._fileSizes || {};
  var pckFileName = globalThis._pckFileName || (executableName + '.pck');
  var pckEmbedded = globalThis._pckEmbedded === true;

  log('exe=' + executableName + ' pck=' + pckFileName + ' embedded=' + pckEmbedded);
  showLoading('初始化游戏...');

  var GODOT_CONFIG = {
    executable: executableName,
    mainPack: pckFileName,
    args: [],
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

  var engine;
  try {
    engine = new Engine(GODOT_CONFIG);
    log('Engine instance created');
  } catch (e) {
    showFatalError('引擎创建失败', e.message + (e.stack ? '\n' + e.stack : ''));
    return;
  }

  var initPromise;

  if (pckEmbedded) {
    showLoading('加载资源...');
    try {
      var pckModule = require('./pck_data.js');
      log('Embedded PCK: ' + pckModule.buffer.byteLength + ' bytes');
      initPromise = engine.preloadFile(pckModule.buffer, pckModule.name).then(function () {
        var exe = engine.config.executable;
        var pack = engine.config.mainPack || (exe + '.pck');
        engine.config.args = ['--main-pack', pack].concat(engine.config.args);
        showLoading('启动中...');
        return engine.init(exe).then(function () {
          log('engine.init() OK, calling start()');
          return engine.start();
        });
      });
    } catch (e) {
      showFatalError('资源加载失败', e.message + (e.stack ? '\n' + e.stack : ''));
      return;
    }
  } else {
    initPromise = engine.startGame();
  }

  initPromise.then(function () {
    hideLoading();
    log('Game started successfully!');
    console.log('[WeChat] Game started!');
  }, function (err) {
    var msg = err && err.message ? err.message : String(err);
    var stack = err && err.stack ? err.stack : '';
    console.error('[Boot] Game failed to start:', msg);
    if (stack) console.error(stack);
    showFatalError('启动失败', msg);
  });
})();
