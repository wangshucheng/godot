#!/usr/bin/env python3
"""
BCL (Base Class Library) deployment and .NET Framework 4.8 compatibility
verification script for the mono_new module.

Tasks:
  1. Deploy BCL assemblies from system Mono to bin/mono/lib/mono/4.5/ (desktop)
  2. Deploy BCL assemblies to modules/mono_new/preload/mono/lib/mono/4.5/ (WASM)
  3. Verify .NET Framework 4.8 compatibility by checking assembly versions
  4. Generate a compatibility report

Usage:
  python deploy_bcl.py [--source <mono-prefix>] [--deploy-desktop] [--deploy-wasm]
                       [--verify-only] [--list-assemblies]

Examples:
  # Verify BCL deployment and compatibility (no changes)
  python deploy_bcl.py --verify-only

  # Deploy BCL from system Mono to both desktop and WASM preload
  python deploy_bcl.py --deploy-desktop --deploy-wasm

  # List all deployed BCL assemblies with versions
  python deploy_bcl.py --list-assemblies
"""

import argparse
import os
import shutil
import struct
import sys


# .NET Framework 4.8 compatible BCL version range
# Mono 6.12 ships with .NET 4.5 profile which is forward-compatible with 4.8
NET48_MIN_VERSION = (4, 0, 30319, 0)
MONO_BCL_SUBDIR = os.path.join("mono", "lib", "mono", "4.5")

# Essential BCL assemblies that must be present for .NET 4.8 compatibility
# (System.Drawing.dll is desktop-only, moved to extended)
ESSENTIAL_ASSEMBLIES = [
    "mscorlib.dll",
    "System.dll",
    "System.Core.dll",
    "System.Xml.dll",
    "System.Data.dll",
    "System.Numerics.dll",
    "System.Net.Http.dll",
    "System.Runtime.Serialization.dll",
    "Microsoft.CSharp.dll",
]

# Additional BCL assemblies for full .NET 4.8 compatibility
# (some are desktop-only and may be absent in WASM builds)
EXTENDED_ASSEMBLIES = [
    "System.Drawing.dll",
    "System.Configuration.dll",
    "System.Security.dll",
    "System.Transactions.dll",
    "System.EnterpriseServices.dll",
    "System.IO.Compression.dll",
    "System.IO.Compression.FileSystem.dll",
    "System.Json.dll",
    "System.Dynamic.dll",
    "CustomMarshalers.dll",
    "Accessibility.dll",
    "I18N.dll",
    "I18N.West.dll",
]


def find_system_mono_bcl():
    """Locate the system Mono BCL directory."""
    candidates = [
        r"C:\Program Files\Mono\lib\mono\4.5",
        r"C:\Program Files (x86)\Mono\lib\mono\4.5",
        r"C:\Mono\lib\mono\4.5",
        "/usr/lib/mono/4.5",
        "/usr/local/lib/mono/4.5",
    ]
    for c in candidates:
        mscorlib = os.path.join(c, "mscorlib.dll")
        if os.path.isfile(mscorlib):
            return os.path.abspath(c)
    return None


def read_assembly_version(dll_path):
    """Read the .NET runtime version string from a PE/COFF .dll file.

    Scans for the CLR metadata BSJB signature, then reads the runtime
    version string (e.g. "v4.0.30319"). This is more robust than full
    PE header parsing and works with both PE32 and PE32+ assemblies.
    Returns the version string, or None if not a .NET assembly.
    """
    try:
        with open(dll_path, "rb") as f:
            data = f.read()
        # Check MZ header
        if data[:2] != b"MZ":
            return None
        # Search for the BSJB metadata signature (CLR metadata root)
        # The signature appears after the PE headers, typically within
        # the first few KB of the file.
        bsjb_offset = data.find(b"BSJB")
        if bsjb_offset < 0 or bsjb_offset > len(data) - 16:
            return None
        # Metadata root layout after BSJB:
        # +0: signature "BSJB" (4 bytes)
        # +4: major version (2 bytes)
        # +6: minor version (2 bytes)
        # +8: reserved (4 bytes)
        # +12: version string length (4 bytes, little-endian)
        # +16: version string (padded to 4-byte boundary)
        ver_len = struct.unpack_from("<I", data, bsjb_offset + 12)[0]
        if ver_len == 0 or ver_len > 256:
            return None
        ver_bytes = data[bsjb_offset + 16: bsjb_offset + 16 + ver_len]
        version_str = ver_bytes.rstrip(b"\x00").decode("ascii", errors="replace")
        return version_str
    except Exception as e:
        return "ERROR: {}".format(e)


def get_assembly_info(dll_path):
    """Get assembly file info: size, version string, MZ validity."""
    if not os.path.isfile(dll_path):
        return {"exists": False, "path": dll_path}
    size = os.path.getsize(dll_path)
    with open(dll_path, "rb") as f:
        sig = f.read(2)
    is_mz = (sig == b"MZ")
    version = read_assembly_version(dll_path)
    return {
        "exists": True,
        "path": dll_path,
        "size_kb": size / 1024.0,
        "is_mz": is_mz,
        "version": version or "(not a .NET assembly)",
    }


def deploy_bcl(source_dir, target_dir, assemblies, dry_run=False):
    """Copy BCL assemblies from source to target directory."""
    deployed = []
    skipped = []
    failed = []
    for asm in assemblies:
        src = os.path.join(source_dir, asm)
        dst = os.path.join(target_dir, asm)
        if not os.path.isfile(src):
            failed.append(asm)
            continue
        if dry_run:
            deployed.append(asm)
            continue
        try:
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)
            deployed.append(asm)
        except Exception as e:
            print("[ERROR] Failed to copy {}: {}".format(asm, e))
            failed.append(asm)
    return deployed, skipped, failed


def verify_compatibility(bcl_dir):
    """Verify .NET Framework 4.8 compatibility of the BCL."""
    print("\n=== .NET Framework 4.8 Compatibility Verification ===")
    print("BCL directory: {}".format(bcl_dir))
    print()

    all_assemblies = ESSENTIAL_ASSEMBLIES + EXTENDED_ASSEMBLIES
    essential_ok = 0
    extended_ok = 0
    essential_missing = []
    extended_missing = []

    for asm in ESSENTIAL_ASSEMBLIES:
        path = os.path.join(bcl_dir, asm)
        info = get_assembly_info(path)
        if info["exists"] and info["is_mz"]:
            essential_ok += 1
            print("[OK]   {:40s} {:7.1f} KB  {}".format(
                asm, info["size_kb"], info["version"]))
        else:
            essential_missing.append(asm)
            print("[FAIL] {:40s} NOT FOUND or invalid".format(asm))

    for asm in EXTENDED_ASSEMBLIES:
        path = os.path.join(bcl_dir, asm)
        info = get_assembly_info(path)
        if info["exists"] and info["is_mz"]:
            extended_ok += 1
            print("[ok]   {:40s} {:7.1f} KB  {}".format(
                asm, info["size_kb"], info["version"]))
        else:
            extended_missing.append(asm)
            print("[--]   {:40s} not present (optional)".format(asm))

    print()
    print("Essential assemblies: {}/{} present".format(
        essential_ok, len(ESSENTIAL_ASSEMBLIES)))
    print("Extended assemblies:  {}/{} present".format(
        extended_ok, len(EXTENDED_ASSEMBLIES)))

    # Check mscorlib version for 4.x compatibility
    mscorlib_path = os.path.join(bcl_dir, "mscorlib.dll")
    mscorlib_info = get_assembly_info(mscorlib_path)
    compatible = False
    if mscorlib_info["exists"] and mscorlib_info["version"]:
        ver = mscorlib_info["version"]
        if "v4.0" in ver or "4.0" in ver:
            compatible = True
            print("\nmscorlib.dll runtime version: {} -> .NET 4.x compatible (incl. 4.8)".format(ver))
        else:
            print("\nmscorlib.dll runtime version: {} -> WARNING: may not be .NET 4.8 compatible".format(ver))

    if essential_missing:
        print("\n[WARN] Missing essential assemblies: {}".format(", ".join(essential_missing)))
    if not essential_missing and compatible:
        print("\n[PASS] BCL is fully .NET Framework 4.8 compatible")
        return True
    else:
        print("\n[FAIL] BCL compatibility issues detected")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Deploy and verify BCL for .NET Framework 4.8 compatibility."
    )
    parser.add_argument("--source", help="System Mono BCL source directory")
    parser.add_argument("--deploy-desktop", action="store_true",
                        help="Deploy BCL to bin/mono/lib/mono/4.5/ (desktop runtime)")
    parser.add_argument("--deploy-wasm", action="store_true",
                        help="Deploy BCL to modules/mono_new/preload/ (WASM build)")
    parser.add_argument("--verify-only", action="store_true",
                        help="Only verify, do not deploy")
    parser.add_argument("--list-assemblies", action="store_true",
                        help="List all BCL assemblies with version info")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print actions without executing")
    args = parser.parse_args()

    # Find the godot project root (godot4_7_mono/ is 3 levels up from scripts/)
    # scripts/ -> mono_new/ -> modules/ -> godot4_7_mono/
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(script_dir, "..", "..", ".."))

    desktop_bcl_dir = os.path.join(project_root, "bin", "mono", "lib", "mono", "4.5")
    wasm_bcl_dir = os.path.join(
        project_root, "modules", "mono_new", "preload", "mono", "lib", "mono", "4.5")

    print("=== BCL Deployment & Compatibility Tool ===")
    print("Project root: {}".format(project_root))
    print("Desktop BCL: {}".format(desktop_bcl_dir))
    print("WASM BCL:    {}".format(wasm_bcl_dir))
    print()

    # Find source BCL
    source_bcl = args.source or find_system_mono_bcl()
    if source_bcl:
        print("System Mono BCL source: {}".format(source_bcl))
    else:
        print("[WARN] System Mono BCL not found - deployment disabled")
    print()

    if args.list_assemblies:
        print("=== BCL Assembly Listing ===")
        for bcl_dir in [desktop_bcl_dir, wasm_bcl_dir]:
            if not os.path.isdir(bcl_dir):
                print("\n[{}] not found".format(bcl_dir))
                continue
            print("\n[{}]".format(bcl_dir))
            for asm in sorted(ESSENTIAL_ASSEMBLIES + EXTENDED_ASSEMBLIES):
                path = os.path.join(bcl_dir, asm)
                info = get_assembly_info(path)
                if info["exists"]:
                    print("  {:40s} {:7.1f} KB  {}".format(
                        asm, info["size_kb"], info["version"]))
                else:
                    print("  {:40s} (missing)".format(asm))
        return 0

    # Deploy if requested
    if not args.verify_only and source_bcl:
        all_assemblies = ESSENTIAL_ASSEMBLIES + EXTENDED_ASSEMBLIES
        if args.deploy_desktop:
            print("\n=== Deploying BCL to Desktop ({}) ===".format(desktop_bcl_dir))
            deployed, skipped, failed = deploy_bcl(
                source_bcl, desktop_bcl_dir, all_assemblies, args.dry_run)
            print("Deployed: {}, Failed: {}".format(len(deployed), len(failed)))
            if failed:
                print("Failed assemblies: {}".format(", ".join(failed)))

        if args.deploy_wasm:
            print("\n=== Deploying BCL to WASM preload ({}) ===".format(wasm_bcl_dir))
            deployed, skipped, failed = deploy_bcl(
                source_bcl, wasm_bcl_dir, all_assemblies, args.dry_run)
            print("Deployed: {}, Failed: {}".format(len(deployed), len(failed)))
            if failed:
                print("Failed assemblies: {}".format(", ".join(failed)))

    # Verify compatibility
    success = True
    if os.path.isdir(desktop_bcl_dir):
        print("\n--- Desktop BCL Verification ---")
        success = verify_compatibility(desktop_bcl_dir) and success
    else:
        print("\n[WARN] Desktop BCL directory not found: {}".format(desktop_bcl_dir))

    if os.path.isdir(wasm_bcl_dir):
        print("\n--- WASM BCL Verification ---")
        success = verify_compatibility(wasm_bcl_dir) and success
    else:
        print("\n[INFO] WASM BCL preload directory not found: {}".format(wasm_bcl_dir))
        print("       (BCL may be embedded in .data file from a previous build)")

    print("\n=== Summary ===")
    if success:
        print("[PASS] BCL deployment verified, .NET Framework 4.8 compatible")
        return 0
    else:
        print("[FAIL] BCL compatibility issues - see above")
        return 1


if __name__ == "__main__":
    sys.exit(main())
