#!/usr/bin/env python3
"""Append the stock AVB tail (embedded vbmeta struct + AVBf footer) to a repacked
init_boot image and update footer original_image_size. Image must fit the
partition; footer sits at the very end of the partition."""
import struct
import sys


def patch(img_path, stock_path, out_path, part_size):
    with open(img_path, 'rb') as f:
        img = f.read()
    with open(stock_path, 'rb') as f:
        stock = f.read()
    assert stock[-64:][:4] == b'AVBf', 'stock image has no AVB footer'
    vbmeta_off, vbmeta_size = struct.unpack_from('>QQ', stock, -64 + 12)
    tail = stock[vbmeta_off:]  # embedded vbmeta + footer
    assert len(img) + len(tail) <= part_size, \
        f'image {len(img)} + avb tail {len(tail)} exceeds partition {part_size}'
    new = bytearray(part_size)
    new[:len(img)] = img
    new[part_size - len(tail):] = tail
    # footer original_image_size -> new content size (end of image data)
    struct.pack_into('>Q', new, part_size - 64 + 12, len(img))
    with open(out_path, 'wb') as f:
        f.write(new)
    print(f'{img_path} -> {out_path}: content {len(img)}B, avb tail {len(tail)}B, '
          f'orig_size updated, total {part_size}B')


if __name__ == '__main__':
    patch(sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4], 0))
