# Mono 6.12 WASM 静态库（含 m2n cookie 扩展补丁）

本目录库存档为 **Mono 6.12.0.206 (sgen)** 面向 `wasm32-unknown-emscripten` 的预编译静态库，是 Web 模板链接 mono_new 的必需输入。2026-07-29 起纳入 git 管理（`git add -f`，原为 `.gitignore` 排除项），原因是其中包含**不可从 stock Mono 直接重建的本地补丁**。

## 关键事实：m2n cookie 扩展表

`libmini.a` 与 `libmonosgen-2.0.a` 中的 `aot-runtime-wasm.c` 成员已替换为**扩展版 m2n cookie 表**（2026-07-20）：

- stock Mono 的 WASM 解释器 icall 分派（m2n）只认一组固定签名 cookie，mono_new 的 icall 有 20 种签名形状不在表内，运行即 `CANNOT HANDLE COOKIE` 崩溃；
- 本目录的库已追加该 20 项（仅追加，未改动原有项），覆盖 mono_new 全部 icall 签名形状；
- 同目录 `.bak_cookies_20260720061933` 两个文件是打补丁前的归档备份（未入库，重建时可比对）。

**警告**：若从 stock Mono 重编这些库，必须重新应用 cookie 扩展并重编 `aot-runtime-wasm.c` 替换归档成员，否则 WASM 下大量 icall 崩溃。扩展表源码与重编步骤见 `godot-mono-port/tools/m2n-cookie/`（`m2n-gen.cs` 全量版 + 重编说明）。

## 各库角色

| 库 | 作用 |
|---|---|
| `libmini.a` | mini 运行时（解释器、trampolines）——**含 cookie 补丁** |
| `libmonosgen-2.0.a` | 运行时总库——**含 cookie 补丁** |
| `libmonoruntimesgen.a` | 运行时类/元数据/SGen 集成 |
| `libmonosgen.a` / `libmono-ee-interp.a` | SGen GC / 解释器执行引擎 |
| `libeglib.a` / `libmonoutils.a` | 基础工具库 |
| `libmono-ilgen.a` / `libmono-icall-table.a` / `libmono-dbg.a` / `libmonomath.a` / `libmonoruntime-config.a` / `libmonoruntime-support.a` / `libz.a` | IL 生成 / icall 表 / 调试 / 数学 / 配置 / 支持库 / zlib |

## 重建

完整流程见 `godot-mono-port/docs/MONO_BUILD_STATIC.md`（Emscripten 3.1.39，`--with-sgen=yes --disable-boehm`）。重建后必须重做 cookie 补丁（见上）。
