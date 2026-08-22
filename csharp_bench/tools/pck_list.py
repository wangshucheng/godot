#!/usr/bin/env python3
"""List entries of a Godot 4 PCK (format v2/v3/v4, unencrypted directory)."""
import struct, sys

def main(path):
    with open(path, "rb") as f:
        data = f.read()
    magic, version = struct.unpack_from("<II", data, 0)
    assert magic == 0x43504447, "not a PCK (magic=%08x)" % magic
    ver_major, ver_minor, ver_patch = struct.unpack_from("<III", data, 8)
    (pack_flags,) = struct.unpack_from("<I", data, 20)
    file_base, = struct.unpack_from("<Q", data, 24)
    off = 32
    if version >= 3:
        dir_offset, = struct.unpack_from("<Q", data, off)
        off = dir_offset
    else:
        off += 16 * 4  # v2: 16 reserved u32 between header and directory
    (count,) = struct.unpack_from("<i", data, off)
    off += 4
    print("pck v%d engine=%d.%d.%d flags=0x%x file_base=%d entries=%d"
          % (version, ver_major, ver_minor, ver_patch, pack_flags, file_base, count))
    total = 0
    for _ in range(count):
        (path_len,) = struct.unpack_from("<I", data, off); off += 4
        name = data[off:off + path_len].split(b"\0")[0].decode("utf-8", "replace")
        off += path_len
        ofs, size = struct.unpack_from("<QQ", data, off); off += 16
        off += 16  # md5
        (flags,) = struct.unpack_from("<I", data, off); off += 4
        total += size
        print("%10d  flags=%d  %s" % (size, flags, name))
    print("total payload bytes: %d" % total)

if __name__ == "__main__":
    main(sys.argv[1])
