// wechat_adapter.js - 微信小游戏适配层
// 提供完整的 Web API polyfill，让 Godot 4.7 Web 导出包在微信小游戏环境中运行
//
// 设计原则：
// 1. 最小侵入：只polyfill Godot引擎实际用到的API，不做过度模拟
// 2. 失败显式：不静默吞错误，关键路径失败必须抛出可诊断的错误
// 3. 性能优先：避免同步IO、避免每帧日志、缓存系统信息
// 4. 生产就绪：调试日志可通过 __WECHAT_DEBUG__ 开关控制
// 5. 代码整洁：提取公共工具函数，消除重复逻辑

(function () {
  'use strict';

  // ============================================================
  // 日志系统
  // ============================================================
  var DEBUG = globalThis.__WECHAT_DEBUG__ === true;

  function log() {
    if (DEBUG) {
      var args = Array.prototype.slice.call(arguments);
      args.unshift('[WeChat]');
      console.log.apply(console, args);
    }
  }

  function warn() {
    var args = Array.prototype.slice.call(arguments);
    args.unshift('[WeChat WARN]');
    console.warn.apply(console, args);
  }

  function error() {
    var args = Array.prototype.slice.call(arguments);
    args.unshift('[WeChat ERROR]');
    console.error.apply(console, args);
  }

  // ============================================================
  // 工具函数
  // ============================================================

  function safeDefineGlobal(name, value) {
    try {
      globalThis[name] = value;
      if (globalThis[name] === value) return;
    } catch (e) {}
    try {
      Object.defineProperty(globalThis, name, {
        value: value,
        writable: true,
        configurable: true,
      });
    } catch (e2) {
      warn('Cannot define global: ' + name);
    }
  }

  function uint8ToArrayBuffer(u8) {
    if (!u8) return new ArrayBuffer(0);
    if (u8.byteOffset === 0 && u8.byteLength === u8.buffer.byteLength) {
      return u8.buffer;
    }
    return u8.buffer.slice(u8.byteOffset, u8.byteOffset + u8.byteLength);
  }

  function toArrayBuffer(data) {
    if (!data) return new ArrayBuffer(0);
    if (data instanceof ArrayBuffer) return data;
    if (data.buffer instanceof ArrayBuffer) {
      return uint8ToArrayBuffer(new Uint8Array(data.buffer, data.byteOffset || 0, data.byteLength || data.length));
    }
    if (typeof data === 'string') {
      var arr = new Uint8Array(data.length);
      for (var i = 0; i < data.length; i++) arr[i] = data.charCodeAt(i) & 0xFF;
      return arr.buffer;
    }
    if (typeof data.length === 'number') {
      var buf = new ArrayBuffer(data.length);
      var view = new Uint8Array(buf);
      for (var j = 0; j < data.length; j++) view[j] = data[j] & 0xFF;
      return buf;
    }
    return new ArrayBuffer(0);
  }

  function tryAccessFile(path) {
    if (!fs) return false;
    try {
      fs.accessSync(path);
      return true;
    } catch (e) {
      return false;
    }
  }

  function readFileAsArrayBuffer(path) {
    if (!fs) throw new Error('File system unavailable');
    var data = fs.readFileSync(path);
    var ab = toArrayBuffer(data);
    if (!ab || ab.byteLength === 0) {
      throw new Error('File is empty or unreadable: ' + path);
    }
    return ab;
  }

  // ============================================================
  // 文件系统初始化
  // ============================================================
  var fs = null;
  var USER_DATA_PATH = '';
  try {
    fs = wx.getFileSystemManager();
    USER_DATA_PATH = (wx.env && wx.env.USER_DATA_PATH) ? wx.env.USER_DATA_PATH : '';
  } catch (e) {
    warn('File system unavailable: ' + e.message);
  }
  log('USER_DATA_PATH = ' + USER_DATA_PATH);

  // ============================================================
  // 临时文件管理
  // ============================================================
  var _tempFileCounter = 0;
  var _activeTempFiles = {};

  function writeTempFile(data, ext) {
    if (!fs || !USER_DATA_PATH) throw new Error('File system unavailable');
    var path = USER_DATA_PATH + '/_wx_tmp_' + (++_tempFileCounter) + '_' + Date.now() + '.' + (ext || 'tmp');
    var u8 = data instanceof Uint8Array ? data : new Uint8Array(toArrayBuffer(data));
    fs.writeFileSync(path, u8, 'binary');
    _activeTempFiles[path] = true;
    return path;
  }

  function deleteTempFile(path) {
    if (!fs || !path) return;
    try {
      fs.unlinkSync(path);
      delete _activeTempFiles[path];
    } catch (e) {}
  }

  function cleanupAllTempFiles() {
    var paths = Object.keys(_activeTempFiles);
    for (var i = 0; i < paths.length; i++) {
      deleteTempFile(paths[i]);
    }
  }

  // ============================================================
  // WebAssembly polyfill
  // 策略：
  //   - .wasm.br (Brotli) 必须用 WXWebAssembly.instantiate(path) 自动解压
  //   - .wasm (未压缩) 优先用原生 WebAssembly.instantiate(ArrayBuffer)，
  //     回退写临时文件用 WXWebAssembly.instantiate(path)
  // ============================================================

  var _WXWA = null;
  var _nativeWA = null;

  try {
    if (typeof WXWebAssembly !== 'undefined') {
      _WXWA = WXWebAssembly;
    }
  } catch (e) {}

  try {
    if (typeof WebAssembly !== 'undefined' && WebAssembly &&
        typeof WebAssembly.instantiate === 'function' &&
        typeof WebAssembly.Memory === 'function') {
      try {
        var minimalWasm = new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]);
        var testResult = WebAssembly.instantiate(minimalWasm.buffer, {});
        if (testResult && typeof testResult.then === 'function') {
          _nativeWA = WebAssembly;
          log('Native WebAssembly available (supports ArrayBuffer instantiation)');
        }
      } catch (e) {}
    }
  } catch (e) {}

  if (!_nativeWA) {
    log('Native WebAssembly not available, will use WXWebAssembly with temp files');
  }

  // ============================================================
  // 分包加载
  // ============================================================
  function loadSubpackage(name, timeoutMs) {
    timeoutMs = timeoutMs || 30000;
    return new Promise(function (resolve, reject) {
      if (!name || typeof wx.loadSubpackage !== 'function') {
        resolve();
        return;
      }
      var settled = false;
      var timer = setTimeout(function () {
        if (settled) return;
        settled = true;
        warn('Subpackage load timeout: ' + name + ', assuming already loaded');
        resolve();
      }, timeoutMs);

      wx.loadSubpackage({
        name: name,
        success: function (res) {
          if (settled) return;
          settled = true;
          clearTimeout(timer);
          log('Subpackage loaded: ' + name);
          resolve(res);
        },
        fail: function (err) {
          if (settled) return;
          settled = true;
          clearTimeout(timer);
          warn('Subpackage load failed: ' + name + ' - ' + (err.errMsg || 'unknown'));
          reject(new Error('Subpackage ' + name + ' load failed: ' + (err.errMsg || 'unknown')));
        },
      });
    });
  }

  // ============================================================
  // CDN 下载与缓存
  // ============================================================
  function downloadFile(url, timeoutMs) {
    timeoutMs = timeoutMs || 60000;
    return new Promise(function (resolve, reject) {
      var settled = false;
      var timer = setTimeout(function () {
        if (settled) return;
        settled = true;
        reject(new Error('Download timeout after ' + timeoutMs + 'ms: ' + url));
      }, timeoutMs);

      wx.downloadFile({
        url: url,
        timeout: timeoutMs,
        success: function (res) {
          if (settled) return;
          if (res.statusCode !== 200 || !res.tempFilePath) {
            settled = true;
            clearTimeout(timer);
            reject(new Error('Download HTTP ' + res.statusCode + ': ' + url));
            return;
          }
          try {
            var ab = readFileAsArrayBuffer(res.tempFilePath);
            var fileName = url.split('/').pop().split('?')[0];
            if (USER_DATA_PATH && fileName) {
              try {
                var cachePath = USER_DATA_PATH + '/' + fileName;
                fs.writeFileSync(cachePath, new Uint8Array(ab), 'binary');
                log('Cached to USER_DATA_PATH: ' + fileName);
              } catch (ce) {
                warn('Failed to cache file: ' + fileName + ' - ' + ce.message);
              }
            }
            settled = true;
            clearTimeout(timer);
            log('Downloaded: ' + fileName + ' (' + (ab.byteLength / 1048576).toFixed(2) + ' MB)');
            resolve(ab);
          } catch (e) {
            settled = true;
            clearTimeout(timer);
            reject(new Error('Read downloaded file failed: ' + e.message));
          }
        },
        fail: function (err) {
          if (settled) return;
          settled = true;
          clearTimeout(timer);
          reject(new Error('Download failed: ' + (err.errMsg || 'unknown') + ' - ' + url));
        },
      });
    });
  }

  function readCachedFile(fileName) {
    if (!fs || !USER_DATA_PATH || !fileName) return null;
    try {
      var path = USER_DATA_PATH + '/' + fileName;
      var stat = fs.statSync(path);
      if (stat.size > 0) {
        var ab = readFileAsArrayBuffer(path);
        log('Cache hit: ' + fileName + ' (' + (ab.byteLength / 1048576).toFixed(2) + ' MB)');
        return ab;
      }
    } catch (e) {}
    return null;
  }

  // ============================================================
  // WASM 路径解析与智能实例化
  // ============================================================

  function resolveWasmPath() {
    var wasmFileName = globalThis._wasmFileName || 'index.wasm';
    var cdnBaseUrl = globalThis._cdnBaseUrl || '';
    var wasmSubpkg = globalThis._wasmSubpkg || '';
    var brInSubpkg = globalThis._wasmBrInSubpkg === true;
    var brFileName = wasmFileName + '.br';

    if (wasmSubpkg && brInSubpkg) {
      var subpkgBrPath = wasmSubpkg + '/' + brFileName;
      return loadSubpackage(wasmSubpkg).then(function () {
        if (tryAccessFile(subpkgBrPath)) {
          log('WASM in subpackage: ' + subpkgBrPath);
          return subpkgBrPath;
        }
        warn('Subpackage loaded but file not found: ' + subpkgBrPath);
        return resolveWasmFallback(wasmFileName, brFileName, cdnBaseUrl);
      }).catch(function () {
        return resolveWasmFallback(wasmFileName, brFileName, cdnBaseUrl);
      });
    }

    return resolveWasmFallback(wasmFileName, brFileName, cdnBaseUrl);
  }

  function resolveWasmFallback(wasmFileName, brFileName, cdnBaseUrl) {
    var candidates = [brFileName, wasmFileName];

    for (var i = 0; i < candidates.length; i++) {
      if (tryAccessFile(candidates[i])) {
        log('WASM in main package: ' + candidates[i]);
        return Promise.resolve(candidates[i]);
      }
    }

    if (USER_DATA_PATH) {
      for (var j = 0; j < candidates.length; j++) {
        var cachedAb = readCachedFile(candidates[j]);
        if (cachedAb) {
          var tmpPath = writeTempFile(cachedAb, 'wasm');
          log('WASM from cache, written to temp: ' + tmpPath);
          return Promise.resolve(tmpPath);
        }
      }
    }

    if (!cdnBaseUrl) {
      return Promise.reject(new Error('WASM not found locally and no CDN URL configured'));
    }

    cdnBaseUrl = cdnBaseUrl.replace(/\/$/, '');

    function downloadOne(fileName) {
      var url = cdnBaseUrl + '/' + fileName;
      return downloadFile(url).then(function (ab) {
        var tmpPath = writeTempFile(ab, fileName.endsWith('.br') ? 'wasm.br' : 'wasm');
        log('WASM downloaded and saved to temp: ' + tmpPath);
        return tmpPath;
      });
    }

    return downloadOne(brFileName).catch(function () {
      log('.wasm.br download failed, trying .wasm');
      return downloadOne(wasmFileName);
    });
  }

  function normalizeWasmResult(result) {
    if (result && result.instance && result.module) return result;
    return { instance: result.instance || result, module: result.module || null };
  }

  function instantiateWasmSmart(imports) {
    return resolveWasmPath().then(function (wasmPath) {
      var isBr = wasmPath && wasmPath.length > 3 && wasmPath.substring(wasmPath.length - 3) === '.br';
      log('WASM path: ' + wasmPath + ' (brotli=' + isBr + ')');

      if (isBr) {
        if (!_WXWA || typeof _WXWA.instantiate !== 'function') {
          return Promise.reject(new Error('.wasm.br requires WXWebAssembly for auto-decompression'));
        }
        return _WXWA.instantiate(wasmPath, imports).then(normalizeWasmResult);
      }

      if (_nativeWA && typeof _nativeWA.instantiate === 'function') {
        return new Promise(function (resolve, reject) {
          try {
            var ab = readFileAsArrayBuffer(wasmPath);
            _nativeWA.instantiate(ab, imports).then(resolve, reject);
          } catch (e) {
            reject(e);
          }
        }).then(normalizeWasmResult, function (err) {
          warn('Native WA failed, falling back to WXWebAssembly: ' + (err.message || err));
          if (!_WXWA) return Promise.reject(err);
          return _WXWA.instantiate(wasmPath, imports).then(normalizeWasmResult);
        });
      }

      if (_WXWA && typeof _WXWA.instantiate === 'function') {
        return _WXWA.instantiate(wasmPath, imports).then(normalizeWasmResult);
      }

      return Promise.reject(new Error('No WebAssembly implementation available'));
    });
  }

  var WAPolyfill = {
    instantiate: function (source, imports) {
      if (typeof source === 'string') {
        return _WXWA ? _WXWA.instantiate(source, imports).then(normalizeWasmResult) :
               Promise.reject(new Error('WXWebAssembly not available'));
      }
      var tmpPath;
      try {
        tmpPath = writeTempFile(source, 'wasm');
      } catch (e) {
        return Promise.reject(e);
      }
      if (!_WXWA) {
        deleteTempFile(tmpPath);
        return Promise.reject(new Error('WXWebAssembly not available'));
      }
      return _WXWA.instantiate(tmpPath, imports).then(function (r) {
        deleteTempFile(tmpPath);
        return normalizeWasmResult(r);
      }, function (err) {
        deleteTempFile(tmpPath);
        throw err;
      });
    },
    compile: function (source) {
      if (typeof source === 'string') {
        return _WXWA ? _WXWA.compile(source) : Promise.reject(new Error('WXWebAssembly not available'));
      }
      var tmpPath;
      try { tmpPath = writeTempFile(source, 'wasm'); } catch (e) { return Promise.reject(e); }
      if (!_WXWA) { deleteTempFile(tmpPath); return Promise.reject(new Error('WXWebAssembly not available')); }
      return _WXWA.compile(tmpPath).then(function (mod) {
        deleteTempFile(tmpPath);
        return mod;
      }, function (err) {
        deleteTempFile(tmpPath);
        throw err;
      });
    },
    validate: function (source) {
      if (typeof source === 'string') {
        return _WXWA ? _WXWA.validate(source) : false;
      }
      var tmpPath;
      try { tmpPath = writeTempFile(source, 'wasm'); } catch (e) { return false; }
      var result = false;
      try { result = _WXWA.validate(tmpPath); } catch (e) {}
      deleteTempFile(tmpPath);
      return result;
    },
    instantiateStreaming: function (source, imports) {
      if (source && typeof source.then === 'function') {
        return source.then(function (resp) { return resp.arrayBuffer(); })
                     .then(function (buf) { return WAPolyfill.instantiate(buf, imports); });
      }
      return Promise.reject(new Error('instantiateStreaming not supported'));
    },
    Memory: (_nativeWA && _nativeWA.Memory) || (_WXWA && _WXWA.Memory) || function () {
      throw new Error('WebAssembly.Memory not available');
    },
    Table: (_nativeWA && _nativeWA.Table) || (_WXWA && _WXWA.Table) || function () {
      throw new Error('WebAssembly.Table not available');
    },
    Global: (_nativeWA && _nativeWA.Global) || (_WXWA && _WXWA.Global) || function () {
      throw new Error('WebAssembly.Global not available');
    },
    RuntimeError: (_nativeWA && _nativeWA.RuntimeError) || (_WXWA && _WXWA.RuntimeError) || Error,
    CompileError: (_nativeWA && _nativeWA.CompileError) || (_WXWA && _WXWA.CompileError) || Error,
    LinkError: (_nativeWA && _nativeWA.LinkError) || (_WXWA && _WXWA.LinkError) || Error,
  };
  safeDefineGlobal('WebAssembly', WAPolyfill);
  safeDefineGlobal('_instantiateWasmSmart', instantiateWasmSmart);
  safeDefineGlobal('_resolveWasmPath', resolveWasmPath);
  safeDefineGlobal('_ensureSubpkgLoaded', loadSubpackage);
  safeDefineGlobal('_cleanupTempFiles', cleanupAllTempFiles);
  log('WebAssembly polyfill installed');

  // ============================================================
  // TextDecoder / TextEncoder polyfill
  // 微信小游戏环境缺少这两个API，Emscripten解码GLSL着色器时必需
  // ============================================================
  if (typeof TextDecoder === 'undefined') {
    function TextDecoder(encoding) {
      this._encoding = (encoding || 'utf-8').toLowerCase();
      if (this._encoding !== 'utf-8' && this._encoding !== 'utf8') {
        warn('TextDecoder: only utf-8 supported, got ' + this._encoding);
      }
    }
    TextDecoder.prototype.decode = function (input) {
      var buf;
      if (input instanceof ArrayBuffer) {
        buf = new Uint8Array(input);
      } else if (ArrayBuffer.isView(input)) {
        buf = new Uint8Array(input.buffer, input.byteOffset, input.byteLength);
      } else {
        throw new TypeError('TextDecoder.decode expects ArrayBuffer or ArrayBufferView');
      }
      var result = '';
      var i = 0;
      while (i < buf.length) {
        var byte1 = buf[i++];
        if (byte1 < 0x80) {
          result += String.fromCharCode(byte1);
        } else if (byte1 >= 0xC0 && byte1 < 0xE0) {
          var byte2 = buf[i++];
          result += String.fromCharCode(((byte1 & 0x1F) << 6) | (byte2 & 0x3F));
        } else if (byte1 >= 0xE0 && byte1 < 0xF0) {
          var b2 = buf[i++];
          var b3 = buf[i++];
          var cp = ((byte1 & 0x0F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
          if (cp >= 0xD800 && cp <= 0xDFFF) {
            result += '\uFFFD';
          } else {
            result += String.fromCharCode(cp);
          }
        } else if (byte1 >= 0xF0 && byte1 < 0xF8) {
          var b2_ = buf[i++];
          var b3_ = buf[i++];
          var b4 = buf[i++];
          var cp2 = ((byte1 & 0x07) << 18) | ((b2_ & 0x3F) << 12) | ((b3_ & 0x3F) << 6) | (b4 & 0x3F);
          cp2 -= 0x10000;
          var surrogate1 = 0xD800 + (cp2 >> 10);
          var surrogate2 = 0xDC00 + (cp2 & 0x3FF);
          result += String.fromCharCode(surrogate1, surrogate2);
        } else {
          result += '\uFFFD';
        }
      }
      return result;
    };
    safeDefineGlobal('TextDecoder', TextDecoder);
  }

  if (typeof TextEncoder === 'undefined') {
    function TextEncoder() {
      this.encoding = 'utf-8';
    }
    TextEncoder.prototype.encode = function (str) {
      var bytes = [];
      for (var i = 0; i < str.length; i++) {
        var code = str.charCodeAt(i);
        if (code < 0x80) {
          bytes.push(code);
        } else if (code < 0x800) {
          bytes.push(0xC0 | (code >> 6), 0x80 | (code & 0x3F));
        } else if (code >= 0xD800 && code <= 0xDBFF && i + 1 < str.length) {
          var next = str.charCodeAt(i + 1);
          if (next >= 0xDC00 && next <= 0xDFFF) {
            var cp = 0x10000 + (((code & 0x3FF) << 10) | (next & 0x3FF));
            bytes.push(0xF0 | (cp >> 18), 0x80 | ((cp >> 12) & 0x3F), 0x80 | ((cp >> 6) & 0x3F), 0x80 | (cp & 0x3F));
            i++;
          } else {
            bytes.push(0xEF, 0xBF, 0xBD);
          }
        } else {
          bytes.push(0xE0 | (code >> 12), 0x80 | ((code >> 6) & 0x3F), 0x80 | (code & 0x3F));
        }
      }
      return new Uint8Array(bytes);
    };
    safeDefineGlobal('TextEncoder', TextEncoder);
  }
  log('TextDecoder/TextEncoder polyfill installed');

  // ============================================================
  // Canvas polyfill
  // ============================================================
  var _mainCanvas = null;
  var _cachedSysInfo = null;

  function getSysInfo() {
    if (!_cachedSysInfo) {
      try { _cachedSysInfo = wx.getSystemInfoSync(); } catch (e) { _cachedSysInfo = {}; }
    }
    return _cachedSysInfo;
  }

  function getMainCanvas() {
    if (!_mainCanvas) {
      _mainCanvas = wx.createCanvas();
      _mainCanvas.id = 'canvas';

      try {
        var gl = _mainCanvas.getContext('webgl2');
        if (!gl) {
          gl = _mainCanvas.getContext('webgl');
        }
        log('Canvas GL context probe: ' + (gl ? (gl instanceof WebGL2RenderingContext ? 'webgl2' : 'webgl') : 'null'));
      } catch (e) {
        warn('Canvas GL probe threw: ' + e.message);
      }

      _mainCanvas.style = _mainCanvas.style || {};
      var info = getSysInfo();
      var dpr = info.pixelRatio || 1;
      var winW = info.windowWidth || 375;
      var winH = info.windowHeight || 667;
      _mainCanvas.width = Math.floor(winW * dpr);
      _mainCanvas.height = Math.floor(winH * dpr);
      _mainCanvas.style.width = winW + 'px';
      _mainCanvas.style.height = winH + 'px';
      _mainCanvas.style.display = 'block';
      _mainCanvas.tabIndex = 0;

      _mainCanvas.getBoundingClientRect = function () {
        return {
          left: 0, top: 0,
          width: _mainCanvas.width, height: _mainCanvas.height,
          right: _mainCanvas.width, bottom: _mainCanvas.height
        };
      };

      var _listeners = {};
      _mainCanvas.addEventListener = function (type, listener) {
        _listeners[type] = _listeners[type] || [];
        if (_listeners[type].indexOf(listener) < 0) {
          _listeners[type].push(listener);
        }
      };
      _mainCanvas.removeEventListener = function (type, listener) {
        if (!_listeners[type]) return;
        var idx = _listeners[type].indexOf(listener);
        if (idx >= 0) _listeners[type].splice(idx, 1);
      };
      _mainCanvas.dispatchEvent = function (event) {
        event.target = _mainCanvas;
        event.currentTarget = _mainCanvas;
        event.preventDefault = event.preventDefault || function () {};
        event.stopPropagation = event.stopPropagation || function () {};
        event.defaultPrevented = false;
        var ls = _listeners[event.type] || [];
        for (var i = 0; i < ls.length; i++) {
          try { ls[i].call(_mainCanvas, event); } catch (e) { error('Canvas event listener error: ' + e.message); }
        }
      };
      _mainCanvas.focus = function () {};

      log('Canvas initialized: ' + _mainCanvas.width + 'x' + _mainCanvas.height + ' (dpr=' + dpr + ')');
    }
    return _mainCanvas;
  }

  function HTMLCanvasElement() {}
  var _origCreateCanvas = wx.createCanvas;
  wx.createCanvas = function () {
    var canvas = _origCreateCanvas.apply(wx, arguments);
    if (!(canvas instanceof HTMLCanvasElement)) {
      Object.setPrototypeOf(HTMLCanvasElement.prototype, Object.getPrototypeOf(canvas));
      Object.setPrototypeOf(canvas, HTMLCanvasElement.prototype);
    }
    return canvas;
  };
  HTMLCanvasElement.prototype.getBoundingClientRect = function () {
    return { left: 0, top: 0, width: this.width, height: this.height, right: this.width, bottom: this.height };
  };
  HTMLCanvasElement.prototype.addEventListener = function () {};
  HTMLCanvasElement.prototype.removeEventListener = function () {};
  HTMLCanvasElement.prototype.focus = function () {};
  safeDefineGlobal('HTMLCanvasElement', HTMLCanvasElement);
  safeDefineGlobal('__godotGetMainCanvas', getMainCanvas);

  // ============================================================
  // Image polyfill
  // ============================================================
  function HTMLImageElement() {
    return wx.createImage();
  }
  safeDefineGlobal('Image', HTMLImageElement);
  safeDefineGlobal('HTMLImageElement', HTMLImageElement);

  // ============================================================
  // AudioContext polyfill - STUB: 最小可用实现
  // Godot音频在微信下需要使用wx.createInnerAudioContext，当前为桩实现
  // TODO: 实现完整的音频支持
  // ============================================================
  function AudioContext() {
    this._ctx = null;
    try { this._ctx = wx.createInnerAudioContext(); } catch (e) {}
    this.destination = {};
    this.sampleRate = 44100;
    this.state = 'running';
  }
  AudioContext.prototype.createBufferSource = function () {
    return {
      buffer: null, loop: false,
      start: function () {}, stop: function () {}, connect: function () {}, disconnect: function () {},
      onended: null, playbackRate: { value: 1 },
    };
  };
  AudioContext.prototype.createGain = function () {
    return { gain: { value: 1, setValueAtTime: function () {}, linearRampToValueAtTime: function () {} }, connect: function () {}, disconnect: function () {} };
  };
  AudioContext.prototype.createScriptProcessor = function () {
    return { connect: function () {}, disconnect: function () {}, onaudioprocess: null };
  };
  AudioContext.prototype.decodeAudioData = function (arrayBuffer, success, error) {
    if (success) setTimeout(function () { success({}); }, 0);
  };
  AudioContext.prototype.close = function () { this.state = 'closed'; return Promise.resolve(); };
  AudioContext.prototype.resume = function () { this.state = 'running'; return Promise.resolve(); };
  AudioContext.prototype.suspend = function () { this.state = 'suspended'; return Promise.resolve(); };
  AudioContext.prototype.createStereoPanner = function () { return { connect: function () {}, disconnect: function () {}, pan: { value: 0 } }; };
  AudioContext.prototype.createOscillator = function () {
    return { connect: function () {}, disconnect: function () {}, start: function () {}, stop: function () {}, frequency: { value: 440, setValueAtTime: function () {} } };
  };
  safeDefineGlobal('AudioContext', AudioContext);
  safeDefineGlobal('webkitAudioContext', AudioContext);

  // ============================================================
  // 输入事件：触摸 + 键盘
  // ============================================================
  var _touchStartX = 0, _touchStartY = 0, _touchActive = false;
  var SWIPE_THRESHOLD = 30;

  function createKeyEvent(type, keyName, keyCode) {
    try {
      return new KeyboardEvent(type, {
        key: keyName, code: keyName, keyCode: keyCode, which: keyCode,
        bubbles: true, cancelable: true,
      });
    } catch (e) {
      return {
        type: type, key: keyName, code: keyName, keyCode: keyCode, which: keyCode,
        bubbles: true, cancelable: true,
        preventDefault: function () {}, stopPropagation: function () {},
        defaultPrevented: false,
      };
    }
  }

  function dispatchKeyEvent(type, keyName, keyCode) {
    var event = createKeyEvent(type, keyName, keyCode);
    var canvas = getMainCanvas();
    canvas.dispatchEvent(event);
  }

  function makeTouchPoint(t) {
    return {
      clientX: t.clientX, clientY: t.clientY, identifier: t.identifier,
      pageX: t.pageX || t.clientX, pageY: t.pageY || t.clientY,
      screenX: t.screenX || t.clientX, screenY: t.screenY || t.clientY
    };
  }

  function dispatchTouch(type, touches) {
    var canvas = getMainCanvas();
    var touchList = touches.map(makeTouchPoint);
    var event = {
      type: type,
      target: canvas,
      currentTarget: canvas,
      touches: touchList,
      changedTouches: touchList,
      preventDefault: function () { this.defaultPrevented = true; },
      stopPropagation: function () {},
      defaultPrevented: false,
      timeStamp: Date.now(),
    };
    canvas.dispatchEvent(event);

    if (touches.length > 0) {
      var t = touches[0];
      if (type === 'touchstart') {
        _touchStartX = t.clientX;
        _touchStartY = t.clientY;
        _touchActive = true;
      } else if (type === 'touchend' && _touchActive) {
        _touchActive = false;
        var dx = t.clientX - _touchStartX;
        var dy = t.clientY - _touchStartY;
        var absDx = dx > 0 ? dx : -dx;
        var absDy = dy > 0 ? dy : -dy;
        if (absDx >= SWIPE_THRESHOLD || absDy >= SWIPE_THRESHOLD) {
          var keyName, keyCode;
          if (absDx > absDy) {
            keyName = dx > 0 ? 'ArrowRight' : 'ArrowLeft';
            keyCode = dx > 0 ? 39 : 37;
          } else {
            keyName = dy > 0 ? 'ArrowDown' : 'ArrowUp';
            keyCode = dy > 0 ? 40 : 38;
          }
          log('Swipe: ' + keyName);
          dispatchKeyEvent('keydown', keyName, keyCode);
          setTimeout(function () { dispatchKeyEvent('keyup', keyName, keyCode); }, 50);
        }
      }
    }
  }

  if (wx.onTouchStart) {
    wx.onTouchStart(function (e) { dispatchTouch('touchstart', e.touches); });
    wx.onTouchMove(function (e) { dispatchTouch('touchmove', e.touches); });
    wx.onTouchEnd(function (e) { dispatchTouch('touchend', e.changedTouches); });
    wx.onTouchCancel(function (e) { dispatchTouch('touchcancel', e.changedTouches); });
  }

  if (wx.onKeyDown && wx.onKeyUp) {
    function makeKeyHandler(type) {
      return function (e) {
        var keyName = e.key || '';
        var keyCode = e.keyCode || 0;
        if (!keyCode) {
          var keyMap = { 'ArrowUp': 38, 'ArrowDown': 40, 'ArrowLeft': 37, 'ArrowRight': 39, 'Enter': 13, 'Backspace': 8, 'Space': 32 };
          keyCode = keyMap[keyName] || (keyName.length === 1 ? keyName.charCodeAt(0) : 0);
        }
        dispatchKeyEvent(type, keyName, keyCode);
      };
    }
    wx.onKeyDown(makeKeyHandler('keydown'));
    wx.onKeyUp(makeKeyHandler('keyup'));
  }

  if (typeof KeyboardEvent === 'undefined') {
    function KeyboardEvent(type, init) {
      init = init || {};
      this.type = type;
      this.key = init.key || '';
      this.code = init.code || init.key || '';
      this.keyCode = init.keyCode || 0;
      this.which = init.which || this.keyCode;
      this.bubbles = init.bubbles !== false;
      this.cancelable = init.cancelable !== false;
      this.defaultPrevented = false;
      this.target = null;
      this.currentTarget = null;
    }
    KeyboardEvent.prototype.preventDefault = function () { this.defaultPrevented = true; };
    KeyboardEvent.prototype.stopPropagation = function () {};
    safeDefineGlobal('KeyboardEvent', KeyboardEvent);
  }

  // ============================================================
  // atob / btoa polyfill
  // ============================================================
  var B64_CHARS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
  var B64_LOOKUP = {};
  for (var _i = 0; _i < B64_CHARS.length; _i++) {
    B64_LOOKUP[B64_CHARS.charAt(_i)] = _i;
  }

  function atobPolyfill(b64) {
    if (wx.base64ToArrayBuffer) {
      try {
        var ab = wx.base64ToArrayBuffer(String(b64).replace(/=+$/, ''));
        var bytes = new Uint8Array(ab);
        var s = '';
        for (var i = 0; i < bytes.length; i++) s += String.fromCharCode(bytes[i]);
        return s;
      } catch (e) {
        warn('wx.base64ToArrayBuffer failed, using JS impl: ' + e.message);
      }
    }
    b64 = String(b64).replace(/=+$/, '');
    var output = '';
    var bs = 0, buff = 0;
    for (var j = 0; j < b64.length; j++) {
      var c = B64_LOOKUP[b64.charAt(j)];
      if (c === undefined) continue;
      buff = (buff << 6) | c;
      bs += 6;
      if (bs >= 8) { bs -= 8; output += String.fromCharCode((buff >> bs) & 0xFF); }
    }
    return output;
  }

  function btoaPolyfill(s) {
    if (wx.arrayBufferToBase64) {
      try {
        var bytes = new Uint8Array(s.length);
        for (var i = 0; i < s.length; i++) bytes[i] = s.charCodeAt(i) & 0xFF;
        return wx.arrayBufferToBase64(bytes.buffer);
      } catch (e) {
        warn('wx.arrayBufferToBase64 failed, using JS impl: ' + e.message);
      }
    }
    var output = '';
    var bs = 0, buff = 0;
    for (var j = 0; j < s.length; j++) {
      buff = (buff << 8) | (s.charCodeAt(j) & 0xFF);
      bs += 8;
      while (bs >= 6) { bs -= 6; output += B64_CHARS.charAt((buff >> bs) & 0x3F); }
    }
    if (bs > 0) output += B64_CHARS.charAt((buff << (6 - bs)) & 0x3F);
    while (output.length % 4) output += '=';
    return output;
  }

  safeDefineGlobal('atob', atobPolyfill);
  safeDefineGlobal('btoa', btoaPolyfill);

  // ============================================================
  // localStorage polyfill
  // ============================================================
  safeDefineGlobal('localStorage', {
    getItem: function (key) { try { return wx.getStorageSync(key); } catch (e) { return null; } },
    setItem: function (key, value) { try { wx.setStorageSync(key, value); } catch (e) {} },
    removeItem: function (key) { try { wx.removeStorageSync(key); } catch (e) {} },
    clear: function () { try { wx.clearStorageSync(); } catch (e) {} },
  });

  // ============================================================
  // URL polyfill
  // ============================================================
  if (typeof URL === 'undefined') {
    function URLShim(url) {
      this.href = url;
      this.pathname = url;
      this.origin = '';
      this.search = '';
      this.hash = '';
    }
    URLShim.createObjectURL = function () { return ''; };
    URLShim.revokeObjectURL = function () {};
    safeDefineGlobal('URL', URLShim);
  } else if (!URL.createObjectURL) {
    URL.createObjectURL = function () { return ''; };
    URL.revokeObjectURL = function () {};
  }

  // ============================================================
  // ReadableStream polyfill
  // ============================================================
  if (typeof ReadableStream === 'undefined') {
    function ReadableStream(underlyingSource) {
      this._source = underlyingSource || {};
      this._started = false;
      this._chunks = [];
      this._waiter = null;
      this._closed = false;
      this._error = null;
      var self = this;
      this._controller = {
        enqueue: function (chunk) {
          if (self._waiter) {
            var w = self._waiter; self._waiter = null;
            w({ done: false, value: chunk });
          } else {
            self._chunks.push(chunk);
          }
        },
        close: function () {
          self._closed = true;
          if (self._waiter) {
            var w = self._waiter; self._waiter = null;
            w({ done: true, value: undefined });
          }
        },
        error: function (err) {
          self._closed = true;
          self._error = err;
          if (self._waiter) {
            var w = self._waiter; self._waiter = null;
            w(Promise.reject(err));
          }
        },
      };
    }
    ReadableStream.prototype.getReader = function () {
      var self = this;
      if (!self._started && self._source.start) {
        self._started = true;
        try { self._source.start(self._controller); } catch (e) { self._controller.error(e); }
      }
      return {
        read: function () {
          if (self._chunks.length > 0) {
            return Promise.resolve({ done: false, value: self._chunks.shift() });
          }
          if (self._closed) return Promise.resolve({ done: true, value: undefined });
          if (self._error) return Promise.reject(self._error);
          return new Promise(function (resolve) { self._waiter = resolve; });
        },
        cancel: function () {
          self._closed = true;
          self._chunks = [];
          if (self._waiter) { var w = self._waiter; self._waiter = null; w({ done: true }); }
          return Promise.resolve();
        },
        releaseLock: function () {},
      };
    };
    safeDefineGlobal('ReadableStream', ReadableStream);
  }

  // ============================================================
  // Headers polyfill
  // ============================================================
  if (typeof Headers === 'undefined') {
    function Headers(init) {
      this._h = {};
      if (init) {
        if (Array.isArray(init)) {
          for (var i = 0; i < init.length; i++) this._h[init[i][0].toLowerCase()] = init[i][1];
        } else if (typeof init === 'object') {
          for (var k in init) if (Object.prototype.hasOwnProperty.call(init, k)) this._h[k.toLowerCase()] = init[k];
        }
      }
    }
    Headers.prototype.get = function (n) { return this._h[n.toLowerCase()] || null; };
    Headers.prototype.set = function (n, v) { this._h[n.toLowerCase()] = String(v); };
    Headers.prototype.has = function (n) { return n.toLowerCase() in this._h; };
    Headers.prototype.forEach = function (cb) {
      for (var k in this._h) if (Object.prototype.hasOwnProperty.call(this._h, k)) cb(this._h[k], k, this);
    };
    Headers.prototype.append = function (n, v) {
      var key = n.toLowerCase();
      this._h[key] = this._h[key] ? this._h[key] + ', ' + v : String(v);
    };
    Headers.prototype.delete = function (n) { delete this._h[n.toLowerCase()]; };
    safeDefineGlobal('Headers', Headers);
  }

  // ============================================================
  // Response polyfill
  // ============================================================
  function bufferToStream(uint8) {
    var idx = 0;
    return {
      getReader: function () {
        return {
          read: function () {
            if (idx >= uint8.length) return Promise.resolve({ done: true });
            var sz = Math.min(65536, uint8.length - idx);
            var chunk = uint8.slice(idx, idx + sz);
            idx += sz;
            return Promise.resolve({ done: false, value: chunk });
          },
          cancel: function () { idx = uint8.length; return Promise.resolve(); },
          releaseLock: function () {},
        };
      },
    };
  }

  function Response(body, init) {
    init = init || {};
    this.status = init.status || 200;
    this.ok = this.status >= 200 && this.status < 300;
    this.statusText = init.statusText || (this.ok ? 'OK' : 'Error');
    this.headers = (init.headers instanceof Headers) ? init.headers : new Headers(init.headers);
    this.bodyUsed = false;
    this.url = init.url || '';
    this.redirected = false;
    this.type = 'basic';
    this._buffer = null;

    if (body instanceof ReadableStream) {
      this.body = body;
    } else if (body instanceof ArrayBuffer) {
      this._buffer = body;
      this.body = bufferToStream(new Uint8Array(body));
    } else if (body && body.buffer instanceof ArrayBuffer) {
      this._buffer = uint8ToArrayBuffer(body);
      this.body = bufferToStream(new Uint8Array(this._buffer));
    } else if (typeof body === 'string') {
      var arr = new Uint8Array(body.length);
      for (var i = 0; i < body.length; i++) arr[i] = body.charCodeAt(i) & 0xFF;
      this._buffer = arr.buffer;
      this.body = bufferToStream(arr);
    } else if (body == null) {
      this._buffer = new ArrayBuffer(0);
      this.body = bufferToStream(new Uint8Array(0));
    } else {
      try {
        var ab = new ArrayBuffer(body.length || 0);
        var v = new Uint8Array(ab);
        for (var j = 0; j < (body.length || 0); j++) v[j] = body[j];
        this._buffer = ab;
        this.body = bufferToStream(v);
      } catch (e) {
        this._buffer = new ArrayBuffer(0);
        this.body = bufferToStream(new Uint8Array(0));
      }
    }
  }

  Response.prototype.arrayBuffer = function () {
    if (this.bodyUsed) return Promise.reject(new TypeError('Body already used'));
    this.bodyUsed = true;
    if (this._buffer) return Promise.resolve(this._buffer.slice(0));
    var reader = this.body.getReader();
    var chunks = [];
    var total = 0;
    function read() {
      return reader.read().then(function (r) {
        if (r.done) {
          var combined = new Uint8Array(total);
          var off = 0;
          for (var i = 0; i < chunks.length; i++) { combined.set(chunks[i], off); off += chunks[i].length; }
          return combined.buffer;
        }
        chunks.push(r.value);
        total += r.value.length;
        return read();
      });
    }
    return read();
  };

  Response.prototype.text = function () {
    return this.arrayBuffer().then(function (buf) {
      if (typeof TextDecoder !== 'undefined') {
        return new TextDecoder('utf-8').decode(buf);
      }
      var arr = new Uint8Array(buf);
      var s = '';
      for (var i = 0; i < arr.length; i++) s += String.fromCharCode(arr[i]);
      try { return decodeURIComponent(escape(s)); } catch (e) { return s; }
    });
  };

  Response.prototype.json = function () {
    return this.text().then(JSON.parse);
  };

  Response.prototype.blob = function () {
    return this.arrayBuffer();
  };

  Response.prototype.clone = function () {
    var self = this;
    if (self._buffer) {
      return new Response(self._buffer.slice(0), {
        status: self.status, statusText: self.statusText, headers: new Headers(self.headers._h || {}),
      });
    }
    var bufPromise = self.arrayBuffer();
    self.bodyUsed = false;
    self.arrayBuffer = function () { return bufPromise; };
    return new Response(new ReadableStream({
      start: function (controller) {
        bufPromise.then(function (buf) {
          var arr = new Uint8Array(buf);
          var off = 0;
          function push() {
            if (off >= arr.length) { controller.close(); return; }
            var sz = Math.min(65536, arr.length - off);
            controller.enqueue(arr.slice(off, off + sz));
            off += sz;
            setTimeout(push, 0);
          }
          push();
        }).catch(controller.error.bind(controller));
      },
    }), { status: self.status, statusText: self.statusText, headers: self.headers });
  };

  safeDefineGlobal('Response', Response);

  // ============================================================
  // fetch polyfill
  // ============================================================
  var _cdnBase = '';

  safeDefineGlobal('_setCdnBase', function (url) {
    _cdnBase = (url || '').replace(/\/$/, '');
    log('CDN base: ' + (_cdnBase || '(none)'));
  });

  function readPackageFile(path) {
    if (!fs) throw new Error('File system unavailable');
    var filename = path.replace(/^\.\//, '').replace(/^\//, '').split('/').pop() || path;
    var attempts = [filename, './' + filename];
    var errors = [];
    for (var i = 0; i < attempts.length; i++) {
      try {
        return readFileAsArrayBuffer(attempts[i]);
      } catch (e) {
        errors.push(attempts[i] + ': ' + e.message);
      }
    }
    throw new Error('File not found: ' + path + ' (' + errors.join('; ') + ')');
  }

  function fetchPolyfill(url, options) {
    options = options || {};
    var method = (options.method || 'GET').toUpperCase();

    if (method !== 'GET') {
      return new Promise(function (resolve, reject) {
        wx.request({
          url: url, method: method, header: options.headers || {},
          responseType: 'arraybuffer', data: options.body,
          success: function (res) {
            resolve(new Response(res.data instanceof ArrayBuffer ? res.data : new ArrayBuffer(0), { status: res.statusCode }));
          },
          fail: function (err) { reject(new Error(err.errMsg || 'fetch failed')); },
        });
      });
    }

    var urlStr = String(url);

    var dataFileName = globalThis._dataFileName || '';
    var dataSubpkg = globalThis._dataSubpkg || '';
    var dataInSubpkg = globalThis._dataInSubpkg === true;
    if (dataFileName && dataInSubpkg && dataSubpkg && urlStr.indexOf(dataFileName) !== -1) {
      var dataPath = dataSubpkg + '/' + dataFileName;
      return loadSubpackage(dataSubpkg).then(function () {
        try {
          var ab = readFileAsArrayBuffer(dataPath);
          log('.data loaded from subpackage: ' + dataPath + ' (' + (ab.byteLength / 1048576).toFixed(2) + ' MB)');
          return new Response(ab, { status: 200, headers: { 'Content-Length': String(ab.byteLength) } });
        } catch (e) {
          if (_cdnBase) {
            var cdnUrl = _cdnBase.replace(/\/$/, '') + '/' + dataFileName;
            warn('Subpackage .data read failed, trying CDN: ' + cdnUrl);
            return downloadFile(cdnUrl).then(function (ab) {
              return new Response(ab, { status: 200, headers: { 'Content-Length': String(ab.byteLength) } });
            });
          }
          throw e;
        }
      });
    }

    if (urlStr.indexOf('http://') === 0 || urlStr.indexOf('https://') === 0) {
      return downloadFile(urlStr).then(function (ab) {
        return new Response(ab, { status: 200, headers: { 'Content-Length': String(ab.byteLength) } });
      });
    }

    if (urlStr.indexOf('file://') === 0) urlStr = urlStr.replace('file://', '');

    if (USER_DATA_PATH && urlStr.indexOf(USER_DATA_PATH) === 0) {
      return new Promise(function (resolve, reject) {
        try {
          var ab = readFileAsArrayBuffer(urlStr);
          resolve(new Response(ab, { status: 200 }));
        } catch (e) { reject(e); }
      });
    }

    return new Promise(function (resolve, reject) {
      var filename = urlStr.split('/').pop() || urlStr;

      if (filename.toLowerCase().endsWith('.wasm') || filename.toLowerCase().endsWith('.wasm.br')) {
        log('WASM fetch detected, returning placeholder (instantiation handled separately): ' + filename);
        resolve(new Response(new ArrayBuffer(1), {
          status: 200,
          headers: { 'Content-Type': 'application/wasm', 'Content-Length': '1' },
        }));
        return;
      }

      try {
        var data = readPackageFile(filename);
        resolve(new Response(data, {
          status: 200,
          headers: { 'Content-Length': String(data.byteLength || 0) },
        }));
      } catch (e) {
        if (_cdnBase) {
          var cdnUrl = _cdnBase + '/' + filename;
          downloadFile(cdnUrl).then(function (ab) {
            resolve(new Response(ab, { status: 200, headers: { 'Content-Length': String(ab.byteLength) } }));
          }).catch(function (cdnErr) {
            reject(new Error('File not found: ' + filename + ' (local: ' + e.message + ', CDN: ' + cdnErr.message + ')'));
          });
        } else {
          reject(new Error('File not found: ' + filename + ' (' + e.message + ')'));
        }
      }
    });
  }

  safeDefineGlobal('fetch', fetchPolyfill);

  // ============================================================
  // XMLHttpRequest polyfill
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
    this.onreadystatechange = null;
    this.onload = null;
    this.onerror = null;
  }
  XMLHttpRequest.prototype.open = function (method, url) {
    this._method = method;
    this._url = url;
    this.readyState = 1;
    if (this.onreadystatechange) this.onreadystatechange();
  };
  XMLHttpRequest.prototype.setRequestHeader = function (k, v) { this._headers[k] = v; };
  XMLHttpRequest.prototype.getResponseHeader = function () { return null; };
  XMLHttpRequest.prototype.send = function (body) {
    var self = this;
    fetchPolyfill(this._url, { method: this._method, headers: this._headers, body: body }).then(function (resp) {
      if (self.responseType === 'arraybuffer') {
        return resp.arrayBuffer().then(function (buf) {
          self.status = resp.status; self.response = buf; self.readyState = 4;
          if (self.onreadystatechange) self.onreadystatechange();
          if (self.onload) self.onload();
        });
      }
      return resp.text().then(function (text) {
        self.status = resp.status; self.responseText = text; self.response = text; self.readyState = 4;
        if (self.onreadystatechange) self.onreadystatechange();
        if (self.onload) self.onload();
      });
    }).catch(function (err) {
      self.status = 0; self.readyState = 4;
      if (self.onerror) self.onerror(err);
    });
  };
  XMLHttpRequest.prototype.abort = function () { this.readyState = 0; };
  safeDefineGlobal('XMLHttpRequest', XMLHttpRequest);

  // ============================================================
  // WebGL 类型
  // ============================================================
  safeDefineGlobal('WebGLRenderingContext', globalThis.WebGLRenderingContext || function () {});
  safeDefineGlobal('WebGL2RenderingContext', globalThis.WebGL2RenderingContext || function () {});

  // ============================================================
  // performance.now
  // ============================================================
  safeDefineGlobal('performance', globalThis.performance || {
    now: function () {
      if (wx.getPerformance) {
        try { return wx.getPerformance().now(); } catch (e) {}
      }
      return Date.now();
    },
  });

  // ============================================================
  // devicePixelRatio
  // ============================================================
  if (typeof globalThis.devicePixelRatio === 'undefined') {
    var info = getSysInfo();
    safeDefineGlobal('devicePixelRatio', info.pixelRatio || 1);
  }

  // ============================================================
  // navigator
  // ============================================================
  safeDefineGlobal('navigator', globalThis.navigator || {
    userAgent: 'WeChat MiniGame', language: 'zh-CN', languages: ['zh-CN', 'en'],
    platform: 'wechat', onLine: true,
  });
  if (!globalThis.navigator.languages) globalThis.navigator.languages = ['zh-CN', 'en'];

  // ============================================================
  // location
  // ============================================================
  safeDefineGlobal('location', globalThis.location || {
    href: 'https://game/gameContext', pathname: '/game/gameContext',
    origin: 'https://game', protocol: 'https:', host: 'game', hostname: 'game',
    search: '', hash: '', port: '',
  });

  // ============================================================
  // window polyfill
  // ============================================================
  safeDefineGlobal('window', globalThis.window || globalThis);

  (function () {
    var win = globalThis.window;
    if (!win || typeof win !== 'object') return;
    try {
      if (typeof win.addEventListener !== 'function') win.addEventListener = function () {};
      if (typeof win.removeEventListener !== 'function') win.removeEventListener = function () {};

      function getProp(name, wxKey, fallback) {
        var info = getSysInfo();
        var v = info[wxKey];
        return (typeof v === 'number' && v > 0) ? v : fallback;
      }

      function defineGetter(obj, name, getter) {
        try {
          Object.defineProperty(obj, name, { get: getter, configurable: true });
        } catch (e) {
          try { obj[name] = getter(); } catch (e2) {}
        }
      }

      defineGetter(win, 'innerWidth', function () { return getProp('innerWidth', 'windowWidth', 375); });
      defineGetter(win, 'innerHeight', function () { return getProp('innerHeight', 'windowHeight', 667); });
      defineGetter(win, 'devicePixelRatio', function () {
        var info = getSysInfo();
        return (typeof info.pixelRatio === 'number' && info.pixelRatio > 0) ? info.pixelRatio : 1;
      });

      if (!win.screen || typeof win.screen !== 'object') {
        try { win.screen = {}; } catch (e) { return; }
      }
      var scr = win.screen;
      defineGetter(scr, 'width', function () { return getProp('width', 'windowWidth', 375); });
      defineGetter(scr, 'height', function () { return getProp('height', 'windowHeight', 667); });
      defineGetter(scr, 'availWidth', function () { return getProp('availWidth', 'windowWidth', 375); });
      defineGetter(scr, 'availHeight', function () { return getProp('availHeight', 'windowHeight', 667); });
    } catch (e) {
      warn('window augment failed: ' + e.message);
    }
  })();

  // ============================================================
  // alert/prompt/confirm - STUB: 仅输出日志
  // ============================================================
  if (typeof globalThis.alert !== 'function') {
    safeDefineGlobal('alert', function (msg) { warn('alert: ' + msg); });
  }
  if (typeof globalThis.prompt !== 'function') {
    safeDefineGlobal('prompt', function (msg) { warn('prompt: ' + msg); return ''; });
  }
  if (typeof globalThis.confirm !== 'function') {
    safeDefineGlobal('confirm', function (msg) { warn('confirm: ' + msg); return false; });
  }

  // ============================================================
  // document polyfill
  // ============================================================
  var _doc = {
    createElement: function (tag) {
      if (tag === 'canvas') return getMainCanvas();
      if (tag === 'img') return new HTMLImageElement();
      return { style: {}, appendChild: function () {}, addEventListener: function () {}, removeEventListener: function () {} };
    },
    getElementById: function (id) { return id === 'canvas' ? getMainCanvas() : null; },
    getElementsByTagName: function (tag) {
      if (tag === 'canvas') {
        var arr = [getMainCanvas()];
        arr.length = 1;
        return arr;
      }
      return [];
    },
    querySelector: function (sel) {
      if (sel === 'canvas' || sel === '#canvas') return getMainCanvas();
      return null;
    },
    addEventListener: function () {},
    removeEventListener: function () {},
    body: { appendChild: function () {}, removeChild: function () {}, style: {}, addEventListener: function () {}, removeEventListener: function () {} },
    documentElement: { style: {}, addEventListener: function () {}, removeEventListener: function () {} },
    hidden: false,
    visibilityState: 'visible',
    currentScript: null,
    location: globalThis.location,
    title: '',
  };
  safeDefineGlobal('document', _doc);
  safeDefineGlobal('__wechatDocument', _doc);

  (function () {
    var targets = [];
    try { if (typeof document !== 'undefined' && document && document !== _doc) targets.push(document); } catch (e) {}
    if (globalThis.document && globalThis.document !== _doc && targets.indexOf(globalThis.document) < 0) {
      targets.push(globalThis.document);
    }
    for (var t = 0; t < targets.length; t++) {
      var doc = targets[t];
      if (typeof doc !== 'object') continue;
      var keys = Object.keys(_doc);
      for (var i = 0; i < keys.length; i++) {
        var k = keys[i];
        if (typeof doc[k] === 'undefined') {
          try { doc[k] = _doc[k]; } catch (e) {}
        }
      }
    }
  })();

  // ============================================================
  // HTMLElement
  // ============================================================
  if (typeof globalThis.HTMLElement === 'undefined') {
    function HTMLElement() {}
    safeDefineGlobal('HTMLElement', HTMLElement);
  }

  // ============================================================
  // requestAnimationFrame
  // ============================================================
  if (typeof globalThis.requestAnimationFrame === 'undefined') {
    var canvas = getMainCanvas();
    if (canvas.requestAnimationFrame) {
      safeDefineGlobal('requestAnimationFrame', canvas.requestAnimationFrame.bind(canvas));
      safeDefineGlobal('cancelAnimationFrame', (canvas.cancelAnimationFrame || function (id) { clearTimeout(id); }).bind(canvas));
    } else {
      var _rafId = 0;
      safeDefineGlobal('requestAnimationFrame', function (cb) {
        _rafId++;
        return setTimeout(function () { cb(Date.now()); }, 16);
      });
      safeDefineGlobal('cancelAnimationFrame', function (id) { clearTimeout(id); });
    }
  }

  log('All polyfills loaded successfully');
})();
