#!/usr/bin/env python3
"""
convert_to_wechat.py - 将 Godot 4.7 Web 导出包转换为微信小游戏

用法:
    python wechat/convert_to_wechat.py \\
        --source exports/web_2048 \\
        --output wechat/build \\
        --cdn-url https://your-cdn.com/game/

核心修补策略:
    1. ES2020+ 语法降级: ?. ?? ??= ||= &&= 类字段 → ES5 兼容
    2. getModuleConfig.instantiateWasm: 调用 _instantiateWasmSmart(imports)
       优先用原生 WebAssembly.instantiate(ArrayBuffer)，回退到 WXWebAssembly.instantiate(path)
    3. wechat_adapter.js 提供完整 Web API polyfill (Response/ReadableStream/Headers/fetch/Canvas等)
    4. game.js 启动 Engine 类并设置 CDN 路径

注意: Godot 4.7 使用 Engine 类 + Module['instantiateWasm'] 回调来实例化 WASM，
     Emscripten 内部的 instantiateAsync 和 createWasm 中的 await 不会在顶层执行，
     因为 instantiateWasm 回调存在时 createWasm 直接走回调路径。
     async 函数内部的 await 是合法的（ES2017），微信支持。

产物:
    wechat/build/
    ├── game.js              (主入口)
    ├── game.json            (小游戏配置)
    ├── project.config.json  (微信开发者工具配置)
    ├── adapter/
    │   ├── game.js
    │   └── wechat_adapter.js
    ├── index.js             (Godot 引擎，语法降级 + WASM 修补)
    ├── index.pck            (主包内游戏资源包)
    └── (CDN) Game2048.wasm, *.data
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
# 修补点: getModuleConfig.instantiateWasm
# 原代码: 优先使用 WebAssembly.instantiateStreaming，回退到 r.arrayBuffer()
# 微信:   WXWebAssembly 不支持 instantiateStreaming，强制使用 arrayBuffer 路径
# ============================================================

def patch_instantiate_wasm(content: str) -> str:
    """替换 Config.prototype.getModuleConfig 中的 instantiateWasm，
    强制使用 arrayBuffer 路径（避免 instantiateStreaming 在微信环境失败）。
    """
    old_code = "\t\t\t'instantiateWasm': function (imports, onSuccess) {\n\t\t\t\tfunction done(result) {\n\t\t\t\t\tonSuccess(result['instance'], result['module']);\n\t\t\t\t}\n\t\t\t\tif (typeof (WebAssembly.instantiateStreaming) !== 'undefined') {\n\t\t\t\t\tWebAssembly.instantiateStreaming(Promise.resolve(r), imports).then(done);\n\t\t\t\t} else {\n\t\t\t\t\tr.arrayBuffer().then(function (buffer) {\n\t\t\t\t\t\tWebAssembly.instantiate(buffer, imports).then(done);\n\t\t\t\t\t});\n\t\t\t\t}\n\t\t\t\tr = null;\n\t\t\t\treturn {};\n\t\t\t},"

    if old_code not in content:
        print("[Patch WASM] getModuleConfig.instantiateWasm: NOT FOUND (skip - may use different indentation)")
        return content

    new_code = "\t\t\t'instantiateWasm': function (imports, onSuccess) {\n\t\t\t\tfunction done(result) {\n\t\t\t\t\tonSuccess(result['instance'], result['module']);\n\t\t\t\t}\n\t\t\t\t// === WeChat Patch: smart WASM instantiation ===\n\t\t\t\t// _instantiateWasmSmart() 由 wechat_adapter.js 提供，自动选择:\n\t\t\t\t//   1. 原生 WebAssembly.instantiate(ArrayBuffer, imports) —— 标准 API\n\t\t\t\t//   2. WXWebAssembly.instantiate(path, imports) —— 文件路径方式\n\t\t\t\tconsole.log('[WeChat] instantiateWasm callback invoked');\n\t\t\t\t_instantiateWasmSmart(imports).then(done, function (err) {\n\t\t\t\t\tconsole.error('[WeChat] WASM instantiation failed:', err);\n\t\t\t\t\tif (err && err.stack) console.error(err.stack);\n\t\t\t\t});\n\t\t\t\tr = null;\n\t\t\t\treturn {};\n\t\t\t},"

    content = content.replace(old_code, new_code, 1)
    print("[Patch WASM] getModuleConfig.instantiateWasm: _instantiateWasmSmart (native WA + ArrayBuffer, fallback WXWebAssembly)")
    return content


# 注入的变量声明（放在 index.js 顶部）
WECHAT_VARS_TEMPLATE = r'''// === WeChat MiniGame Bootstrap Variables ===
// 使用 globalThis 而非 var，确保跨模块（index.js -> game.js）可访问
globalThis._cdnBaseUrl = {cdn_url_json};
globalThis._wasmFileName = "{wasm_file}";
globalThis._dataFileName = "{data_file}";
globalThis._pckFileName = "{pck_file}";
globalThis._executableName = "{executable}";
globalThis._fileSizes = {file_sizes};
globalThis._pckEmbedded = {pck_embedded};
globalThis._wasmSubpkg = "{wasm_subpkg}";      // F4: 承载 .wasm.br 的分包名（空字符串表示无分包）
globalThis._wasmBrInSubpkg = {wasm_br_in_subpkg};  // F4: 分包内是否有 .wasm.br
globalThis._dataSubpkg = "{data_subpkg}";      // F5: 承载 .data 的分包名（空字符串表示无分包）
globalThis._dataInSubpkg = {data_in_subpkg};   // F5: 分包内是否有 .data
globalThis._dataBinName = "{data_bin_name}";   // F5: 分包内 .dat 文件名（.data 改扩展名以绕过 DevTools .data permission + .bin atob bug）
// === End WeChat Variables ==='''.strip()


# game.json - 微信小游戏配置
# subpackages 在 convert() 中动态追加（用于承载超 4MB 的 .wasm.br）
# 注意: 必须有 game.json (而非 app.json) 才会被识别为小游戏
GAME_JSON_TEMPLATE = {
    "deviceOrientation": "portrait",
    "showStatusBar": False,
    "networkTimeout": {
        "request": 30000,
        "connectSocket": 30000,
        "uploadFile": 30000,
        "downloadFile": 30000
    },
    "subpackages": []
}

# F4 修复: .wasm.br 走分包方案（WXWebAssembly.instantiate 只接受包内路径，
# 拒绝 wxfile:/http:; 主包限 4MB，故将 .wasm.br 放入 20MB 分包）
WASM_SUBPACKAGE_NAME = "wasm_pkg"
WASM_SUBPACKAGE_ROOT = "wasm_pkg/"

# F5 修复: .data 走分包方案（避免依赖外部 CDN server，让小游戏完全自包含）
# .data 通常 19MB 左右，分包限 20MB，刚好能放下
DATA_SUBPACKAGE_NAME = "data_pkg"
DATA_SUBPACKAGE_ROOT = "data_pkg/"


# project.config.json - 微信开发者工具配置
# 注意: 不包含 miniprogramRoot 字段（该字段名含 "miniprogram" 可能干扰项目类型识别）
# 极简配置，仅保留必需字段，让 DevTools 根据 compileType + game.json 自动识别
PROJECT_CONFIG_TEMPLATE = {
    "description": "2048 WeChat MiniGame",
    "compileType": "game",
    "libVersion": "3.17.0",
    "appid": "wxc07c26935264a5e5",
    "projectname": "2048-minigame",
    "setting": {
        "urlCheck": False,
        "es6": False,
        "enhance": False,
        "postcss": False,
        "minified": False,
        "babelSetting": {
            "ignore": ["index.js"],
            "disablePlugins": [],
            "outputPath": ""
        }
    }
}


def patch_index_js(content: str, cdn_url: str, wasm_file: str, data_file: str, pck_file: str, file_sizes: dict, executable: str, pck_embedded: bool, wasm_subpkg: str = "", wasm_br_in_subpkg: bool = False, data_subpkg: str = "", data_in_subpkg: bool = False, data_bin_name: str = "") -> str:
    """对 Godot index.js 应用核心修补点"""
    # 1. 在文件开头注入变量
    vars_block = WECHAT_VARS_TEMPLATE.format(
        # S6 加固: cdn_url 是用户输入，用 json.dumps 转义后再进 JS 字符串字面量，
        # 防止引号/反斜杠/特殊字符破坏 index.js 语法（原直接 format 进引号内）
        cdn_url_json=json.dumps(cdn_url.rstrip('/')),
        wasm_file=wasm_file,
        data_file=data_file,
        pck_file=pck_file,
        executable=executable,
        file_sizes=json.dumps(file_sizes),
        pck_embedded="true" if pck_embedded else "false",
        wasm_subpkg=wasm_subpkg,
        wasm_br_in_subpkg="true" if wasm_br_in_subpkg else "false",
        data_subpkg=data_subpkg,
        data_in_subpkg="true" if data_in_subpkg else "false",
        data_bin_name=data_bin_name,
    )
    content = vars_block + "\n\n" + content

    # 2. 修补 getModuleConfig.instantiateWasm（强制 arrayBuffer 路径）
    content = patch_instantiate_wasm(content)

    # 2.5 修补 document.querySelector（新版开发者工具基础库的 document 是冻结对象，
    # 没有 querySelector → emscripten findEventTarget 在创建 WebGL 上下文时崩溃。
    # 重定向到适配层暴露的完整 polyfill 对象 __wechatDocument）
    content = patch_document_queryselector(content)

    # 2.6 修补 window.alert/prompt（微信无对话框 API，window 是冻结对象没有这些方法；
    # 重定向到适配层定义的全局桩 alert()/prompt()）
    content = patch_window_dialogs(content)

    # 2.7 修补 WebGL2 探测（godot_js_display_has_webgl 用 document.createElement 新建
    # 离屏 canvas 探测 webgl2；微信里新建 canvas 不支持 webgl2（只有主 canvas 支持），
    # 改走适配层 polyfill 的 createElement（返回主 canvas），否则引擎误报
    # "browser seems not to support WebGL 2" 并退到 RasterizerDummy 不渲染）
    content = patch_webgl_probe(content)

    # 2.8 禁用 IME 初始化（微信无真实 DOM，GodotIME.init 往 document.body.appendChild
    # 必崩；参考项目 inject_wechat_extensions.py 同样直接禁用 _godot_js_set_ime_cb）
    content = patch_ime_init(content)

    # 2.9 禁用 AudioWorklet（微信音频上下文无 ctx.audioWorklet，addModule 必崩；
    # 让 _godot_audio_has_worklet 返回 0，引擎退回 ScriptProcessor 路径——
    # 参考项目 inject_wechat_extensions.py 同款处理）
    content = patch_audio_worklet(content)

    # 2.10 包裹 audioWorklet.addModule（JS 侧 GodotAudio.init 无条件调 addModule，
    # ctx.audioWorklet 为 undefined 必抛；守卫后返回永不 resolve 的 Promise，
    # worklet 永不启动，配合 2.9 走 ScriptProcessor 路径）
    content = patch_audio_worklet_module(content)

    # 2.11 禁用窗口图标设置（微信无 document.getElementById/head/Blob/createObjectURL；
    # 参考项目 inject_wechat_extensions.py 同款禁用）
    content = patch_window_icon(content)

    # 2.12 minified 类字段降级（emcc 压缩输出的 class X{f=v;...} 无换行无缩进，
    # 老的按行匹配抓不到 → 预览通道旧 JS 引擎报 SyntaxError: Unexpected token =）
    content = patch_minified_class_fields(content)

    # 3. ES2020+ 语法降级: ?. ?? ??= ||= &&= 类字段
    content = patch_optional_chaining(content)

    # S6 加固: 注入结果 fail-fast 校验（S6 的教训是"静默失效"，不是模式本身）。
    # 变量块必须存在于产物中；显式传入 CDN URL 时，其转义后的值必须真的写进去了。
    if 'globalThis._cdnBaseUrl = ' not in content:
        raise RuntimeError("[S6] CDN 变量注入失败：产物 index.js 缺少 _cdnBaseUrl")
    if cdn_url and json.dumps(cdn_url.rstrip('/')) not in content:
        raise RuntimeError("[S6] CDN URL 未正确写入产物 index.js: " + cdn_url)

    return content


# ============================================================
# 修补点 2.5: document.querySelector 重定向
# 新版微信开发者工具基础库的 document 是冻结对象（无 querySelector），
# 且不可替换/不可扩展 → emscripten findEventTarget 创建 WebGL 上下文时崩溃。
# 适配层暴露了完整 polyfill 对象 globalThis.__wechatDocument，重定向过去。
# ============================================================

def patch_document_queryselector(content: str) -> str:
    old = "document.querySelector"
    count = content.count(old)
    if count == 0:
        # 锚点缺失必须响（S6 教训：静默失效比失败更糟）
        print("[Patch 2.5] WARNING: document.querySelector NOT FOUND in index.js (engine glue changed?)")
        return content
    content = content.replace(old, "(globalThis.__wechatDocument||document).querySelector")
    print(f"[Patch 2.5] document.querySelector -> __wechatDocument ({count} occurrence(s))")
    return content


# ============================================================
# 修补点 2.6: window.alert/prompt 重定向
# 微信无对话框 API，window 是冻结对象（window.alert is not a function 崩溃）。
# 适配层已定义全局桩 alert()/prompt()，把 window.* 引用改指过去。
# ============================================================

def patch_window_dialogs(content: str) -> str:
    for old, new in [("window.alert", "globalThis.alert"), ("window.prompt", "globalThis.prompt")]:
        count = content.count(old)
        if count == 0:
            print(f"[Patch 2.6] NOTE: {old} not found (0 occurrences, skip)")
            continue
        content = content.replace(old, new)
        print(f"[Patch 2.6] {old} -> {new} ({count} occurrence(s))")
    return content


# ============================================================
# 修补点 2.7: WebGL2 探测改走主 canvas
# ============================================================

def patch_webgl_probe(content: str) -> str:
    old = "document.createElement('canvas').getContext("
    count = content.count(old)
    if count == 0:
        print("[Patch 2.7] WARNING: webgl probe anchor NOT FOUND (engine glue changed?)")
        return content
    content = content.replace(old, "(globalThis.__wechatDocument||document).createElement('canvas').getContext(")
    print(f"[Patch 2.7] webgl probe -> __wechatDocument.createElement (main canvas) ({count} occurrence(s))")
    return content


# ============================================================
# 修补点 2.8: 禁用 IME 初始化
# ============================================================

def patch_ime_init(content: str) -> str:
    old = "GodotIME.init(ime_cb,key_cb,code,key)"
    count = content.count(old)
    if count == 0:
        print("[Patch 2.8] WARNING: GodotIME.init anchor NOT FOUND (engine glue changed?)")
        return content
    content = content.replace(old, "0")
    print(f"[Patch 2.8] GodotIME.init disabled ({count} occurrence(s))")
    return content


# ============================================================
# 修补点 2.9: 禁用 AudioWorklet（has_worklet 返回 0）
# ============================================================

def patch_audio_worklet(content: str) -> str:
    old = "function _godot_audio_has_worklet(){return GodotAudio.ctx&&GodotAudio.ctx.audioWorklet?1:0}"
    count = content.count(old)
    if count == 0:
        print("[Patch 2.9] WARNING: _godot_audio_has_worklet anchor NOT FOUND (engine glue changed?)")
        return content
    content = content.replace(old, "function _godot_audio_has_worklet(){return 0}")
    print(f"[Patch 2.9] _godot_audio_has_worklet -> 0 (ScriptProcessor fallback) ({count} occurrence(s))")
    return content


# ============================================================
# 修补点 2.10: audioWorklet.addModule 守卫
# ============================================================

def patch_audio_worklet_module(content: str) -> str:
    # 注意顺序：先长串（主 worklet），再带等号的短串（position worklet），
    # 避免短串命中长串子串造成 GodotAudio.(...) 语法破坏。
    pairs = [
        ("GodotAudio.ctx.audioWorklet.addModule(path)",
         "(GodotAudio.ctx.audioWorklet?GodotAudio.ctx.audioWorklet.addModule(path):new Promise(function(){}))"),
        ("=ctx.audioWorklet.addModule(path);",
         "=(ctx.audioWorklet?ctx.audioWorklet.addModule(path):new Promise(function(){}));"),
    ]
    for old, new in pairs:
        count = content.count(old)
        if count == 0:
            print(f"[Patch 2.10] NOTE: '{old[:40]}' not found (skip)")
            continue
        content = content.replace(old, new)
        print(f"[Patch 2.10] guarded '{old[:40]}' ({count} occurrence(s))")
    return content


# ============================================================
# 修补点 2.11: 禁用窗口图标设置
# ============================================================

def patch_window_icon(content: str) -> str:
    old = "function _godot_js_display_window_icon_set(p_ptr,p_len){"
    count = content.count(old)
    if count == 0:
        print("[Patch 2.11] WARNING: window_icon_set anchor NOT FOUND (engine glue changed?)")
        return content
    content = content.replace(old, "function _godot_js_display_window_icon_set(p_ptr,p_len){return;")
    print(f"[Patch 2.11] _godot_js_display_window_icon_set disabled ({count} occurrence(s))")
    return content


# ============================================================
# 修补点 2.12: minified 类字段降级
# emcc 压缩输出的类字段（class ExitStatus{name="ExitStatus";constructor...）
# 紧跟在 { 后、无换行无缩进，老的按行匹配（patch_class_fields）抓不到。
# 这里处理 minified 形态：抽出字段值，移入 constructor（无 constructor 且无
# extends 时合成一个；extends 类不动，避免缺 super() 调用）。
# ============================================================

def patch_minified_class_fields(content: str) -> str:
    import re
    # 只匹配无 extends 的类（合成 constructor 需要 super() 的情况安全起见不处理）
    pat = re.compile(r'(class\s*\w*\s*)\{((?:(?:\w+)=(?:\{\}|\[\]|[^;{}()]+);)+)')
    out = []
    last = 0
    total = 0
    for m in pat.finditer(content):
        head, fields_src = m.group(1), m.group(2)
        fields = re.findall(r'(\w+)=(\{\}|\[\]|[^;{}()]+);', fields_src)
        assigns = ''.join('this.%s=%s;' % (f, v) for f, v in fields)
        rest = content[m.end():]
        ctor = re.match(r'constructor(\([^)]*\))\{', rest)
        out.append(content[last:m.start()])
        if ctor:
            out.append(head + '{constructor' + ctor.group(1) + '{' + assigns)
            last = m.end() + ctor.end()
        else:
            out.append(head + '{constructor(){' + assigns + '}')
            last = m.end()
        total += len(fields)
    out.append(content[last:])
    print(f"[Patch 2.12] minified class fields: moved {total} field(s) into constructor")
    return ''.join(out)


def patch_optional_chaining(content: str) -> str:
    """修补点 3: 替换可选链操作符 ?. 为 ES5 兼容写法

    微信小游戏的 JS 引擎（基于 JavaScriptCore/V8）不支持 ES2020 的可选链操作符 `?.`，
    会报 "SyntaxError: Unexpected token ." 错误。

    替换规则:
        a?.b        → (a != null && a.b)
        a?.b?.c     → (a != null && a.b != null && a.b.c)
        a?.()       → (a != null && a())
        a?.b()      → (a != null && a.b())
        a?.[expr]   → (a != null && a[expr])

    用 `!= null` 而非 `&&` 链式短路，因为 `0 && 0.b` 返回 0 而非 undefined。
    """
    import re

    original_count = content.count('?.')
    if original_count == 0:
        print("[Patch 3/3] optional chaining: NOT FOUND (skip)")
        return content

    # 策略: 从左到右逐个匹配 "前缀?.后缀" 模式
    # 前缀: 标识符链，如 globalThis.document, Module['onAbort'], HEAP8
    # 后缀: 属性名(.prop)、计算属性([expr])、或函数调用(())
    #
    # 核心正则: 匹配最短的 "标识符链?.xxx" 并替换为 "(标识符链 != null && 标识符链.xxx)"
    # 反复执行直到没有 ?. 为止

    # 匹配前缀表达式: 标识符链 或 函数调用 或 数组索引 或 括号表达式
    # 标识符链: word.word.word 或 word['key'] 或 word["key"] 或 word[0] 或 word[varName]
    # 函数调用: word(args) 或 word.word(args) — 参数中无嵌套括号
    # 括号表达式: (...) — 用于已替换的中间结果
    IDENT_CHAIN = r"(?:[\w$]+(?:\.[\w$]+|\[['\"][\w$]+['\"]\]|\[\d+\]|\[[\w$]+\])*(?:\([^()]*\))?|\([^()]*\))"

    # 模式 1: a?.prop (属性访问，后跟可选的函数调用)
    # 例如: globalThis.document?.currentScript 或 a?.method(args)
    # 需要特殊处理 a?.method(args) → (a != null && a.method(args))
    # 而不是先替换 a?.method → (a != null && a.method) 再被误匹配为函数调用
    pattern_prop = re.compile(r'(' + IDENT_CHAIN + r')\?\.([\w$]+)')

    # 模式 2: a?.(args) (函数调用)
    # 例如: Module['onAbort']?.(what) 或 SOCKFS.callbacks[event]?.(param)
    # 由于参数中可能嵌套括号，用平衡括号计数法匹配完整的 (args)
    pattern_call = re.compile(r'(' + IDENT_CHAIN + r')\?\.\(')

    # 模式 3: a?.[expr] (计算属性访问)
    # 例如: obj?.['key'] 或 obj?.[0]
    # 由于 expr 中可能嵌套方括号，同样用平衡计数法
    pattern_index = re.compile(r'(' + IDENT_CHAIN + r')\?\.\[')

    def find_matching_paren(s, start):
        """从 start 位置的 '(' 开始，找到匹配的 ')' 位置。返回 ')' 的索引，未找到返回 -1。"""
        depth = 0
        i = start
        while i < len(s):
            if s[i] == '(':
                depth += 1
            elif s[i] == ')':
                depth -= 1
                if depth == 0:
                    return i
            elif s[i] in '"\'':
                # 跳过字符串
                quote = s[i]
                i += 1
                while i < len(s) and s[i] != quote:
                    if s[i] == '\\':
                        i += 1
                    i += 1
            i += 1
        return -1

    def find_matching_bracket(s, start):
        """从 start 位置的 '[' 开始，找到匹配的 ']' 位置。"""
        depth = 0
        i = start
        while i < len(s):
            if s[i] == '[':
                depth += 1
            elif s[i] == ']':
                depth -= 1
                if depth == 0:
                    return i
            elif s[i] in '"\'':
                quote = s[i]
                i += 1
                while i < len(s) and s[i] != quote:
                    if s[i] == '\\':
                        i += 1
                    i += 1
            i += 1
        return -1

    # 反复替换直到没有 ?. 为止（处理链式调用 a?.b?.c）
    max_iterations = 200
    iteration = 0
    while '?.' in content and iteration < max_iterations:
        iteration += 1

        # 先处理函数调用 a?.(args)  →  (a != null && a(args))
        # 用平衡括号计数法找到完整的参数列表
        m = pattern_call.search(content)
        if m:
            prefix = m.group(1)
            paren_start = m.end() - 1  # 指向 '('
            paren_end = find_matching_paren(content, paren_start)
            if paren_end > paren_start:
                args = content[paren_start + 1:paren_end]
                old = content[m.start():paren_end + 1]
                new = f'({prefix} != null && {prefix}({args}))'
                content = content.replace(old, new, 1)
                continue

        # 处理计算属性 a?.[expr]  →  (a != null && a[expr])
        m = pattern_index.search(content)
        if m:
            prefix = m.group(1)
            bracket_start = m.end() - 1  # 指向 '['
            bracket_end = find_matching_bracket(content, bracket_start)
            if bracket_end > bracket_start:
                expr = content[bracket_start + 1:bracket_end]
                old = content[m.start():bracket_end + 1]
                new = f'({prefix} != null && {prefix}[{expr}])'
                content = content.replace(old, new, 1)
                continue

        # 处理属性访问 a?.prop  →  (a != null && a.prop)
        # 如果后面紧跟 (args)，则一起替换: a?.prop(args) → (a != null && a.prop(args))
        m = pattern_prop.search(content)
        if m:
            prefix = m.group(1)
            prop = m.group(2)
            match_end = m.end()
            # 检查后面是否紧跟 (args)
            if match_end < len(content) and content[match_end] == '(':
                paren_end = find_matching_paren(content, match_end)
                if paren_end > match_end:
                    args = content[match_end + 1:paren_end]
                    old = content[m.start():paren_end + 1]
                    new = f'({prefix} != null && {prefix}.{prop}({args}))'
                    content = content.replace(old, new, 1)
                    continue
            # 普通属性访问
            old = content[m.start():m.end()]
            new = f'({prefix} != null && {prefix}.{prop})'
            content = content.replace(old, new, 1)
            continue

        # 如果三种模式都没匹配，但还有 ?.,可能是复杂表达式，手动跳过
        break

    # 处理空值合并运算符 ?? (ES2020)
    # 微信小游戏引擎不支持 ??，会报 "SyntaxError: Unexpected token ?"
    # a ?? b  →  (a != null ? a : b)
    # 用扫描器方法处理任意复杂度的左右操作数（函数调用、数组访问、括号表达式等）
    # 因为 ?? 不能与 || 和 && 混用（除非加括号），所以操作数边界很清晰：
    #   左边界: , ; = ? : ( [ { && || return/typeof/new 等关键字
    #   右边界: , ; ) ] } ? : && ||
    def _find_nullish_left_start(text, q_pos):
        """从 ?? 位置向前扫描，找到左操作数的起始位置。"""
        i = q_pos - 1
        # 跳过空白
        while i >= 0 and text[i] in ' \t\n\r':
            i -= 1
        if i < 0:
            return 0

        depth = 0
        while i >= 0:
            ch = text[i]
            # 跳过字符串（反向扫描）
            if ch in '"\'`':
                quote = ch
                i -= 1
                while i >= 0:
                    if text[i] == '\\':
                        i -= 1
                        continue
                    if text[i] == quote:
                        i -= 1
                        break
                    i -= 1
                continue
            if ch in ')]}':
                depth += 1
                i -= 1
                continue
            if ch in '([{':
                if depth > 0:
                    depth -= 1
                    i -= 1
                    continue
                # depth == 0: 这是未匹配的开括号（外层表达式的边界）
                # 例如 foo(a ?? b) 中的 (  —— ?? 在函数参数内，左操作数是 a，不是 foo(a
                # 对于 foo() ?? b，foo() 的 () 已被匹配（depth 先 +1 后 -1），不会到这里
                break
            if depth > 0:
                i -= 1
                continue
            # depth == 0
            if ch in ',;=:?':
                break
            if ch == '|' and i > 0 and text[i - 1] == '|':
                break
            if ch == '&' and i > 0 and text[i - 1] == '&':
                break
            if ch == '!':
                break
            # 标识符字符、点号、数字: 继续
            if ch.isalnum() or ch in '_$.':
                i -= 1
                continue
            break

        start = i + 1
        while start < q_pos and text[start] in ' \t\n\r':
            start += 1
        return start

    def _find_nullish_right_end(text, q_pos):
        """从 ?? 后面扫描，找到右操作数的结束位置。"""
        length = len(text)
        i = q_pos + 2  # 跳过 ??
        while i < length and text[i] in ' \t\n\r':
            i += 1
        if i >= length:
            return length

        depth = 0
        while i < length:
            ch = text[i]
            # 跳过字符串
            if ch in '"\'`':
                quote = ch
                i += 1
                while i < length:
                    if text[i] == '\\':
                        i += 2
                        continue
                    if text[i] == quote:
                        i += 1
                        break
                    i += 1
                continue
            # 跳过行内注释
            if ch == '/' and i + 1 < length and text[i + 1] == '/':
                nl = text.find('\n', i)
                if nl == -1:
                    i = length
                else:
                    i = nl + 1
                continue
            if ch in '([{':
                depth += 1
                i += 1
                continue
            if ch in ')]}':
                if depth > 0:
                    depth -= 1
                    i += 1
                    continue
                break
            if depth > 0:
                i += 1
                continue
            # depth == 0
            if ch in ',;:?':
                break
            if ch == '|' and i + 1 < length and text[i + 1] == '|':
                break
            if ch == '&' and i + 1 < length and text[i + 1] == '&':
                break
            # 标识符字符、点号、数字: 继续
            if ch.isalnum() or ch in '_$.':
                i += 1
                continue
            # 其他字符（如 [, {, !, - 等）: 尝试继续（可能是 [] 或 {} 字面量）
            if ch in '!-+~':
                i += 1
                continue
            break
        return i

    # 扫描所有 ?? 位置（排除 ??= 和 ?.）
    nullish_positions = []
    i = 0
    while i < len(content):
        if (content[i] == '?' and i + 1 < len(content) and content[i + 1] == '?'
                and not (i + 2 < len(content) and content[i + 2] == '=')
                and not (i > 0 and content[i - 1] == '.')):
            nullish_positions.append(i)
            i += 2
        else:
            i += 1

    nullish_count = len(nullish_positions)
    if nullish_count > 0:
        # 从右到左处理，避免位置偏移
        replaced = 0
        for pos in reversed(nullish_positions):
            left_start = _find_nullish_left_start(content, pos)
            right_end = _find_nullish_right_end(content, pos)
            left = content[left_start:pos].strip()
            right = content[pos + 2:right_end].strip()
            if not left or not right:
                continue
            replacement = f'({left} != null ? {left} : {right})'
            content = content[:left_start] + replacement + content[right_end:]
            replaced += 1
        print(f"[Patch 3/3] nullish coalescing ?? : REPLACED {replaced}/{nullish_count} occurrences (scanner-based)")

    # 处理逻辑赋值运算符 (ES2021): ??=, ||=, &&=
    # 这些运算符微信小游戏引擎不支持，会报 "SyntaxError: Unexpected token ?"
    # a ??= b   →  a = a != null ? a : b
    # a ||= b   →  a = a || b
    # a &&= b   →  a = a && b
    # 不加外层括号，避免破坏语句上下文
    LOGICAL_ASSIGN_TARGET = r"(?:[\w$]+(?:\.[\w$]+|\[['\"][\w$]+['\"]\]|\[\d+\]|\[[\w$]+\])*|[\w$]+\([^()]*\))"
    pattern_nullish_assign = re.compile(r'(' + LOGICAL_ASSIGN_TARGET + r')\s*\?\?=\s*')
    new_content = pattern_nullish_assign.sub(r'\1 = \1 != null ? \1 : ', content)
    if new_content != content:
        count = len(re.findall(r'\?\?=', content))
        content = new_content
        print(f"[Patch 3/3] logical assign ??= : REPLACED {count} occurrences")

    pattern_or_assign = re.compile(r'(' + LOGICAL_ASSIGN_TARGET + r')\s*\|\|=\s*')
    new_content = pattern_or_assign.sub(r'\1 = \1 || ', content)
    if new_content != content:
        count = len(re.findall(r'\|\|=', content))
        content = new_content
        print(f"[Patch 3/3] logical assign ||= : REPLACED {count} occurrences")

    pattern_and_assign = re.compile(r'(' + LOGICAL_ASSIGN_TARGET + r')\s*&&=\s*')
    new_content = pattern_and_assign.sub(r'\1 = \1 && ', content)
    if new_content != content:
        count = len(re.findall(r'&&=', content))
        content = new_content
        print(f"[Patch 3/3] logical assign &&= : REPLACED {count} occurrences")

    # 处理类字段声明 (ES2022): 在类体内直接 fieldName = value;
    # 微信小游戏引擎不支持类字段语法，会报 "SyntaxError: Unexpected token ="
    # 转换: 删除类字段行，在 constructor 开头插入 this.fieldName = value;
    # 如果类没有 constructor，则添加一个空 constructor 并放入字段初始化
    def patch_class_fields(text):
        """遍历所有 class 定义，将类字段移到 constructor 中。

        关键: 只处理类体顶层的字段声明（depth==1），不处理方法体内的赋值语句。
        例如 cacheLength() 方法内的 `end = Math.min(...)` 不是类字段，不能移动。
        通过逐行大括号深度计数实现精确识别。
        """

        def count_braces_in_line(line):
            """统计一行中 { 和 } 的净变化，跳过字符串和注释。"""
            depth = 0
            i = 0
            length = len(line)
            while i < length:
                ch = line[i]
                # 遇到行内注释，剩余部分忽略
                if ch == '/' and i + 1 < length and line[i + 1] == '/':
                    break
                # 跳过单引号字符串
                if ch == "'":
                    i += 1
                    while i < length:
                        if line[i] == '\\':
                            i += 2
                            continue
                        if line[i] == "'":
                            i += 1
                            break
                        i += 1
                    continue
                # 跳过双引号字符串
                if ch == '"':
                    i += 1
                    while i < length:
                        if line[i] == '\\':
                            i += 2
                            continue
                        if line[i] == '"':
                            i += 1
                            break
                        i += 1
                    continue
                # 跳过模板字面量（简化处理，不处理嵌套 ${}）
                if ch == '`':
                    i += 1
                    while i < length:
                        if line[i] == '\\':
                            i += 2
                            continue
                        if line[i] == '`':
                            i += 1
                            break
                        i += 1
                    continue
                # 计数大括号
                if ch == '{':
                    depth += 1
                elif ch == '}':
                    depth -= 1
                i += 1
            return depth

        def find_class_body_end(text, start):
            """从 class body 的 { 后面开始，找到匹配的 } 的位置（返回 } 后一个字符的索引）。
            跳过字符串、注释、模板字面量，避免误计数。
            """
            depth = 1
            i = start
            length = len(text)
            while i < length and depth > 0:
                ch = text[i]
                # 跳过单行注释
                if ch == '/' and i + 1 < length and text[i + 1] == '/':
                    nl = text.find('\n', i)
                    if nl == -1:
                        break
                    i = nl + 1
                    continue
                # 跳过多行注释
                if ch == '/' and i + 1 < length and text[i + 1] == '*':
                    end = text.find('*/', i + 2)
                    if end == -1:
                        break
                    i = end + 2
                    continue
                # 跳过单引号字符串
                if ch == "'":
                    i += 1
                    while i < length:
                        if text[i] == '\\':
                            i += 2
                            continue
                        if text[i] == "'":
                            i += 1
                            break
                        i += 1
                    continue
                # 跳过双引号字符串
                if ch == '"':
                    i += 1
                    while i < length:
                        if text[i] == '\\':
                            i += 2
                            continue
                        if text[i] == '"':
                            i += 1
                            break
                        i += 1
                    continue
                # 跳过模板字面量
                if ch == '`':
                    i += 1
                    while i < length:
                        if text[i] == '\\':
                            i += 2
                            continue
                        if text[i] == '`':
                            i += 1
                            break
                        i += 1
                    continue
                # 大括号计数
                if ch == '{':
                    depth += 1
                elif ch == '}':
                    depth -= 1
                i += 1
            return i  # 指向 } 的后面

        result = []
        i = 0
        while i < len(text):
            # 查找 class 关键字（支持匿名类表达式: class extends XXX {）
            class_match = re.match(r'\bclass\s*(\w+)?\s*(?:extends\s+[^{]+)?\{', text[i:])
            if class_match:
                class_start = i + class_match.start()
                body_start = i + class_match.end()
                body_end_pos = find_class_body_end(text, body_start)
                body_end = body_end_pos - 1  # 指向最后的 }
                body = text[body_start:body_end]

                # 遍历类体行，仅处理 depth==1 的行（类体顶层，不在方法内）
                fields = []
                lines = body.split('\n')
                new_lines = []
                current_depth = 1  # 进入类体，深度为 1
                for line in lines:
                    line_brace_change = count_braces_in_line(line)
                    # 仅当当前行处于类体顶层（depth==1）时才尝试匹配类字段
                    # 这避免误匹配方法体内的赋值语句（如 end = Math.min(...); ）
                    if current_depth == 1:
                        # 匹配类字段: 缩进的 标识符 = 值;
                        # 值部分用非贪婪 .+? 允许包含分号（如数组、字符串内分号），
                        # 行尾允许可选的行内注释 // ...（否则 chunks = []; // comment 会匹配失败）
                        m = re.match(r'^(\s+)(\w+)\s*=\s*(.+?);\s*(?://.*)?$', line)
                        if m and m.group(2) not in ('constructor', 'get', 'set', 'async'):
                            # 不是方法定义 (标识符后面不是 ()
                            if not re.match(r'^\s+\w+\s*\(', line):
                                indent = m.group(1)
                                field_name = m.group(2)
                                field_value = m.group(3).strip()
                                fields.append((indent, field_name, field_value))
                                current_depth += line_brace_change
                                continue  # 跳过这行（移除类字段）
                    new_lines.append(line)
                    current_depth += line_brace_change
                new_body = '\n'.join(new_lines)

                if fields:
                    # 检查是否有 constructor
                    ctor_match = re.search(r'^(\s+)constructor\s*\(([^)]*)\)\s*\{', new_body, re.MULTILINE)
                    if ctor_match:
                        # 在 constructor 开头插入 this.fieldName = value;
                        ctor_indent = ctor_match.group(1)
                        insert_pos = ctor_match.end()
                        insert_lines = []
                        for indent, fname, fval in fields:
                            insert_lines.append(f'\n{ctor_indent}  this.{fname} = {fval};')
                        new_body = new_body[:insert_pos] + ''.join(insert_lines) + new_body[insert_pos:]
                    else:
                        # 添加新的 constructor
                        # 使用第一个字段的缩进作为 constructor 的缩进
                        ctor_indent = fields[0][0]
                        ctor_block = f'\n{ctor_indent}constructor() {{'
                        for indent, fname, fval in fields:
                            ctor_block += f'\n{ctor_indent}  this.{fname} = {fval};'
                        ctor_block += f'\n{ctor_indent}}}\n'
                        # 在类体开头插入
                        new_body = ctor_block + new_body

                    result.append(text[class_start:body_start])
                    result.append(new_body)
                    result.append('}')
                    i = body_end + 1
                    continue

                result.append(text[class_start:body_start])
                result.append(new_body)
                result.append('}')
                i = body_end + 1
                continue
            result.append(text[i])
            i += 1
        return ''.join(result)

    new_content = patch_class_fields(content)
    if new_content != content:
        # 统计替换的字段数
        old_fields = len(re.findall(r'^\s+\w+\s*=\s*.+?;\s*(?://.*)?$', content, re.MULTILINE))
        content = new_content
        print(f"[Patch 3/3] class field : REPLACED {old_fields} fields (moved to constructor)")

    remaining_count = content.count('?.')
    replaced_count = original_count - remaining_count
    print(f"[Patch 3/3] optional chaining: REPLACED {replaced_count}/{original_count} occurrences")
    if remaining_count > 0:
        print(f"[Patch 3/3] WARNING: {remaining_count} ?. remain (may be in complex expressions)")

    return content


def convert(source_dir: str, output_dir: str, cdn_url: str, cdn_only: bool = True) -> str:
    """执行完整转换流程

    如果输出目录被锁定（如微信开发者工具正在使用），自动切换到带时间戳的新目录。
    返回实际使用的输出目录路径。
    """
    source = Path(source_dir)
    output = Path(output_dir)

    if not source.exists():
        print(f"ERROR: Source directory not found: {source}")
        sys.exit(1)

    # 清理输出目录
    # 如果目录被锁定无法删除，自动生成带时间戳的新目录，避免阻塞构建
    if output.exists():
        try:
            shutil.rmtree(output)
        except (PermissionError, OSError) as e:
            from datetime import datetime
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            new_output = output.parent / f"{output.name}_{timestamp}"
            print(f"[WARN] Output directory locked ({e}), using new directory: {new_output}")
            output = new_output

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
    worklet_js_files = []
    for f in js_files:
        if "worklet" in f.name:
            worklet_js_files.append(f)
        elif "engine" not in f.name.lower():
            main_js = f
    if main_js is None:
        main_js = js_files[0]

    # executable 名称 = 主 JS 文件名（不含扩展名）
    # Godot 4.7 用 executable 作为 wasm/pck 的基础名: ${executable}.wasm, ${executable}.pck
    executable_name = main_js.stem  # e.g. "Game2048"

    wasm_file = wasm_files[0]
    pck_file = pck_files[0] if pck_files else None
    data_file = data_files[0] if data_files else None

    print(f"[Identify] Main JS: {main_js.name} (executable: {executable_name})")
    print(f"[Identify] WASM: {wasm_file.name}")
    if pck_file:
        print(f"[Identify] PCK: {pck_file.name}")
    if data_file:
        print(f"[Identify] Data: {data_file.name}")
    print(f"[Identify] Worklet JS: {[f.name for f in worklet_js_files]}")

    # 文件大小映射（供 Engine preloader 进度条使用）
    file_sizes = {}

    # 3. 处理 WASM 文件
    # F4 修复: 优先使用 .wasm.br 分包方案
    #   - .wasm.br 放入分包（20MB 限制），WXWebAssembly.instantiate 自动解压
    #   - 无 .wasm.br 时: 小 .wasm 放主包，大 .wasm 走 CDN（真机 wxfile 路径）
    wasm_size = wasm_file.stat().st_size
    wasm_size_mb = wasm_size / (1024 * 1024)
    wasm_in_main = wasm_size_mb <= 3.8
    wasm_output_name = f"{executable_name}.wasm"
    file_sizes[wasm_output_name] = wasm_size

    # 查找同名 .wasm.br（Brotli 压缩，微信原生支持自动解压）
    wasm_br_file = wasm_file.with_suffix(".wasm.br")
    wasm_br_in_subpkg = False
    wasm_subpkg_name = ""

    # 防陈旧缓存：.br 缺失或比 .wasm 旧时现场重生成。
    # （参考项目踩过的坑："原始文件更新但 .br 未重生成 → 微信加载旧版二进制"。
    #  此处若静默复用旧 .br，会把过期 wasm 打进分包——曾因此微信端报
    #  CompileError: expected table index 0，实为旧 -O0 构建的 reference-types 版本。）
    import time
    need_regen_br = (not wasm_br_file.exists()) or (wasm_br_file.stat().st_mtime < wasm_file.stat().st_mtime)
    if need_regen_br:
        try:
            import brotli
        except ImportError:
            raise RuntimeError("[BR] .wasm.br 缺失或已陈旧，且 python brotli 模块不可用，无法现场重生成: " + str(wasm_br_file))
        print(f"[Subpkg] regenerating {wasm_br_file.name} from {wasm_file.name} (stale or missing)...")
        _t0 = time.time()
        wasm_br_file.write_bytes(brotli.compress(wasm_file.read_bytes(), quality=11))
        print(f"[Subpkg] regenerated in {time.time() - _t0:.1f}s ({wasm_br_file.stat().st_size / (1024 * 1024):.2f} MB)")
    else:
        print(f"[Subpkg] reuse fresh {wasm_br_file.name} (mtime >= {wasm_file.name})")

    if wasm_br_file.exists():
        br_size = wasm_br_file.stat().st_size
        br_size_mb = br_size / (1024 * 1024)
        if br_size_mb <= 19.0 and not cdn_only:  # cdn_only 模式跳过分包，大文件全走 CDN
            # 创建分包目录并复制 .wasm.br
            subpkg_dir = output / WASM_SUBPACKAGE_ROOT.rstrip("/")
            subpkg_dir.mkdir(parents=True, exist_ok=True)
            shutil.copy(wasm_br_file, subpkg_dir / f"{executable_name}.wasm.br")
            # 微信小游戏分包硬性要求：每个分包 root 下必须有 game.js 入口文件
            # 否则报错 "未找到 [subpackages][N][root] 对应的 /xxx/game.js 文件"
            # 并可能回退识别为小程序而非小游戏
            subpkg_entry = subpkg_dir / "game.js"
            subpkg_entry.write_text(
                f"// {WASM_SUBPACKAGE_NAME} subpackage entry (auto-generated)\n"
                f"// 本分包仅用于承载 {executable_name}.wasm.br，无需任何 JS 逻辑\n",
                encoding="utf-8"
            )
            wasm_br_in_subpkg = True
            wasm_subpkg_name = WASM_SUBPACKAGE_NAME
            print(f"[Subpkg] {wasm_br_file.name} -> {WASM_SUBPACKAGE_ROOT}{executable_name}.wasm.br ({br_size_mb:.2f} MB, in subpackage)")
            print(f"[Subpkg] Wrote {WASM_SUBPACKAGE_ROOT}game.js (subpackage entry stub)")
            file_sizes[f"{wasm_output_name}.br"] = br_size
        else:
            print(f"[CDN]  {wasm_br_file.name} ({br_size_mb:.2f} MB) -> CDN (exceeds 20MB subpackage limit)")

    if not wasm_br_in_subpkg:
        # 无 .wasm.br 分包，按原逻辑处理 .wasm
        if wasm_in_main:
            shutil.copy(wasm_file, output / wasm_output_name)
            print(f"[Copy] {wasm_file.name} -> {wasm_output_name} ({wasm_size_mb:.2f} MB, in main package)")
        else:
            print(f"[CDN]  {wasm_file.name} ({wasm_size_mb:.2f} MB) -> CDN (exceeds 4MB limit)")

    # 4. 处理 PCK 文件
    # 微信小游戏不允许 readFileSync 读取包内 .pck 文件（permission denied）
    # 方案：小文件转 base64 JS 模块（通过 require 加载，绕过文件系统限制）
    #       大文件走 CDN
    pck_output_name = ""
    pck_embedded = False  # 是否已嵌入为 base64 模块
    if pck_file:
        pck_size = pck_file.stat().st_size
        pck_size_mb = pck_size / (1024 * 1024)
        pck_output_name = f"{executable_name}.pck"
        file_sizes[pck_output_name] = pck_size
        if pck_size_mb <= 0.2:  # < 200KB，转 base64 JS 模块
            import base64
            pck_data = pck_file.read_bytes()
            pck_b64 = base64.b64encode(pck_data).decode('ascii')
            # 生成 JS 模块：module.exports = { base64: "...", name: "Game2048.pck" }
            pck_module = f'// Auto-generated base64 module for {pck_output_name} ({pck_size} bytes)\n'
            pck_module += f'// Do not edit - generated by convert_to_wechat.py\n'
            pck_module += f'var data = "{pck_b64}";\n'
            pck_module += f'var bin = atob(data);\n'
            pck_module += f'var buf = new ArrayBuffer(bin.length);\n'
            pck_module += f'var view = new Uint8Array(buf);\n'
            pck_module += f'for (var i = 0; i < bin.length; i++) view[i] = bin.charCodeAt(i);\n'
            pck_module += f'module.exports = {{ name: "{pck_output_name}", buffer: buf }};\n'
            (output / "pck_data.js").write_text(pck_module, encoding='utf-8')
            print(f"[Embed] {pck_file.name} -> pck_data.js ({pck_size} bytes -> {len(pck_b64)} b64 chars, embedded in main package)")
            pck_embedded = True
        elif pck_size_mb <= 3.8:
            shutil.copy(pck_file, output / pck_output_name)
            print(f"[Copy] {pck_file.name} -> {pck_output_name} ({pck_size_mb:.2f} MB, in main package)")
        else:
            print(f"[CDN]  {pck_file.name} ({pck_size_mb:.2f} MB) -> CDN")

    # 5. 处理 .data 文件
    # F5 修复: 优先走 data_pkg 分包（避免依赖外部 CDN server）
    #   - 小 .data (≤3.8MB) 放主包
    #   - 大 .data (≤19MB) 放 data_pkg 分包（让小游戏完全自包含）
    #   - 超大 .data (>19MB) 走 CDN
    data_output_name = ""
    data_in_subpkg = False
    data_subpkg_name = ""
    data_bin_name = ""
    if data_file:
        data_size = data_file.stat().st_size
        data_size_mb = data_size / (1024 * 1024)
        data_output_name = data_file.name  # 保留原始 Emscripten 命名
        file_sizes[data_output_name] = data_size
        if data_size_mb <= 3.8:
            shutil.copy(data_file, output / data_output_name)
            print(f"[Copy] {data_file.name} ({data_size_mb:.2f} MB, in main package)")
        elif data_size_mb <= 19.9 and not cdn_only:
            # F5: 把 .data 改扩展名为 .dat 放入分包
            # 原因：编辑器(DevTools) 对 .data 扩展名 readFileSync 返回 permission denied，
            # 对 .bin 扩展名 readFileSync 内部调原生 atob 嵬尽失败（二进制被当 base64 解码）。
            # .dat 扩展名不受这两个限制影响。
            # 原始二进制，不 base64，不拆分，单分包即可。
            data_subpkg_dir = output / DATA_SUBPACKAGE_ROOT.rstrip("/")
            data_subpkg_dir.mkdir(parents=True, exist_ok=True)
            # 复制时改扩展名为 .dat（adapter 读取时用 .dat 路径）
            _bin_output_name = data_output_name.rsplit('.', 1)[0] + '.dat'
            shutil.copy(data_file, data_subpkg_dir / _bin_output_name)
            data_bin_name = _bin_output_name
            # 分包硬性要求：root 下必须有 game.js 入口
            (data_subpkg_dir / "game.js").write_text(
                f"// {DATA_SUBPACKAGE_NAME} subpackage entry (auto-generated)\n"
                f"// 本分包仅用于承载 {_bin_output_name}，无需任何 JS 逻辑\n"
                f"console.log('[{DATA_SUBPACKAGE_NAME}] subpackage entry loaded');\n",
                encoding="utf-8"
            )
            data_in_subpkg = True
            data_subpkg_name = DATA_SUBPACKAGE_NAME
            # 记录 .dat 文件名（供 adapter 使用，通过 globalThis._dataBinName 传递）
            print(f"[Subpkg] {data_file.name} -> {DATA_SUBPACKAGE_ROOT}{_bin_output_name} ({data_size_mb:.2f} MB, in subpackage, .dat extension to bypass DevTools .data permission + .bin atob bug)")
            print(f"[Subpkg] Wrote {DATA_SUBPACKAGE_ROOT}game.js (subpackage entry stub)")
        else:
            print(f"[CDN]  {data_file.name} ({data_size_mb:.2f} MB) -> CDN (exceeds 20MB subpackage limit)")

    # 6. 复制音频 worklet 文件（主包，因为很小）
    for f in worklet_js_files:
        shutil.copy(f, output / f.name)
        file_sizes[f.name] = f.stat().st_size
        print(f"[Copy] {f.name} ({f.stat().st_size} bytes)")

    # 7. 修补 index.js（主引擎 JS，输出为 index.js，微信入口 require('./index.js')）
    print("\n=== Patching index.js ===")
    js_content = main_js.read_text(encoding='utf-8')
    patched_content = patch_index_js(
        js_content,
        cdn_url=cdn_url,
        wasm_file=wasm_output_name,
        data_file=data_output_name,
        pck_file=pck_output_name,
        file_sizes=file_sizes,
        executable=executable_name,
        pck_embedded=pck_embedded,
        wasm_subpkg=wasm_subpkg_name,
        wasm_br_in_subpkg=wasm_br_in_subpkg,
        data_subpkg=data_subpkg_name,
        data_in_subpkg=data_in_subpkg,
        data_bin_name=data_bin_name,
    )
    (output / "index.js").write_text(patched_content, encoding='utf-8')
    print(f"[Write] index.js (patched, {len(patched_content)} chars)")

    # 8. 生成 game.json（动态追加 .wasm.br / .data 分包）
    game_json = dict(GAME_JSON_TEMPLATE)
    subpackages_list = []
    if wasm_br_in_subpkg:
        subpackages_list.append({
            "name": WASM_SUBPACKAGE_NAME,
            "root": WASM_SUBPACKAGE_ROOT,
        })
        print(f"[Write] game.json: subpackage {WASM_SUBPACKAGE_NAME} (WASM)")
    if data_in_subpkg:
        subpackages_list.append({
            "name": DATA_SUBPACKAGE_NAME,
            "root": DATA_SUBPACKAGE_ROOT,
        })
        print(f"[Write] game.json: subpackage {DATA_SUBPACKAGE_NAME} (data, .dat extension)")
    if subpackages_list:
        game_json["subpackages"] = subpackages_list
    else:
        print(f"[Write] game.json (no subpackage)")
    with open(output / "game.json", "w", encoding="utf-8") as f:
        json.dump(game_json, f, ensure_ascii=False, indent=2)

    # 9. 生成 project.config.json
    with open(output / "project.config.json", "w", encoding="utf-8") as f:
        json.dump(PROJECT_CONFIG_TEMPLATE, f, ensure_ascii=False, indent=2)
    print(f"[Write] project.config.json")

    # 9.1 不生成 project.private.config.json
    # 该文件优先级高于 project.config.json，如果生成错配置会覆盖正确的 compileType。
    # 让 DevTools 首次打开时基于 project.config.json 自动创建，确保 compileType=minigame 生效。
    print(f"[Skip] project.private.config.json (let DevTools create from project.config.json)")

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
    print(f"  3. AppID in project.config.json: {PROJECT_CONFIG_TEMPLATE['appid']}")
    if cdn_only:
        # CDN 模式：大文件未打入包内，必须上传到 CDN 服务器
        print(f"  4. [CDN mode] Upload large files to: {cdn_url}")
        print(f"     - {wasm_file.name} ({wasm_size_mb:.2f} MB)")
        if data_file:
            print(f"     - {data_file.name} ({data_file.stat().st_size / (1024*1024):.2f} MB)")
        print(f"     NOTE: Mobile preview requires CDN reachable from device network.")
    else:
        # 分包模式：小游戏自包含，无需外部 CDN
        print(f"  4. [Subpackage mode] Self-contained: .wasm.br in wasm_pkg/, .dat in data_pkg/")
        print(f"     No CDN upload needed. Mobile preview works without external server.")
        if data_file and data_file.stat().st_size / (1024*1024) > 18.0:
            data_mb = data_file.stat().st_size / (1024*1024)
            print(f"     WARNING: .data is {data_mb:.2f} MB, close to 20MB subpackage limit.")
            print(f"     If engine updates cause .data to exceed 20MB, switch to --cdn-only.")

    return str(output)


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
        help="CDN base URL for large files (.wasm/.data). Example: https://cdn.example.com/game/ "
             "Only used when --cdn-only is set; in subpackage mode (--no-cdn-only) this is ignored."
    )
    # === 大文件加载策略 ===
    # 默认 --no-cdn-only: .wasm.br 和 .data 走子包，小游戏自包含，手机预览无需外部 CDN。
    #   - 主包 ~530KB（JS + pck_data.js base64）
    #   - wasm_pkg 子包: Game2048.wasm.br (~6.9MB，由 WXWebAssembly 原生解压)
    #   - data_pkg 子包: <data_name>.dat (~19.35MB，原始二进制，扩展名 .dat 绕过 DevTools 限制)
    # --cdn-only: 大文件全走 CDN，主包极小但需要 CDN 服务器在线且手机可达。
    #   适用于 .data 接近/超过 20MB 子包限制、或已有公网 CDN 的场景。
    parser.add_argument(
        "--cdn-only", action="store_true", default=False,
        help="Large files (.wasm.br/.data) only via CDN, no subpackages. "
             "Use this when .data exceeds 20MB subpackage limit or you have a public CDN."
    )
    parser.add_argument(
        "--no-cdn-only", dest="cdn_only", action="store_false",
        help="Use subpackages for large files (default). Self-contained mini game, "
             "works on mobile preview without external CDN server."
    )
    parser.set_defaults(cdn_only=False)
    args = parser.parse_args()

    if args.cdn_only and not args.cdn_url:
        print("WARNING: --cdn-only mode requires --cdn-url but none provided.")
        print("         For local testing, set --cdn-url=http://localhost:8000/")
        args.cdn_url = "http://localhost:8000"

    if not args.cdn_only:
        # 分包模式不需要 CDN；cdn_url 仍传入但 adapter 不会用到
        if args.cdn_url:
            print(f"[INFO] --no-cdn-only mode: ignoring --cdn-url={args.cdn_url} (subpackages are self-contained)")
        args.cdn_url = args.cdn_url or ""

    actual_output = convert(args.source, args.output, args.cdn_url, cdn_only=args.cdn_only)
    if actual_output and actual_output != args.output:
        print(f"\n[INFO] 实际输出目录: {actual_output}")
        print(f"[INFO] 请在微信开发者工具中导入此目录")


if __name__ == "__main__":
    main()
