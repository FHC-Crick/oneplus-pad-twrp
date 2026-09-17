#!/usr/bin/env python3
"""Insert device-node entries into an existing GNU newc cpio stream,
right after the './dev' directory entry, so the kernel initramfs unpacker
finds them with their parent dir already created."""
import sys


def pad4(n):
    return (4 - n % 4) % 4


def entry(name, data, mode, rdev_major=0, rdev_minor=0):
    if isinstance(data, str):
        data = data.encode()
    hdr = (f"070701{0:08x}{mode:08x}{0:08x}{0:08x}{1:08x}{0:08x}"
           f"{len(data):08x}{0:08x}{0:08x}{rdev_major:08x}{rdev_minor:08x}"
           f"{len(name)+1:08x}{0:08x}").encode()
    return hdr + name.encode() + b'\x00' + data + b'\x00' * pad4(len(hdr) + len(name) + 1 + len(data))


def find_entry_end(d, pos, name):
    """pos points at header start; return offset after this entry's data+pad."""
    namesize = int(d[pos + 94:pos + 102], 16)
    filesize = int(d[pos + 54:pos + 62], 16)
    total = 110 + namesize + filesize
    return pos + total + pad4(total)


def main(src, dst):
    d = open(src, 'rb').read()
    dev_entry = None
    pos = 0
    while pos + 110 <= len(d):
        if d[pos:pos + 6] != b'070701':
            pos += 4
            continue
        try:
            namesize = int(d[pos + 94:pos + 102], 16)
            filesize = int(d[pos + 54:pos + 62], 16)
        except ValueError:
            pos += 4
            continue
        if not (0 < namesize <= 4096 and 0 <= filesize < 64 * 1024 * 1024):
            pos += 4
            continue
        name = d[pos + 110:pos + 110 + namesize - 1].decode(errors='replace')
        if name in ('./dev', './dev/', 'dev', 'dev/'):
            dev_entry = pos
            break
        total = 110 + namesize + filesize
        pos += total + pad4(total)
    if dev_entry is None:
        sys.exit("no ./dev directory entry found in cpio")
    after = find_entry_end(d, dev_entry, './dev/')
    nodes = b''
    nodes += entry('dev/console', b'', 0o020600, 5, 1)
    nodes += entry('dev/null', b'', 0o020666, 1, 3)
    nodes += entry('dev/zero', b'', 0o020666, 1, 5)
    out = d[:after] + nodes + d[after:]
    open(dst, 'wb').write(out)
    print(f"inserted 3 device nodes after ./dev at {after}; {src} -> {dst} ({len(out)}B)")


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
