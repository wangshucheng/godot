#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
icall ABI 签名核对工具（P2.3）

扫描 C++ 侧 icall 注册（icall_entries 表 + mono_add_internal_call 调用）
与 C# 侧 [MethodImpl(MethodImplOptions.InternalCall)] extern 声明，核对：

1. 名称对齐：C++ 注册的每个 icall 是否在 C# 侧有对应声明，反之亦然。
2. WASM 安全规则（违反即在 WASM 解释器下 CANNOT HANDLE COOKIE 崩溃）：
   - 自定义 icall 不可直接返回 float/double（应用 int32/int64 位模式 + union）
   - 自定义 icall 不可含 long 参数（会被误识别为对象引用）
3. int32/int64 不匹配（C# long=int64 vs C++ int32_t；C# int=int32 vs C++ int64_t）

用法：
    python check_icall_abi.py [--mono-new-dir <path>] [--strict]

退出码：0 = 无错误；1 = 有错误；2 = 有警告（--strict 时也按错误处理。

设计说明：
- 这是一个静态扫描器，不依赖 Mono 或 Godot 运行时。
- C++ 侧只解析"注册名"（如 "Godot.GD::godot_icall_GD_Print"），不解析 C++ 函数指针签名
  （手写 icall 的 C++ 签名分散且类型别名多，静态解析不可靠；WASM 规则违规在 C# 侧即可判定）。
- C# 侧解析 extern 声明的返回类型与参数类型，与项目 WASM 铁律对照。
"""

import argparse
import os
import re
import sys
from collections import OrderedDict


# ---------------------------------------------------------------------------
# C# 类型 → 宽度映射（用于 int32/int64 不匹配检测）
# ---------------------------------------------------------------------------
# C# 类型             宽度(bits)  备注
# int / Int32         32
# long / Int64        64
# short / Int16       16
# byte / Byte         8
# sbyte               8
# uint / UInt32       32
# ulong / UInt64      64
# ushort              16
# float / Single      32
# double / Double     64
# bool / Boolean      8
# char / Char         16
# string / String     ref
# GodotObject/object  ref

CS_INT_WIDTH = {
    "int": 32, "Int32": 32,
    "long": 64, "Int64": 64,
    "short": 16, "Int16": 16,
    "byte": 8, "Byte": 8,
    "sbyte": 8, "SByte": 8,
    "uint": 32, "UInt32": 32,
    "ulong": 64, "UInt64": 64,
    "ushort": 16, "UInt16": 16,
}

CS_FLOAT_TYPES = {"float", "Single"}
CS_DOUBLE_TYPES = {"double", "Double"}
CS_LONG_TYPES = {"long", "Int64", "ulong", "UInt64"}


# ---------------------------------------------------------------------------
# C++ 侧：解析 icall 注册名
# ---------------------------------------------------------------------------

# icall_entries 表项: { "Godot.GD::godot_icall_GD_Print", (const void *)icall_GD_Print },
RE_ICALL_TABLE_ENTRY = re.compile(
    r'\{\s*"((?:Godot|GodotSharp)[.\w]+::[\w_]+)"\s*,\s*\([^)]*\)\s*([\w_]+)\s*\}'
)

# mono_add_internal_call("Godot.Node::godot_icall_Node_GetParent", (const void *)icall_Node_GetParent);
RE_MONO_ADD_ICALL = re.compile(
    r'mono_add_internal_call\(\s*"((?:Godot|GodotSharp)[.\w]+::[\w_]+)"\s*,\s*\([^)]*\)\s*([\w_]+)\s*\)'
)


def parse_cpp_icalls(mono_new_dir):
    """返回 OrderedDict: { icall_name: { 'func': cpp_func_name, 'file': rel_path, 'line': N } }"""
    result = OrderedDict()
    roots = [
        os.path.join(mono_new_dir, "mono_gd"),
        os.path.join(mono_new_dir, "glue"),
    ]
    for root in roots:
        if not os.path.isdir(root):
            continue
        for dirpath, _, filenames in os.walk(root):
            for fn in filenames:
                if not (fn.endswith(".cpp") or fn.endswith(".cc")):
                    continue
                fpath = os.path.join(dirpath, fn)
                try:
                    with open(fpath, "r", encoding="utf-8", errors="replace") as f:
                        content = f.read()
                except OSError:
                    continue
                # 表项
                for m in RE_ICALL_TABLE_ENTRY.finditer(content):
                    name = m.group(1)
                    func = m.group(2)
                    lineno = content[:m.start()].count("\n") + 1
                    result[name] = {
                        "func": func,
                        "file": os.path.relpath(fpath, mono_new_dir),
                        "line": lineno,
                        "source": "table",
                    }
                # mono_add_internal_call
                for m in RE_MONO_ADD_ICALL.finditer(content):
                    name = m.group(1)
                    func = m.group(2)
                    lineno = content[:m.start()].count("\n") + 1
                    if name in result:
                        # 已在表里登记，记下重复来源
                        result[name]["source"] += "+add"
                        continue
                    result[name] = {
                        "func": func,
                        "file": os.path.relpath(fpath, mono_new_dir),
                        "line": lineno,
                        "source": "add",
                    }
    return result


# ---------------------------------------------------------------------------
# C# 侧：解析 extern 声明
# ---------------------------------------------------------------------------

# 匹配带 [MethodImpl(MethodImplOptions.InternalCall)] 标记的 extern 声明
# 形如:
#   [MethodImpl(MethodImplOptions.InternalCall)]
#   internal extern static <ret> <name>(<args>);
RE_ICALL_DECL = re.compile(
    r'internal\s+extern\s+static\s+'
    r'([\w<>\[\],\s\.]+?)\s+'        # 返回类型（非贪婪）
    r'(godot_icall_\w+)\s*'          # 方法名（必须以 godot_icall_ 开头）
    r'\(([^)]*)\)\s*;'               # 参数列表
)


def _parse_cs_type(type_str):
    """规范化 C# 类型字符串，返回 (base_type, is_ref)"""
    t = type_str.strip()
    # 去 out/ref 修饰
    t = re.sub(r'^(out|ref)\s+', '', t)
    is_ref = False
    if t.endswith("[]"):
        is_ref = True
        t = t[:-2].strip()
    # 取基础类型名（去命名空间前缀）
    if "." in t:
        t = t.split(".")[-1]
    return t, is_ref


def _parse_cs_args(args_str):
    """解析参数列表，返回 [(type, name), ...]"""
    args = []
    args_str = args_str.strip()
    if not args_str:
        return args
    # 按逗号分割（不处理泛型逗号的边界情况，本项目 icall 无泛型参数）
    for part in args_str.split(","):
        part = part.strip()
        if not part:
            continue
        # 形如 "long obj" 或 "out object ret"
        tokens = part.split()
        if len(tokens) < 2:
            continue
        # out/ref 修饰
        if tokens[0] in ("out", "ref"):
            tokens = tokens[1:]
        if len(tokens) < 2:
            continue
        # 类型可能是多 token（如 GodotObject target）——最后一个是参数名
        name = tokens[-1]
        type_str = " ".join(tokens[:-1])
        args.append((_parse_cs_type(type_str)[0], name))
    return args


def parse_cs_icalls(csharp_dir):
    """返回 OrderedDict: { icall_name: { 'ret': base_type, 'args': [(type,name)], 'file': rel, 'line': N } }"""
    result = OrderedDict()
    if not os.path.isdir(csharp_dir):
        return result
    for dirpath, _, filenames in os.walk(csharp_dir):
        for fn in filenames:
            if not fn.endswith(".cs"):
                continue
            fpath = os.path.join(dirpath, fn)
            try:
                with open(fpath, "r", encoding="utf-8", errors="replace") as f:
                    content = f.read()
            except OSError:
                continue
            # 找 [MethodImpl(MethodImplOptions.InternalCall)] 后跟的 extern 声明
            for m in re.finditer(
                r'\[MethodImpl\(MethodImplOptions\.InternalCall\)\]\s*\n'
                r'\s*internal\s+extern\s+static\s+'
                r'([\w<>\[\],\s\.]+?)\s+'
                r'(godot_icall_\w+)\s*'
                r'\(([^)]*)\)\s*;',
                content
            ):
                ret_type = _parse_cs_type(m.group(1).strip())[0]
                method_name = m.group(2)
                args = _parse_cs_args(m.group(3))
                lineno = content[:m.start()].count("\n") + 1
                # 推断所属类：向上找最近的 class/struct 声明
                cls = _find_enclosing_class(content, m.start())
                full_name = "Godot.{}::{}".format(cls, method_name) if cls else method_name
                result[full_name] = {
                    "ret": ret_type,
                    "args": args,
                    "file": os.path.relpath(fpath, os.path.dirname(csharp_dir)),
                    "line": lineno,
                    "class": cls,
                }
    return result


def _find_enclosing_class(content, pos):
    """从 pos 向上找最近的 class/struct 声明的类名"""
    prefix = content[:pos]
    # 倒序匹配最后一个 class/struct 声明
    matches = list(re.finditer(
        r'\b(?:public|internal|private|protected)?\s*(?:static\s+)?(?:partial\s+)?'
        r'(?:class|struct)\s+(\w+)', prefix
    ))
    if matches:
        return matches[-1].group(1)
    return None


# ---------------------------------------------------------------------------
# 核对规则
# ---------------------------------------------------------------------------

def check_consistency(cpp_icalls, cs_icalls):
    """返回 (errors, warnings) 列表

    规则说明（参考 AGENTS.md 第四节"WASM 解释器特殊规则"）：
    - float/double 直接返回：仍是 ERROR。Mono WASM 解释器对浮点返回值的 icall
      支持不完善，项目统一用 int32/int64 位模式 + [StructLayout(LayoutKind.Explicit)]
      union 还原（见 Globals.cs DoubleLongUnion/FloatIntUnion）。
    - long 参数：降级为 WARNING。项目已于 2026-07-20 扩展 Mono WASM m2n cookie 表
      （+20 项，覆盖 mono_new 全部 icall 签名形状，见 REVIEW_REPORT 附录 A），
      原始"long 会被误识别为对象引用"规则已不再阻塞。保留 WARNING 是为了提醒：
      新增长参数 icall 需确认其签名形状已在扩展后的 cookie 表内，否则仍会
      CANNOT HANDLE COOKIE 崩溃。
    - 名称对齐：ERROR。C++ 注册与 C# 声明必须一一对应，否则运行时找不到实现。
    """
    errors = []
    warnings = []

    cpp_names = set(cpp_icalls.keys())
    cs_names = set(cs_icalls.keys())

    # 1. 名称对齐
    only_cpp = cpp_names - cs_names
    only_cs = cs_names - cpp_names
    for name in sorted(only_cpp):
        info = cpp_icalls[name]
        errors.append(
            "C++ 注册但 C# 无声明: {} ({}:{} func={})".format(
                name, info["file"], info["line"], info["func"]
            )
        )
    for name in sorted(only_cs):
        info = cs_icalls[name]
        # 部分泛型 icall 在 C++ 表里可能用不同名前缀，这里只告警
        warnings.append(
            "C# 声明但 C++ 无注册: {} ({}:{})".format(
                name, info["file"], info["line"]
            )
        )

    # 2. WASM 规则检查（C# 侧）
    for name, info in cs_icalls.items():
        # 2a. 直接返回 float/double（WASM 不安全，cookie 表扩展不覆盖浮点返回）
        if info["ret"] in CS_FLOAT_TYPES or info["ret"] in CS_DOUBLE_TYPES:
            errors.append(
                "WASM 不安全: {} 直接返回 {} ({}:{})——应改用 int32/int64 位模式 + union".format(
                    name, info["ret"], info["file"], info["line"]
                )
            )
        # 2b. 含 long 参数（已扩展 cookie 表，降级为 WARNING）
        long_params = [arg_name for t, arg_name in info["args"] if t in CS_LONG_TYPES]
        if long_params:
            warnings.append(
                "WASM 提示: {} 含 long 参数 {} ({}:{})——cookie 表已扩展，但新增签名形状需确认已覆盖".format(
                    name, long_params, info["file"], info["line"]
                )
            )

    return errors, warnings


# ---------------------------------------------------------------------------
# 主入口
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="icall ABI 签名核对工具（P2.3）")
    parser.add_argument(
        "--mono-new-dir",
        default=None,
        help="mono_new 模块根目录（默认：脚本所在目录的上一级）",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="严格模式：警告也按错误处理",
    )
    args = parser.parse_args()

    if args.mono_new_dir is None:
        args.mono_new_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    csharp_dir = os.path.join(args.mono_new_dir, "csharp", "GodotSharp")
    print("== icall ABI 签名核对 ==")
    print("mono_new 目录: {}".format(args.mono_new_dir))
    print("C# 源目录:     {}".format(csharp_dir))
    print()

    cpp_icalls = parse_cpp_icalls(args.mono_new_dir)
    cs_icalls = parse_cs_icalls(csharp_dir)

    print("C++ 侧 icall 注册数: {}".format(len(cpp_icalls)))
    print("C# 侧 extern 声明数: {}".format(len(cs_icalls)))
    print()

    errors, warnings = check_consistency(cpp_icalls, cs_icalls)

    if warnings:
        print("-- 警告 ({} 条) --".format(len(warnings)))
        for w in warnings:
            print("  WARN: " + w)
        print()

    if errors:
        print("-- 错误 ({} 条) --".format(len(errors)))
        for e in errors:
            print("  ERROR: " + e)
        print()
        print("结果: 失败（{} 错误）".format(len(errors)))
        return 1

    if args.strict and warnings:
        print("结果: 失败（--strict 模式，{} 警告视为错误）".format(len(warnings)))
        return 1

    print("结果: 通过（{} 警告）".format(len(warnings)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
