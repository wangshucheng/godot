#!/usr/bin/env python3
"""
NuGet package restore for the mono_new module.

Restores NuGet packages declared in a project's `packages.config` file by
downloading .nupkg archives directly from nuget.org and extracting them to
a `packages/` directory next to the project. Returns the list of managed
DLL references that should be passed to the C# compiler.

This avoids the dependency on nuget.exe or dotnet CLI's `restore` (which
requires SDK-style .csproj). It is intentionally minimal: it downloads,
extracts, and resolves references — it does NOT run install scripts or
transform config files.

Usage:
  python nuget_restore.py --project-dir <res://>
                          [--source <nuget-source-url>]
                          [--no-restore]
                          [--print-references]

Exit codes:
  0  Success (or packages.config not found and --no-restore given)
  1  Fatal error (network, parse, extraction)

Output (when --print-references is set):
  One reference path per line, absolute, POSIX-style slashes:
    C:/project/packages/Newtonsoft.Json.13.0.1/lib/net45/Newtonsoft.Json.dll
"""

import argparse
import json
import os
import sys
import shutil
import tempfile
import urllib.request
import urllib.error
import zipfile
import xml.etree.ElementTree as ET


DEFAULT_NUGET_SOURCE = "https://api.nuget.org/v3-flatcontainer/"
DEFAULT_USER_AGENT = "mono_new-nuget-restore/1.0"


def log_info(msg):
    print("[NuGet] " + msg, file=sys.stderr)


def log_error(msg):
    print("[NuGet ERROR] " + msg, file=sys.stderr)


def parse_packages_config(packages_config_path):
    """Parse packages.config and return a list of (id, version) tuples.

    packages.config format:
      <?xml version="1.0" encoding="utf-8"?>
      <packages>
        <package id="Newtonsoft.Json" version="13.0.1" targetFramework="net45" />
      </packages>
    """
    if not os.path.isfile(packages_config_path):
        return []

    try:
        tree = ET.parse(packages_config_path)
    except ET.ParseError as e:
        log_error("Failed to parse {}: {}".format(packages_config_path, e))
        return []

    root = tree.getroot()
    packages = []
    for pkg in root.findall("package"):
        pkg_id = pkg.get("id")
        pkg_version = pkg.get("version")
        if not pkg_id or not pkg_version:
            continue
        packages.append((pkg_id, pkg_version))
    return packages


def download_nupkg(pkg_id, pkg_version, source_url, dest_path):
    """Download a .nupkg file from the NuGet flat container API.

    URL pattern: {source}/{lowercase-id}/{version}/{lowercase-id}.{version}.nupkg
    """
    lower_id = pkg_id.lower()
    url = "{}/{}/{}/{}.{}.nupkg".format(
        source_url.rstrip("/"), lower_id, pkg_version, lower_id, pkg_version
    )
    log_info("Downloading: {}".format(url))
    req = urllib.request.Request(url, headers={"User-Agent": DEFAULT_USER_AGENT})
    try:
        with urllib.request.urlopen(req, timeout=60) as resp:
            data = resp.read()
    except urllib.error.HTTPError as e:
        log_error("HTTP {} downloading {}: {}".format(e.code, pkg_id, e.reason))
        return False
    except urllib.error.URLError as e:
        log_error("Network error downloading {}: {}".format(pkg_id, str(e.reason)))
        return False

    with open(dest_path, "wb") as f:
        f.write(data)
    log_info("Downloaded {} {} ({} KB)".format(
        pkg_id, pkg_version, len(data) / 1024.0))
    return True


def extract_nupkg(nupkg_path, extract_dir):
    """Extract a .nupkg (zip) to the given directory."""
    try:
        with zipfile.ZipFile(nupkg_path, "r") as zf:
            zf.extractall(extract_dir)
    except (zipfile.BadZipFile, OSError) as e:
        log_error("Failed to extract {}: {}".format(nupkg_path, e))
        return False
    return True


def find_best_lib_dir(extract_dir, pkg_id):
    """Find the best lib/ subdirectory for .NET Framework 4.x.

    NuGet packages store framework-specific DLLs under lib/<tfm>/.
    We prefer (in order): net48, net462, net461, net46, net45, net40, net35.
    If none match, fall back to the first lib/ subdirectory alphabetically.
    Returns the absolute path to the directory containing the .dll files.
    """
    lib_dir = os.path.join(extract_dir, "lib")
    if not os.path.isdir(lib_dir):
        return None

    # Preferred TFMs in descending order of preference for net48 target.
    preferred_tfms = [
        "net48", "net472", "net471", "net47",
        "net462", "net461", "net46",
        "net452", "net451", "net45",
        "net403", "net40", "net35", "net20", "net11",
        # Some packages use portable/profiles as fallback
    ]

    subdirs = [d for d in os.listdir(lib_dir)
               if os.path.isdir(os.path.join(lib_dir, d))]
    if not subdirs:
        return None

    for tfm in preferred_tfms:
        if tfm in subdirs:
            return os.path.join(lib_dir, tfm)

    # Fallback: pick the first subdir that contains at least one .dll
    for d in sorted(subdirs):
        dpath = os.path.join(lib_dir, d)
        for f in os.listdir(dpath):
            if f.lower().endswith(".dll"):
                return dpath

    return None


def find_managed_dlls(lib_dir, pkg_id):
    """Find all .dll files in a lib directory.

    Returns a list of absolute paths. Excludes native DLLs (.ni.dll, .dll
    files under runtimes/ which are native) and XML documentation files.
    """
    if not lib_dir or not os.path.isdir(lib_dir):
        return []

    dlls = []
    for f in sorted(os.listdir(lib_dir)):
        if f.lower().endswith(".dll"):
            # Skip .ni.dll (NGen images) - these are not managed assemblies.
            if f.lower().endswith(".ni.dll"):
                continue
            dlls.append(os.path.join(lib_dir, f))
    return dlls


def restore_package(pkg_id, pkg_version, packages_dir, source_url, force=False):
    """Restore a single package. Returns list of managed DLL paths.

    The package is extracted to: <packages_dir>/<id>.<version>/
    """
    pkg_dir_name = "{}.{}".format(pkg_id, pkg_version)
    pkg_extract_dir = os.path.join(packages_dir, pkg_dir_name)

    # If already restored and not forced, skip download.
    sentinel = os.path.join(pkg_extract_dir, ".restored")
    if os.path.isfile(sentinel) and not force:
        log_info("Already restored: {} {}".format(pkg_id, pkg_version))
    else:
        os.makedirs(packages_dir, exist_ok=True)
        # Download to a temp file then extract.
        with tempfile.NamedTemporaryFile(suffix=".nupkg", delete=False) as tmp:
            tmp_nupkg = tmp.name
        try:
            if not download_nupkg(pkg_id, pkg_version, source_url, tmp_nupkg):
                return []
            if not extract_nupkg(tmp_nupkg, pkg_extract_dir):
                return []
            # Write sentinel to mark successful restore.
            with open(sentinel, "w") as sf:
                sf.write("{} {}\n".format(pkg_id, pkg_version))
        finally:
            try:
                os.unlink(tmp_nupkg)
            except OSError:
                pass

    # Find managed DLLs in the lib/ directory.
    lib_dir = find_best_lib_dir(pkg_extract_dir, pkg_id)
    if not lib_dir:
        log_error("No lib/ directory found in package {} {}".format(
            pkg_id, pkg_version))
        return []

    dlls = find_managed_dlls(lib_dir, pkg_id)
    if not dlls:
        log_error("No managed DLLs found in package {} {}".format(
            pkg_id, pkg_version))
        return []

    return dlls


def restore_all(project_dir, source_url, force=False):
    """Restore all packages from packages.config in the project directory.

    Returns a list of absolute DLL reference paths (POSIX slashes).
    """
    packages_config = os.path.join(project_dir, "packages.config")
    packages = parse_packages_config(packages_config)
    if not packages:
        log_info("No packages.config found at {} (or empty). Skipping restore.".format(
            packages_config))
        return []

    log_info("Found {} package(s) in packages.config".format(len(packages)))

    packages_dir = os.path.join(project_dir, "packages")
    os.makedirs(packages_dir, exist_ok=True)

    all_references = []
    for pkg_id, pkg_version in packages:
        log_info("Restoring {} {}...".format(pkg_id, pkg_version))
        dlls = restore_package(pkg_id, pkg_version, packages_dir, source_url, force)
        if not dlls:
            log_error("Failed to restore {} {}".format(pkg_id, pkg_version))
            continue
        all_references.extend(dlls)
        log_info("  -> {} reference(s)".format(len(dlls)))

    return all_references


def main():
    parser = argparse.ArgumentParser(
        description="Restore NuGet packages for a mono_new C# project."
    )
    parser.add_argument("--project-dir", required=True,
                        help="Project directory (containing packages.config)")
    parser.add_argument("--source", default=DEFAULT_NUGET_SOURCE,
                        help="NuGet flat container API URL "
                             "(default: {})".format(DEFAULT_NUGET_SOURCE))
    parser.add_argument("--force", action="store_true",
                        help="Re-download even if already restored")
    parser.add_argument("--print-references", action="store_true",
                        help="Print one DLL reference path per line on stdout")
    args = parser.parse_args()

    project_dir = os.path.abspath(args.project_dir)
    if not os.path.isdir(project_dir):
        log_error("Project directory not found: " + project_dir)
        return 1

    references = restore_all(project_dir, args.source, args.force)

    if args.print_references:
        # Print one reference per line, with POSIX slashes for portability.
        for ref in references:
            print(ref.replace("\\", "/"))

    log_info("Restore complete. {} reference(s).".format(len(references)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
