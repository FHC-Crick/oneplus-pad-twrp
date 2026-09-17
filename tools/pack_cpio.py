#!/usr/bin/env python3
"""Pack a directory (with device nodes) into a newc cpio archive, then lz4-compress.
Device nodes are declared in a manifest to avoid needing root mknod."""
import os
import stat
import struct
import sys


def c_ino(size=0):
    return b''

def pad4(n):
    return (4 - n % 4) % 4


_ino = [0x000493e0]  # official images start around this value


def entry(name, data, mode, uid=0, gid=0, rdev_major=0, rdev_minor=0, ino=None):
    """Emit one newc entry matching the official mkbootfs layout:
    - non-zero incrementing inode numbers (kernel-sensitive)
    - name field and data field each padded to a 4-byte boundary"""
    if isinstance(data, str):
        data = data.encode()
    if ino is None:
        ino = _ino[0]
        _ino[0] += 1
    hdr = f"{'070701':s}{ino:08x}{mode:08x}{uid:08x}{gid:08x}{1:08x}{0:08x}{len(data):08x}{0:08x}{0:08x}{rdev_major:08x}{rdev_minor:08x}{len(name)+1:08x}{0:08x}".encode()
    body = hdr + name.encode() + b'\x00'
    body += b'\x00' * ((4 - len(body) % 4) % 4)
    body += data
    body += b'\x00' * ((4 - len(body) % 4) % 4)
    return body


def trailer():
    """Official mkbootfs trailer: mode 0o755, non-zero ino, then zero padding
    out to the next 512-byte boundary."""
    global _ino
    ino = _ino[0]
    _ino[0] += 1
    body = f"070701{ino:08x}{0o755:08x}{0:08x}{0:08x}{1:08x}{0:08x}{0:08x}{0:08x}{0:08x}{11:08x}{0:08x}".encode() + b"TRAILER!!!\x00"
    body += b'\x00' * ((4 - len(body) % 4) % 4)
    body += b'\x00' * ((512 - len(body) % 512) % 512)
    return body


def pack_dir(root, devs, out):
    """devs: list of (path, mode, rdev_major, rdev_minor)"""
    out_b = b''
    # Collect all entries, then emit in deterministic order with every parent
    # directory appearing before its children (required by the kernel initramfs
    # unpacker). Device nodes must therefore come AFTER their directory.
    items = []  # (depth, path_for_sort, bytes)
    for path, mode, rmaj, rmin in devs:
        items.append((path.count('/'), path, entry(path, b'', mode, rdev_major=rmaj, rdev_minor=rmin)))
    for dirpath, dirnames, filenames in os.walk(root):
        rel = os.path.relpath(dirpath, root)
        for d in dirnames:
            p = d if rel == '.' else f"{rel}/{d}"
            st = os.lstat(os.path.join(dirpath, d))
            items.append((p.count('/'), p, entry(p + '/', b'', stat.S_IMODE(st.st_mode) | stat.S_IFDIR)))
        for fn in filenames:
            p = fn if rel == '.' else f"{rel}/{fn}"
            fp = os.path.join(dirpath, fn)
            st = os.lstat(fp)
            if stat.S_ISLNK(st.st_mode):
                items.append((p.count('/'), p, entry(p, os.readlink(fp), stat.S_IFLNK | 0o777)))
            else:
                with open(fp, 'rb') as f:
                    items.append((p.count('/'), p, entry(p, f.read(), stat.S_IMODE(st.st_mode) | stat.S_IFREG)))
    items.sort(key=lambda t: (t[0], t[1]))
    for _, _, blob in items:
        out_b += blob
    out_b += trailer()
    with open(out, 'wb') as f:
        f.write(out_b)
    print(f"packed {out}: {len(out_b)} bytes")
    return out_b


if __name__ == '__main__':
    root, out = sys.argv[1], sys.argv[2]
    pack_dir(root, [
        ('dev/console', stat.S_IFCHR | 0o600, 5, 1),
        ('dev/null', stat.S_IFCHR | 0o666, 1, 3),
        ('dev/zero', stat.S_IFCHR | 0o666, 1, 5),
    ], out)
