#!/usr/bin/env python3
"""Rewrite the cmdline field of a boot image header (v4) in place."""
import struct
import sys


def patch(src, out, cmdline):
    d = bytearray(open(src, 'rb').read())
    assert d[:8] == b'ANDROID!', 'not a boot image'
    hv = struct.unpack_from('<I', d, 40)[0]
    hs = struct.unpack_from('<I', d, 20)[0]
    cb = cmdline.encode()
    assert len(cb) < 1536, 'cmdline too long'
    cur = d[44:44+1536].split(b'\x00')[0]
    print(f'header_version={hv} header_size={hs} old_cmdline={cur.decode()!r}')
    d[44:44+1536] = cb + b'\x00' * (1536 - len(cb))
    open(out, 'wb').write(d)
    print(f'new_cmdline={cmdline!r} -> {out} ({len(d)} bytes)')


if __name__ == '__main__':
    patch(sys.argv[1], sys.argv[2], sys.argv[3])
