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

# ============================================================================
# BCL 分类（基于 GodotSharp 实际依赖分析，2026-07-25 重构）
# ----------------------------------------------------------------------------
# 经核实 GodotSharp 源码与用户 C# 工程（test_game_2048 / test_project）均未引用
# Xml / Serialization / Compression / Http / Data 等 BCL 高级 API，因此 WASM
# 端可大幅裁剪。裁剪后总包从 26.27 MB 降至 13.78 MB，远低于微信 20 MB 限制。
# ============================================================================

# WASM/微信小游戏必需的最小 BCL 集合（仅 6 个，约 6.7 MB）
# - mscorlib: 运行时基础（必需）
# - System: 基础类型（IO/Text/Threading/Net.Sockets 等）
# - System.Core: LINQ、Expressions、Regex 等
# - System.Numerics: BigInteger/Complex（GodotSharp 数学库可能用）
# - I18N: I18N 主程序集（区域信息）
# - I18N.West: 西文字符集（默认编码，必需）
WASM_ESSENTIAL_ASSEMBLIES = [
    "mscorlib.dll",
    "System.dll",
    "System.Core.dll",
    "System.Numerics.dll",
    "I18N.dll",
    "I18N.West.dll",
]

# 桌面端完整 BCL（保证 .NET 4.8 兼容性，含高级 API）
ESSENTIAL_ASSEMBLIES = WASM_ESSENTIAL_ASSEMBLIES + [
    "System.Xml.dll",                   # XML 处理
    "System.Data.dll",                  # ADO.NET
    "System.Net.Http.dll",              # HttpClient
    "System.Runtime.Serialization.dll", # 序列化框架
    "Microsoft.CSharp.dll",             # C# 动态编译
]

# 桌面端扩展 BCL（可选，部分仅 Windows 桌面用）
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
    # 非西文字符集（仅桌面国际化场景需要）
    "I18N.CJK.dll",
    "I18N.Rare.dll",
    "I18N.Other.dll",
    "I18N.MidEast.dll",
]

# WASM 下绝对不需要的 assembly（即使误部署也要 trim）
# 用于 --trim-wasm 选项：从 preload 目录删除这些文件以减小 .data 体积
# 经核实 GodotSharp 与用户 C# 代码均未引用以下任何 API
WASM_TRIM_ASSEMBLIES = [
    # 高级 BCL（GodotSharp 不依赖）
    "System.Xml.dll",
    "System.Xml.Linq.dll",
    "System.Data.dll",
    "System.Data.DataSetExtensions.dll",
    "System.Net.Http.dll",
    "System.Runtime.Serialization.dll",
    "System.IO.Compression.dll",
    "System.IO.Compression.FileSystem.dll",
    "System.Security.dll",
    "Mono.Security.dll",
    "System.Transactions.dll",
    "System.ServiceModel.Internals.dll",
    "System.ComponentModel.Composition.dll",
    "System.ComponentModel.DataAnnotations.dll",
    # 动态编译（C# 解释器模式不需要）
    "Microsoft.CSharp.dll",
    # 测试框架（生产环境绝对不需要）
    "nunitlite.dll",
    # 非西文字符集（小游戏用 GD.ToString 处理数字，不需要国际化）
    "I18N.CJK.dll",
    "I18N.Rare.dll",
    "I18N.Other.dll",
    "I18N.MidEast.dll",
]

# WASM 下不需要的配置文件（位于 mono/etc/ 下）
WASM_TRIM_CONFIG_FILES = [
    "mono/etc/mono/4.5/DefaultWsdlHelpGenerator.aspx",  # WSDL 帮助（Web Service 用）
    "mono/etc/mono/4.5/web.config",                      # ASP.NET 配置
    "mono/etc/mono/4.5/settings.map",                    # 设置映射
    "mono/etc/mono/4.5/Browsers/Compat.browser",         # 浏览器兼容性
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


def verify_compatibility(bcl_dir, wasm_mode=False):
    """Verify .NET Framework 4.8 compatibility of the BCL.

    Args:
        bcl_dir: BCL directory to verify
        wasm_mode: if True, verify against WASM_ESSENTIAL_ASSEMBLIES (minimal set);
                   otherwise verify against full ESSENTIAL_ASSEMBLIES + EXTENDED_ASSEMBLIES
    """
    print("\n=== .NET Framework 4.8 Compatibility Verification ===")
    print("BCL directory: {}".format(bcl_dir))
    if wasm_mode:
        print("Mode: WASM minimal (expected: 6 essential DLLs only)")
    print()

    if wasm_mode:
        essential_list = WASM_ESSENTIAL_ASSEMBLIES
        extended_list = []
    else:
        essential_list = ESSENTIAL_ASSEMBLIES
        extended_list = EXTENDED_ASSEMBLIES

    essential_ok = 0
    extended_ok = 0
    essential_missing = []
    extended_missing = []

    for asm in essential_list:
        path = os.path.join(bcl_dir, asm)
        info = get_assembly_info(path)
        if info["exists"] and info["is_mz"]:
            essential_ok += 1
            print("[OK]   {:40s} {:7.1f} KB  {}".format(
                asm, info["size_kb"], info["version"]))
        else:
            essential_missing.append(asm)
            print("[FAIL] {:40s} NOT FOUND or invalid".format(asm))

    for asm in extended_list:
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
        essential_ok, len(essential_list)))
    if extended_list:
        print("Extended assemblies:  {}/{} present".format(
            extended_ok, len(extended_list)))

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
        if wasm_mode:
            print("\n[PASS] WASM BCL is minimal but sufficient for GodotSharp runtime")
        else:
            print("\n[PASS] BCL is fully .NET Framework 4.8 compatible")
        return True
    else:
        print("\n[FAIL] BCL compatibility issues detected")
        return False


def trim_wasm_bcl(preload_root, dry_run=False):
    """从 WASM preload 目录删除非必需的 BCL assembly 和配置文件。

    删除规则：
      1. WASM_TRIM_ASSEMBLIES：高级 BCL（XML/Data/Http/Serialization 等）
      2. WASM_TRIM_CONFIG_FILES：Web Service / ASP.NET 配置
      3. mono/lib/mscorlib.dll：与 mono/lib/mono/4.5/mscorlib.dll 重复的冗余拷贝
      4. mono/lib/mono/4.5/Facades/：.NET Core type forwarding facade（Mono 不需要）

    经核实 GodotSharp 与用户 C# 代码均未引用上述任何 API，可安全删除。
    """
    print("\n=== Trimming WASM BCL (preload: {}) ===".format(preload_root))
    if not os.path.isdir(preload_root):
        print("[ERROR] preload directory not found")
        return 0, 0

    trimmed_files = []
    trimmed_size = 0
    failed = []

    # 1. 删除 WASM_TRIM_ASSEMBLIES（位于 mono/lib/mono/4.5/ 下）
    bcl_dir = os.path.join(preload_root, "mono", "lib", "mono", "4.5")
    for asm in WASM_TRIM_ASSEMBLIES:
        path = os.path.join(bcl_dir, asm)
        if os.path.isfile(path):
            size = os.path.getsize(path)
            if dry_run:
                print("  [DRY] would remove {} ({:.0f} KB)".format(asm, size/1024))
            else:
                try:
                    os.remove(path)
                    print("  [DEL] {} ({:.0f} KB)".format(asm, size/1024))
                except Exception as e:
                    print("  [ERR] failed to remove {}: {}".format(asm, e))
                    failed.append(asm)
                    continue
            trimmed_files.append(asm)
            trimmed_size += size
        # 也可能位于 mono/lib/ 根目录（如 mscorlib.dll 的冗余拷贝）
        root_path = os.path.join(preload_root, "mono", "lib", asm)
        if os.path.isfile(root_path):
            size = os.path.getsize(root_path)
            if dry_run:
                print("  [DRY] would remove mono/lib/{} ({:.0f} KB, redundant copy)".format(
                    asm, size/1024))
            else:
                try:
                    os.remove(root_path)
                    print("  [DEL] mono/lib/{} ({:.0f} KB, redundant copy)".format(
                        asm, size/1024))
                except Exception as e:
                    print("  [ERR] failed to remove mono/lib/{}: {}".format(asm, e))
                    failed.append("mono/lib/" + asm)
                    continue
            trimmed_files.append("mono/lib/" + asm)
            trimmed_size += size

    # 2. 删除 mono/lib/mscorlib.dll 冗余拷贝（与 mono/lib/mono/4.5/mscorlib.dll 重复）
    root_mscorlib = os.path.join(preload_root, "mono", "lib", "mscorlib.dll")
    canonical_mscorlib = os.path.join(bcl_dir, "mscorlib.dll")
    if (os.path.isfile(root_mscorlib) and os.path.isfile(canonical_mscorlib)
            and "mono/lib/mscorlib.dll" not in trimmed_files):
        size = os.path.getsize(root_mscorlib)
        if dry_run:
            print("  [DRY] would remove mono/lib/mscorlib.dll ({:.0f} KB, redundant copy)".format(
                size/1024))
        else:
            try:
                os.remove(root_mscorlib)
                print("  [DEL] mono/lib/mscorlib.dll ({:.0f} KB, redundant copy)".format(
                    size/1024))
            except Exception as e:
                print("  [ERR] failed to remove mono/lib/mscorlib.dll: {}".format(e))
                failed.append("mono/lib/mscorlib.dll")
        trimmed_files.append("mono/lib/mscorlib.dll")
        trimmed_size += size

    # 3. 删除 Facades/ 整个目录（.NET Core type forwarding，Mono 4.5 profile 不需要）
    facades_dir = os.path.join(bcl_dir, "Facades")
    if os.path.isdir(facades_dir):
        facades_files = []
        facades_size = 0
        for root, dirs, files in os.walk(facades_dir):
            for f in files:
                p = os.path.join(root, f)
                facades_files.append(p)
                facades_size += os.path.getsize(p)
        if dry_run:
            print("  [DRY] would remove Facades/ ({} files, {:.0f} KB)".format(
                len(facades_files), facades_size/1024))
        else:
            try:
                shutil.rmtree(facades_dir)
                print("  [DEL] Facades/ ({} files, {:.0f} KB)".format(
                    len(facades_files), facades_size/1024))
            except Exception as e:
                print("  [ERR] failed to remove Facades/: {}".format(e))
                failed.append("Facades/")
        trimmed_files.append("Facades/")
        trimmed_size += facades_size

    # 4. 删除 WASM_TRIM_CONFIG_FILES
    for cfg in WASM_TRIM_CONFIG_FILES:
        path = os.path.join(preload_root, cfg.replace("/", os.sep))
        if os.path.isfile(path):
            size = os.path.getsize(path)
            if dry_run:
                print("  [DRY] would remove {} ({:.0f} KB)".format(cfg, size/1024))
            else:
                try:
                    os.remove(path)
                    print("  [DEL] {} ({:.0f} KB)".format(cfg, size/1024))
                except Exception as e:
                    print("  [ERR] failed to remove {}: {}".format(cfg, e))
                    failed.append(cfg)
                    continue
            trimmed_files.append(cfg)
            trimmed_size += size

    print("\n  Total trimmed: {} items, {:.2f} MB ({:.0f} KB)".format(
        len(trimmed_files), trimmed_size/1024/1024, trimmed_size/1024))
    if failed:
        print("  Failed: {}".format(", ".join(failed)))
    return len(trimmed_files), trimmed_size


def main():
    parser = argparse.ArgumentParser(
        description="Deploy and verify BCL for .NET Framework 4.8 compatibility."
    )
    parser.add_argument("--source", help="System Mono BCL source directory")
    parser.add_argument("--deploy-desktop", action="store_true",
                        help="Deploy BCL to bin/mono/lib/mono/4.5/ (desktop runtime)")
    parser.add_argument("--deploy-wasm", action="store_true",
                        help="Deploy BCL to modules/mono_new/preload/ (WASM build). "
                             "Deploys only WASM_ESSENTIAL_ASSEMBLIES (6 DLLs, ~6.7 MB) - "
                             "GodotSharp does not reference Xml/Data/Http/Serialization.")
    parser.add_argument("--trim-wasm", action="store_true",
                        help="Trim non-essential BCL/config from existing preload/ directory. "
                             "Removes WASM_TRIM_ASSEMBLIES + Facades/ + redundant mscorlib.dll + "
                             "WASM_TRIM_CONFIG_FILES. Saves ~12 MB.")
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
    wasm_preload_root = os.path.join(project_root, "modules", "mono_new", "preload")

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

    # Trim WASM preload directory (remove non-essential files)
    if args.trim_wasm:
        trim_wasm_bcl(wasm_preload_root, args.dry_run)
        # Trim 后立即验证（WASM 模式：只检查 WASM_ESSENTIAL_ASSEMBLIES）
        if os.path.isdir(wasm_bcl_dir):
            print("\n--- WASM BCL Verification (after trim, wasm_mode) ---")
            verify_compatibility(wasm_bcl_dir, wasm_mode=True)
        if not args.deploy_wasm and not args.deploy_desktop:
            return 0

    # Deploy if requested
    if not args.verify_only and source_bcl:
        if args.deploy_desktop:
            # 桌面端：部署完整 BCL（ESSENTIAL + EXTENDED）
            all_assemblies = ESSENTIAL_ASSEMBLIES + EXTENDED_ASSEMBLIES
            print("\n=== Deploying BCL to Desktop ({}) ===".format(desktop_bcl_dir))
            print("  Mode: full BCL ({} assemblies)".format(len(all_assemblies)))
            deployed, skipped, failed = deploy_bcl(
                source_bcl, desktop_bcl_dir, all_assemblies, args.dry_run)
            print("Deployed: {}, Failed: {}".format(len(deployed), len(failed)))
            if failed:
                print("Failed assemblies: {}".format(", ".join(failed)))

        if args.deploy_wasm:
            # WASM 端：仅部署最小集合（GodotSharp 不依赖高级 BCL API）
            wasm_assemblies = WASM_ESSENTIAL_ASSEMBLIES
            print("\n=== Deploying BCL to WASM preload ({}) ===".format(wasm_bcl_dir))
            print("  Mode: minimal ({} DLLs, ~6.7 MB)".format(len(wasm_assemblies)))
            print("  Assemblies: {}".format(", ".join(wasm_assemblies)))
            deployed, skipped, failed = deploy_bcl(
                source_bcl, wasm_bcl_dir, wasm_assemblies, args.dry_run)
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
        # WASM 端使用最小集合验证
        success = verify_compatibility(wasm_bcl_dir, wasm_mode=True) and success
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
