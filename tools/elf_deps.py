#!/usr/bin/env python3
"""Parse ELF dynamic deps of all binaries/libs, compute closure from root set, list removable libs."""
import struct, sys, os
from collections import defaultdict

def elf_needed(path):
    try:
        with open(path, 'rb') as f:
            d = f.read()
    except OSError:
        return set(), set()
    if d[:4] != b'\x7fELF':
        return set(), set()
    is64 = d[4] == 2
    if is64:
        e_phoff = struct.unpack_from('<Q', d, 32)[0]
        e_phentsize = struct.unpack_from('<H', d, 54)[0]
        e_phnum = struct.unpack_from('<H', d, 56)[0]
        ph_fmt = '<IIQQQQQQ'
    else:
        e_phoff = struct.unpack_from('<I', d, 28)[0]
        e_phentsize = struct.unpack_from('<H', d, 42)[0]
        e_phnum = struct.unpack_from('<H', d, 44)[0]
        ph_fmt = '<IIIIIIII'
    needed, rpaths = set(), set()
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        if off + struct.calcsize(ph_fmt) > len(d):
            break
        ph = struct.unpack_from(ph_fmt, d, off)
        p_type = ph[0]
        if p_type == 2:  # PT_DYNAMIC
            if is64:
                p_offset = ph[2]
            else:
                p_offset = ph[2]
            dyn = d[p_offset:]
            j = 0
            while j + 16 <= len(dyn):
                tag = struct.unpack_from('<q' if is64 else '<i', dyn, j)[0]
                if tag == 0:
                    break
                val = struct.unpack_from('<Q' if is64 else '<I', dyn, j + 8)[0]
                if tag == 1:  # DT_NEEDED
                    pass  # string table needed; handle below
                j += 16
            # simpler: parse via strtab scan is complex; fallback below
            break
    # Fallback: scan for DT_NEEDED with strtab
    if is64:
        e_shoff = struct.unpack_from('<Q', d, 40)[0]
        e_shentsize = struct.unpack_from('<H', d, 58)[0]
        e_shnum = struct.unpack_from('<H', d, 60)[0]
    else:
        e_shoff = struct.unpack_from('<I', d, 32)[0]
        e_shentsize = struct.unpack_from('<H', d, 46)[0]
        e_shnum = struct.unpack_from('<H', d, 48)[0]
    dyn_str = b''
    for i in range(min(e_shnum, 2000)):
        off = e_shoff + i * e_shentsize
        if off + 0x40 > len(d):
            break
        sh = struct.unpack_from('<IIQQQQIIQQ' if is64 else '<IIIIIIIIII', d, off)
        sh_type = sh[1]
        if sh_type == 6:  # SHT_DYNAMIC
            # find associated strtab by sh_link
            pass
    return needed, rpaths

def elf_needed2(path):
    """Robust DT_NEEDED extraction using pyelftools-free manual parse."""
    with open(path, 'rb') as f:
        d = f.read()
    if d[:4] != b'\x7fELF':
        return set()
    is64 = d[4] == 2
    if is64:
        e_phoff = struct.unpack_from('<Q', d, 32)[0]
        e_phentsize = struct.unpack_from('<H', d, 54)[0]
        e_phnum = struct.unpack_from('<H', d, 56)[0]
    else:
        e_phoff = struct.unpack_from('<I', d, 28)[0]
        e_phentsize = struct.unpack_from('<H', d, 42)[0]
        e_phnum = struct.unpack_from('<H', d, 44)[0]
    dyn_off, dyn_sz = None, None
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        if is64:
            p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack_from('<IIQQQQQQ', d, off)
        else:
            p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align = struct.unpack_from('<IIIIIIII', d, off)
        if p_type == 2:
            dyn_off, dyn_sz = p_offset, p_filesz
            break
    if dyn_off is None:
        return set()
    needed, strtab_off = set(), None
    j = 0
    while j + 16 <= dyn_sz:
        if is64:
            tag, val = struct.unpack_from('<qQ', d, dyn_off + j)
        else:
            tag, val = struct.unpack_from('<iI', d, dyn_off + j)
        if tag == 0:
            break
        if tag == 1:
            needed.add(val)
        if tag == 5:  # DT_STRTAB
            strtab_off = val
        j += 16
    # map vaddr->file offset: usually strtab_off is vaddr; find segment containing it
    if strtab_off is not None:
        for i in range(e_phnum):
            off = e_phoff + i * e_phentsize
            if is64:
                p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack_from('<IIQQQQQQ', d, off)
            else:
                p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align = struct.unpack_from('<IIIIIIII', d, off)
            if p_type == 1 and p_vaddr <= strtab_off < p_vaddr + p_filesz:
                base = p_offset + (strtab_off - p_vaddr)
                names = set()
                for nv in needed:
                    e = d.find(b'\x00', base + nv)
                    names.add(d[base+nv:e].decode(errors='replace'))
                return names
    return set()

def main():
    root = sys.argv[1] if len(sys.argv) > 1 else '.'
    roots = sys.argv[2].split(',') if len(sys.argv) > 2 else [
        'system/bin/recovery','system/bin/init','system/bin/adbd','system/bin/sh',
        'system/bin/toybox','system/bin/e2fsck','system/bin/mke2fs','system/bin/resize2fs',
        'system/bin/fsck.f2fs','system/bin/mkfs.f2fs','system/bin/sload_f2fs','system/bin/minadbd',
        'system/bin/linker64','system/bin/tune2fs']
    all_files = []
    for dirpath, _, files in os.walk(root):
        for fn in files:
            p = os.path.join(dirpath, fn)
            all_files.append(p)
    # name -> set of NEEDED
    provided = {}
    for p in all_files:
        if p.endswith('.so') or (os.path.dirname(p).endswith('/bin')):
            n = elf_needed2(p)
            provided[p] = n
    # closure
    needed_names = set()
    queue = list(roots)
    seen = set()
    while queue:
        r = queue.pop()
        if r in seen:
            continue
        seen.add(r)
        if r not in provided:
            continue
        for n in provided[r]:
            needed_names.add(n)
            queue.append(os.path.join('system/lib64', n))
            queue.append(os.path.join('system/lib', n))
    # removable: .so files whose basename not in needed_names closure
    removable = []
    for p in sorted(all_files):
        if '.so' in p and '/system/lib' in p:
            base = os.path.basename(p)
            # check if any NEEDED (including transitive) references it
            if base not in needed_names:
                removable.append((p, os.path.getsize(p)))
    print(f"# needed closure: {len(needed_names)} libs")
    print("### REMOVABLE:")
    tot = 0
    for p, sz in removable:
        print(f"{sz//1024:5d} KB  {p}")
        tot += sz
    print(f"# total removable: {tot/1024/1024:.1f} MB")

if __name__ == '__main__':
    main()
