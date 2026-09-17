#!/usr/bin/env python3
"""Pack a compressed ramdisk into an init_boot image (header v4) matching stock params."""
import struct
import sys

PAGE = 4096
HEADER_SIZE = 1584
OS_VERSION = 0x1C000194  # stock init_boot os_version (14.0.0.x per stock header)


def pack(ramdisk_path, out_path, os_version=OS_VERSION, cmdline=""):
    with open(ramdisk_path, 'rb') as f:
        ramdisk = f.read()
    ramdisk_size = len(ramdisk)

    hdr = bytearray(HEADER_SIZE)
    struct.pack_into('<8s', hdr, 0, b'ANDROID!')
    struct.pack_into('<I', hdr, 8, 0)            # kernel_size
    struct.pack_into('<I', hdr, 12, ramdisk_size)
    struct.pack_into('<I', hdr, 16, os_version)
    struct.pack_into('<I', hdr, 20, HEADER_SIZE)
    # reserved[4] at 24..40 already zero
    struct.pack_into('<I', hdr, 40, 4)           # header_version
    cmd_b = cmdline.encode()
    if len(cmd_b) > 1536:
        sys.exit("cmdline too long")
    struct.pack_into(f'<{len(cmd_b)}s', hdr, 44, cmd_b)
    struct.pack_into('<I', hdr, HEADER_SIZE - 4, 0)  # signature_size (v4 tail)

    # ramdisk at page-aligned offset after header
    pad = (PAGE - HEADER_SIZE % PAGE) % PAGE
    blob = bytes(hdr) + b'\x00' * pad + ramdisk

    with open(out_path, 'wb') as f:
        f.write(blob)
    print(f"packed {out_path}: header v4, ramdisk {ramdisk_size} bytes, total {len(blob)} bytes ({len(blob)/1024/1024:.2f} MiB)")
    if len(blob) > 0x800000:
        print(f"WARNING: exceeds 8 MiB init_boot partition!")


if __name__ == '__main__':
    pack(sys.argv[1], sys.argv[2])
