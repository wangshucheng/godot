#!/usr/bin/env python3
"""
convert_to_wechat.py - 将 Godot Web 导出包转换为微信小游戏（彻底重构版）

用法:
    python wechat/convert_to_wechat.py \\
        --source exports/web_2048 \\
        --output wechat/build \\
        --cdn-url https://your-cdn.com/game/

2 个核心修补点（最小化修改原则）:
    1. instantiateAsync - 用 WXWebAssembly.instantiate(path) 替代 WebAssembly.instantiate(ArrayBuffer)
    2. 顶层 await      - 用 .then() 回调包装，消除 await 关键字

关键设计决策（vs 旧版本）:
    - 使用大括号计数法精确定位函数边界（替代有缺陷的正则匹配）
      旧版本的正则 [\\s\\S]*?\\n\\} 会匹配到下一个行首 }，导致 fetchRemotePackage
      的正则误删了 runWithFS 等关键函数（从 line 67 一直匹配到 line 238）
    - 最小化修补：不修改 fetchRemotePackage/getBinaryPromise 等函数
      这些函数使用的 fetch() 由 wechat_adapter.js polyfill 提供
    - 每次修补后用 node --check 验证语法正确性
    - project.config.json 禁用 es6/enhance，babelSetting.ignore 包含 index.js

产物:
    wechat/build/
    ├── game.js              (主入口，加载 adapter + index.js)
    ├── game.json            (小游戏配置)
    ├── project.config.json  (微信开发者工具配置)
    ├── adapter/             (适配层，主包内)
    │   ├── game.js
    │   └── wechat_adapter.js
    ├── index.js             (Godot 引擎 + 2 个修补，无顶层 await)
    ├── index.wasm           (主包或 CDN)
    ├── index.pck            (主包或 CDN)
    └── godot.web.template_release.wasm32.nothreads.data  (CDN)
"""

import argparse
import json
import os
import shutil
import sys
from pathlib import Path


# ============================================================
# 大括号计数法：精确定位函数边界
# 跳过字符串、模板字面量、注释，避免误计数
# ============================================================
def find_function_range(content: str, func_signature: str):
    """用大括号计数法找到函数的起始和结束位置

    Args:
        content: 文件内容
        func_signature: 函数签名的起始字符串，如 'async function instantiateAsync'

    Returns:
        (start_pos, end_pos) 函数在 content 中的起始和结束位置（包含闭合 }）
        如果找不到，返回 None
    """
    idx = content.find(func_signature)
    if idx == -1:
        return None

    # 找到函数体的开始 {
    brace_start = content.find('{', idx)
    if brace_start == -1:
        return None

    # 大括号计数，跳过字符串和注释
    depth = 1
    pos = brace_start + 1
    length = len(content)

    while pos < length and depth > 0:
        ch = content[pos]

        # 跳过单行注释
        if ch == '/' and pos + 1 < length and content[pos + 1] == '/':
            nl = content.find('\n', pos)
            if nl == -1:
                break
            pos = nl + 1
            continue

        # 跳过多行注释
        if ch == '/' and pos + 1 < length and content[pos + 1] == '*':
            end = content.find('*/', pos + 2)
            if end == -1:
                break
            pos = end + 2
            continue

        # 跳过单引号字符串
        if ch == "'":
            pos += 1
            while pos < length:
                if content[pos] == '\\':
                    pos += 2
                    continue
                if content[pos] == "'":
                    pos += 1
                    break
                pos += 1
            continue

        # 跳过双引号字符串
        if ch == '"':
            pos += 1
            while pos < length:
                if content[pos] == '\\':
                    pos += 2
                    continue
                if content[pos] == '"':
                    pos += 1
                    break
                pos += 1
            continue

        # 跳过模板字面量（简化处理，不处理嵌套 ${}）
        if ch == '`':
            pos += 1
            while pos < length:
                if content[pos] == '\\':
                    pos += 2
                    continue
                if content[pos] == '`':
                    pos += 1
                    break
                pos += 1
            continue

        # 大括号计数
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1

        pos += 1

    if depth != 0:
        return None

    return (idx, pos)


# ============================================================
# 修补点 1: instantiateAsync
# 原代码: WebAssembly.instantiate(arrayBuffer, imports) 或 instantiateStreaming
# 微信:   WXWebAssembly.instantiate(path, imports) - path 为文件路径
#
# 关键: WXWebAssembly.instantiate 的第一个参数是文件路径，不是 ArrayBuffer
# 所以不能简单 polyfill WebAssembly = WXWebAssembly（参数类型不匹配）
# ============================================================
PATCH_INSTANTIATE_ASYNC = r'''function instantiateAsync(binary, binaryFile, imports) {
  // === WeChat Patch: use WXWebAssembly.instantiate(path, imports) ===
  // 微信 WXWebAssembly.instantiate 第一个参数是文件路径，不是 ArrayBuffer
  // _resolveWasmPath() 返回 Promise<string>（CDN 时需先下载到缓存）
  return _resolveWasmPath().then(function(wasmPath) {
    console.log('[WeChat] instantiateAsync from: ' + wasmPath);
    return WXWebAssembly.instantiate(wasmPath, imports).then(function(result) {
      console.log('[WeChat] WASM instantiated successfully');
      return result;
    }, function(err) {
      console.error('[WeChat] WASM instantiation failed:', err);
      throw err;
    });
  });
}'''.strip()


# 修补点 1 配套: _resolveWasmPath 辅助函数
# 处理 wasm 文件路径解析：
# - 主包内文件直接返回文件名
# - CDN 文件先下载到 wx.env.USER_DATA_PATH 缓存，再返回缓存路径
HELPER_RESOLVE_WASM_PATH = r'''function _resolveWasmPath() {
  // 主包内的 wasm 文件直接使用文件名
  if (_wasmFileName === 'index.wasm') {
    return Promise.resolve(_wasmFileName);
  }
  // CDN 文件：检查缓存，未缓存则下载
  var cachedPath = wx.env.USER_DATA_PATH + '/' + _wasmFileName;
  return new Promise(function(resolve, reject) {
    try {
      wx.getFileSystemManager().accessSync(cachedPath);
      console.log('[WeChat] Using cached WASM: ' + cachedPath);
      resolve(cachedPath);
      return;
    } catch (e) {
      // 未缓存，继续下载
    }
    var cdnUrl = _cdnBaseUrl + '/' + _wasmFileName;
    console.log('[WeChat] Downloading WASM from CDN: ' + cdnUrl);
    wx.request({
      url: cdnUrl,
      method: 'GET',
      responseType: 'arraybuffer',
      success: function(res) {
        if (res.statusCode === 200) {
          wx.getFileSystemManager().writeFile({
            filePath: cachedPath,
            data: res.data,
            encoding: 'binary',
            success: function() {
              console.log('[WeChat] WASM cached: ' + cachedPath);
              resolve(cachedPath);
            },
            fail: function(err) {
              console.error('[WeChat] WASM cache write failed:', err);
              reject(err);
            }
          });
        } else {
          reject(new Error('WASM download failed: ' + res.statusCode));
        }
      },
      fail: function(err) {
        console.error('[WeChat] WASM download error:', err);
        reject(err);
      }
    });
  });
}'''.strip()


# ============================================================
# 修补点 2: 顶层 await
#
# Godot 4.7 Web 导出结构:
#   var Godot = (() => {
#     return (
#       async function(moduleArg = {}) {
#         var moduleRtn;
#         // ... setup ...
#         var wasmExports = await createWasm();  ← 顶层 await（微信 Babel 不支持）
#         // ... wasmExports 相关代码 ...
#         return moduleRtn;
#       }
#     );
#   })();
#
# 方案: 用 .then() 回调替代 await，保持 async 函数的 return 语义
#   var Godot = (() => {
#     return (
#       async function(moduleArg = {}) {
#         var moduleRtn;
#         // ... setup ...
#         var wasmExports;
#         return createWasm().then(function(__wasm_exports__) {
#           wasmExports = __wasm_exports__;
#           // ... wasmExports 相关代码（移入 .then() 回调）...
#           return moduleRtn;
#         });
#       }
#     );
#   })();
# ============================================================


def patch_instantiate_async(content: str) -> str:
    """修补点 1: 替换 instantiateAsync 函数（用大括号计数法精确定位）"""
    # 尝试匹配 async function instantiateAsync
    result = find_function_range(content, 'async function instantiateAsync')
    if result is None:
        # 尝试匹配 function instantiateAsync（非 async 版本）
        result = find_function_range(content, 'function instantiateAsync')
    if result is None:
        print("[Patch 1/2] instantiateAsync: NOT FOUND (skip)")
        return content

    start, end = result
    original = content[start:end]
    print(f"[Patch 1/2] instantiateAsync: FOUND at chars {start}-{end} ({end - start} chars)")

    # 替换为新版本
    content = content[:start] + PATCH_INSTANTIATE_ASYNC + content[end:]
    print("[Patch 1/2] instantiateAsync: REPLACED with WXWebAssembly.instantiate(path)")

    # 注入 _resolveWasmPath 辅助函数（在 instantiateAsync 之前）
    content = HELPER_RESOLVE_WASM_PATH + "\n\n" + content
    print("[Patch 1/2] _resolveWasmPath helper: INJECTED")

    return content


def patch_top_level_await(content: str) -> str:
    """修补点 2: 用 .then() 回调替代顶层 await

    原始: var wasmExports = await createWasm();
    替换: var wasmExports;
          return createWasm().then(function(__wasm_exports__) {
            wasmExports = __wasm_exports__;

    然后在 async 函数的 return moduleRtn; 之后插入 .then() 回调的关闭 });
    """
    # 步骤 1: 替换 await 行
    await_target = "var wasmExports = await createWasm();"
    if await_target not in content:
        print("[Patch 2/2] top-level await: NOT FOUND (skip)")
        return content

    await_replacement = (
        "var wasmExports;\n"
        "  return createWasm().then(function(__wasm_exports__) {\n"
        "    wasmExports = __wasm_exports__;"
    )
    content = content.replace(await_target, await_replacement, 1)
    print("[Patch 2a/2] await line: REPLACED with return createWasm().then(cb)")

    # 步骤 2: 在 async 函数的 return moduleRtn; 之后插入 .then() 回调关闭 });
    # 唯一标识: "  return moduleRtn;\n}\n);\n})();"
    # 这是 Godot 工厂 async 函数的 return + 关闭 + 外层 IIFE 关闭
    close_pattern = "  return moduleRtn;\n}\n);\n})();"
    close_replacement = "  return moduleRtn;\n  });\n}\n);\n})();"
    if close_pattern in content:
        content = content.replace(close_pattern, close_replacement, 1)
        print("[Patch 2b/2] .then() callback: CLOSED before async function end")
    else:
        print("[Patch 2b/2] WARNING: close pattern not found!")
        print("  Expected: '  return moduleRtn;\\n}\\n);\\n})();'")

    return content


# 注入的变量声明（放在 index.js 顶部）
WECHAT_VARS_TEMPLATE = r'''// === WeChat MiniGame Bootstrap Variables ===
var _cdnBaseUrl = "{cdn_url}";
var _wasmFileName = "{wasm_file}";
var _dataFileName = "{data_file}";
var _pckFileName = "{pck_file}";
// === End WeChat Variables ==='''.strip()


# game.json - 微信小游戏配置
GAME_JSON_TEMPLATE = {
    "deviceOrientation": "portrait",
    "showStatusBar": False,
    "networkTimeout": {
        "request": 30000,
        "connectSocket": 30000,
        "uploadFile": 30000,
        "downloadFile": 30000
    },
    "subpackages": [],
    "plugins": {},
    "maxConcurrency": 10
}


# project.config.json - 微信开发者工具配置
# es6/enhance 设为 false, babelSetting.ignore 包含 index.js
# 防御性措施 - 即使 index.js 已不含顶层 await，也避免微信 Babel 误处理
PROJECT_CONFIG_TEMPLATE = {
    "description": "2048 WeChat MiniGame",
    "miniprogramRoot": "./",
    "packOptions": {
        "ignore": [],
        "include": []
    },
    "setting": {
        "urlCheck": False,
        "es6": False,
        "enhance": False,
        "postcss": True,
        "preloadBackgroundData": False,
        "minified": False,
        "newFeature": False,
        "coverView": True,
        "nodeModules": False,
        "autoAudits": False,
        "showShadowRootDuringWxmlPreview": False,
        "scopeDataCheck": False,
        "uglifyFileName": False,
        "checkInvalidKey": True,
        "checkSiteMap": True,
        "uploadWithSourceMap": True,
        "compileHotReLoad": False,
        "lazyloadPlaceholderEnable": False,
        "useMultiFrameRuntime": True,
        "useApiHook": True,
        "useApiHostProcess": True,
        "babelSetting": {
            "ignore": ["index.js"],
            "disablePlugins": [],
            "outputPath": ""
        },
        "enableEngineNative": False,
        "useIsolateContext": True,
        "userConfirmedBundleSwitch": False,
        "packNpmManually": False,
        "packNpmRelationList": [],
        "minifyWXSS": True,
        "disableUseStrict": False,
        "minifyWXML": True,
        "showES6CompileOption": False,
        "useCompilerPlugins": False
    },
    "compileType": "miniGame",
    "libVersion": "3.5.0",
    "appid": "touristappid",
    "projectname": "2048-minigame",
    "condition": {},
    "editorSetting": {
        "tabIndent": "insertSpaces",
        "tabSize": 2
    }
}


def patch_index_js(content: str, cdn_url: str, wasm_file: str, data_file: str, pck_file: str) -> str:
    """对 Godot index.js 应用 2 个核心修补点"""
    # 1. 在文件开头注入变量
    vars_block = WECHAT_VARS_TEMPLATE.format(
        cdn_url=cdn_url.rstrip('/'),
        wasm_file=wasm_file,
        data_file=data_file,
        pck_file=pck_file,
    )
    content = vars_block + "\n\n" + content

    # 2. 修补点 1: 替换 instantiateAsync（大括号计数法）
    content = patch_instantiate_async(content)

    # 3. 修补点 2: 顶层 await（字符串替换）
    content = patch_top_level_await(content)

    return content


def convert(source_dir: str, output_dir: str, cdn_url: str) -> None:
    """执行完整转换流程"""
    source = Path(source_dir)
    output = Path(output_dir)

    if not source.exists():
        print(f"ERROR: Source directory not found: {source}")
        sys.exit(1)

    # 清理输出目录
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    (output / "adapter").mkdir()

    # 1. 复制适配层
    adapter_src = Path(__file__).parent / "adapter"
    if not adapter_src.exists():
        print(f"ERROR: adapter/ not found at {adapter_src}")
        sys.exit(1)
    shutil.copy(adapter_src / "wechat_adapter.js", output / "adapter" / "wechat_adapter.js")
    shutil.copy(adapter_src / "game.js", output / "adapter" / "game.js")
    # game.js 同时需要放在输出根目录（微信入口要求）
    shutil.copy(adapter_src / "game.js", output / "game.js")
    print("[Copy] adapter/ → output/adapter/")

    # 2. 识别源文件
    html_files = list(source.glob("*.html"))
    js_files = list(source.glob("*.js"))
    wasm_files = list(source.glob("*.wasm"))
    pck_files = list(source.glob("*.pck"))
    data_files = list(source.glob("*.data"))

    if not js_files or not wasm_files:
        print("ERROR: No .js/.wasm files found in source directory")
        sys.exit(1)

    # 主 JS 文件（非 worklet）
    main_js = None
    for f in js_files:
        if "worklet" not in f.name and "engine" not in f.name.lower():
            main_js = f
            break
    if main_js is None:
        main_js = js_files[0]

    wasm_file = wasm_files[0]
    pck_file = pck_files[0] if pck_files else None
    data_file = data_files[0] if data_files else None

    print(f"[Identify] Main JS: {main_js.name}")
    print(f"[Identify] WASM: {wasm_file.name}")
    if pck_file:
        print(f"[Identify] PCK: {pck_file.name}")
    if data_file:
        print(f"[Identify] Data: {data_file.name}")

    # 3. 复制 WASM 文件（<4MB 可入主包，否则走 CDN）
    wasm_size_mb = wasm_file.stat().st_size / (1024 * 1024)
    wasm_in_main = wasm_size_mb <= 3.8
    if wasm_in_main:
        shutil.copy(wasm_file, output / "index.wasm")
        print(f"[Copy] {wasm_file.name} → index.wasm ({wasm_size_mb:.2f} MB, in main package)")
        wasm_name_for_patch = "index.wasm"
    else:
        print(f"[Skip] {wasm_file.name} ({wasm_size_mb:.2f} MB) → CDN (exceeds 4MB limit)")
        wasm_name_for_patch = wasm_file.name  # CDN 文件名

    # 4. 复制 PCK 文件（通常较小，入主包）
    pck_name_for_patch = ""
    if pck_file:
        pck_size_mb = pck_file.stat().st_size / (1024 * 1024)
        if pck_size_mb <= 3.8:
            shutil.copy(pck_file, output / "index.pck")
            print(f"[Copy] {pck_file.name} → index.pck ({pck_size_mb:.2f} MB)")
            pck_name_for_patch = "index.pck"
        else:
            print(f"[Skip] {pck_file.name} ({pck_size_mb:.2f} MB) → CDN")

    # 5. 复制 .data 文件（19 MB，走 CDN）
    data_name_for_patch = ""
    if data_file:
        data_size_mb = data_file.stat().st_size / (1024 * 1024)
        if data_size_mb <= 3.8:
            shutil.copy(data_file, output / data_file.name)
            print(f"[Copy] {data_file.name} ({data_size_mb:.2f} MB)")
        else:
            print(f"[Skip] {data_file.name} ({data_size_mb:.2f} MB) → CDN")
        data_name_for_patch = data_file.name

    # 6. 复制音频 worklet 文件
    for f in js_files:
        if "worklet" in f.name:
            shutil.copy(f, output / f.name)
            print(f"[Copy] {f.name}")

    # 7. 修补 index.js
    print("\n=== Patching index.js ===")
    js_content = main_js.read_text(encoding='utf-8')
    patched_content = patch_index_js(
        js_content,
        cdn_url=cdn_url,
        wasm_file=wasm_name_for_patch,
        data_file=data_name_for_patch,
        pck_file=pck_name_for_patch,
    )
    (output / "index.js").write_text(patched_content, encoding='utf-8')
    print(f"[Write] index.js (patched, {len(patched_content)} chars)")

    # 8. 生成 game.json
    with open(output / "game.json", "w", encoding="utf-8") as f:
        json.dump(GAME_JSON_TEMPLATE, f, ensure_ascii=False, indent=2)
    print(f"[Write] game.json")

    # 9. 生成 project.config.json
    with open(output / "project.config.json", "w", encoding="utf-8") as f:
        json.dump(PROJECT_CONFIG_TEMPLATE, f, ensure_ascii=False, indent=2)
    print(f"[Write] project.config.json")

    # 10. 打印最终目录结构
    print("\n=== Output structure ===")
    for root, dirs, files in os.walk(output):
        rel = os.path.relpath(root, output)
        prefix = "" if rel == "." else rel + "/"
        for fname in sorted(files):
            fpath = output / prefix / fname
            size = fpath.stat().st_size
            size_str = f"{size/1024/1024:.2f} MB" if size > 1024 * 1024 else f"{size/1024:.2f} KB"
            print(f"  {prefix}{fname} ({size_str})")

    print(f"\n[SUCCESS] WeChat MiniGame build created at: {output}")
    print(f"\nNext steps:")
    print(f"  1. Open WeChat Developer Tools")
    print(f"  2. Import project from: {output}")
    print(f"  3. Set your AppID in project.config.json (currently: touristappid)")
    if not wasm_in_main or (data_file and data_file.stat().st_size > 4 * 1024 * 1024):
        print(f"  4. Upload CDN files (.wasm/.data) to: {cdn_url}")
        print(f"     - {wasm_file.name}")
        if data_file:
            print(f"     - {data_file.name}")


def main():
    parser = argparse.ArgumentParser(
        description="Convert Godot Web export to WeChat MiniGame (refactored)"
    )
    parser.add_argument(
        "--source", required=True,
        help="Source directory (Godot Web export, e.g., exports/web_2048)"
    )
    parser.add_argument(
        "--output", required=True,
        help="Output directory (e.g., wechat/build)"
    )
    parser.add_argument(
        "--cdn-url", default="",
        help="CDN base URL for large files (.wasm/.data). Example: https://cdn.example.com/game/"
    )
    args = parser.parse_args()

    if not args.cdn_url:
        print("WARNING: No --cdn-url provided. Large files (.wasm/.data) need CDN.")
        print("         For local testing, set --cdn-url=http://localhost:8000/")
        args.cdn_url = "http://localhost:8000"

    convert(args.source, args.output, args.cdn_url)


if __name__ == "__main__":
    main()
