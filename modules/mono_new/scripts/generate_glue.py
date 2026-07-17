#!/usr/bin/env python3
"""
Source-code generator for mono_new icall glue.

Reads a JSON "glue spec" describing a Godot class's bindings and emits two
files:
  - <Class>_glue.cpp   : C++ icall implementations + register_<class>_icalls()
  - <Class>Ext.cs      : C# icall declarations + public wrapper methods

Both files follow the patterns established by hand-written bindings in
glue/glue_cpp/node_glue.cpp and csharp/GodotSharp/NodeExt.cs.

WASM-safety rules baked into the generator
------------------------------------------
The Mono interpreter's do_icall dispatch on WebAssembly cannot handle icall
signatures that return or accept float/double directly. To stay WASM-safe,
float/double values are passed as bit-pattern integers:
  - double  <-> int64_t  (C++) / long             (C#) + DoubleLongUnion
  - float   <-> int32_t  (C++) / int              (C#) + FloatIntUnion

The generator inspects each method's return/param types and emits the
appropriate marshaling code automatically.

Spec format (JSON)
------------------
{
  "class": "Timer",                  # Godot class name
  "inherits": "Node",                # Parent class (for ClassDB validation)
  "cpp_header": "scene/main/timer.h",
  "namespace": "Godot",
  "methods": [
    {
      "name": "set_wait_time",       # C++ method name (snake_case)
      "csharp_name": "SetWaitTime",  # C# wrapper name (PascalCase)
      "return_type": "void",
      "params": [
        {"type": "double", "name": "p_time"}
      ],
      "cpp_method": "set_wait_time"  # C++ method to call (default: same as 'name')
    }
  ]
}

Usage:
  python generate_glue.py --spec timer_glue.spec.json \\
      --cpp-out glue/glue_cpp/timer_glue.cpp \\
      --cs-out  csharp/GodotSharp/TimerExt.cs
"""

import argparse
import json
import os
import sys
from typing import Dict, List, Tuple


# ---------------------------------------------------------------------------
# Type system: spec type -> (C++ icall type, C# extern type, marshaling kind)
# ---------------------------------------------------------------------------
# marshaling kind:
#   "direct"     -> pass-through, no conversion
#   "bool"       -> mono_bool <-> bool
#   "string_in"  -> MonoString* -> String (RAII holder)
#   "string_out" -> String -> MonoString* (utf8)
#   "double_in"  -> int64_t bit-pattern -> double (WASM-safe)
#   "double_out" -> double -> int64_t bit-pattern (WASM-safe)
#   "float_in"   -> int32_t bit-pattern -> float (WASM-safe)
#   "float_out"  -> float -> int32_t bit-pattern (WASM-safe)
#   "ptr_in"     -> int64_t -> T* (cast)
#   "ptr_out"    -> T* -> int64_t (cast)

TYPE_MAP: Dict[str, Tuple[str, str, str]] = {
    "void":     ("void",        "void",   "direct"),
    "bool":     ("mono_bool",   "bool",   "bool"),
    "int":      ("int32_t",     "int",    "direct"),
    "int32":    ("int32_t",     "int",    "direct"),
    "int64":    ("int64_t",     "long",   "direct"),
    "uint":     ("uint32_t",    "uint",   "direct"),
    "long":     ("int64_t",     "long",   "direct"),
    "float":    ("int32_t",     "int",    "float_in"),  # for params
    "double":   ("int64_t",     "long",   "double_in"), # for params
    "string":   ("MonoString*", "string", "string_in"),
    # pointer-like types (Node*, Object*, Resource*, etc.)
    "Node":     ("int64_t",     "long",   "ptr_in"),
    "Object":   ("int64_t",     "long",   "ptr_in"),
    "Resource": ("int64_t",     "long",   "ptr_in"),
    "Timer":    ("int64_t",     "long",   "ptr_in"),
    "Variant":  ("int64_t",     "long",   "ptr_in"),
}


def cpp_type(spec_type: str, is_return: bool = False) -> str:
    """Get the C++ icall type for a spec type."""
    if spec_type not in TYPE_MAP:
        raise ValueError(f"Unknown spec type: {spec_type}")
    cpp_t, _, _ = TYPE_MAP[spec_type]

    # Return-type overrides for WASM safety
    if is_return:
        if spec_type == "double":
            return "int64_t"  # bit-pattern
        if spec_type == "float":
            return "int32_t"  # bit-pattern
        if spec_type == "string":
            return "MonoString *"
    return cpp_t


def csharp_extern_type(spec_type: str, is_return: bool = False) -> str:
    """Get the C# extern type for a spec type."""
    if spec_type not in TYPE_MAP:
        raise ValueError(f"Unknown spec type: {spec_type}")
    _, cs_t, _ = TYPE_MAP[spec_type]

    if is_return:
        if spec_type == "double":
            return "long"  # bit-pattern, caller uses DoubleLongUnion
        if spec_type == "float":
            return "int"   # bit-pattern, caller uses FloatIntUnion
        if spec_type == "string":
            return "string"
    return cs_t


def csharp_public_type(spec_type: str) -> str:
    """Get the C# public-API type for a spec type (post-conversion)."""
    if spec_type == "void":
        return "void"
    if spec_type == "bool":
        return "bool"
    if spec_type in ("int", "int32"):
        return "int"
    if spec_type in ("int64", "long"):
        return "long"
    if spec_type == "uint":
        return "uint"
    if spec_type == "float":
        return "float"
    if spec_type == "double":
        return "double"
    if spec_type == "string":
        return "string"
    # pointer-like types map to the spec type itself (e.g. Node, Timer)
    return spec_type


def marshal_kind(spec_type: str, is_return: bool = False) -> str:
    """Get the marshaling kind for a spec type."""
    if spec_type not in TYPE_MAP:
        raise ValueError(f"Unknown spec type: {spec_type}")
    _, _, kind = TYPE_MAP[spec_type]

    if is_return:
        if spec_type == "double":
            return "double_out"
        if spec_type == "float":
            return "float_out"
        if spec_type == "string":
            return "string_out"
        if spec_type == "bool":
            return "bool"  # mono_bool return
    return kind


# ---------------------------------------------------------------------------
# C++ code generation
# ---------------------------------------------------------------------------

CPP_HEADER = """// AUTO-GENERATED by generate_glue.py - DO NOT EDIT BY HAND.
// Source spec: {spec_name}
//
// ICall bindings for {class_name} - generated from a JSON spec.
// All float/double values are passed as integer bit-patterns to comply with
// the WASM Mono interpreter's do_icall signature constraints.

#include "../../mono_gd/interop/gd_mono_interop_variant.h"
#include "../../utils/mono_logger.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
{extra_includes}
#include <mono/mono-publib.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" {{
char *mono_string_to_utf8(MonoString *s);
void mono_free(void *ptr);
}}

namespace {{

// RAII helper for MonoString -> String conversion.
struct GenMonoStringHolder {{
\tchar *utf8;
\tGenMonoStringHolder(MonoString *s) : utf8(s ? mono_string_to_utf8(s) : nullptr) {{}}
\t~GenMonoStringHolder() {{ if (utf8) mono_free(utf8); }}
\tbool valid() const {{ return utf8 != nullptr; }}
\tString to_string() const {{ return utf8 ? String::utf8(utf8) : String(); }}
}};

{icall_impls}

// ClassDB validation: verify {class_name} has the methods we bind.
static void scan_and_validate_{class_lower}_methods() {{
\tconst char *essential_methods[] = {{
{method_list}\t\tnullptr
\t}};
\tfor (int i = 0; essential_methods[i] != nullptr; i++) {{
\t\tif (!ClassDB::has_method("{class_name}", essential_methods[i])) {{
\t\t\tMonoLogger::log_warning(vformat("ClassDB: {class_name}::%s not found", essential_methods[i]));
\t\t}}
\t}}
}}

}} // anonymous namespace

namespace GDMonoInterop {{

void register_{class_lower}_icalls() {{
\tMonoLogger::log("Registering {class_name} icalls (generated)...");
\tscan_and_validate_{class_lower}_methods();

{registrations}
\tMonoLogger::log("{class_name} icalls registered ({count} methods)");
}}

}} // namespace GDMonoInterop
"""


def gen_cpp_param_decls(params: List[Dict]) -> str:
    """Generate C++ parameter declarations for an icall."""
    parts = []
    for p in params:
        t = p["type"]
        n = p["name"]
        cpp_t = cpp_type(t)
        # First parameter is always the receiver (int64_t p_<class>)
        parts.append(f"{cpp_t} {n}")
    return ", ".join(parts)


def gen_cpp_marshal_in(param: Dict) -> Tuple[str, str]:
    """Generate C++ marshaling code for a single input parameter.
    Returns (declarations, expression_to_use_in_call).
    """
    t = param["type"]
    n = param["name"]
    kind = marshal_kind(t, is_return=False)

    if kind == "direct":
        return "", f"({cpp_type(t)}){n}"
    if kind == "bool":
        return "", f"({n} != 0)"
    if kind == "string_in":
        holder = f"{n}_h"
        decl = (f"\tGenMonoStringHolder {holder}({n});\n"
                f"\tif (!{holder}.valid()) return;\n")
        # Note: the 'return' assumes a void function; caller adjusts for non-void
        return decl, f"{holder}.to_string()"
    if kind == "double_in":
        # int64_t bit-pattern -> double
        tmp = f"{n}_d"
        decl = f"\tdouble {tmp};\n\tmemcpy(&{tmp}, &{n}, sizeof(double));\n"
        return decl, tmp
    if kind == "float_in":
        tmp = f"{n}_f"
        decl = f"\tfloat {tmp};\n\tmemcpy(&{tmp}, &{n}, sizeof(float));\n"
        return decl, tmp
    if kind == "ptr_in":
        return "", f"({t} *)(intptr_t){n}"
    raise ValueError(f"Unsupported input marshaling kind: {kind}")


def gen_cpp_return_expr(spec_return: str, expr: str) -> str:
    """Wrap a C++ return expression to produce the icall return value."""
    if spec_return == "void":
        return f"\t{expr};\n"
    if spec_return == "bool":
        return f"\treturn (mono_bool)({expr});\n"
    if spec_return in ("int", "int32", "uint", "int64", "long"):
        return f"\treturn ({cpp_type(spec_return, is_return=True)})({expr});\n"
    if spec_return == "double":
        # Convert double -> int64_t bit-pattern
        return (f"\tdouble _ret_d = ({expr});\n"
                f"\tint64_t _ret_bits = 0;\n"
                f"\tmemcpy(&_ret_bits, &_ret_d, sizeof(double));\n"
                f"\treturn _ret_bits;\n")
    if spec_return == "float":
        return (f"\tfloat _ret_f = ({expr});\n"
                f"\tint32_t _ret_bits = 0;\n"
                f"\tmemcpy(&_ret_bits, &_ret_f, sizeof(float));\n"
                f"\treturn _ret_bits;\n")
    if spec_return == "string":
        return (f"\tString _ret_s = ({expr});\n"
                f"\tCharString _ret_cs = _ret_s.utf8();\n"
                f"\treturn mono_string_new(mono_domain_get(), _ret_cs.get_data());\n")
    # pointer-like return
    return f"\treturn (int64_t)(intptr_t)({expr});\n"


def gen_cpp_icall(spec: Dict, method: Dict) -> str:
    """Generate one C++ icall function."""
    class_name = spec["class"]
    method_name = method["name"]
    csharp_name = method.get("csharp_name", method_name)
    cpp_method = method.get("cpp_method", method_name)
    spec_return = method.get("return_type", "void")
    params = method.get("params", [])

    # The first parameter is always the receiver: int64_t p_<class_lower>
    # If the spec lists it explicitly, use it; otherwise prepend.
    if not params or not params[0].get("is_receiver", False):
        receiver = {"type": class_name if class_name in TYPE_MAP else "Object",
                    "name": f"p_{class_name.lower()}", "is_receiver": True}
        all_params = [receiver] + params
    else:
        all_params = list(params)

    # Function signature
    ret_cpp = cpp_type(spec_return, is_return=True)
    param_decls = gen_cpp_param_decls(all_params)
    icall_name = f"icall_{class_name}_{csharp_name}"

    out = [f"// {class_name}::{cpp_method}",
           f"static {ret_cpp} {icall_name}({param_decls}) {{"]

    receiver_name = all_params[0]["name"]
    out.append(f"\t{class_name} *self = ({class_name} *)(intptr_t){receiver_name};")
    out.append(f"\tif (!self) return{'' if spec_return == 'void' else ' 0'};")

    # Marshal remaining params
    call_args = ["self"]
    for p in all_params[1:]:
        decl, expr = gen_cpp_marshal_in(p)
        if decl:
            # For string params, the 'return' on failure depends on return type
            if marshal_kind(p["type"]) == "string_in":
                # Adjust early-return value based on spec_return
                if spec_return == "void":
                    decl = decl.replace("return;", "return;")
                elif spec_return == "string":
                    decl = decl.replace("return;", "return mono_string_new(mono_domain_get(), \"\");")
                else:
                    decl = decl.replace("return;", f"return ({cpp_type(spec_return, is_return=True)})0;")
            out.append(decl.rstrip("\n"))
        call_args.append(expr)

    # Generate the call
    call_expr = f"{call_args[0]}->{cpp_method}({', '.join(call_args[1:])})"
    out.append(gen_cpp_return_expr(spec_return, call_expr))
    out.append("}")
    return "\n".join(out)


def gen_cpp_registration(spec: Dict, method: Dict) -> str:
    class_name = spec["class"]
    namespace = spec.get("namespace", "Godot")
    csharp_name = method.get("csharp_name", method["name"])
    icall_name = f"icall_{class_name}_{csharp_name}"
    return (f'\tmono_add_internal_call("{namespace}.{class_name}::godot_icall_{class_name}_{csharp_name}", '
            f'(const void *){icall_name});')


def generate_cpp(spec: Dict, spec_name: str) -> str:
    class_name = spec["class"]
    class_lower = class_name.lower()
    methods = spec.get("methods", [])
    cpp_header = spec.get("cpp_header", "")
    extra_includes = ""
    if cpp_header:
        extra_includes = f'#include "{cpp_header}"'

    # icall implementations
    icall_impls = "\n\n".join(gen_cpp_icall(spec, m) for m in methods)

    # ClassDB method list (snake_case names)
    method_names = [m.get("cpp_method", m["name"]) for m in methods]
    method_list = "".join(f'\t\t"{n}",\n' for n in method_names)

    # Registrations
    registrations = "\n".join(gen_cpp_registration(spec, m) for m in methods)

    return CPP_HEADER.format(
        spec_name=spec_name,
        class_name=class_name,
        class_lower=class_lower,
        extra_includes=extra_includes,
        icall_impls=icall_impls,
        method_list=method_list,
        registrations=registrations,
        count=len(methods),
    )


# ---------------------------------------------------------------------------
# C# code generation
# ---------------------------------------------------------------------------

CS_HEADER = """// AUTO-GENERATED by generate_glue.py - DO NOT EDIT BY HAND.
// Source spec: {spec_name}
//
// C# icall declarations and public wrapper methods for {class_name}.

using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace {namespace}
{{
{cs_class_body}
}}
"""


CS_CLASS_TEMPLATE = """public partial class {class_name}
{{
{icall_decls}

{wrapper_methods}
}}"""


def gen_cs_extern_decl(spec: Dict, method: Dict) -> str:
    class_name = spec["class"]
    csharp_name = method.get("csharp_name", method["name"])
    spec_return = method.get("return_type", "void")
    params = method.get("params", [])

    # First param is always the receiver (long nativeInstance)
    if not params or not params[0].get("is_receiver", False):
        all_params = [{"type": class_name if class_name in TYPE_MAP else "Object",
                       "name": "node", "is_receiver": True}] + params
    else:
        all_params = list(params)

    ret_cs = csharp_extern_type(spec_return, is_return=True)
    decl_params = ["long node"]  # receiver
    for p in all_params[1:]:
        decl_params.append(f"{csharp_extern_type(p['type'])} {p['name']}")

    return (f'\t[MethodImpl(MethodImplOptions.InternalCall)]\n'
            f'\tinternal extern static {ret_cs} godot_icall_{class_name}_{csharp_name}({", ".join(decl_params)});')


def gen_cs_wrapper_method(spec: Dict, method: Dict) -> str:
    class_name = spec["class"]
    csharp_name = method.get("csharp_name", method["name"])
    spec_return = method.get("return_type", "void")
    params = method.get("params", [])

    # Public method signature uses converted C# types
    pub_params = []
    for p in params:
        pub_params.append(f"{csharp_public_type(p['type'])} {p['name']}")

    sig = f"\tpublic {csharp_public_type(spec_return)} {csharp_name}({', '.join(pub_params)})"

    # Build the call expression
    call_args = ["nativeInstance"]
    body_lines = []
    needs_open_brace = True

    # Pre-convert double/float params to bit-patterns using the existing
    # DoubleLongUnion / FloatIntUnion structs (field-based syntax, see Globals.cs).
    for p in params:
        t = p["type"]
        n = p["name"]
        if t == "double":
            body_lines.append(f"\t\tlong {n}_bits = new DoubleLongUnion {{ DoubleValue = {n} }}.LongValue;")
            call_args.append(f"{n}_bits")
        elif t == "float":
            body_lines.append(f"\t\tint {n}_bits = new FloatIntUnion {{ FloatValue = {n} }}.IntValue;")
            call_args.append(f"{n}_bits")
        elif t == "string":
            call_args.append(n)
        else:
            # For pointer-like types, marshal to long
            if t in TYPE_MAP and TYPE_MAP[t][2] == "ptr_in":
                call_args.append(f"{n} != null ? {n}.nativeInstance : 0")
            else:
                call_args.append(n)

    invoke = f"godot_icall_{class_name}_{csharp_name}({', '.join(call_args)})"

    if spec_return == "void":
        body_lines.append(f"\t\t{invoke};")
    elif spec_return == "double":
        body_lines.append(f"\t\treturn new DoubleLongUnion {{ LongValue = {invoke} }}.DoubleValue;")
    elif spec_return == "float":
        body_lines.append(f"\t\treturn new FloatIntUnion {{ IntValue = {invoke} }}.FloatValue;")
    elif spec_return == "bool":
        body_lines.append(f"\t\treturn {invoke};")
    elif spec_return == "string":
        body_lines.append(f"\t\treturn {invoke};")
    elif spec_return in ("Node", "Object", "Resource", "Timer"):
        # Pointer return - wrap as the target type
        body_lines.append(f"\t\tlong ptr = {invoke};")
        body_lines.append(f"\t\treturn ptr != 0 ? new {spec_return}(ptr) : null;")
    else:
        body_lines.append(f"\t\treturn {invoke};")

    return f"{sig}\n\t{{\n" + "\n".join(body_lines) + "\n\t}"


def generate_cs(spec: Dict, spec_name: str) -> str:
    class_name = spec["class"]
    namespace = spec.get("namespace", "Godot")
    methods = spec.get("methods", [])

    icall_decls = "\n\n".join(gen_cs_extern_decl(spec, m) for m in methods)
    wrapper_methods = "\n\n".join(gen_cs_wrapper_method(spec, m) for m in methods)

    cs_class_body = CS_CLASS_TEMPLATE.format(
        class_name=class_name,
        icall_decls=icall_decls,
        wrapper_methods=wrapper_methods,
    )

    return CS_HEADER.format(
        spec_name=spec_name,
        class_name=class_name,
        namespace=namespace,
        cs_class_body=cs_class_body,
    )


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description="Generate C++ glue and C# wrappers from a JSON spec.")
    ap.add_argument("--spec", required=True, help="Path to JSON spec file.")
    ap.add_argument("--cpp-out", help="Output path for the C++ glue file. If omitted, prints to stdout.")
    ap.add_argument("--cs-out", help="Output path for the C# wrapper file. If omitted, prints to stdout.")
    ap.add_argument("--dry-run", action="store_true", help="Print to stdout instead of writing files.")
    args = ap.parse_args()

    if not os.path.isfile(args.spec):
        print(f"[ERROR] Spec file not found: {args.spec}", file=sys.stderr)
        sys.exit(1)

    with open(args.spec, "r", encoding="utf-8") as f:
        spec = json.load(f)

    spec_name = os.path.basename(args.spec)
    cpp_code = generate_cpp(spec, spec_name)
    cs_code = generate_cs(spec, spec_name)

    if args.dry_run or (not args.cpp_out and not args.cs_out):
        print("===== C++ GLUE =====")
        print(cpp_code)
        print("\n===== C# WRAPPER =====")
        print(cs_code)
        return

    if args.cpp_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.cpp_out)) or ".", exist_ok=True)
        with open(args.cpp_out, "w", encoding="utf-8") as f:
            f.write(cpp_code)
        print(f"[OK] Wrote C++ glue to {args.cpp_out}")

    if args.cs_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.cs_out)) or ".", exist_ok=True)
        with open(args.cs_out, "w", encoding="utf-8") as f:
            f.write(cs_code)
        print(f"[OK] Wrote C# wrapper to {args.cs_out}")


if __name__ == "__main__":
    main()
