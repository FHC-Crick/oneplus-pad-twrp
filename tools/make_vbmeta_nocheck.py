#!/usr/bin/env python3
"""Strip hash/chain descriptors from a vbmeta image (keep full AVB structure).
lk parses a valid vbmeta but finds no partition descriptors -> no boot chain
verification. Signature becomes invalid (tolerated on unlocked devices)."""
import struct
import sys


def strip(path, out):
    with open(path, 'rb') as f:
        d = bytearray(f.read())
    assert d[:4] == b'AVB0', f'{path}: not a vbmeta image'
    # AvbVBMetaImageHeader is big-endian:
    # descriptors_offset at 72, descriptors_size at 80
    desc_off = struct.unpack_from('>Q', d, 72)[0]
    desc_size = struct.unpack_from('>Q', d, 80)[0]
    print(f'{path}: descriptors at {desc_off}, size {desc_size}')
    if desc_size:
        d[desc_off:desc_off + desc_size] = b'\x00' * desc_size
        struct.pack_into('>Q', d, 80, 0)  # descriptors_size = 0
    with open(out, 'wb') as f:
        f.write(d)
    print(f'  -> {out} (descriptors cleared, structure intact)')


if __name__ == '__main__':
    strip(sys.argv[1], sys.argv[2])
