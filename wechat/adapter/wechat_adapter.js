// wechat_adapter.js - 微信小游戏适配层
// 提供 13 个 polyfill 模块，让 Godot Web 导出包在微信小游戏环境中运行
//
// 核心差异:
// - 微信小游戏使用 WXWebAssembly 而非标准 WebAssembly
// - 微信无 DOM API (document/window/XMLHttpRequest/fetch)
// - Canvas 通过 wx.createCanvas() 创建
// - 触摸事件通过 wx.onTouchStart/Move/End/Cancel 注册
//
// 本文件在 game.js 中通过 require() 最先加载，确保后续 index.js 运行时
// 所有 polyfill 已就位。

(function () {
  'use strict';

  // ============================================================
  // 模块 1: WXWebAssembly → WebAssembly polyfill
  // 微信提供 WXWebAssembly.instantiate(path, imports)，path 为文件路径
  // 标准用 WebAssembly.instantiate(ArrayBuffer, imports)
  // ============================================================
  if (typeof WebAssembly === 'undefined' && typeof WXWebAssembly !== 'undefined') {
    globalThis.WebAssembly = WXWebAssembly;
  }

  // ============================================================
  // 模块 2: Canvas (HTMLCanvasElement) polyfill
  // 微信通过 wx.createCanvas() 创建主画布
  // ============================================================
  let _mainCanvas = null;
  function getMainCanvas() {
    if (!_mainCanvas) {
      _mainCanvas = wx.createCanvas();
      // 模拟 HTMLCanvasElement API
      _mainCanvas.style = _mainCanvas.style || {};
      _mainCanvas.getBoundingClientRect = function () {
        return {
          left: 0,
          top: 0,
          width: _mainCanvas.width,
          height: _mainCanvas.height,
          right: _mainCanvas.width,
          bottom: _mainCanvas.height,
        };
      };
      _mainCanvas.addEventListener = function () {};
      _mainCanvas.removeEventListener = function () {};
      _mainCanvas.focus = function () {};
    }
    return _mainCanvas;
  }

  function HTMLCanvasElement() {}
  HTMLCanvasElement.prototype.getContext = function (type, attributes) {
    return getMainCanvas().getContext(type, attributes);
  };
  HTMLCanvasElement.prototype.addEventListener = function () {};
  HTMLCanvasElement.prototype.removeEventListener = function () {};

  // ============================================================
  // 模块 3: Image polyfill
  // 微信通过 wx.createImage() 创建图片对象
  // ============================================================
  function HTMLImageElement() {
    var img = wx.createImage();
    return img;
  }
  globalThis.Image = HTMLImageElement;

  // ============================================================
  // 模块 4: Audio polyfill (Web Audio API)
  // 微信通过 wx.createInnerAudioContext() 创建音频
  // ============================================================
  function AudioContext() {
    this._ctx = wx.createInnerAudioContext();
  }
  AudioContext.prototype.createBufferSource = function () {
    return {
      buffer: null,
      loop: false,
      start: function () {},
      stop: function () {},
      connect: function () {},
      disconnect: function () {},
    };
  };
  AudioContext.prototype.createGain = function () {
    return {
      gain: { value: 1 },
      connect: function () {},
      disconnect: function () {},
    };
  };
  AudioContext.prototype.decodeAudioData = function (arrayBuffer, success, error) {
    if (success) success({});
  };
  AudioContext.prototype.close = function () {};
  AudioContext.prototype.resume = function () {};
  AudioContext.prototype.suspend = function () {};
  globalThis.AudioContext = AudioContext;
  globalThis.webkitAudioContext = AudioContext;

  // ============================================================
  // 模块 5: FileSystem polyfill
  // 微信通过 wx.getFileSystemManager() 访问文件系统
  // ============================================================
  var fileSystemManager = wx.getFileSystemManager ? wx.getFileSystemManager() : null;
  globalThis.fs = globalThis.fs || {
    readFileSync: function (path, encoding) {
      if (fileSystemManager) {
        return fileSystemManager.readFileSync(path, encoding);
      }
      return null;
    },
    writeFileSync: function (path, data, encoding) {
      if (fileSystemManager) {
        return fileSystemManager.writeFileSync(path, data, encoding);
      }
    },
    existsSync: function (path) {
      if (fileSystemManager) {
        try {
          fileSystemManager.accessSync(path);
          return true;
        } catch (e) {
          return false;
        }
      }
      return false;
    },
  };

  // ============================================================
  // 模块 6: TouchEvents polyfill
  // 微信: wx.onTouchStart/Move/End/Cancel
  // 标准: canvas.addEventListener('touchstart', ...)
  // ============================================================
  var _touchListeners = {
    touchstart: [],
    touchmove: [],
    touchend: [],
    touchcancel: [],
  };

  function dispatchTouch(type, touches) {
    var event = {
      type: type,
      target: getMainCanvas(),
      currentTarget: getMainCanvas(),
      touches: touches.map(function (t) {
        return { clientX: t.clientX, clientY: t.clientY, identifier: t.identifier };
      }),
      changedTouches: touches.map(function (t) {
        return { clientX: t.clientX, clientY: t.clientY, identifier: t.identifier };
      }),
      preventDefault: function () {},
      stopPropagation: function () {},
      timeStamp: Date.now(),
    };
    _touchListeners[type].forEach(function (cb) {
      try { cb(event); } catch (e) { console.error(e); }
    });
  }

  if (wx.onTouchStart) {
    wx.onTouchStart(function (e) {
      dispatchTouch('touchstart', e.touches);
    });
    wx.onTouchMove(function (e) {
      dispatchTouch('touchmove', e.touches);
    });
    wx.onTouchEnd(function (e) {
      dispatchTouch('touchend', e.changedTouches);
    });
    wx.onTouchCancel(function (e) {
      dispatchTouch('touchcancel', e.changedTouches);
    });
  }

  // ============================================================
  // 模块 7: Storage polyfill (localStorage)
  // 微信: wx.setStorageSync/getStorageSync
  // ============================================================
  globalThis.localStorage = {
    getItem: function (key) {
      try { return wx.getStorageSync(key); } catch (e) { return null; }
    },
    setItem: function (key, value) {
      try { wx.setStorageSync(key, value); } catch (e) {}
    },
    removeItem: function (key) {
      try { wx.removeStorageSync(key); } catch (e) {}
    },
    clear: function () {
      try { wx.clearStorageSync(); } catch (e) {}
    },
  };

  // ============================================================
  // 模块 8: Network polyfill (fetch)
  // 微信无 fetch，用 wx.request 适配
  // ============================================================
  globalThis.fetch = function (url, options) {
    options = options || {};
    return new Promise(function (resolve, reject) {
      wx.request({
        url: url,
        method: options.method || 'GET',
        headers: options.headers || {},
        responseType: options.responseType || 'text',
        success: function (res) {
          resolve({
            ok: res.statusCode >= 200 && res.statusCode < 300,
            status: res.statusCode,
            statusText: '',
            headers: {},
            text: function () { return Promise.resolve(String(res.data)); },
            json: function () { return Promise.resolve(JSON.parse(res.data)); },
            arrayBuffer: function () { return Promise.resolve(res.data instanceof ArrayBuffer ? res.data : new ArrayBuffer(0)); },
            blob: function () { return Promise.resolve(res.data); },
          });
        },
        fail: function (err) {
          reject(new Error(err.errMsg || 'wx.request failed'));
        },
      });
    });
  };

  // ============================================================
  // 模块 9: Timer polyfill
  // 微信已内置 setTimeout/setInterval，这里只需对齐 clearXxx
  // ============================================================
  if (typeof globalThis.setTimeout === 'undefined') {
    globalThis.setTimeout = function (cb, delay) { return setTimeout(cb, delay); };
    globalThis.clearTimeout = function (id) { return clearTimeout(id); };
    globalThis.setInterval = function (cb, delay) { return setInterval(cb, delay); };
    globalThis.clearInterval = function (id) { return clearInterval(id); };
  }

  // ============================================================
  // 模块 10: XMLHttpRequest polyfill
  // 微信无 XHR，用 wx.request 适配
  // ============================================================
  function XMLHttpRequest() {
    this.readyState = 0;
    this.status = 0;
    this.responseText = '';
    this.response = null;
    this.responseType = '';
    this._headers = {};
    this._method = 'GET';
    this._url = '';
  }
  XMLHttpRequest.prototype.open = function (method, url) {
    this._method = method;
    this._url = url;
    this.readyState = 1;
    if (this.onreadystatechange) this.onreadystatechange();
  };
  XMLHttpRequest.prototype.setRequestHeader = function (key, value) {
    this._headers[key] = value;
  };
  XMLHttpRequest.prototype.getResponseHeader = function () { return null; };
  XMLHttpRequest.prototype.send = function (body) {
    var self = this;
    wx.request({
      url: this._url,
      method: this._method,
      header: this._headers,
      responseType: this.responseType === 'arraybuffer' ? 'arraybuffer' : 'text',
      data: body,
      success: function (res) {
        self.status = res.statusCode;
        self.responseText = typeof res.data === 'string' ? res.data : '';
        self.response = res.data;
        self.readyState = 4;
        if (self.onreadystatechange) self.onreadystatechange();
        if (self.onload) self.onload();
      },
      fail: function (err) {
        self.status = 0;
        self.readyState = 4;
        if (self.onerror) self.onerror(err);
      },
    });
  };
  XMLHttpRequest.prototype.abort = function () {
    this.readyState = 0;
  };
  globalThis.XMLHttpRequest = XMLHttpRequest;

  // ============================================================
  // 模块 11: WebGL/WebGL2 context 增强
  // 微信 Canvas 的 WebGL2 context 已可用，这里确保扩展属性齐全
  // ============================================================
  var _webglExtensions = ['EXT_blend_minmax', 'OES_texture_float', 'OES_standard_derivatives'];
  globalThis.WebGLRenderingContext = globalThis.WebGLRenderingContext || function () {};
  if (typeof WebGL2RenderingContext !== 'undefined') {
    var _origGetExtension = WebGL2RenderingContext.prototype.getExtension;
    WebGL2RenderingContext.prototype.getExtension = function (name) {
      if (_webglExtensions.indexOf(name) >= 0) return _origGetExtension ? _origGetExtension.call(this, name) : null;
      return _origGetExtension ? _origGetExtension.call(this, name) : null;
    };
  }

  // ============================================================
  // 模块 12: performance.now polyfill
  // 微信: wx.getPerformance().now()
  // ============================================================
  globalThis.performance = globalThis.performance || {
    now: function () {
      if (wx.getPerformance) {
        return wx.getPerformance().now();
      }
      return Date.now();
    },
  };

  // ============================================================
  // 模块 13: DevicePixelRatio polyfill
  // 微信: wx.getSystemInfoSync().pixelRatio
  // ============================================================
  if (typeof globalThis.devicePixelRatio === 'undefined') {
    try {
      globalThis.devicePixelRatio = wx.getSystemInfoSync().pixelRatio;
    } catch (e) {
      globalThis.devicePixelRatio = 1;
    }
  }

  // ============================================================
  // 全局 window / document 最小化 shim
  // Godot index.js 会访问 window 和 document 的部分属性
  // ============================================================
  globalThis.window = globalThis.window || globalThis;
  globalThis.navigator = globalThis.navigator || {
    userAgent: 'WeChat MiniGame',
    language: 'zh-CN',
    platform: 'wechat',
  };

  // document 最小化 shim
  globalThis.document = globalThis.document || {
    createElement: function (tagName) {
      if (tagName === 'canvas') return getMainCanvas();
      if (tagName === 'img') return new HTMLImageElement();
      return { style: {}, appendChild: function () {} };
    },
    getElementById: function () { return null; },
    addEventListener: function () {},
    removeEventListener: function () {},
    body: { appendChild: function () {}, removeChild: function () {} },
    documentElement: { style: {} },
    hidden: false,
    visibilityState: 'visible',
  };

  // ============================================================
  // 路径解析: 在本地缓存和包内路径间查找
  // - 首先查 USER_DATA_PATH 缓存 (CDN 已下载)
  // - 然后查包内 (主包或分包)
  // ============================================================
  var USER_DATA_PATH = wx.env && wx.env.USER_DATA_PATH ? wx.env.USER_DATA_PATH : '';
  globalThis._resolveFilePath = function (filename) {
    // 1. 本地缓存
    if (USER_DATA_PATH) {
      var cachedPath = USER_DATA_PATH + '/' + filename;
      if (globalThis.fs.existsSync(cachedPath)) {
        return cachedPath;
      }
    }
    // 2. 包内
    return filename;
  };

  console.log('[WeChat Adapter] All 13 polyfill modules loaded.');
})();
