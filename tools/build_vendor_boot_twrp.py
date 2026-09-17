#!/usr/bin/env python3
"""Build vendor_boot_a.img: copy stock vendor_boot, replace vendor ramdisk region
(offset 4096, size = stock vendor_ramdisk_size) with TWRP gzip ramdisk + zero pad.
Header fields (incl. vendor_ramdisk_size and vrt) stay identical to stock, so lk
parses it exactly like the stock image."""
import struct
import sys

PAGE = 4096
HDR = 2128  # stock header_size


def build(stock_path, twrp_gz_path, out_path):
    with open(stock_path, 'rb') as f:
        stock = bytearray(f.read())
    with open(twrp_gz_path, 'rb') as f:
        twrp = f.read()

    # stock header says where the vendor ramdisk lives and how big it is
    assert stock[:8] == b'VNDRBOOT', 'not a vendor_boot image'
    hv = struct.unpack_from('<I', stock, 8)[0]
    vramdisk_size = struct.unpack_from('<I', stock, 24)[0]
    print(f'stock header_version={hv} vendor_ramdisk_size={vramdisk_size}')

    ramdisk_off = (HDR + PAGE - 1) // PAGE * PAGE  # 4096
    assert len(twrp) < vramdisk_size, f'twrp.gz {len(twrp)} too big for {vramdisk_size}'

    # zero out the whole stock ramdisk region, then write twrp.gz at its start
    stock[ramdisk_off:ramdisk_off + vramdisk_size] = b'\x00' * vramdisk_size
    stock[ramdisk_off:ramdisk_off + len(twrp)] = twrp

    with open(out_path, 'wb') as f:
        f.write(stock)
    print(f'built {out_path}: twrp.gz {len(twrp)/1024/1024:.1f} MiB at offset {ramdisk_off}, '
          f'region {vramdisk_size/1024/1024:.1f} MiB, total {len(stock)/1024/1024:.1f} MiB')


if __name__ == '__main__':
    build(sys.argv[1], sys.argv[2], sys.argv[3])
