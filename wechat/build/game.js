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
  }
}

_diag('[WeChat Boot] game.js executing');
_diag('[WeChat Boot] typeof wx = ' + typeof wx);
_diag('[WeChat Boot] typeof require = ' + typeof require);

// ★ 关键修复：使用微信原生 showLoading 显示加载状态，
//   绝不可以在 adapter 加载前调用 wx.createCanvas()，否则会抢占主画布，
//   导致 adapter 的 getMainCanvas() 只能拿到离屏 canvas（WebGL 渲染不可见）。
var _loadingVisible = false;
function _showLoading(title) {
  _diag('[Boot Status] ' + title);
  try {
    if (typeof wx !== 'undefined' && wx.showLoading) {
      wx.showLoading({ title: title || '加载中...', mask: true });
      _loadingVisible = true;
    }
  } catch (e) {
    console.warn('[WeChat] showLoading failed: ' + e.message);
  }
}
function _hideLoading() {
  try {
    if (_loadingVisible && typeof wx !== 'undefined' && wx.hideLoading) {
      wx.hideLoading();
      _loadingVisible = false;
    }
  } catch (e) {}
}
globalThis.__updateBootStatus = _showLoading;

_showLoading('加载适配器...');

// 错误显示：启动失败时用原生弹窗 + 尝试canvas显示
function _showBootError(title, details) {
  _hideLoading();
  var fullMsg = title + (details ? '\n\n' + details.substring(0, 300) : '');
  console.error('[WeChat BOOT ERROR] ' + fullMsg);
  _diag('[WeChat Boot] FATAL ERROR: ' + title);
  try {
    wx.showModal({
      title: '启动失败',
      content: title.length > 80 ? title.substring(0, 77) + '...' : title,
      showCancel: false,
      confirmText: '确定',
    });
  } catch (e) {}
  try {
    var c = (typeof globalThis.__godotGetMainCanvas === 'function') ? globalThis.__godotGetMainCanvas() : null;
    if (c) {
      var ctx = c.getContext('2d');
      if (ctx) {
        var w = c.width || 375, h = c.height || 667;
        ctx.fillStyle = '#1a1a2e';
        ctx.fillRect(0, 0, w, h);
        ctx.fillStyle = '#ff6b6b';
        ctx.font = 'bold 20px sans-serif';
        ctx.fillText('BOOT ERROR', 20, 60);
        ctx.fillStyle = '#e0e0e0';
        ctx.font = '14px sans-serif';
        ctx.fillText(title || 'Unknown error', 20, 100);
      }
    }
  } catch(e) {
    console.error('[WeChat] Failed to render error on canvas:', e);
  }
}

try {
  _diag('[WeChat Boot] requiring wechat_adapter.js...');
  require('./adapter/wechat_adapter.js');
  _diag('[WeChat Boot] wechat_adapter.js loaded OK');
  _showLoading('加载引擎...');
} catch (e) {
  _diag('[WeChat Boot] adapter load FAILED: ' + e.message);
  if (e.stack) _diag('[WeChat Boot] stack: ' + e.stack);
  _showBootError('Adapter load failed: ' + e.message, e.stack || '');
}

try {
  _diag('[WeChat Boot] requiring index.js...');
  require('./index.js');
  _diag('[WeChat Boot] index.js loaded OK');
  _showLoading('初始化中...');
} catch (e) {
  _diag('[WeChat Boot] index.js load FAILED: ' + e.message);
  if (e.stack) _diag('[WeChat Boot] stack: ' + e.stack);
  _showBootError('Engine load failed: ' + e.message, e.stack || '');
}

var cdnBaseUrl = globalThis._cdnBaseUrl || '';
if (cdnBaseUrl && typeof _setCdnBase === 'function') {
  _setCdnBase(cdnBaseUrl);
  console.log('[WeChat] CDN base: ' + cdnBaseUrl);
  _diag('[WeChat Boot] CDN base set: ' + cdnBaseUrl);
} else {
  console.log('[WeChat] No CDN base set (local-only mode)');
  _diag('[WeChat Boot] No CDN base set (local-only mode)');
}

_diag('[WeChat Boot] _dataSubpkg = ' + (globalThis._dataSubpkg || '(none)'));
_diag('[WeChat Boot] _dataInSubpkg = ' + globalThis._dataInSubpkg);
_diag('[WeChat Boot] _wasmSubpkg = ' + (globalThis._wasmSubpkg || '(none)'));
_diag('[WeChat Boot] _wasmBrInSubpkg = ' + globalThis._wasmBrInSubpkg);

console.log('[WeChat] Starting Godot 4.7 Engine...');
_diag('[WeChat Boot] Starting Godot 4.7 Engine...');
_showLoading('创建引擎实例...');

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
  _showLoading('加载游戏数据...');

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
    initPromise = engine.preloadFile(pckModule.buffer, pckModule.name).then(function () {
      _diag('[WeChat Boot] PCK preloaded, starting engine...');
      console.log('[WeChat] PCK preloaded, starting engine...');
      var exe = engine.config.executable;
      var pack = engine.config.mainPack || (exe + '.pck');
      engine.config.args = ['--main-pack', pack].concat(engine.config.args);
      return engine.init(exe).then(function () {
        _diag('[WeChat Boot] engine.init() resolved, calling start()...');
        console.log('[WeChat] Engine init OK, starting...');
        _showLoading('启动游戏...');
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
    _hideLoading();
    _diag('[WeChat Boot] Game started successfully!');
    console.log('[WeChat] Game started successfully!');
  }, function (err) {
    clearInterval(_hbTimer);
    var errMsg = err && err.message ? err.message : String(err);
    _diag('[WeChat Boot] Game failed to start: ' + errMsg);
    console.error('[WeChat] Game failed to start:', err);
    if (err && err.stack) {
      _diag('[WeChat Boot] stack: ' + err.stack);
      console.error(err.stack);
    }
    _showBootError('启动失败: ' + errMsg, err && err.stack ? err.stack : '');
  });
} catch (e) {
  _diag('[WeChat Boot] Engine start error: ' + e.message);
  if (e.stack) _diag('[WeChat Boot] stack: ' + e.stack);
  console.error('[WeChat] Engine start error:', e);
  if (e && e.stack) console.error(e.stack);
  _showBootError('引擎启动错误: ' + (e.message || e), e.stack || '');
}
