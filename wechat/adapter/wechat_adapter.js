// wechat_adapter.js - 微信小游戏适配层
// 提供完整的 Web API polyfill，让 Godot 4.7 Web 导出包在微信小游戏环境中运行
//
// 核心差异:
// - 微信小游戏使用 WXWebAssembly 而非标准 WebAssembly
// - 微信无 DOM API (window/document/XMLHttpRequest/fetch/Response/ReadableStream)
// - Canvas 通过 wx.createCanvas() 创建
// - 触摸事件通过 wx.onTouchStart/Move/End/Cancel 注册
// - 文件系统: 主包文件通过 wx.getFileSystemManager() 读取，CDN文件通过wx.downloadFile下载

(function () {
  'use strict';

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
      console.warn('[WeChat Adapter] Cannot define global: ' + name);
    }
  }

  // ============================================================
  // 前置: 文件系统 (提前定义，供 module 0 使用)
  // ============================================================
  var fileSystemManager = wx.getFileSystemManager ? wx.getFileSystemManager() : null;
  var USER_DATA_PATH = (wx.env && wx.env.USER_DATA_PATH) ? wx.env.USER_DATA_PATH : '';
  console.log('[WeChat Adapter] USER_DATA_PATH = ' + USER_DATA_PATH);

  // ============================================================
  // 模块 0a: zlib 解压（已删除 - F2 修复）
  // 原实现按 MSB-first 读位（DEFLATE 是 LSB-first）+ decodeSymbol
  // 无退出保护，node 实测死循环。改为：
  //   1. .wasm.br 文件直接交给 WXWebAssembly（微信原生支持 .br 自动解压）
  //   2. 不再自解压，避免死循环
  // ============================================================
  var Zlib = null; // 已废弃，保留引用避免下游报错

  // ============================================================
  // 模块 0: WebAssembly 诊断 + polyfill
  // 关键问题: devtool 的 WXWebAssembly.instantiate(path) 只接受主包内裸文件名
  //   拒绝所有 wxfile: / http: 路径。104MB WASM 无法放进 4MB 主包。
  // 唯一出路: devtool JS 上下文是否提供原生 V8 WebAssembly (接受 ArrayBuffer)?
  // ============================================================
  console.log('[WeChat Diag] === WebAssembly Environment Diagnostic ===');

  // 诊断 1: WXWebAssembly
  var _WXWA = null;
  try {
    if (typeof WXWebAssembly !== 'undefined') {
      _WXWA = WXWebAssembly;
      console.log('[WeChat Diag] WXWebAssembly type: ' + typeof _WXWA);
      console.log('[WeChat Diag] WXWebAssembly keys: ' + (_WXWA ? Object.keys(_WXWA).join(',') : 'null'));
      console.log('[WeChat Diag] WXWebAssembly.instantiate type: ' + typeof (_WXWA && _WXWA.instantiate));
      console.log('[WeChat Diag] WXWebWA.compile type: ' + typeof (_WXWA && _WXWA.compile));
      console.log('[WeChat Diag] WXWebWA.validate type: ' + typeof (_WXWA && _WXWA.validate));
    } else {
      console.log('[WeChat Diag] WXWebAssembly: UNDEFINED');
    }
  } catch (e) {
    console.log('[WeChat Diag] WXWebAssembly access error: ' + e.message);
  }

  // 诊断 2: 原生 WebAssembly (V8 提供，接受 ArrayBuffer)
  // 关键: 微信小游戏上下文可能不提供原生 WebAssembly，只有 WXWebAssembly
  var _nativeWA = null;
  try {
    var waType = typeof WebAssembly;
    console.log('[WeChat Diag] global WebAssembly type: ' + waType);
    if (waType !== 'undefined' && WebAssembly) {
      console.log('[WeChat Diag] WebAssembly keys: ' + Object.keys(WebAssembly).join(','));
      console.log('[WeChat Diag] WebAssembly.instantiate type: ' + typeof WebAssembly.instantiate);
      console.log('[WeChat Diag] WebAssembly.compile type: ' + typeof WebAssembly.compile);
      console.log('[WeChat Diag] WebAssembly.Memory type: ' + typeof WebAssembly.Memory);
      console.log('[WeChat Diag] WebAssembly.Table type: ' + typeof WebAssembly.Table);
      // 检查 instantiate 是否接受 ArrayBuffer (标准 V8 WebAssembly 接受)
      // WXWebAssembly.instantiate 只接受 string path
      var instStr = (WebAssembly.instantiate && WebAssembly.instantiate.toString) ? WebAssembly.instantiate.toString() : 'unknown';
      console.log('[WeChat Diag] WebAssembly.instantiate signature: ' + instStr.substring(0, 200));
      // 如果 instantiate 存在且不是 WXWebAssembly (不同对象)，视为原生
      if (typeof WebAssembly.instantiate === 'function') {
        _nativeWA = WebAssembly;
        console.log('[WeChat Diag] => Native WebAssembly SAVED for ArrayBuffer instantiation');
      }
    } else {
      console.log('[WeChat Diag] => Native WebAssembly NOT AVAILABLE in this context');
    }
  } catch (e) {
    console.log('[WeChat Diag] WebAssembly access error: ' + e.message);
  }

  // 诊断 3: 尝试用原生 WebAssembly.instantiate 编译最小 WASM 模块 (8 字节空模块)
  // 这能确认原生 WebAssembly 是否真正接受 ArrayBuffer
  if (_nativeWA) {
    try {
      // 最小有效 WASM 模块: magic + version + empty module
      var minimalWasm = new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]);
      console.log('[WeChat Diag] Testing native WebAssembly.instantiate(ArrayBuffer)...');
      _nativeWA.instantiate(minimalWasm.buffer, {}).then(function (res) {
        console.log('[WeChat Diag] ✓ Native WebAssembly.instantiate(ArrayBuffer) WORKS! instance: ' + typeof res.instance);
      }, function (err) {
        console.log('[WeChat Diag] ✗ Native WebAssembly.instantiate(ArrayBuffer) FAILED: ' + (err && err.message ? err.message : err));
        // 失败说明不是真正的 V8 WebAssembly，清除
        _nativeWA = null;
      });
    } catch (e) {
      console.log('[WeChat Diag] ✗ Native WebAssembly.instantiate synchronous throw: ' + e.message);
      _nativeWA = null;
    }
  }

  console.log('[WeChat Diag] === End Diagnostic ===');

  var _tempWasmCounter = 0;

  function _writeBufferToTempWasm(buffer) {
    var tempPath = USER_DATA_PATH + '/_temp_' + (++_tempWasmCounter) + '_' + Date.now() + '.wasm';
    var data = buffer instanceof ArrayBuffer ? new Uint8Array(buffer) : new Uint8Array(buffer.buffer || buffer);
    fileSystemManager.writeFileSync(tempPath, data.buffer, 'binary');
    return tempPath;
  }

  var WAPolyfill = {
    instantiate: function (source, imports) {
      if (typeof source === 'string') {
        // 文件路径，直接用 WXWebAssembly
        return _WXWA.instantiate(source, imports);
      }
      // ArrayBuffer/TypedArray: 写入临时文件再实例化
      return new Promise(function (resolve, reject) {
        var tempPath;
        try {
          tempPath = _writeBufferToTempWasm(source);
        } catch (e) {
          reject(new Error('Write temp wasm failed: ' + e.message));
          return;
        }
        _WXWA.instantiate(tempPath, imports).then(function (result) {
          try { fileSystemManager.unlinkSync(tempPath); } catch (e) {}
          resolve(result);
        }, function (err) {
          try { fileSystemManager.unlinkSync(tempPath); } catch (e) {}
          reject(err);
        });
      });
    },
    compile: function (source) {
      if (typeof source === 'string') return _WXWA.compile(source);
      return new Promise(function (resolve, reject) {
        var tempPath;
        try {
          tempPath = _writeBufferToTempWasm(source);
        } catch (e) { reject(e); return; }
        _WXWA.compile(tempPath).then(function (mod) {
          try { fileSystemManager.unlinkSync(tempPath); } catch (e) {}
          resolve(mod);
        }, function (err) {
          try { fileSystemManager.unlinkSync(tempPath); } catch (e) {}
          reject(err);
        });
      });
    },
    validate: function (source) {
      if (typeof source === 'string') return _WXWA.validate(source);
      // ArrayBuffer 模式：写临时文件再验证
      try {
        var tempPath = _writeBufferToTempWasm(source);
        var result = _WXWA.validate(tempPath);
        try { fileSystemManager.unlinkSync(tempPath); } catch (e) {}
        return result;
      } catch (e) { return false; }
    },
    instantiateStreaming: function (source, imports) {
      // 不支持 streaming，回退到 instantiate
      if (source && typeof source.then === 'function') {
        return source.then(function (resp) {
          return resp.arrayBuffer();
        }).then(function (buf) {
          return WAPolyfill.instantiate(buf, imports);
        });
      }
      return Promise.reject(new Error('instantiateStreaming not supported'));
    },
    // H6 修复: 保留原始 WebAssembly.Memory/Table/Global 构造器。
    // Emscripten 运行时会用 new WebAssembly.Memory(...) 创建线性内存，
    // 空函数会产生无 buffer 的空对象导致崩溃。
    // 优先用原生 V8 构造器，其次 WXWebAssembly 的，最后才回退空函数。
    Memory: (_nativeWA && _nativeWA.Memory) ? _nativeWA.Memory : (_WXWA && _WXWA.Memory) ? _WXWA.Memory : function () {},
    Table: (_nativeWA && _nativeWA.Table) ? _nativeWA.Table : (_WXWA && _WXWA.Table) ? _WXWA.Table : function () {},
    Global: (_nativeWA && _nativeWA.Global) ? _nativeWA.Global : (_WXWA && _WXWA.Global) ? _WXWA.Global : function () {},
    RuntimeError: (_nativeWA && _nativeWA.RuntimeError) ? _nativeWA.RuntimeError : Error,
    CompileError: (_nativeWA && _nativeWA.CompileError) ? _nativeWA.CompileError : Error,
    LinkError: (_nativeWA && _nativeWA.LinkError) ? _nativeWA.LinkError : Error,
  };
  safeDefineGlobal('WebAssembly', WAPolyfill);
  console.log('[WeChat Adapter] WebAssembly polyfill installed (path + buffer support)');

  // ============================================================
  // 模块 0b: _resolveWasmPath - 解析 WASM 文件路径
  // F4 修复（分包方案）: 优先分包 -> 主包 -> USER_DATA_PATH 缓存 -> CDN 下载
  //   分包路径（如 "wasm_pkg/Game2048.wasm.br"）是裸相对路径，无 wxfile:/http: 前缀，
  //   是 WXWebAssembly.instantiate 唯一接受的格式（devtool/真机均如此）。
  //   USER_DATA_PATH 缓存路径（http://usr/... 或 wxfile://...）仅真机可用，devtool 拒绝。
  // ============================================================

  // 加载分包（如果已加载则立即 resolve）。返回 Promise。
  function _ensureSubpkgLoaded(subpkgName, probeFile, timeoutMs) {
    if (!subpkgName) return Promise.resolve();
    timeoutMs = timeoutMs || 30000;

    // 1. 先同步探测：若分包文件已可访问，跳过 loadSubpackage（规避 DevTools __name__ bug）
    if (probeFile) {
      try {
        fileSystemManager.accessSync(probeFile);
        console.log('[WeChat] Subpackage already accessible (skip loadSubpackage): ' + subpkgName + ' -> ' + probeFile);
        return Promise.resolve();
      } catch (e) {
        // 文件不可访问，继续走 loadSubpackage
      }
    }

    // 2. 调 wx.loadSubpackage，带超时和同步异常防御
    return new Promise(function (resolve, reject) {
      if (typeof wx !== 'undefined' && typeof wx.loadSubpackage === 'function') {
        var settled = false;
        var timer = setTimeout(function () {
          if (settled) return;
          settled = true;
          console.warn('[WeChat] Subpackage load timeout: ' + subpkgName + ', assuming ready');
          resolve(); // 超时也 resolve，让后续 accessSync 再判断
        }, timeoutMs);

        try {
          wx.loadSubpackage({
            name: subpkgName,
            success: function (res) {
              if (settled) return;
              settled = true;
              clearTimeout(timer);
              console.log('[WeChat] Subpackage loaded: ' + subpkgName + ', res=' + JSON.stringify(res || {}));
              resolve(res);
            },
            fail: function (err) {
              if (settled) return;
              settled = true;
              clearTimeout(timer);
              console.warn('[WeChat] Subpackage load failed: ' + subpkgName + ': ' + (err.errMsg || 'unknown') + ', assuming ready');
              resolve(); // 失败也 resolve：分包可能已预加载，后续 accessSync 会再检查
            },
          });
        } catch (syncErr) {
          // DevTools __name__ bug 可能导致 loadSubpackage 同步抛出
          if (!settled) {
            settled = true;
            clearTimeout(timer);
            console.warn('[WeChat] wx.loadSubpackage threw synchronously (likely DevTools __name__ bug): ' + syncErr.message + ', assuming ready');
            resolve();
          }
        }
      } else {
        // 不支持 loadSubpackage（老版本），假设已可用
        resolve();
      }
    });
  }

  safeDefineGlobal('_resolveWasmPath', function () {
    var wasmFileName = globalThis._wasmFileName || 'index.wasm';
    var cdnBaseUrl = globalThis._cdnBaseUrl || '';
    var wasmSubpkg = globalThis._wasmSubpkg || '';
    var brInSubpkg = globalThis._wasmBrInSubpkg === true;
    var brFileName = wasmFileName + '.br';

    // 候选文件列表：优先 .wasm.br，回退 .wasm
    var candidates = [brFileName, wasmFileName];

    // 1. 分包内 .wasm.br（F4 主路径 - 唯一在 devtool 和真机都可用的方案）
    if (wasmSubpkg && brInSubpkg) {
      var subpkgBrPath = wasmSubpkg + '/' + brFileName;
      return _ensureSubpkgLoaded(wasmSubpkg, subpkgBrPath).then(function () {
        try {
          fileSystemManager.accessSync(subpkgBrPath);
          console.log('[WeChat] WASM in subpackage: ' + subpkgBrPath);
          return subpkgBrPath;
        } catch (e) {
          console.warn('[WeChat] Subpackage accessible but file not found: ' + subpkgBrPath + ' (' + e.message + ')');
          throw e;
        }
      }).catch(function (loadErr) {
        // 分包加载失败，回退到主包/CDN 路径
        console.warn('[WeChat] Subpackage fallback to main/CDN: ' + loadErr.message);
        return null;
      }).then(function (path) {
        if (path) return path;
        // 继续后续候选
        return _resolveWasmFallback(candidates, cdnBaseUrl);
      });
    }

    return _resolveWasmFallback(candidates, cdnBaseUrl);
  });

  // 回退路径：主包 -> USER_DATA_PATH 缓存 -> CDN 下载
  function _resolveWasmFallback(candidates, cdnBaseUrl) {
    var wasmFileName = globalThis._wasmFileName || 'index.wasm';
    var brFileName = wasmFileName + '.br';

    // 2. 尝试主包内文件（先 .wasm.br 再 .wasm）
    for (var i = 0; i < candidates.length; i++) {
      var fname = candidates[i];
      try {
        fileSystemManager.accessSync(fname);
        console.log('[WeChat] WASM in main package: ' + fname);
        return Promise.resolve(fname);
      } catch (e) {}
    }

    // 3. 检查 USER_DATA_PATH 缓存（仅真机可用，devtool 路径为 http://usr/... 被拒绝）
    if (USER_DATA_PATH) {
      for (var i = 0; i < candidates.length; i++) {
        var fname = candidates[i];
        var cachedPath = USER_DATA_PATH + '/' + fname;
        try {
          var stat = fileSystemManager.statSync(cachedPath);
          if (stat.size > 0) {
            console.log('[WeChat] Using cached WASM: ' + cachedPath + ' (' + (stat.size / 1048576).toFixed(2) + ' MB)');
            return Promise.resolve(cachedPath);
          }
        } catch (e) {}
      }
    }

    // 4. 从 CDN 下载（优先 .wasm.br）
    if (!cdnBaseUrl) {
      return Promise.reject(new Error('No CDN URL for WASM download'));
    }

    function downloadOne(fileName) {
      var cdnUrl = cdnBaseUrl + '/' + fileName;
      console.log('[WeChat] Downloading: ' + cdnUrl);
      return new Promise(function (resolve, reject) {
        wx.downloadFile({
          url: cdnUrl,
          success: function (res) {
            if (res.statusCode !== 200 || !res.tempFilePath) {
              reject(new Error('Download ' + fileName + ' HTTP ' + res.statusCode));
              return;
            }
            var tempFilePath = res.tempFilePath;
            console.log('[WeChat] Downloaded: ' + fileName + ' -> ' + tempFilePath);

            if (!USER_DATA_PATH) {
              reject(new Error('USER_DATA_PATH unavailable'));
              return;
            }
            var savedPath = USER_DATA_PATH + '/' + fileName;
            try { fileSystemManager.unlinkSync(savedPath); } catch (e) {}

            var saved = false;
            try {
              fileSystemManager.saveFileSync(tempFilePath, savedPath);
              saved = true;
            } catch (saveErr) {
              console.warn('[WeChat] saveFileSync failed: ' + saveErr.message + ', trying manual copy');
            }
            if (!saved) {
              try {
                var fileData = fileSystemManager.readFileSync(tempFilePath);
                fileSystemManager.writeFileSync(savedPath, fileData, 'binary');
                saved = true;
              } catch (copyErr) {
                console.error('[WeChat] Manual copy failed: ' + copyErr.message);
              }
            }
            if (!saved) {
              reject(new Error('Failed to save ' + fileName));
              return;
            }
            try {
              var st = fileSystemManager.statSync(savedPath);
              if (!st || st.size <= 0) {
                reject(new Error('Saved ' + fileName + ' is empty'));
                return;
              }
              console.log('[WeChat] Saved: ' + savedPath + ' (' + (st.size / 1048576).toFixed(2) + ' MB)');
            } catch (e) {
              console.warn('[WeChat] statSync failed: ' + e.message);
            }
            resolve(savedPath);
          },
          fail: function (err) {
            reject(new Error('Download ' + fileName + ' failed: ' + (err.errMsg || 'unknown')));
          },
        });
      });
    }

    // 依次尝试 .wasm.br 和 .wasm
    return downloadOne(brFileName).catch(function (brErr) {
      console.warn('[WeChat] .wasm.br failed: ' + brErr.message + ', trying .wasm');
      return downloadOne(wasmFileName);
    });
  }

  // ============================================================
  // 模块 0c: _resolveWasmBuffer - 解析 WASM 路径并读取为 ArrayBuffer
  // 用于原生 WebAssembly.instantiate(buffer, imports) 实例化
  // ============================================================
  safeDefineGlobal('_resolveWasmBuffer', function () {
    var wasmFileName = globalThis._wasmFileName || 'index.wasm';
    var cdnBaseUrl = globalThis._cdnBaseUrl || '';

    // F2 修复: 已禁用自研 zlib inflate（死循环）。
    // .wasm.br 由 WXWebAssembly 原生解压（不需要 JS 层解压）。
    // 此函数仅下载普通 .wasm 文件并读为 ArrayBuffer。
    return _resolveWasmPath().then(function (wasmPath) {
      console.log('[WeChat] Reading WASM as ArrayBuffer: ' + wasmPath);
      return new Promise(function (resolve, reject) {
        try {
          var buffer = fileSystemManager.readFileSync(wasmPath);
          if (buffer instanceof ArrayBuffer) {
            console.log('[WeChat] WASM ArrayBuffer: ' + (buffer.byteLength / 1048576).toFixed(2) + ' MB');
            resolve(buffer);
          } else if (buffer && buffer.buffer instanceof ArrayBuffer) {
            console.log('[WeChat] WASM ArrayBuffer (from view): ' + (buffer.buffer.byteLength / 1048576).toFixed(2) + ' MB');
            resolve(buffer.buffer);
          } else {
            reject(new Error('readFileSync returned unexpected type: ' + typeof buffer));
          }
        } catch (e) {
          reject(new Error('readFileSync failed: ' + e.message));
        }
      });
    });
  });

  // ============================================================
  // 模块 0d: _instantiateWasmSmart - 智能实例化 WASM
  // F4 修复（重写）: 根据 .wasm vs .wasm.br 选择正确实例化路径
  //   - .wasm.br (Brotli 压缩): 必须用 WXWebAssembly.instantiate(path) 自动解压
  //     （不能用 native WebAssembly.instantiate(buffer)，因为 buffer 是压缩字节）
  //   - .wasm (未压缩): 优先 native WebAssembly.instantiate(buffer)，
  //     回退 WXWebAssembly.instantiate(path)
  // 返回 Promise<{instance, module}>
  // ============================================================
  safeDefineGlobal('_instantiateWasmSmart', function (imports) {
    console.log('[WeChat] _instantiateWasmSmart called, imports keys: ' + (imports ? Object.keys(imports).join(',') : 'none'));

    return _resolveWasmPath().then(function (wasmPath) {
      var isBr = wasmPath && wasmPath.length > 3 && wasmPath.substring(wasmPath.length - 3) === '.br';
      console.log('[WeChat] Resolved WASM path: ' + wasmPath + ' (isBr=' + isBr + ')');

      if (isBr) {
        // .wasm.br: 只能用 WXWebAssembly.instantiate(path) 自动解压
        if (!_WXWA || typeof _WXWA.instantiate !== 'function') {
          return Promise.reject(new Error('.wasm.br requires WXWebAssembly.instantiate(path) for auto-decompression, but WXWebAssembly is unavailable'));
        }
        console.log('[WeChat] Strategy A: WXWebAssembly.instantiate(.wasm.br path) - auto-decompress');
        return _WXWA.instantiate(wasmPath, imports).then(function (result) {
          console.log('[WeChat] WXWebAssembly.instantiate(.wasm.br) succeeded');
          if (result && result.instance && result.module) return result;
          return { instance: result.instance || result, module: result.module || null };
        });
      }

      // .wasm (未压缩): 优先 native WebAssembly + ArrayBuffer
      if (_nativeWA && typeof _nativeWA.instantiate === 'function') {
        console.log('[WeChat] Strategy B1: native WebAssembly.instantiate(ArrayBuffer)');
        return new Promise(function (resolve, reject) {
          try {
            var buffer = fileSystemManager.readFileSync(wasmPath);
            if (buffer && buffer.buffer instanceof ArrayBuffer) buffer = buffer.buffer;
            if (!(buffer instanceof ArrayBuffer)) {
              reject(new Error('readFileSync returned non-ArrayBuffer: ' + typeof buffer));
              return;
            }
            console.log('[WeChat] WASM ArrayBuffer: ' + (buffer.byteLength / 1048576).toFixed(2) + ' MB');
            _nativeWA.instantiate(buffer, imports).then(resolve, reject);
          } catch (e) {
            reject(new Error('Read WASM file failed: ' + e.message));
          }
        }).then(function (result) {
          console.log('[WeChat] Native WebAssembly.instantiate succeeded');
          return result;
        }, function (err) {
          console.warn('[WeChat] Native WA failed: ' + (err && err.message ? err.message : err) + ', fallback to WXWebAssembly');
          // 回退到 WXWebAssembly.instantiate(path)
          if (!_WXWA) return Promise.reject(err);
          return _WXWA.instantiate(wasmPath, imports).then(function (result) {
            console.log('[WeChat] WXWebAssembly.instantiate(.wasm) succeeded (fallback)');
            if (result && result.instance && result.module) return result;
            return { instance: result.instance || result, module: result.module || null };
          });
        });
      }

      // 既无 native WA，又是 .wasm，直接用 WXWebAssembly.instantiate(path)
      if (_WXWA && typeof _WXWA.instantiate === 'function') {
        console.log('[WeChat] Strategy B2: WXWebAssembly.instantiate(.wasm path)');
        return _WXWA.instantiate(wasmPath, imports).then(function (result) {
          console.log('[WeChat] WXWebAssembly.instantiate(.wasm) succeeded');
          if (result && result.instance && result.module) return result;
          return { instance: result.instance || result, module: result.module || null };
        });
      }

      return Promise.reject(new Error('No WebAssembly implementation available'));
    });
  });

  function _tryBufferToFileToWXWA(imports) {
    return _resolveWasmBuffer().then(function (buffer) {
      console.log('[WeChat] Writing WASM to temp file (' + (buffer.byteLength / 1048576).toFixed(2) + ' MB)...');
      var tempPath;
      try {
        tempPath = _writeBufferToTempWasm(buffer);
      } catch (e) {
        return Promise.reject(new Error('Write temp wasm failed: ' + e.message));
      }
      console.log('[WeChat] WXWebAssembly.instantiate(temp file): ' + tempPath);
      if (!_WXWA) {
        try { fileSystemManager.unlinkSync(tempPath); } catch (e) {}
        return Promise.reject(new Error('WXWebAssembly not available'));
      }
      return _WXWA.instantiate(tempPath, imports).then(function (result) {
        try { fileSystemManager.unlinkSync(tempPath); } catch (e) {}
        console.log('[WeChat] WXWebAssembly.instantiate(temp file) succeeded');
        if (result && result.instance && result.module) {
          return result;
        }
        return { instance: result.instance || result, module: result.module || null };
      }, function (err) {
        try { fileSystemManager.unlinkSync(tempPath); } catch (e) {}
        console.warn('[WeChat] WXWebAssembly.instantiate(temp file) failed: ' + (err && err.message ? err.message : err));
        // 回退到策略 3
        return _tryWXInstantiate(imports);
      });
    });
  }

  function _tryWXInstantiate(imports) {
    return _resolveWasmPath().then(function (wasmPath) {
      console.log('[WeChat] Strategy 3 (fallback): WXWebAssembly.instantiate: ' + wasmPath);
      if (!_WXWA) {
        return Promise.reject(new Error('WXWebAssembly not available'));
      }
      return _WXWA.instantiate(wasmPath, imports);
    }).then(function (result) {
      console.log('[WeChat] WXWebAssembly.instantiate(path) succeeded');
      if (result && result.instance && result.module) {
        return result;
      }
      return { instance: result.instance || result, module: result.module || null };
    });
  }

  // ============================================================
  // 模块 1: Canvas (HTMLCanvasElement) polyfill
  // ============================================================
  var _mainCanvas = null;

  function getMainCanvas() {
    if (!_mainCanvas) {
      _mainCanvas = wx.createCanvas();
      // 引擎用 `#${canvas.id}` 作为选择器找 canvas（_godot_js_config_canvas_id_get），
      // 不设置 id 会变成 #undefined → querySelector 返回 null → WebGL2 误判为不支持
      _mainCanvas.id = 'canvas';
      // 尽早探测并锁定 webgl2：微信 canvas 上下文类型粘滞（一旦被 getContext('2d'/'webgl')
      // 拿走，webgl2 永远返回 null）。在引擎探测前先把主 canvas 锁到 webgl2。
      try {
        var _g2 = _mainCanvas.getContext('webgl2');
        console.log('[WeChat Diag] main canvas webgl2 probe: ' + (_g2 ? 'OK (locked)' : 'null'));
      } catch (e) {
        console.warn('[WeChat Diag] main canvas webgl2 probe THREW: ' + e);
      }
      _mainCanvas.style = _mainCanvas.style || {};
      var sysInfo = wx.getSystemInfoSync();
      var dpr = sysInfo.pixelRatio || 1;
      var winW = sysInfo.windowWidth || 375;
      var winH = sysInfo.windowHeight || 667;
      _mainCanvas.width = Math.floor(winW * dpr);
      _mainCanvas.height = Math.floor(winH * dpr);
      _mainCanvas.style.width = winW + 'px';
      _mainCanvas.style.height = winH + 'px';
      _mainCanvas.style.display = 'block';
      console.log('[WeChat Adapter] canvas init: ' + _mainCanvas.width + 'x' + _mainCanvas.height + ' (dpr=' + dpr + ', win=' + winW + 'x' + winH + ')');
      _mainCanvas.tabIndex = 0;
      _mainCanvas.getBoundingClientRect = function () {
        return {
          left: 0, top: 0,
          width: _mainCanvas.width, height: _mainCanvas.height,
          right: _mainCanvas.width, bottom: _mainCanvas.height,
        };
      };
      _mainCanvas.addEventListener = function (type, listener) {
        _mainCanvas._listeners = _mainCanvas._listeners || {};
        _mainCanvas._listeners[type] = _mainCanvas._listeners[type] || [];
        _mainCanvas._listeners[type].push(listener);
      };
      _mainCanvas.removeEventListener = function (type, listener) {
        if (!_mainCanvas._listeners || !_mainCanvas._listeners[type]) return;
        var arr = _mainCanvas._listeners[type];
        var idx = arr.indexOf(listener);
        if (idx >= 0) arr.splice(idx, 1);
      };
      _mainCanvas.dispatchEvent = function (event) {
        if (!_mainCanvas._listeners) return;
        var listeners = _mainCanvas._listeners[event.type] || [];
        event.target = _mainCanvas;
        event.currentTarget = _mainCanvas;
        for (var i = 0; i < listeners.length; i++) {
          try { listeners[i].call(_mainCanvas, event); } catch (e) { console.error(e); }
        }
      };
      _mainCanvas.focus = function () {};
      // 诊断：记录每次 getContext 的类型与成败（微信 canvas 对 webgl2 的支持情况不明，
      // 让事实说话——返回 null 还是方法缺失/抛错）
      if (_mainCanvas.getContext) {
        var _origGetContext = _mainCanvas.getContext;
        _mainCanvas.getContext = function (type, attrs) {
          var ctx = null;
          try {
            ctx = _origGetContext.call(this, type, attrs);
          } catch (e) {
            console.warn('[WeChat Adapter] getContext(' + type + ') THREW: ' + e);
            throw e;
          }
          console.log('[WeChat Adapter] getContext(' + type + ') -> ' + (ctx ? 'OK' : 'null'));
          return ctx;
        };
      } else {
        console.warn('[WeChat Adapter] main canvas has NO getContext method!');
      }
    }
    return _mainCanvas;
  }

  function HTMLCanvasElement() {}
  var _origCreateCanvas = wx.createCanvas;
  wx.createCanvas = function () {
    var canvas = _origCreateCanvas.apply(wx, arguments);
    if (!(canvas instanceof HTMLCanvasElement)) {
      // 保留原原型链：getContext 等原生方法可能挂在 wx canvas 的原型上，
      // 直接 setPrototypeOf 替换会让这些方法丢失（表现为 getContext undefined →
      // WebGL2 探测失败/GL 上下文创建失败）。链接为
      // canvas → HTMLCanvasElement.prototype → wx 原生 canvas 原型。
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
  // 暴露给 game.js：引擎配置 canvas 必须用这个带完整增强（style/事件/dispatchEvent）
  // 的主 canvas，而不是 wx.createCanvas() 返回的裸对象（无 .style，会在
  // _godot_js_display_setup_canvas 里报 "Cannot set property 'position' of undefined"）
  safeDefineGlobal('__godotGetMainCanvas', getMainCanvas);

  // ============================================================
  // 模块 2: Image polyfill
  // ============================================================
  function HTMLImageElement() {
    return wx.createImage();
  }
  safeDefineGlobal('Image', HTMLImageElement);
  safeDefineGlobal('HTMLImageElement', HTMLImageElement);

  // ============================================================
  // 模块 3: AudioContext polyfill
  // ============================================================
  function AudioContext() {
    this._ctx = wx.createInnerAudioContext();
  }
  AudioContext.prototype.createBufferSource = function () {
    return { buffer: null, loop: false, start: function () {}, stop: function () {}, connect: function () {}, disconnect: function () {} };
  };
  AudioContext.prototype.createGain = function () {
    return { gain: { value: 1 }, connect: function () {}, disconnect: function () {} };
  };
  AudioContext.prototype.createScriptProcessor = function () {
    return { connect: function () {}, disconnect: function () {}, onaudioprocess: null };
  };
  AudioContext.prototype.decodeAudioData = function (arrayBuffer, success, error) {
    if (success) success({});
  };
  AudioContext.prototype.close = function () {};
  AudioContext.prototype.resume = function () {};
  AudioContext.prototype.suspend = function () {};
  AudioContext.prototype.destination = {};
  AudioContext.prototype.sampleRate = 44100;
  safeDefineGlobal('AudioContext', AudioContext);
  safeDefineGlobal('webkitAudioContext', AudioContext);

  // ============================================================
  // 模块 4: FileSystem helpers (fileSystemManager 和 USER_DATA_PATH 已在文件顶部前置定义)
  // ============================================================

  // ============================================================
  // 模块 5: TouchEvents polyfill + swipe-to-keyboard
  // ============================================================
  var _touchStartX = 0, _touchStartY = 0, _touchActive = false;
  var SWIPE_THRESHOLD = 30;

  function dispatchKeyEvent(type, keyName, keyCode) {
    var canvas = getMainCanvas();
    try {
      var event;
      if (typeof KeyboardEvent === 'function') {
        event = new KeyboardEvent(type, {
          key: keyName,
          code: keyName,
          keyCode: keyCode,
          which: keyCode,
          bubbles: true,
          cancelable: true,
        });
      } else {
        event = { type: type, key: keyName, code: keyName, keyCode: keyCode, which: keyCode, bubbles: true, cancelable: true };
      }
      canvas.dispatchEvent(event);
      if (globalThis.window && globalThis.window !== globalThis) {
        try { globalThis.window.dispatchEvent(event); } catch (e) {}
      }
    } catch (e) {
      console.warn('[WeChat Adapter] dispatchKeyEvent failed: ' + e);
    }
  }

  function dispatchTouch(type, touches) {
    var canvas = getMainCanvas();
    var event = {
      type: type,
      target: canvas,
      currentTarget: canvas,
      touches: touches.map(function (t) {
        return { clientX: t.clientX, clientY: t.clientY, identifier: t.identifier, pageX: t.pageX, pageY: t.pageY, screenX: t.screenX, screenY: t.screenY };
      }),
      changedTouches: touches.map(function (t) {
        return { clientX: t.clientX, clientY: t.clientY, identifier: t.identifier, pageX: t.pageX, pageY: t.pageY, screenX: t.screenX, screenY: t.screenY };
      }),
      preventDefault: function () {},
      stopPropagation: function () {},
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
        if (absDx < SWIPE_THRESHOLD && absDy < SWIPE_THRESHOLD) {
          if (gameOverDetected && gameOverDetected()) {
            dispatchKeyEvent('keydown', 'Enter', 13);
            setTimeout(function () { dispatchKeyEvent('keyup', 'Enter', 13); }, 50);
          }
          return;
        }
        var keyName, keyCode;
        if (absDx > absDy) {
          keyName = dx > 0 ? 'ArrowRight' : 'ArrowLeft';
          keyCode = dx > 0 ? 39 : 37;
        } else {
          keyName = dy > 0 ? 'ArrowDown' : 'ArrowUp';
          keyCode = dy > 0 ? 40 : 38;
        }
        console.log('[WeChat Swipe] dx=' + dx + ' dy=' + dy + ' -> ' + keyName);
        dispatchKeyEvent('keydown', keyName, keyCode);
        setTimeout(function () { dispatchKeyEvent('keyup', keyName, keyCode); }, 50);
      }
    }
  }

  function gameOverDetected() { return true; }

  if (wx.onTouchStart) {
    wx.onTouchStart(function (e) { dispatchTouch('touchstart', e.touches); });
    wx.onTouchMove(function (e) { dispatchTouch('touchmove', e.touches); });
    wx.onTouchEnd(function (e) { dispatchTouch('touchend', e.changedTouches); });
    wx.onTouchCancel(function (e) { dispatchTouch('touchcancel', e.changedTouches); });
  }

  // ============================================================
  // 模块 5b: atob/btoa polyfill（F1 修复）
  // 微信小游戏运行时无全局 atob，pck_data.js 解码 base64 必需
  // 优先使用 wx.base64ToArrayBuffer（性能好），否则纯 JS 实现
  // ============================================================
  var B64_CHARS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
  var B64_LOOKUP = (function () {
    var t = {};
    for (var i = 0; i < B64_CHARS.length; i++) t[B64_CHARS.charAt(i)] = i;
    return t;
  })();

  function atobPolyfill(b64) {
    // 优先使用微信原生 API（性能更好，返回 ArrayBuffer）
    if (typeof wx !== 'undefined' && wx.base64ToArrayBuffer) {
      var ab = wx.base64ToArrayBuffer(b64);
      // 标准 atob 返回 binary string，这里同步行为
      var bytes = new Uint8Array(ab);
      var s = '';
      for (var i = 0; i < bytes.length; i++) s += String.fromCharCode(bytes[i]);
      return s;
    }
    // 纯 JS 回退实现
    b64 = String(b64).replace(/=+$/, '');
    var output = '';
    var bs = 0, buff = 0;
    for (var i = 0; i < b64.length; i++) {
      var c = B64_LOOKUP[b64.charAt(i)];
      if (c === undefined) continue;
      buff = (buff << 6) | c;
      bs += 6;
      if (bs >= 8) {
        bs -= 8;
        output += String.fromCharCode((buff >> bs) & 0xFF);
      }
    }
    return output;
  }

  function btoaPolyfill(s) {
    if (typeof wx !== 'undefined' && wx.arrayBufferToBase64) {
      var bytes = new Uint8Array(s.length);
      for (var i = 0; i < s.length; i++) bytes[i] = s.charCodeAt(i) & 0xFF;
      return wx.arrayBufferToBase64(bytes.buffer);
    }
    var output = '';
    var bs = 0, buff = 0;
    for (var i = 0; i < s.length; i++) {
      buff = (buff << 8) | (s.charCodeAt(i) & 0xFF);
      bs += 8;
      while (bs >= 6) {
        bs -= 6;
        output += B64_CHARS.charAt((buff >> bs) & 0x3F);
      }
    }
    if (bs > 0) {
      output += B64_CHARS.charAt((buff << (6 - bs)) & 0x3F);
    }
    while (output.length % 4) output += '=';
    return output;
  }

  safeDefineGlobal('atob', atobPolyfill);
  safeDefineGlobal('btoa', btoaPolyfill);
  console.log('[WeChat Adapter] atob/btoa polyfill installed');

  // ============================================================
  // 模块 6: localStorage polyfill
  // ============================================================
  safeDefineGlobal('localStorage', {
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
  });

  // ============================================================
  // 模块 7: URL polyfill
  // ============================================================
  if (typeof URL === 'undefined') {
    function URLShim(url, base) {
      this.href = url;
      this.pathname = url;
      this.origin = '';
    }
    URLShim.createObjectURL = function () { return ''; };
    URLShim.revokeObjectURL = function () {};
    safeDefineGlobal('URL', URLShim);
  } else if (!URL.createObjectURL) {
    URL.createObjectURL = function () { return ''; };
    URL.revokeObjectURL = function () {};
  }

  // ============================================================
  // 模块 8: ReadableStream polyfill
  // Godot 4.7 的 Preloader.getTrackedResponse 使用 new ReadableStream({start, controller})
  // ============================================================
  if (typeof ReadableStream === 'undefined') {
    function ReadableStream(underlyingSource) {
      this._underlyingSource = underlyingSource || {};
      this._started = false;
      this._chunks = [];
      this._waitResolver = null;
      this._closed = false;
      this._controller = {
        enqueue: function (chunk) {
          if (this._waitResolver) {
            var resolve = this._waitResolver;
            this._waitResolver = null;
            resolve({ done: false, value: chunk });
          } else {
            this._chunks.push(chunk);
          }
        }.bind(this),
        close: function () {
          this._closed = true;
          if (this._waitResolver) {
            var resolve = this._waitResolver;
            this._waitResolver = null;
            resolve({ done: true, value: undefined });
          }
        }.bind(this),
        error: function (err) {
          this._closed = true;
          this._error = err;
          if (this._waitResolver) {
            var resolve = this._waitResolver;
            this._waitResolver = null;
            resolve(Promise.reject(err));
          }
        }.bind(this),
      };
    }

    ReadableStream.prototype.getReader = function () {
      var stream = this;
      if (!stream._started && stream._underlyingSource.start) {
        stream._started = true;
        try {
          stream._underlyingSource.start(stream._controller);
        } catch (e) {
          console.error('[ReadableStream] start error:', e);
        }
      }
      return {
        read: function () {
          if (stream._chunks.length > 0) {
            return Promise.resolve({ done: false, value: stream._chunks.shift() });
          }
          if (stream._closed) {
            return Promise.resolve({ done: true, value: undefined });
          }
          if (stream._error) {
            return Promise.reject(stream._error);
          }
          return new Promise(function (resolve) {
            stream._waitResolver = resolve;
          });
        },
        cancel: function () {
          stream._closed = true;
          stream._chunks = [];
          if (stream._waitResolver) {
            var resolve = stream._waitResolver;
            stream._waitResolver = null;
            resolve({ done: true, value: undefined });
          }
          return Promise.resolve();
        },
        releaseLock: function () {},
      };
    };

    safeDefineGlobal('ReadableStream', ReadableStream);
  }

  // ============================================================
  // 模块 9: Headers polyfill
  // ============================================================
  if (typeof Headers === 'undefined') {
    function Headers(init) {
      this._headers = {};
      if (init) {
        if (Array.isArray(init)) {
          for (var i = 0; i < init.length; i++) {
            this._headers[init[i][0].toLowerCase()] = init[i][1];
          }
        } else if (typeof init === 'object') {
          for (var key in init) {
            if (init.hasOwnProperty(key)) {
              this._headers[key.toLowerCase()] = init[key];
            }
          }
        }
      }
    }
    Headers.prototype.get = function (name) {
      return this._headers[name.toLowerCase()] || null;
    };
    Headers.prototype.set = function (name, value) {
      this._headers[name.toLowerCase()] = String(value);
    };
    Headers.prototype.has = function (name) {
      return name.toLowerCase() in this._headers;
    };
    Headers.prototype.forEach = function (callback) {
      for (var key in this._headers) {
        if (this._headers.hasOwnProperty(key)) {
          callback(this._headers[key], key, this);
        }
      }
    };
    safeDefineGlobal('Headers', Headers);
  }

  // ============================================================
  // 模块 10: Response polyfill
  // 支持两种构造方式:
  //   1. createResponse(buffer, options) - 从 ArrayBuffer 创建（用于 fetch 返回）
  //   2. new Response(readableStream, {headers}) - 从 ReadableStream 创建（getTrackedResponse 使用）
  // ============================================================
  function createResponseBodyFromBuffer(uint8) {
    var _readIndex = 0;
    return {
      getReader: function () {
        return {
          read: function () {
            if (_readIndex >= uint8.length) {
              return Promise.resolve({ done: true, value: undefined });
            }
            var chunkSize = Math.min(65536, uint8.length - _readIndex);
            var chunk = uint8.slice(_readIndex, _readIndex + chunkSize);
            _readIndex += chunkSize;
            return Promise.resolve({ done: false, value: chunk });
          },
          cancel: function () { _readIndex = uint8.length; return Promise.resolve(); },
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

    if (body instanceof ReadableStream) {
      this.body = body;
      this._buffer = null;
    } else if (body instanceof ArrayBuffer || (body && body.buffer instanceof ArrayBuffer)) {
      var ab = body instanceof ArrayBuffer ? body : body.buffer;
      this._buffer = ab;
      this.body = createResponseBodyFromBuffer(new Uint8Array(ab));
    } else if (typeof body === 'string') {
      var arr = new Uint8Array(body.length);
      for (var i = 0; i < body.length; i++) {
        arr[i] = body.charCodeAt(i) & 0xFF;
      }
      this._buffer = arr.buffer;
      this.body = createResponseBodyFromBuffer(arr);
    } else if (body == null) {
      this._buffer = new ArrayBuffer(0);
      this.body = createResponseBodyFromBuffer(new Uint8Array(0));
    } else {
      this._buffer = body;
      this.body = createResponseBodyFromBuffer(new Uint8Array(body));
    }
  }

  Response.prototype.arrayBuffer = function () {
    if (this._buffer) {
      return Promise.resolve(this._buffer.slice ? this._buffer.slice(0) : this._buffer);
    }
    // 从 ReadableStream 读取
    var reader = this.body.getReader();
    var chunks = [];
    var totalLength = 0;
    function read() {
      return reader.read().then(function (result) {
        if (result.done) {
          var combined = new Uint8Array(totalLength);
          var offset = 0;
          for (var i = 0; i < chunks.length; i++) {
            combined.set(chunks[i], offset);
            offset += chunks[i].length;
          }
          return combined.buffer;
        }
        chunks.push(result.value);
        totalLength += result.value.length;
        return read();
      });
    }
    return read();
  };

  Response.prototype.text = function () {
    return this.arrayBuffer().then(function (buf) {
      var arr = new Uint8Array(buf);
      var text = '';
      for (var i = 0; i < arr.length; i++) {
        text += String.fromCharCode(arr[i]);
      }
      try { return decodeURIComponent(escape(text)); } catch (e) { return text; }
    });
  };

  Response.prototype.json = function () {
    return this.text().then(function (t) { return JSON.parse(t); });
  };

  Response.prototype.blob = function () {
    return this.arrayBuffer();
  };

  Response.prototype.clone = function () {
    // Clone body: create new ReadableStream that reads from same source
    // For simplicity, read all data first then create new response
    var self = this;
    if (self._buffer) {
      return new Response(self._buffer.slice ? self._buffer.slice(0) : self._buffer, {
        status: self.status,
        statusText: self.statusText,
        headers: self.headers,
      });
    }
    // If body is a ReadableStream (not yet read), create a tee-like clone
    // Simple approach: read all, then create two responses
    // But this is called before arrayBuffer() in flow, so we need a different approach.
    // Actually, Godot uses: const cloned = new Response(response.clone().body, {headers});
    // So clone() returns a Response whose .body is a ReadableStream.
    // We return a new Response with same stream (but note: stream can only be read once).
    // For the Godot use case: response is from loadFetch (getTrackedResponse), which is
    // a ReadableStream. Then response.clone().body is passed to new Response(), and
    // the cloned Response's body is used in instantiateWasm via r.arrayBuffer().
    // Since both getTrackedResponse's onloadprogress and instantiateWasm need to read,
    // we need to tee the stream. But for simplicity in WeChat, let's buffer the whole thing.
    var buffered = self.arrayBuffer().then(function (buf) { return buf; });
    var resp = new Response(new ReadableStream({
      start: function (controller) {
        buffered.then(function (buf) {
          var arr = new Uint8Array(buf);
          var offset = 0;
          function pushChunk() {
            if (offset >= arr.length) {
              controller.close();
              return;
            }
            var chunkSize = Math.min(65536, arr.length - offset);
            controller.enqueue(arr.slice(offset, offset + chunkSize));
            offset += chunkSize;
            setTimeout(pushChunk, 0);
          }
          pushChunk();
        });
      }
    }), {
      status: self.status,
      statusText: self.statusText,
      headers: self.headers,
    });
    // Store buffer for parent's later use
    self.arrayBuffer = function () { return buffered; };
    resp._bufferPromise = buffered;
    resp.arrayBuffer = function () { return buffered; };
    return resp;
  };

  safeDefineGlobal('Response', Response);

  // ============================================================
  // 模块 11: fetch polyfill
  // 支持:
  //   1. 主包内文件 (如 index.pck) → wx.getFileSystemManager().readFileSync
  //   2. CDN URL (http:// 或 https://) → wx.downloadFile 下载（支持大文件）
  //   3. 本地文件路径 (wx.env.USER_DATA_PATH) → fs.readFileSync
  //   4. 本地找不到 → CDN 回退（如果配置了 _cdnBase）
  // ============================================================
  var _cdnBase = '';

  safeDefineGlobal('_setCdnBase', function (url) {
    _cdnBase = url.replace(/\/$/, '');
    console.log('[WeChat Adapter] CDN base set to: ' + _cdnBase);
  });

  function readMainPackageFile(path) {
    if (!fileSystemManager) throw new Error('File system not available');
    // 微信小游戏中包内文件读取：
    // - 必须用相对路径（从包根目录开始），不能用 '/' 开头（会 permission denied）
    // - 去掉前导 './' 和 '/'
    var filename = path.replace(/^\.\//, '').replace(/^\//, '').split('/').pop() || path;
    // 只尝试相对路径格式（避免 / 开头导致 permission denied）
    var attempts = [
      filename,
      './' + filename,
    ];
    var errors = [];
    for (var i = 0; i < attempts.length; i++) {
      try {
        console.log('[WeChat fs] readFileSync: ' + attempts[i]);
        var data = fileSystemManager.readFileSync(attempts[i]);
        console.log('[WeChat fs] OK: ' + attempts[i] + ' type=' + Object.prototype.toString.call(data) + ' len=' + (data.byteLength || data.length || 0));
        if (data instanceof ArrayBuffer) return data;
        if (data && data.buffer instanceof ArrayBuffer) return data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
        if (typeof data === 'string') {
          var arr = new Uint8Array(data.length);
          for (var j = 0; j < data.length; j++) arr[j] = data.charCodeAt(j) & 0xFF;
          return arr.buffer;
        }
        // Uint8Array 或其他类型
        if (data && typeof data.length === 'number') {
          var buf = new ArrayBuffer(data.length);
          var view = new Uint8Array(buf);
          for (var k = 0; k < data.length; k++) view[k] = data[k];
          return buf;
        }
        return data;
      } catch (e) {
        errors.push(attempts[i] + ': ' + e.message);
      }
    }
    throw new Error('File not found in package: ' + path + ' (errors: ' + errors.join('; ') + ')');
  }

  // downloadCdnFile: 从 CDN 下载文件为 ArrayBuffer。
  //
  // 根因分析（基于 DevTools 2.02.2607232 实测）:
  //   wx.downloadFile 成功后，临时文件 http://tmp/...bin 用 readFileSync(path, 'binary')
  //   读取返回空 Object（ctor=Object, byteLength=undefined）—— DevTools 的 'binary'
  //   encoding 对大文件不生效。downloadFile + readFileSync 组合在 DevTools 下不可靠。
  //
  //   wx.request(responseType: 'arraybuffer') 直接在 JS 层返回 ArrayBuffer，完全绕过
  //   文件系统，是 DevTools 下获取大文件 ArrayBuffer 的唯一可靠方式。
  //   真机环境下分包读取通常已成功，CDN fallback 很少触发；即使触发，wx.request
  //   在真机上也可靠（但 19MB 响应体会全量加载到内存，注意内存压力）。
  //
  // 策略:
  //   1. 检查 USER_DATA_PATH 缓存（真机有效；DevTools readFileSync 返回空则跳过）
  //   2. 优先 wx.request(responseType: 'arraybuffer') —— 绕过文件系统，DevTools 可靠
  //   3. wx.request 失败 → downloadFile + readFileSync('binary') 兜底
  //   4. 下载成功后写入 USER_DATA_PATH 缓存（真机有效，DevTools 可能写入失败但不影响）
  function downloadCdnFile(url) {
    return new Promise(function (resolve, reject) {
      var fileName = '';
      var parts = url.split('/');
      fileName = parts[parts.length - 1].split('?')[0];

      // 1. 检查缓存（USER_DATA_PATH 下的文件）
      if (USER_DATA_PATH && fileName) {
        var cachedPath = USER_DATA_PATH + '/' + fileName;
        try {
          fileSystemManager.accessSync(cachedPath);
          var stat = fileSystemManager.statSync(cachedPath);
          if (stat.size > 0) {
            var cachedData = fileSystemManager.readFileSync(cachedPath, 'binary');
            var cachedAb = _coerceToArrayBufferGlobal(cachedData, cachedPath);
            if (cachedAb.byteLength > 0) {
              console.log('[WeChat] Cache hit: ' + fileName + ' (' + (cachedAb.byteLength / 1024 / 1024).toFixed(2) + ' MB)');
              resolve(cachedAb);
              return;
            }
          }
        } catch (e) {
          // 缓存读取失败（DevTools readFileSync 可能返回空），继续走网络下载
        }
      }

      // 2. 优先 wx.request(responseType: 'arraybuffer') —— 绕过文件系统
      console.log('[WeChat] Downloading via wx.request: ' + url);
      wx.request({
        url: url,
        method: 'GET',
        responseType: 'arraybuffer',
        success: function (res) {
          console.log('[WeChat] wx.request success: statusCode=' + res.statusCode + ' data type=' + typeof res.data +
                      (res.data ? ' byteLength=' + res.data.byteLength + ' instanceof ArrayBuffer=' + (res.data instanceof ArrayBuffer) : ''));
          if (res.statusCode === 200 && res.data instanceof ArrayBuffer && res.data.byteLength > 0) {
            var ab = res.data;
            console.log('[WeChat] Downloaded: ' + fileName + ' (' + (ab.byteLength / 1024 / 1024).toFixed(2) + ' MB)');
            // 写入缓存（真机有效，DevTools 可能失败但不影响本次返回）
            if (USER_DATA_PATH && fileName) {
              try {
                fileSystemManager.writeFileSync(USER_DATA_PATH + '/' + fileName, ab, 'binary');
              } catch (we) {
                console.warn('[WeChat] Cache write failed: ' + (we && we.message));
              }
            }
            resolve(ab);
          } else {
            // wx.request 返回非 ArrayBuffer 或 0 字节，fallback 到 downloadFile
            console.warn('[WeChat] wx.request returned non-ArrayBuffer or 0 bytes (data=' + typeof res.data + '), trying downloadFile');
            _downloadViaDownloadFile(url, fileName, resolve, reject);
          }
        },
        fail: function (err) {
          console.warn('[WeChat] wx.request failed: ' + (err && err.errMsg) + ', trying downloadFile');
          _downloadViaDownloadFile(url, fileName, resolve, reject);
        }
      });
    });
  }

  // downloadFile + readFileSync 兜底路径（wx.request 失败时使用）
  function _downloadViaDownloadFile(url, fileName, resolve, reject) {
    wx.downloadFile({
      url: url,
      success: function (res) {
        console.log('[WeChat] downloadFile success: statusCode=' + res.statusCode + ' tempFilePath=' + res.tempFilePath);
        if (res.statusCode === 200 && res.tempFilePath) {
          try {
            var data = fileSystemManager.readFileSync(res.tempFilePath, 'binary');
            var ab = _coerceToArrayBufferGlobal(data, res.tempFilePath);
            console.log('[WeChat] Downloaded via downloadFile: ' + fileName + ' (' + (ab.byteLength / 1024 / 1024).toFixed(2) + ' MB)');
            // 缓存
            if (USER_DATA_PATH && fileName && ab.byteLength > 0) {
              try {
                fileSystemManager.writeFileSync(USER_DATA_PATH + '/' + fileName, ab, 'binary');
              } catch (we) {}
            }
            resolve(ab);
          } catch (e) {
            reject(new Error('Read downloaded file failed: ' + e.message));
          }
        } else {
          reject(new Error('downloadFile non-200: ' + res.statusCode));
        }
      },
      fail: function (err) {
        reject(new Error('downloadFile failed: ' + (err && err.errMsg || 'unknown')));
      }
    });
  }

  // 全局类型转换辅助（downloadCdnFile 等非 fetch 上下文使用）
  function _coerceToArrayBufferGlobal(data, label) {
    if (data instanceof ArrayBuffer) {
      return data;
    }
    if (data && data.buffer instanceof ArrayBuffer) {
      return data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
    }
    throw new Error('readFileSync returned uncoercible type: ' + typeof data +
                    ' constructor=' + (data && data.constructor ? data.constructor.name : 'N/A') +
                    ' for ' + label);
  }

  safeDefineGlobal('fetch', function (url, options) {
    options = options || {};
    var method = (options.method || 'GET').toUpperCase();

    // 非 GET 请求走 wx.request
    if (method !== 'GET') {
      return new Promise(function (resolve, reject) {
        wx.request({
          url: url,
          method: method,
          header: options.headers || {},
          responseType: 'arraybuffer',
          data: options.body,
          success: function (res) {
            resolve(new Response(res.data instanceof ArrayBuffer ? res.data : new ArrayBuffer(0), { status: res.statusCode }));
          },
          fail: function (err) { reject(new Error(err.errMsg || 'fetch failed')); },
        });
      });
    }

    var urlStr = String(url);

    // _coerceToArrayBuffer: 将 readFile/readFileSync 的各种可能返回类型统一转为 ArrayBuffer。
    // 真机返回 ArrayBuffer；DevTools 'binary' encoding 可能返回 Uint8Array 或其他 TypedArray。
    function _coerceToArrayBuffer(data, label) {
      if (data instanceof ArrayBuffer) {
        return data;
      }
      // TypedArray (Uint8Array 等): 通过 .buffer 取底层 ArrayBuffer 副本
      if (data && data.buffer instanceof ArrayBuffer) {
        return data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
      }
      throw new Error('readFile returned uncoercible type: ' + typeof data +
                      ' constructor=' + (data && data.constructor ? data.constructor.name : 'N/A') +
                      ' for ' + label);
    }

    // _readDataFileAsync: 异步读取分包内二进制文件为 ArrayBuffer。
    //
    // 根因分析（基于 DevTools 2.02.2607232 多轮实测）:
    //   readFileSync(path) 不传 encoding → DevTools 内部 coverRes 调 atob → "Failed to execute 'atob'"
    //   readFileSync(path, 'binary') → DevTools 返回空 Object（19MB 大文件，数据丢失）
    //   readFile(path, {encoding: 'binary'}) 异步 → 同样 bug，大文件返回空 Object
    //   readFile(path, {}) 不传 encoding → DevTools 回调不触发，Promise 永久悬挂（致命！）
    //   readFile(path, {encoding: 'base64'}) → DevTools 返回 Object 而非 string，不可靠
    //   wx.request(code包相对路径) → "invalid url" 错误（wx.request 只接受 http/https URL）
    //   结论: DevTools 2.02.2607232 的 FileSystemManager 对 19MB 大文件所有整文件读取 API 都不可靠。
    //
    // 根本性修复: 分块读取（Chunked Reading）
    //   DevTools 的 bug 是针对"大文件"的，对小块数据（1MB）的 readFile 应该可靠。
    //   用 readFile 的 offset + length 参数，把 19MB 文件分成 20 块 1MB，逐块异步读取后拼接。
    //   - 每块 1MB，远低于 DevTools 大文件 bug 阈值
    //   - 异步 readFile 不传 encoding，官方默认返回 ArrayBuffer
    //   - 并行读取所有块（Promise.all），总时间 ≈ 单块时间（非 N 倍）
    //   - 每块加 10s 超时保护，避免回调不触发导致永久悬挂
    //   - 真机也兼容（offset/length 是官方标准参数）
    //
    //   失败时降级到整文件读取（策略 2/3），作为真机兼容兜底。
    function _readDataFileAsync(dataPath) {
      // _withTimeout: 给 Promise 加超时保护，避免回调不触发导致永久悬挂
      function _withTimeout(promise, ms, label) {
        return new Promise(function (resolve, reject) {
          var settled = false;
          var timer = setTimeout(function () {
            if (!settled) {
              settled = true;
              reject(new Error(label + ' timeout after ' + ms + 'ms (callback not fired)'));
            }
          }, ms);
          promise.then(function (val) {
            if (!settled) {
              settled = true;
              clearTimeout(timer);
              resolve(val);
            }
          }, function (err) {
            if (!settled) {
              settled = true;
              clearTimeout(timer);
              reject(err);
            }
          });
        });
      }

      // 获取文件总大小（从 _fileSizes 注入变量）
      var _fileSizes = globalThis._fileSizes || {};
      var _dataFileName = globalThis._dataFileName || '';
      var totalSize = _fileSizes[_dataFileName] || 0;
      var CHUNK_SIZE = 1024 * 1024;  // 1MB per chunk

      // ============================================================
      // 策略 1 (ROOT-CAUSE FIX): fd-based sequential reading
      // ============================================================
      // 根因分析 (2026-07-24):
      //   DevTools 2.02.2607232 的 FileSystemManager.readFile({offset, length})
      //   忽略 offset 参数 — 所有 chunk 都从 offset 0 读取，导致组装后的 buffer
      //   前 1MB 重复 20 次。Emscripten .data 元数据（在文件头部）能正确解析，
      //   所以 MEMFS 中文件路径/大小正确，但文件内容（在 .data 后半部分）全部
      //   错误 → mscorlib.dll 报 "invalid CIL image"（MZ 头丢失）。
      //
      // 根本性修复: 用 openSync + readSync 顺序读取，完全不依赖 offset 参数。
      //   fd 内部维护文件位置，每次 readSync 自动从前次位置继续。
      //   每次 read 1MB（小块数据可靠），顺序读 20 次完成全文件读取。
      //   - 不依赖 readFile 的 offset 参数（规避 DevTools bug）
      //   - 不依赖 readFileSync 整文件读取（规避大文件返回空 Object bug）
      //   - 真机兼容（openSync/readSync 是 2.16.0+ 标准API）
      // ============================================================
      if (totalSize > 0 && fileSystemManager.openSync && fileSystemManager.readSync) {
        console.log('[WeChat fetch F5] Strategy 1 (fd-based sequential): ' + totalSize + ' bytes in 1MB chunks');
        try {
          var fd = fileSystemManager.openSync({ filePath: dataPath, flag: 'r' });
          console.log('[WeChat fetch F5] openSync OK, fd=' + fd);

          var fdChunks = [];
          var fdRemaining = totalSize;
          var fdChunkIndex = 0;
          var fdMaxChunks = Math.ceil(totalSize / CHUNK_SIZE) + 2;  // safeguard against infinite loop
          while (fdRemaining > 0 && fdChunkIndex < fdMaxChunks) {
            var readLen = Math.min(CHUNK_SIZE, fdRemaining);
            var readBuf = new ArrayBuffer(readLen);
            var readResult = fileSystemManager.readSync({
              fd: fd,
              arrayBuffer: readBuf,
              offset: 0,
              length: readLen,
            });
            var bytesRead = (readResult && typeof readResult.bytesRead === 'number') ? readResult.bytesRead : 0;
            if (bytesRead === 0) {
              console.warn('[WeChat fetch F5] readSync returned 0 at chunk ' + fdChunkIndex + ' (EOF?), stopping');
              break;
            }
            // Partial read: use only the bytes actually read
            fdChunks.push(bytesRead < readLen ? readBuf.slice(0, bytesRead) : readBuf);
            fdRemaining -= bytesRead;
            fdChunkIndex++;
          }
          if (fdChunkIndex >= fdMaxChunks && fdRemaining > 0) {
            console.warn('[WeChat fetch F5] readSync hit max chunk limit (' + fdMaxChunks + '), file position may not be advancing');
          }
          try { fileSystemManager.closeSync({ fd: fd }); } catch (closeErr) {
            console.warn('[WeChat fetch F5] closeSync failed: ' + closeErr.message);
          }

          // Assemble chunks into single ArrayBuffer
          var actualSize = totalSize - fdRemaining;
          var fdAb = new ArrayBuffer(actualSize);
          var fdView = new Uint8Array(fdAb);
          var fdPos = 0;
          for (var fi = 0; fi < fdChunks.length; fi++) {
            fdView.set(new Uint8Array(fdChunks[fi]), fdPos);
            fdPos += fdChunks[fi].byteLength;
          }
          console.log('[WeChat fetch F5] Strategy 1 OK: ' + fdAb.byteLength + ' bytes (' + fdChunks.length + ' chunks)');

          // === 诊断: 验证每个 chunk 的前 4 字节 ===
          // 如果 offset bug 存在，所有 chunk 的前 4 字节会相同（都是文件开头的字节）
          // 修复后，每个 chunk 的前 4 字节应该不同（因为是顺序读取不同位置的数据）
          var fdDiag = [];
          for (var dk = 0; dk < Math.min(fdChunks.length, 6); dk++) {
            var dck = new Uint8Array(fdChunks[dk]);
            fdDiag.push(dk + ':[' +
              (dck[0] < 16 ? '0' : '') + dck[0].toString(16) +
              (dck[1] < 16 ? '0' : '') + dck[1].toString(16) +
              (dck[2] < 16 ? '0' : '') + dck[2].toString(16) +
              (dck[3] < 16 ? '0' : '') + dck[3].toString(16) + ']');
          }
          console.log('[WeChat Diag] fd-based chunk first-4-bytes: ' + fdDiag.join(' '));

          // 验证: 搜索组装后 buffer 中的 MZ 头位置
          var fdMzPos = [];
          var fdSearchEnd = Math.min(fdView.length, 16 * 1024 * 1024);
          for (var fmi = 0; fmi < fdSearchEnd - 1; fmi++) {
            if (fdView[fmi] === 0x4D && fdView[fmi + 1] === 0x5A) {
              fdMzPos.push(fmi);
              if (fdMzPos.length >= 5) break;
            }
          }
          console.log('[WeChat Diag] fd-based MZ positions (first 5): ' + JSON.stringify(fdMzPos));
          // mscorlib.dll 在 .data 中通常在 offset ~12MB 处
          if (fdView.length > 12198599) {
            console.log('[WeChat Diag] bytes @12198595 (expected mscorlib MZ): ' +
              (fdView[12198595] < 16 ? '0' : '') + fdView[12198595].toString(16) +
              (fdView[12198596] < 16 ? '0' : '') + fdView[12198596].toString(16) + ' ' +
              (fdView[12198597] < 16 ? '0' : '') + fdView[12198597].toString(16) +
              (fdView[12198598] < 16 ? '0' : '') + fdView[12198598].toString(16));
          }

          if (fdAb.byteLength === totalSize) {
            return Promise.resolve(fdAb);
          }
          console.warn('[WeChat fetch F5] Strategy 1 got ' + fdAb.byteLength + '/' + totalSize + ' bytes (short read), trying fallback');
        } catch (fdErr) {
          console.warn('[WeChat fetch F5] Strategy 1 (fd-based) failed: ' + fdErr.message + ', falling back to parallel chunked');
        }
      }

      // ============================================================
      // 策略 2 (FALLBACK): Parallel chunked reading (legacy, DevTools offset bug 可能触发)
      // 仅在 openSync/readSync 不可用或失败时使用。真机环境通常不需要走这里。
      // ============================================================
      if (totalSize > 0) {
        var chunkCount = Math.ceil(totalSize / CHUNK_SIZE);
        console.log('[WeChat fetch F5] Strategy 2 (parallel chunked): ' + totalSize + ' bytes / ' + chunkCount + ' chunks x ' + CHUNK_SIZE + ' bytes');

        var chunkPromises = [];
        for (var i = 0; i < chunkCount; i++) {
          var offset = i * CHUNK_SIZE;
          var length = Math.min(CHUNK_SIZE, totalSize - offset);
          chunkPromises.push(_readChunkAsync(dataPath, offset, length, i, _withTimeout));
        }

        return Promise.all(chunkPromises).then(function (chunks) {
          var ab = new ArrayBuffer(totalSize);
          var view = new Uint8Array(ab);
          var pos = 0;
          for (var j = 0; j < chunks.length; j++) {
            if (!chunks[j] || chunks[j].byteLength === 0) {
              throw new Error('chunk ' + j + ' is empty');
            }
            view.set(new Uint8Array(chunks[j]), pos);
            pos += chunks[j].byteLength;
          }
          console.log('[WeChat fetch F5] Strategy 2 OK: ' + ab.byteLength + ' bytes (' + chunks.length + ' chunks)');

          // 诊断: chunk first-4-bytes（如果 offset bug 存在，所有 chunk 前 4 字节相同）
          var chunkDiag = [];
          for (var k = 0; k < Math.min(chunks.length, 6); k++) {
            var ck = new Uint8Array(chunks[k]);
            chunkDiag.push(k + ':[' +
              (ck[0] < 16 ? '0' : '') + ck[0].toString(16) +
              (ck[1] < 16 ? '0' : '') + ck[1].toString(16) +
              (ck[2] < 16 ? '0' : '') + ck[2].toString(16) +
              (ck[3] < 16 ? '0' : '') + ck[3].toString(16) + ']');
          }
          console.log('[WeChat Diag] parallel chunk first-4-bytes: ' + chunkDiag.join(' '));

          return ab;
        }).catch(function (err) {
          console.warn('[WeChat fetch F5] strategy 2 (chunked) failed: ' + err.message + ', falling back to whole-file base64');
          // 降级到策略 3: 整文件 base64 读取（真机可靠）
          return _withTimeout(_readDataFileBase64(dataPath), 15000, 'readFile(base64)');
        }).catch(function (err2) {
          console.warn('[WeChat fetch F5] strategy 3 (base64) failed: ' + err2.message + ', trying sync binary');
          // 降级到策略 4: 整文件同步 binary 读取（真机可靠，DevTools 可能失败）
          try {
            var syncData = fileSystemManager.readFileSync(dataPath, 'binary');
            var syncAb = _coerceToArrayBuffer(syncData, dataPath);
            if (syncAb.byteLength === 0) {
              throw new Error('readFileSync(binary) returned empty buffer (DevTools large-file bug)');
            }
            console.log('[WeChat fetch F5] strategy 4 (sync binary) OK: ' + syncAb.byteLength + ' bytes');
            return syncAb;
          } catch (e3) {
            throw new Error('All strategies failed: [chunked: ' + err2.message + ' | sync: ' + e3.message + ']');
          }
        });
      }

      // totalSize 未知，直接走整文件读取（真机路径）
      console.warn('[WeChat fetch F5] totalSize unknown, falling back to whole-file base64');
      return _withTimeout(_readDataFileBase64(dataPath), 15000, 'readFile(base64)').catch(function (err2) {
        console.warn('[WeChat fetch F5] base64 failed: ' + err2.message + ', trying sync binary');
        try {
          var syncData2 = fileSystemManager.readFileSync(dataPath, 'binary');
          var ab2 = _coerceToArrayBuffer(syncData2, dataPath);
          if (ab2.byteLength === 0) {
            throw new Error('readFileSync(binary) returned empty buffer');
          }
          return ab2;
        } catch (e3) {
          throw new Error('All strategies failed: [' + err2.message + ' | ' + e3.message + ']');
        }
      });
    }

    // _readChunkAsync: 读取文件的某一块（offset + length 参数），返回 Promise<ArrayBuffer>。
    // 小块数据（1MB）的 readFile 在 DevTools 下可靠，规避大文件 bug。
    function _readChunkAsync(dataPath, offset, length, index, _withTimeout) {
      return _withTimeout(new Promise(function (resolve, reject) {
        fileSystemManager.readFile({
          filePath: dataPath,
          offset: offset,
          length: length,
          // 不传 encoding: 官方默认返回 ArrayBuffer
          success: function (res) {
            var data = res && res.data;
            if (data instanceof ArrayBuffer && data.byteLength > 0) {
              resolve(data);
            } else if (data && data.buffer instanceof ArrayBuffer && data.byteLength > 0) {
              // TypedArray: 取底层 ArrayBuffer 副本
              resolve(data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength));
            } else {
              reject(new Error('chunk ' + index + ' returned invalid data: type=' + typeof data +
                              ' byteLength=' + (data ? data.byteLength : 'N/A')));
            }
          },
          fail: function (err) {
            reject(new Error('chunk ' + index + ' readFile failed: ' + (err && err.errMsg || JSON.stringify(err))));
          },
        });
      }), 10000, 'chunk ' + index).then(function (ab) {
        if ((index + 1) % 5 === 0 || index === 0) {
          console.log('[WeChat fetch F5] chunk ' + index + ' OK: ' + ab.byteLength + ' bytes');
        }
        return ab;
      });
    }

    // _readDataFileBase64: 通过 base64 编码读取二进制文件并转为 ArrayBuffer（真机可靠，DevTools 失败）。
    function _readDataFileBase64(dataPath) {
      return new Promise(function (resolve, reject) {
        fileSystemManager.readFile({
          filePath: dataPath,
          encoding: 'base64',
          success: function (res) {
            var b64 = res && res.data;
            if (typeof b64 !== 'string' || b64.length === 0) {
              reject(new Error('readFile(base64) returned non-string or empty: type=' + typeof b64));
              return;
            }
            console.log('[WeChat fetch F5] readFile(base64) got ' + b64.length + ' chars, decoding...');
            // 优先用 wx.base64ToArrayBuffer 原生 API（高效，无中间 binary string）
            if (typeof wx !== 'undefined' && typeof wx.base64ToArrayBuffer === 'function') {
              try {
                var ab = wx.base64ToArrayBuffer(b64);
                if (ab && ab.byteLength > 0) {
                  console.log('[WeChat fetch F5] wx.base64ToArrayBuffer OK: ' + ab.byteLength + ' bytes');
                  resolve(ab);
                  return;
                }
                console.warn('[WeChat fetch F5] wx.base64ToArrayBuffer returned empty, falling back to atob');
              } catch (e) {
                console.warn('[WeChat fetch F5] wx.base64ToArrayBuffer threw: ' + e.message + ', falling back to atob');
              }
            }
            // 兜底: atob + 手动填充 Uint8Array（兼容性最高，但慢）
            try {
              var bin = atob(b64);
              var ab2 = new ArrayBuffer(bin.length);
              var view = new Uint8Array(ab2);
              for (var i = 0; i < bin.length; i++) {
                view[i] = bin.charCodeAt(i);
              }
              console.log('[WeChat fetch F5] atob decode OK: ' + ab2.byteLength + ' bytes');
              resolve(ab2);
            } catch (e2) {
              reject(new Error('atob decode failed: ' + e2.message));
            }
          },
          fail: function (err) {
            reject(new Error('readFile(base64) failed: ' + (err && err.errMsg || JSON.stringify(err))));
          },
        });
      });
    }

    // === F5: .data 文件从分包异步读取（分块读取策略）===
    // convert 脚本把 .data 原样复制为 .dat 放入 data_pkg 分包（≤20MB 限制内）。
    // _readDataFileAsync 内部按可靠性顺序尝试 3 种读取方式：
    //   1. 分块读取（Chunked Reading）: readFile offset+length，每块 1MB，规避 DevTools 大文件 bug
    //   2. 整文件 base64 读取 + wx.base64ToArrayBuffer（真机可靠，DevTools 失败时降级）
    //   3. 整文件同步 binary 读取（真机可靠，DevTools 可能失败，最后兜底）
    // 全部失败时走 CDN fallback（downloadCdnFile 内部 wx.request + wx.downloadFile 双层兜底）。
    var _dataFileName = globalThis._dataFileName || '';
    var _dataBinName = globalThis._dataBinName || '';
    var _dataInSubpkg = globalThis._dataInSubpkg === true;
    var _dataSubpkg = globalThis._dataSubpkg || 'data_pkg';
    if (_dataFileName && _dataInSubpkg && urlStr.indexOf(_dataFileName) !== -1 && _dataBinName) {
      console.log('[WeChat fetch F5] .data file detected, loading from subpackage: ' + _dataSubpkg + ' (file: ' + _dataBinName + ')');
      var dataPath = _dataSubpkg + '/' + _dataBinName;
      return _ensureSubpkgLoaded(_dataSubpkg, dataPath).then(function () {
        // 异步 readFile(path, {encoding: 'binary'}) 绕过 DevTools readFileSync bug
        return _readDataFileAsync(dataPath);
      }).then(function (ab) {
        console.log('[WeChat fetch F5] data loaded from subpackage: ' + dataPath + ' (' + (ab.byteLength / 1048576).toFixed(2) + ' MB)');
        return new Response(ab, {
          status: 200,
          headers: { 'Content-Length': String(ab.byteLength) },
        });
      }).catch(function (err) {
        // 分包读取失败，走 CDN fallback
        if (_cdnBase) {
          var cdnUrl = _cdnBase.replace(/\/$/, '') + '/' + _dataFileName;
          console.warn('[WeChat fetch F5] subpackage read failed, fallback CDN: ' + cdnUrl + ' (' + (err && err.message) + ')');
          return downloadCdnFile(cdnUrl).then(function (ab) {
            console.log('[WeChat fetch F5] data loaded from CDN: ' + (ab.byteLength / 1048576).toFixed(2) + ' MB');
            return new Response(ab, {
              status: 200,
              headers: { 'Content-Length': String(ab.byteLength) },
            });
          });
        }
        throw err;
      });
    }

    // 非 .data 文件，走原始流程
    return _fetchContinue(urlStr);

    function _fetchContinue(urlStr) {
    // 绝对 URL (http/https)
    if (urlStr.indexOf('http://') === 0 || urlStr.indexOf('https://') === 0) {
      return downloadCdnFile(urlStr).then(function (ab) {
        return new Response(ab, {
          status: 200,
          headers: { 'Content-Length': String(ab.byteLength) },
        });
      });
    }

    // file:// 协议
    if (urlStr.indexOf('file://') === 0) {
      urlStr = urlStr.replace('file://', '');
    }

    // USER_DATA_PATH 文件
    if (USER_DATA_PATH && urlStr.indexOf(USER_DATA_PATH) === 0) {
      return new Promise(function (resolve, reject) {
        try {
          var data = fileSystemManager.readFileSync(urlStr);
          var ab = data instanceof ArrayBuffer ? data : (data.buffer ? data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength) : new ArrayBuffer(0));
          resolve(new Response(ab, { status: 200 }));
        } catch (e) {
          reject(e);
        }
      });
    }

    // 主包内文件
    return new Promise(function (resolve, reject) {
      var filename = urlStr.split('/').pop() || urlStr;
      console.log('[WeChat fetch] Looking for: ' + filename + ' (url: ' + urlStr + ')');

      // === WASM 文件特殊处理: 不读取内容，返回轻量占位 Response ===
      // 原因: 104MB WASM 通过 getTrackedResponse 流式读取 (64KB chunk × 1664 次) 会卡死
      // 真正的 WASM 实例化由 instantiateWasm 回调处理 (用文件路径，不经过 fetch)
      if (filename.toLowerCase().endsWith('.wasm') || filename.toLowerCase().endsWith('.wasm.br')) {
        console.log('[WeChat fetch] WASM file detected, returning placeholder response: ' + filename);
        // 返回一个 1 字节的占位 ArrayBuffer，避免 null 检查失败
        var placeholder = new ArrayBuffer(1);
        resolve(new Response(placeholder, {
          status: 200,
          headers: { 'Content-Type': 'application/wasm', 'Content-Length': '1' },
        }));
        return;
      }

      try {
        var data = readMainPackageFile(filename);
        console.log('[WeChat fetch] Found in main package: ' + filename + ' (' + (data.byteLength || data.length) + ' bytes)');
        resolve(new Response(data, {
          status: 200,
          headers: { 'Content-Length': String(data.byteLength || data.length || 0) },
        }));
      } catch (e) {
        console.log('[WeChat fetch] Not in main package: ' + filename + ', trying CDN...');
        // 本地找不到，尝试 CDN
        if (_cdnBase) {
          var cdnUrl = _cdnBase + '/' + filename;
          console.log('[WeChat fetch] CDN URL: ' + cdnUrl);
          downloadCdnFile(cdnUrl).then(function (ab) {
            console.log('[WeChat fetch] CDN download OK: ' + filename + ' (' + ab.byteLength + ' bytes)');
            resolve(new Response(ab, {
              status: 200,
              headers: { 'Content-Length': String(ab.byteLength) },
            }));
          }).catch(function(cdnErr) {
            console.error('[WeChat fetch] CDN download failed: ' + filename + ': ' + (cdnErr.message || cdnErr));
            reject(new Error('File not found: ' + filename + ' (main package: ' + e.message + ', CDN: ' + (cdnErr.message || cdnErr) + ')'));
          });
          return;
        }
        console.error('[WeChat fetch] No CDN base set, cannot fetch: ' + filename);
        reject(new Error('File not found: ' + filename + ' (no CDN fallback available)'));
      }
    });
    } // end _fetchContinue
  });

  // ============================================================
  // 模块 12: XMLHttpRequest polyfill
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
  XMLHttpRequest.prototype.setRequestHeader = function (key, value) {
    this._headers[key] = value;
  };
  XMLHttpRequest.prototype.getResponseHeader = function () { return null; };
  XMLHttpRequest.prototype.send = function (body) {
    var self = this;
    globalThis.fetch(this._url, { method: this._method, headers: this._headers, body: body }).then(function (response) {
      if (self.responseType === 'arraybuffer') {
        response.arrayBuffer().then(function (buf) {
          self.status = response.status;
          self.response = buf;
          self.readyState = 4;
          if (self.onreadystatechange) self.onreadystatechange();
          if (self.onload) self.onload();
        });
      } else {
        response.text().then(function (text) {
          self.status = response.status;
          self.responseText = text;
          self.response = text;
          self.readyState = 4;
          if (self.onreadystatechange) self.onreadystatechange();
          if (self.onload) self.onload();
        });
      }
    }).catch(function (err) {
      self.status = 0;
      self.readyState = 4;
      if (self.onerror) self.onerror(err);
    });
  };
  XMLHttpRequest.prototype.abort = function () { this.readyState = 0; };
  safeDefineGlobal('XMLHttpRequest', XMLHttpRequest);

  // ============================================================
  // 模块 13: WebGL context
  // ============================================================
  safeDefineGlobal('WebGLRenderingContext', globalThis.WebGLRenderingContext || function () {});
  safeDefineGlobal('WebGL2RenderingContext', globalThis.WebGL2RenderingContext || function () {});

  // ============================================================
  // 模块 14: performance.now polyfill
  // ============================================================
  safeDefineGlobal('performance', globalThis.performance || {
    now: function () {
      if (wx.getPerformance) {
        return wx.getPerformance().now();
      }
      return Date.now();
    },
  });

  // ============================================================
  // 模块 15: devicePixelRatio
  // ============================================================
  if (typeof globalThis.devicePixelRatio === 'undefined') {
    try {
      safeDefineGlobal('devicePixelRatio', wx.getSystemInfoSync().pixelRatio);
    } catch (e) {
      safeDefineGlobal('devicePixelRatio', 1);
    }
  }

  // ============================================================
  // 模块 16: navigator.languages
  // ============================================================
  safeDefineGlobal('navigator', globalThis.navigator || {
    userAgent: 'WeChat MiniGame',
    language: 'zh-CN',
    languages: ['zh-CN', 'en'],
    platform: 'wechat',
    onLine: true,
  });
  if (!globalThis.navigator.languages) {
    globalThis.navigator.languages = ['zh-CN', 'en'];
  }

  // ============================================================
  // 模块 17: window polyfill
  // ============================================================
  safeDefineGlobal('window', globalThis.window || globalThis);
  // 新版开发者工具基础库自带受限 window/document 桩：safeDefineGlobal 可能覆盖不进去。
  // 退而求其次：缺什么补什么，直接给原生对象增量化。
  //
  // 关键修复: innerWidth/innerHeight/devicePixelRatio 必须是 getter（动态读 wx 系统信息），
  // 不能是静态赋值。原因: Godot 引擎在 canvasResizePolicy=2 (FullWindow) 下会调用
  //   width = Math.floor(window.innerWidth * scale)
  //   height = Math.floor(window.innerHeight * scale)
  // 如果 window.innerWidth 是 undefined 或 0、scale=3，canvas 会被缩成 3x3。
  // 同时引擎在 _godot_js_display_screen_get 监听 resize 事件时也会读 window.screen，
  // 所以 screen.width/height 也需要补全。
  (function () {
    var _win = globalThis.window;
    if (!_win || typeof _win !== 'object') return;
    try {
      if (typeof _win.addEventListener !== 'function') _win.addEventListener = function () {};
      if (typeof _win.removeEventListener !== 'function') _win.removeEventListener = function () {};

      // 缓存一份系统信息（wx.getSystemInfoSync 较慢，每帧调用会拖垮性能）
      var _cachedInfo = null;
      function getSysInfo() {
        if (!_cachedInfo) {
          try { _cachedInfo = wx.getSystemInfoSync(); } catch (e) { _cachedInfo = {}; }
        }
        return _cachedInfo;
      }

      // 用 getter 动态返回（防止引擎覆盖）
      // 关键: 微信 devtool 自带的 window 桩会把 innerWidth/innerHeight 设成 1（占位值），
      // 不能用 "> 0" 判断已设置——1 > 0 会跳过赋值，引擎拿到 1 * devicePixelRatio=3 → canvas=3x3。
      // 阈值改成 100（真实屏幕尺寸一定 > 100），低于此值视为无效占位，强制覆盖。
      function defineWindowProp(name, fallback, minValid) {
        var cur = _win[name];
        if (typeof cur === 'number' && cur >= (minValid || 100)) {
          console.log('[WeChat Adapter] window.' + name + ' already valid: ' + cur);
          return;
        }
        console.log('[WeChat Adapter] window.' + name + ' invalid (' + cur + '), overriding with getter');
        try {
          Object.defineProperty(_win, name, {
            get: function () {
              var info = getSysInfo();
              var key = name === 'devicePixelRatio' ? 'pixelRatio' :
                        name === 'innerWidth' ? 'windowWidth' :
                        name === 'innerHeight' ? 'windowHeight' : '';
              var v = info[key];
              if (typeof v !== 'number' || v <= 0) v = fallback;
              return v;
            },
            configurable: true,
          });
          console.log('[WeChat Adapter] window.' + name + ' getter installed');
        } catch (e) {
          // defineProperty 失败（冻结对象），降级为静态赋值
          try { _win[name] = fallback; } catch (e2) {}
        }
      }
      defineWindowProp('innerWidth', 375, 100);
      defineWindowProp('innerHeight', 667, 100);
      // devicePixelRatio 永远 > 0，但 devtool 设的 3 是正确的，直接信任
      defineWindowProp('devicePixelRatio', 1, 1);

      // screen 对象（引擎 _godot_js_display_screen_get 会读 window.screen.width/height）
      if (!_win.screen || typeof _win.screen !== 'object') {
        _win.screen = {};
      }
      var _scr = _win.screen;
      ['width', 'height', 'availWidth', 'availHeight'].forEach(function (k) {
        if (typeof _scr[k] !== 'number' || _scr[k] <= 0) {
          try {
            Object.defineProperty(_scr, k, {
              get: function () {
                var info = getSysInfo();
                if (k === 'width' || k === 'availWidth') return info.windowWidth || 375;
                return info.windowHeight || 667;
              },
              configurable: true,
            });
          } catch (e) {
            try { _scr[k] = 375; } catch (e2) {}
          }
        }
      });
    } catch (e) {
      console.warn('[WeChat Adapter] window augment failed: ' + e);
    }
  })();

  // ============================================================
  // 模块 17b: alert/prompt/confirm 对话框桩
  // 微信无对话框 API，Godot 的 display_alert、'WebGL context lost' 提示等会调
  // window.alert / 裸 alert()。定义为全局绑定供 convert 把 window.alert 重定向过来。
  // ============================================================
  if (typeof globalThis.alert !== 'function') {
    safeDefineGlobal('alert', function (msg) { console.warn('[alert]', msg); });
  }
  if (typeof globalThis.prompt !== 'function') {
    safeDefineGlobal('prompt', function (msg) { console.warn('[prompt suppressed]', msg); return ''; });
  }
  if (typeof globalThis.confirm !== 'function') {
    safeDefineGlobal('confirm', function (msg) { console.warn('[confirm suppressed]', msg); return false; });
  }

  // ============================================================
  // 模块 18: document polyfill
  // 关键: getElementsByTagName('canvas') 必须返回 wx canvas，且 instanceof HTMLCanvasElement
  // ============================================================
  var _document = {
    createElement: function (tagName) {
      if (tagName === 'canvas') return getMainCanvas();
      if (tagName === 'img') return new HTMLImageElement();
      return { style: {}, appendChild: function () {}, addEventListener: function () {}, removeEventListener: function () {} };
    },
    getElementById: function (id) {
      if (id === 'canvas') return getMainCanvas();
      return null;
    },
    getElementsByTagName: function (tagName) {
      if (tagName === 'canvas') {
        var arr = [getMainCanvas()];
        arr.length = 1;
        return arr;
      }
      var empty = [];
      empty.length = 0;
      return empty;
    },
    querySelector: function (sel) {
      if (sel === 'canvas' || sel === '#canvas') return getMainCanvas();
      return null;
    },
    addEventListener: function () {},
    removeEventListener: function () {},
    body: {
      appendChild: function () {},
      removeChild: function () {},
      style: {},
      addEventListener: function () {},
      removeEventListener: function () {},
    },
    documentElement: { style: {}, addEventListener: function () {}, removeEventListener: function () {} },
    hidden: false,
    visibilityState: 'visible',
    currentScript: null,
    location: { href: '', pathname: '', origin: '', search: '' },
    title: '',
  };
  safeDefineGlobal('document', _document);
  // 暴露完整 polyfill 对象本身：新版基础库的 document 是冻结对象（不可替换不可扩展），
  // convert_to_wechat.py 会把 index.js 里的 document.querySelector 重定向到这里。
  safeDefineGlobal('__wechatDocument', _document);
  // 新版开发者工具基础库自带只读 document 桩，且可能是非 globalThis 的魔法绑定：
  // safeDefineGlobal 与 globalThis 增量化都可能无效。双路径增量化
  // （bare document 引用 + globalThis.document），逐成员 try/catch，
  // 最后实测 querySelector 是否可用（该日志同时标记适配层是否跑到了模块 18）。
  (function () {
    var targets = [];
    try {
      if (typeof document !== 'undefined' && document && document !== _document) targets.push(document);
    } catch (e) {}
    if (globalThis.document && globalThis.document !== _document && targets.indexOf(globalThis.document) < 0) {
      targets.push(globalThis.document);
    }
    var keys = Object.keys(_document);
    for (var t = 0; t < targets.length; t++) {
      var doc = targets[t];
      if (typeof doc !== 'object') continue;
      for (var i = 0; i < keys.length; i++) {
        var k = keys[i];
        if (typeof doc[k] === 'undefined') {
          try { doc[k] = _document[k]; } catch (e) {}
        }
      }
      console.warn('[WeChat Adapter] native document augmented (' + keys.length + ' members checked)');
    }
    try {
      console.log('[WeChat Adapter] document ready: querySelector=' + (typeof document.querySelector) +
        ', getElementsByTagName=' + (typeof document.getElementsByTagName) +
        ', createElement=' + (typeof document.createElement));
    } catch (e) {
      console.warn('[WeChat Adapter] document check failed: ' + e);
    }
  })();

  // ============================================================
  // 模块 19: requestAnimationFrame
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

  // ============================================================
  // 模块 19b: rAF 帧计数诊断（判断主循环是否在跑、canvas 尺寸）
  // ============================================================
  (function () {
    var _raf = globalThis.requestAnimationFrame;
    if (typeof _raf !== 'function') {
      console.warn('[WeChat Diag] NO requestAnimationFrame available!');
      return;
    }
    var _ticks = 0;
    globalThis.requestAnimationFrame = function (cb) {
      _ticks++;
      if (_ticks === 1 || _ticks % 300 === 0) {
        var c = getMainCanvas();
        console.log('[WeChat Diag] rAF ticks=' + _ticks + ', canvas=' + c.width + 'x' + c.height);
      }
      return _raf(cb);
    };
  })();

  // ============================================================
  // 模块 20: 其他必要 polyfill
  // ============================================================
  if (typeof globalThis.HTMLElement === 'undefined') {
    function HTMLElement() {}
    safeDefineGlobal('HTMLElement', HTMLElement);
  }

  console.log('[WeChat Adapter] All polyfill modules loaded successfully.');
})();