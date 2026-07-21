#!/usr/bin/env python3
"""
split_data_to_base64.py - 把 .data 文件拆成 2 个 base64 .js 模块

微信 devtool 对 .data 扩展名的文件 readFileSync 返回 "permission denied"，
但 .js 文件可以通过 require() 加载。所以把 .data base64 编码后放进 .js 模块，
类似 pck_data.js 的做法。

.data 19.35MB → base64 ~25.8MB，超过单个分包 20MB 限制，拆成 2 个分包。
"""
import base64
import os
import shutil
from pathlib import Path

# 路径
MINIGAME_DIR = Path(r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\minigame")
DATA_FILE = MINIGAME_DIR / "data_pkg" / "godot.web.template_release.wasm32.nothreads.data"

# 读取 .data
print(f"Reading: {DATA_FILE}")
with open(DATA_FILE, 'rb') as f:
    data = f.read()

total_size = len(data)
print(f"Data size: {total_size} bytes ({total_size/1024/1024:.2f} MB)")

# 拆成 2 个 chunk（每个 ~9.7MB，base64 后 ~13MB，小于 20MB 分包限制）
half = total_size // 2
chunk1 = data[:half]
chunk2 = data[half:]

print(f"Chunk 1: {len(chunk1)} bytes ({len(chunk1)/1024/1024:.2f} MB)")
print(f"Chunk 2: {len(chunk2)} bytes ({len(chunk2)/1024/1024:.2f} MB)")

# Base64 编码
b64_1 = base64.b64encode(chunk1).decode('ascii')
b64_2 = base64.b64encode(chunk2).decode('ascii')

print(f"Base64 1: {len(b64_1)} bytes ({len(b64_1)/1024/1024:.2f} MB)")
print(f"Base64 2: {len(b64_2)} bytes ({len(b64_2)/1024/1024:.2f} MB)")

# 验证不超过分包限制（20MB = 20*1024*1024）
MAX_SUBPKG = 19 * 1024 * 1024  # 留 1MB 余量给 JS wrapper
assert len(b64_1) <= MAX_SUBPKG, f"Chunk 1 base64 too big: {len(b64_1)}"
assert len(b64_2) <= MAX_SUBPKG, f"Chunk 2 base64 too big: {len(b64_2)}"

# 生成 .js 模块
# 注意：单个字符串字面量太大可能影响解析速度，但 V8 能处理 13MB 的字符串
js1 = f'''// Auto-generated: base64-encoded .data chunk 1 of 2
// Original file: godot.web.template_release.wasm32.nothreads.data
// Chunk 1: bytes 0..{half} ({len(chunk1)} bytes, base64 {len(b64_1)} chars)
var _b64 = "{b64_1}";
var _bin = atob(_b64);
var _buf = new Uint8Array(_bin.length);
for (var i = 0; i < _bin.length; i++) _buf[i] = _bin.charCodeAt(i);
module.exports = {{ buffer: _buf.buffer, size: _buf.length, offset: 0, total: {total_size} }};
'''

js2 = f'''// Auto-generated: base64-encoded .data chunk 2 of 2
// Original file: godot.web.template_release.wasm32.nothreads.data
// Chunk 2: bytes {half}..{total_size} ({len(chunk2)} bytes, base64 {len(b64_2)} chars)
var _b64 = "{b64_2}";
var _bin = atob(_b64);
var _buf = new Uint8Array(_bin.length);
for (var i = 0; i < _bin.length; i++) _buf[i] = _bin.charCodeAt(i);
module.exports = {{ buffer: _buf.buffer, size: _buf.length, offset: {half}, total: {total_size} }};
'''

# 创建 data_pkg_1 目录
pkg1_dir = MINIGAME_DIR / "data_pkg_1"
if pkg1_dir.exists():
    shutil.rmtree(pkg1_dir)
pkg1_dir.mkdir(parents=True)

# 写 data_part1.js
part1_path = pkg1_dir / "data_part1.js"
print(f"Writing: {part1_path}")
with open(part1_path, 'w', encoding='utf-8') as f:
    f.write(js1)
print(f"  size: {part1_path.stat().st_size} bytes ({part1_path.stat().st_size/1024/1024:.2f} MB)")

# 写 data_pkg_1/game.js (分包入口)
game1_path = pkg1_dir / "game.js"
with open(game1_path, 'w', encoding='utf-8') as f:
    f.write('''// data_pkg_1 subpackage entry - loads base64 .data chunk 1
console.log('[data_pkg_1] entry loaded');
try {
  var part = require('./data_part1.js');
  globalThis._dataChunk1 = part;
  console.log('[data_pkg_1] chunk 1 loaded, size=' + part.size + ' offset=' + part.offset);
} catch (e) {
  console.log('[data_pkg_1] FAILED: ' + e.message);
  globalThis._dataChunk1 = null;
  globalThis._dataChunk1Error = e.message;
}
''')

# 创建 data_pkg_2 目录
pkg2_dir = MINIGAME_DIR / "data_pkg_2"
if pkg2_dir.exists():
    shutil.rmtree(pkg2_dir)
pkg2_dir.mkdir(parents=True)

# 写 data_part2.js
part2_path = pkg2_dir / "data_part2.js"
print(f"Writing: {part2_path}")
with open(part2_path, 'w', encoding='utf-8') as f:
    f.write(js2)
print(f"  size: {part2_path.stat().st_size} bytes ({part2_path.stat().st_size/1024/1024:.2f} MB)")

# 写 data_pkg_2/game.js (分包入口)
game2_path = pkg2_dir / "game.js"
with open(game2_path, 'w', encoding='utf-8') as f:
    f.write('''// data_pkg_2 subpackage entry - loads base64 .data chunk 2
console.log('[data_pkg_2] entry loaded');
try {
  var part = require('./data_part2.js');
  globalThis._dataChunk2 = part;
  console.log('[data_pkg_2] chunk 2 loaded, size=' + part.size + ' offset=' + part.offset);
} catch (e) {
  console.log('[data_pkg_2] FAILED: ' + e.message);
  globalThis._dataChunk2 = null;
  globalThis._dataChunk2Error = e.message;
}
''')

print("\n=== Done! ===")
print(f"data_pkg_1/: data_part1.js ({part1_path.stat().st_size/1024/1024:.2f} MB) + game.js")
print(f"data_pkg_2/: data_part2.js ({part2_path.stat().st_size/1024/1024:.2f} MB) + game.js")
print(f"\nTotal base64: {(len(b64_1)+len(b64_2))/1024/1024:.2f} MB (original: {total_size/1024/1024:.2f} MB)")
