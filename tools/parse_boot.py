#!/usr/bin/env python3
"""Parse Android boot/vendor_boot/init_boot header (v3/v4) and extract kernel/ramdisk/dtb info.
v3/v4 layout: kernel at align(header_size, 4096), ramdisk at align(kernel_off+kernel_size, 4096).
"""
import struct
import sys

PAGE = 4096

def align(n):
    return (n + PAGE - 1) // PAGE * PAGE

def parse(path):
    with open(path, 'rb') as f:
        data = f.read()
    if data[:8] != b'ANDROID!':
        print(f"{path}: not a standard boot image, first 16 bytes: {data[:16].hex()}")
        return
    (kernel_size, ramdisk_size, os_version, header_size) = struct.unpack_from('<IIII', data, 8)
    header_version = struct.unpack_from('<I', data, 40)[0]
    koff = align(header_size)
    roff = align(koff + kernel_size)
    print(f"=== {path} ===")
    print(f"header_version   : {header_version}")
    print(f"header_size      : {header_size}")
    print(f"kernel_size      : {kernel_size} @ {koff}")
    print(f"ramdisk_size     : {ramdisk_size} @ {roff}")
    print(f"os_version       : {os_version} (0x{os_version:08x})")
    print(f"file_size        : {len(data)}")
    if kernel_size:
        print(f"kernel_magic     : {data[koff:koff+8].hex()} ({data[koff:koff+4]!r})")
    if ramdisk_size:
        print(f"ramdisk_magic    : {data[roff:roff+8].hex()} ({data[roff:roff+4]!r})")
    sig_size = struct.unpack_from('<I', data, header_size - 4)[0]
    print(f"signature_size   : {sig_size}")
    pos = align(roff + ramdisk_size)
    if len(data) > pos:
        rest = data[pos:]
        nz = len(rest.rstrip(b'\x00'))
        print(f"trailing         : {len(rest)} bytes at {pos}, non-zero tail {nz} bytes, first16 {rest[:16].hex()}")

def extract(path, what, out):
    with open(path, 'rb') as f:
        data = f.read()
    if data[:8] != b'ANDROID!':
        sys.exit(f"{path}: not a boot image")
    kernel_size = struct.unpack_from('<I', data, 8)[0]
    ramdisk_size = struct.unpack_from('<I', data, 16)[0]
    header_size = struct.unpack_from('<I', data, 20)[0]
    koff = align(header_size)
    roff = align(koff + kernel_size)
    if what == 'kernel':
        blob = data[koff:koff + kernel_size]
    elif what == 'ramdisk':
        blob = data[roff:roff + ramdisk_size]
    else:
        sys.exit("what must be kernel|ramdisk")
    with open(out, 'wb') as f:
        f.write(blob)
    print(f"extracted {what} -> {out} ({len(blob)} bytes), magic: {blob[:4]!r}")

if __name__ == '__main__':
    args = sys.argv[1:]
    if '--extract' in args:
        i = args.index('--extract')
        extract(args[i+1], args[i+2], args[i+3])
        args = args[:i] + args[i+4:]
    for p in args:
        parse(p)
