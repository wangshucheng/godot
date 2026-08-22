#!/usr/bin/env python3
"""Rewrite entries inside a Godot 4 PCK (format v2/v3/v4, unencrypted).

Usage:
  python pck_rewrite.py <pck> [--set res_path=file]... [--dump]

Reads the existing PCK, replaces/adds entries from local files, and writes
the PCK back in place (header layout preserved: same engine version, pack
flags, file_base; directory offset recomputed; per-entry MD5 recomputed).

Entry offsets honor PACK_REL_FILEBASE (pack_flags & 2): stored offsets are
relative to file_base, matching Godot's PackedSourcePCK reader.
"""
import hashlib
import struct
import sys


def read_entries(data):
    magic, version = struct.unpack_from("<II", data, 0)
    assert magic == 0x43504447, "not a PCK"
    ver_major, ver_minor, ver_patch = struct.unpack_from("<III", data, 8)
    (pack_flags,) = struct.unpack_from("<I", data, 20)
    (file_base,) = struct.unpack_from("<Q", data, 24)
    off = 32
    if version >= 3:
        (dir_offset,) = struct.unpack_from("<Q", data, off)
        off = dir_offset
    else:
        off += 16 * 4
    (count,) = struct.unpack_from("<i", data, off)
    off += 4
    entries = []
    for _ in range(count):
        (path_len,) = struct.unpack_from("<I", data, off)
        off += 4
        raw = data[off:off + path_len]
        off += path_len
        name = raw.split(b"\0")[0].decode("utf-8", "replace")
        ofs, size = struct.unpack_from("<QQ", data, off)
        off += 16
        md5 = data[off:off + 16]
        off += 16
        (flags,) = struct.unpack_from("<I", data, off)
        off += 4
        abs_ofs = file_base + ofs if (pack_flags & 2) else ofs
        entries.append({"name": name, "abs_ofs": abs_ofs, "size": size,
                        "flags": flags})
    hdr = {"version": version, "ver_major": ver_major, "ver_minor": ver_minor,
           "ver_patch": ver_patch, "pack_flags": pack_flags,
           "file_base": file_base}
    return hdr, entries


def build(hdr, entries_with_data):
    """entries_with_data: list of (name, bytes, flags)."""
    file_base = hdr["file_base"]
    head = struct.pack("<IIIIII", 0x43504447, hdr["version"], hdr["ver_major"],
                       hdr["ver_minor"], hdr["ver_patch"], hdr["pack_flags"])
    head += struct.pack("<Q", file_base)
    head += struct.pack("<Q", 0)  # dir_offset placeholder
    assert len(head) <= file_base, "header larger than file_base"
    head += b"\0" * (file_base - len(head))

    out = bytearray(head)
    dir_blob = bytearray()
    dir_blob += struct.pack("<i", len(entries_with_data))
    for name, blob, flags in entries_with_data:
        rel_ofs = len(out) - file_base if (hdr["pack_flags"] & 2) else len(out)
        out += blob
        enc = name.encode("utf-8") + b"\0"
        dir_blob += struct.pack("<I", len(enc)) + enc
        dir_blob += struct.pack("<QQ", rel_ofs, len(blob))
        dir_blob += hashlib.md5(blob).digest()
        dir_blob += struct.pack("<I", flags)
    dir_offset = len(out)
    out += dir_blob
    struct.pack_into("<Q", out, 32, dir_offset)
    return bytes(out)


def main():
    argv = sys.argv[1:]
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    pck = argv[0]
    sets = []
    dump_only = False
    i = 1
    while i < len(argv):
        a = argv[i]
        if a == "--set":
            res_path, _, local = argv[i + 1].partition("=")
            sets.append((res_path, local))
            i += 2
        elif a == "--dump":
            dump_only = True
            i += 1
        else:
            raise SystemExit("unknown arg: " + a)

    with open(pck, "rb") as f:
        data = f.read()
    hdr, entries = read_entries(data)

    if dump_only:
        print("pck v%d engine=%d.%d.%d flags=0x%x file_base=%d entries=%d"
              % (hdr["version"], hdr["ver_major"], hdr["ver_minor"],
                 hdr["ver_patch"], hdr["pack_flags"], hdr["file_base"],
                 len(entries)))
        for e in entries:
            print("%10d  flags=%d  %s" % (e["size"], e["flags"], e["name"]))
        return 0

    # Apply updates: replace existing entry or append new one.
    by_name = {e["name"]: e for e in entries}
    changed = []
    for res_path, local in sets:
        with open(local, "rb") as f:
            blob = f.read()
        flags = by_name[res_path]["flags"] if res_path in by_name else 0
        if res_path in by_name:
            old = by_name[res_path]
            is_same = (data[old["abs_ofs"]:old["abs_ofs"] + old["size"]] == blob)
            action = "same" if is_same else "replaced"
        else:
            action = "added"
        by_name[res_path] = {"name": res_path, "blob": blob, "flags": flags}
        changed.append((res_path, action, len(blob)))

    merged = []
    for e in entries:
        if e["name"] in by_name and "blob" in by_name[e["name"]]:
            m = by_name[e["name"]]
            merged.append((e["name"], m["blob"], m["flags"]))
        else:
            blob = data[e["abs_ofs"]:e["abs_ofs"] + e["size"]]
            merged.append((e["name"], blob, e["flags"]))
    # New entries not present in original order.
    orig_names = {e["name"] for e in entries}
    for name, m in by_name.items():
        if name not in orig_names:
            merged.append((name, m["blob"], m["flags"]))

    out = build(hdr, merged)
    with open(pck, "wb") as f:
        f.write(out)

    # Round-trip verification.
    with open(pck, "rb") as f:
        data2 = f.read()
    hdr2, entries2 = read_entries(data2)
    assert len(entries2) == len(merged), "entry count mismatch after rewrite"
    for e, (name, blob, _fl) in zip(entries2, merged):
        assert e["name"] == name
        assert data2[e["abs_ofs"]:e["abs_ofs"] + e["size"]] == blob, \
            "payload mismatch for " + name
    print("pck rewritten ok: %d entries, %d bytes" % (len(entries2), len(data2)))
    for res_path, action, size in changed:
        print("  %-40s %-9s %d bytes" % (res_path, action, size))
    return 0


if __name__ == "__main__":
    sys.exit(main())
