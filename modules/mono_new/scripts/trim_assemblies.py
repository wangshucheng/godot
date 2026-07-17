#!/usr/bin/env python3
"""
IL Trimming / AOT Optimization script for the mono_new module.

Phase 9: Reduces C# assembly size for Web/WASM exports by:
  1. Running monolinker to remove unreachable IL code (dead code elimination)
  2. Running mono-cil-strip to strip debug symbols from release assemblies
  3. Optionally stripping .pdb/.mdb sidecar files

NOTE on AOT: True AOT compilation is NOT supported in this project because
  the WASM Mono runtime uses MONO_EE_MODE_INTERP (interpreter mode, value
  1000). MONO_AOT_MODE_INTERP (mode 5) triggers 'invalid CIL image' errors
  because it requires AOT trampoline modules that are not present. The
  mono-aot-cross tool is also not bundled with this Mono SDK. Therefore,
  the only size optimization available for WASM is IL trimming, not AOT.

Usage:
  python trim_assemblies.py --input <assembly.dll> [--root <root.dll> ...]
                            [--output <output-dir>]
                            [--references <ref.dll> ...]
                            [--strip-debug]
                            [--preserve-public]
                            [--monolinker <path>]
                            [--cil-strip <path>]
                            [--dry-run]

Examples:
  # Trim GodotSharp.dll conservatively (preserve all public API)
  python trim_assemblies.py \\
      --input bin/.mono/assemblies/GodotSharp.dll \\
      --output bin/.mono/assemblies/trimmed \\
      --references bin/mono/lib/mono/4.5/mscorlib.dll \\
      --preserve-public --strip-debug

  # Trim a user assembly
  python trim_assemblies.py \\
      --input .mono/assemblies/MyGame.dll \\
      --root .mono/assemblies/GodotSharp.dll \\
      --output .mono/assemblies/trimmed \\
      --references bin/mono/lib/mono/4.5/mscorlib.dll \\
      --strip-debug
"""

import argparse
import os
import shutil
import subprocess
import sys


def find_tool(name, explicit_path=None):
    """Locate a Mono SDK tool either via explicit path, PATH, or known location."""
    if explicit_path and os.path.isfile(explicit_path):
        return explicit_path
    # Search common Mono SDK locations relative to the Godot bin/ dir.
    candidates = [
        # Bundled with Godot 4.7 Mono (Windows)
        os.path.join("bin", "mono", "lib", "mono", "4.5", name),
        os.path.join("bin", "mono", "lib", "mono", "4.5", name + ".exe"),
        # System Mono install
        os.path.join("C:\\Program Files\\Mono", "lib", "mono", "4.5", name),
        os.path.join("C:\\Program Files\\Mono", "lib", "mono", "4.5", name + ".exe"),
    ]
    for c in candidates:
        if os.path.isfile(c):
            return os.path.abspath(c)
    # Try PATH
    found = shutil.which(name) or shutil.which(name + ".exe")
    if found:
        return found
    return None


def find_mono_runtime():
    """Locate the mono.exe runtime host needed to run Mono SDK tools.

    The .exe tools in bin/mono/lib/mono/4.5/ are .NET assemblies that require
    a Mono runtime host (not .NET Framework, which lacks some Mono-specific
    APIs like string.Split(char, StringSplitOptions)).
    """
    candidates = [
        "C:\\Program Files\\Mono\\bin\\mono.exe",
        "C:\\Program Files (x86)\\Mono\\bin\\mono.exe",
        "C:\\Mono\\bin\\mono.exe",
    ]
    for c in candidates:
        if os.path.isfile(c):
            return c
    # Try PATH
    found = shutil.which("mono") or shutil.which("mono.exe")
    if found:
        return found
    return None


def build_tool_cmd(tool_path, mono_runtime, *args):
    """Build a command list, prepending mono.exe if the tool needs a runtime host.

    On Windows, the .NET Framework can run .exe assemblies directly, but Mono
    SDK tools depend on Mono-specific APIs. We always use mono.exe when
    available to ensure compatibility. On Unix, mono is typically in PATH.
    """
    if mono_runtime:
        return [mono_runtime, tool_path] + list(args)
    return [tool_path] + list(args)


def run_monolinker(monolinker, mono_runtime, input_asm, output_dir, roots, references,
                   preserve_public, dry_run):
    """Run monolinker for dead code elimination.

    Conservative settings are used because C++ invokes C# methods via
    mono_runtime_invoke(), which the linker's static reachability analysis
    cannot see. We preserve all public types/methods to avoid stripping
    code that is reached only through native interop.
    """
    if not monolinker:
        print("[WARN] monolinker not found - skipping IL dead code elimination")
        return False

    os.makedirs(output_dir, exist_ok=True)

    # Build args (without the tool path itself)
    args = []
    # Root assemblies (entry points for reachability analysis)
    args.extend(["-a", input_asm])
    for root in roots:
        args.extend(["-a", root])

    # Output directory for trimmed assemblies (monolinker uses -out, not -o)
    args.extend(["-out", output_dir])

    # Framework assembly action: 'skip' = don't modify BCL/framework assemblies.
    # This is critical - modifying mscorlib.dll would break the Mono runtime.
    args.extend(["-c", "skip"])

    # User assembly action: when preserve_public is set, use 'skip' (don't trim
    # user assemblies either) because C++ -> C# interop via mono_runtime_invoke
    # is invisible to the linker's reachability analysis. Without preserve_public,
    # use 'link' (default) to trim unreachable code.
    if preserve_public:
        args.extend(["-u", "skip"])
    # else: default is '-u link' which trims user assemblies

    # Reference assemblies (BCL etc.) for type resolution
    for ref in references:
        args.extend(["-reference", ref])

    cmd = build_tool_cmd(monolinker, mono_runtime, *args)
    print("[INFO] monolinker command:", " ".join(cmd))
    if dry_run:
        print("[DRY-RUN] Skipping execution")
        return True

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        if result.returncode != 0:
            print("[ERROR] monolinker failed (exit {}): {}".format(
                result.returncode, result.stderr or result.stdout))
            return False
        print("[INFO] monolinker succeeded")
        if result.stdout:
            print("  stdout:", result.stdout[:500])
        return True
    except subprocess.TimeoutExpired:
        print("[ERROR] monolinker timed out")
        return False
    except FileNotFoundError as e:
        print("[ERROR] monolinker not executable:", e)
        return False


def run_cil_strip(cil_strip, mono_runtime, input_asm, output_asm, dry_run):
    """Strip debug symbols and metadata from an assembly using mono-cil-strip.

    mono-cil-strip takes a single positional file argument and modifies it
    in place. To avoid clobbering the input, we copy first then strip the
    copy.
    """
    if not cil_strip:
        print("[WARN] mono-cil-strip not found - skipping debug symbol stripping")
        return False

    # mono-cil-strip modifies in place; copy input to output first.
    shutil.copy2(input_asm, output_asm)

    cmd = build_tool_cmd(cil_strip, mono_runtime, output_asm)
    print("[INFO] mono-cil-strip command:", " ".join(cmd))
    if dry_run:
        print("[DRY-RUN] Skipping execution")
        return True

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        if result.returncode != 0:
            print("[ERROR] mono-cil-strip failed (exit {}): {}".format(
                result.returncode, result.stderr or result.stdout))
            return False
        print("[INFO] mono-cil-strip succeeded:", output_asm)
        return True
    except (subprocess.TimeoutExpired, FileNotFoundError) as e:
        print("[ERROR] mono-cil-strip failed:", e)
        return False


def strip_debug_sidecars(assembly_path):
    """Remove .pdb and .mdb sidecar files next to an assembly."""
    base, _ = os.path.splitext(assembly_path)
    for ext in (".pdb", ".mdb"):
        sidecar = base + ext
        if os.path.isfile(sidecar):
            os.remove(sidecar)
            print("[INFO] Removed debug sidecar:", sidecar)


def get_assembly_size(path):
    """Return assembly size in KB for reporting."""
    if not os.path.isfile(path):
        return 0
    return os.path.getsize(path) / 1024.0


def main():
    parser = argparse.ArgumentParser(
        description="Trim C# assemblies for WASM/Web export (Phase 9 IL optimization)."
    )
    parser.add_argument("--input", required=True,
                        help="Input assembly (.dll) to trim")
    parser.add_argument("--output", default="trimmed",
                        help="Output directory for trimmed assembly")
    parser.add_argument("--root", action="append", default=[],
                        help="Additional root assembly for reachability analysis "
                             "(can be specified multiple times)")
    parser.add_argument("--references", action="append", default=[],
                        help="Reference assemblies for type resolution (e.g. mscorlib.dll)")
    parser.add_argument("--strip-debug", action="store_true",
                        help="Also strip debug symbols via mono-cil-strip")
    parser.add_argument("--preserve-public", action="store_true",
                        help="Preserve all public types/methods (recommended for "
                             "assemblies invoked from C++ via mono_runtime_invoke)")
    parser.add_argument("--monolinker",
                        help="Explicit path to monolinker.exe")
    parser.add_argument("--cil-strip",
                        help="Explicit path to mono-cil-strip.exe")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print commands without executing")
    args = parser.parse_args()

    input_asm = os.path.abspath(args.input)
    if not os.path.isfile(input_asm):
        print("[ERROR] Input assembly not found:", input_asm)
        return 1

    print("[INFO] Input assembly:", input_asm)
    print("[INFO] Input size: {:.1f} KB".format(get_assembly_size(input_asm)))

    # Locate tools
    monolinker = find_tool("monolinker", args.monolinker)
    cil_strip = find_tool("mono-cil-strip", args.cil_strip)
    mono_runtime = find_mono_runtime()
    print("[INFO] monolinker:", monolinker or "(not found)")
    print("[INFO] mono-cil-strip:", cil_strip or "(not found)")
    print("[INFO] mono runtime:", mono_runtime or "(not found - tools may fail on Windows)")

    output_dir = os.path.abspath(args.output)
    print("[INFO] Output directory:", output_dir)

    # Step 1: Run monolinker for dead code elimination
    success = run_monolinker(
        monolinker, mono_runtime, input_asm, output_dir, args.root,
        args.references, args.preserve_public, args.dry_run
    )

    # Locate the trimmed output (monolinker copies the trimmed assembly to
    # <output_dir>/<assembly-name>.dll)
    trimmed_asm = os.path.join(output_dir, os.path.basename(input_asm))

    if success and os.path.isfile(trimmed_asm):
        print("[INFO] Trimmed size: {:.1f} KB".format(get_assembly_size(trimmed_asm)))
        reduction = get_assembly_size(input_asm) - get_assembly_size(trimmed_asm)
        if get_assembly_size(input_asm) > 0:
            pct = (reduction / get_assembly_size(input_asm)) * 100.0
            print("[INFO] Reduction: {:.1f} KB ({:.1f}%)".format(reduction, pct))

    # Step 2: Optionally strip debug symbols
    if args.strip_debug:
        strip_target = trimmed_asm if os.path.isfile(trimmed_asm) else input_asm
        stripped_output = strip_target + ".stripped"
        if run_cil_strip(cil_strip, mono_runtime, strip_target, stripped_output, args.dry_run):
            # Replace the original trimmed file with the stripped version
            if os.path.isfile(stripped_output):
                os.replace(stripped_output, strip_target)
                print("[INFO] Stripped size: {:.1f} KB".format(
                    get_assembly_size(strip_target)))
        # Remove sidecar debug files
        if not args.dry_run:
            strip_debug_sidecars(strip_target)

    print("[INFO] Done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
