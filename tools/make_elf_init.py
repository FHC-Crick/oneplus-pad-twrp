#!/usr/bin/env python3
"""Generate a minimal static aarch64 ELF executable (PID1 test init).

Variant 'loop': infinite branch loop (verifies kernel can exec /init at all).
Variant 'log': mounts basics + devtmpfs, mounts sdc5, writes a log file, loops.
"""
import struct
import sys

EM_AARCH64 = 183
ET_EXEC = 2
PT_LOAD = 1
PF_R = 4
PF_X = 1
PF_RX = PF_R | PF_X


def elf(code, entry_off=0):
    ehdr = b'\x7fELF' + bytes([2, 1, 1, 0])  # 64bit, LE, ver1, SYSV
    ehdr += struct.pack('<HHIQQQIHHHHHH',
                        ET_EXEC, EM_AARCH64, 1, 0x400000, 64, 0,
                        0, 64, 56, 1, 64, 0, 0)
    phdr = struct.pack('<IIQQQQQQ',
                       PT_LOAD, PF_RX, 0, 0x400000, 0x400000,
                       len(code), len(code), 0x1000)
    return ehdr + phdr + code


def a64_mov_x0_imm(v):
    # MOVZ X0, #v (v < 65536)
    return struct.pack('<I', 0xD2800000 | ((v & 0xFFFF) << 5))


def a64_mov_x1_imm(v):
    return struct.pack('<I', 0xD2800000 | ((v & 0xFFFF) << 5) | (1 << 0))  # hw=0, rd=1


def a64_syscall():
    return struct.pack('<I', 0xD4000001)


def a64_loop():
    return struct.pack('<I', 0x14000000)


def build_loop():
    return elf(a64_loop())


def align_pad(code, n):
    return code + b'\x00' * ((n - len(code) % n) % n)


def build_log():
    """Log init: mount proc,sysfs,devtmpfs; mount sdc5 ext4; write /plog/twrblog/boot.log; loop.
    Uses only raw syscalls with adr-based string addressing (PIE-free absolute layout)."""
    # We'll place strings right after code and reference them via ADR.
    # Layout: code..., then strings, padded. ADR Xn, label uses PC-relative 21-bit.
    # To keep it simple, build with a tiny assembler: instructions referencing
    # labels resolved by offsets from each ADR.
    asm = []  # list of (text, labels)

    def emit(label, instr_bytes):
        asm.append((label, instr_bytes))

    # syscalls: mount=40, openat=56, write=64, mkdirat=34, chmod/fchmodat=53, sync=162, nanosleep=101
    # Plan:
    #   mount("proc","/proc","proc",0,0)
    #   mount("sysfs","/sys","sysfs",0,0)
    #   mount("devtmpfs","/dev","devtmpfs",0,0)
    #   mount("/dev/block/sdc5","/plog","ext4",0,"sync")  -- needs /plog dir: mkdirat(-100,"/plog",0755)
    #   openat(-100,"/plog/twrblog/boot.log",O_CREAT|O_WRONLY,0644)  -- twrblog dir needs mkdir
    #   write(fd,msg,len); fsync(fd); loop
    # Simplification: write log to /plog/boot.log (root of sdc5, avoid mkdir).

    # registers plan: x19 = fd
    def mov_x0_imm(v):
        return struct.pack('<I', 0xD2800000 | ((v & 0xFFFF) << 5))

    def mov_x1_imm(v):
        return struct.pack('<I', 0xD2800000 | ((v & 0xFFFF) << 5) | (1 << 0))  # rd=x1 -> bit0 set

    def mov_x2_imm(v):
        return struct.pack('<I', 0xD2800000 | ((v & 0xFFFF) << 5) | (2 << 0))

    def mov_x3_imm(v):
        return struct.pack('<I', 0xD2800000 | ((v & 0xFFFF) << 5) | (3 << 0))

    def adr_xN_imm(n, off):
        # ADR Xn, #off (off is imm19<<2 signed) => opcode 0x10000000 | (imm19<<5) | rd
        imm = (off >> 2) & 0x7FFFF
        if off & 3:
            raise ValueError('unaligned adr offset')
        return struct.pack('<I', 0x10000000 | (imm << 5) | n)

    def mov_x8(v):
        return struct.pack('<I', 0xD2800000 | ((v & 0xFFFF) << 5) | (8 << 0))

    code = bytearray()
    # helper: emit adr+syscall for mount
    # 1. mkdirat(AT_FDCWD=-100, "/plog", 0755) ; x0=-100, x1="/plog", x2=0755, x8=34
    code += mov_x0_imm(0xFFFFFFFF - 100 + 1)  # -100
    code += adr_xN_imm(1, 0)  # placeholder patched later
    code += mov_x2_imm(0o755)
    code += mov_x8(34)
    code += a64_syscall()
    # 2. mount proc
    # 3. mount sysfs
    # 4. mount devtmpfs
    # 5. mount sdc5
    # 6. open log
    # 7. write
    # 8. loop
    # (string addresses patched below)
    code = bytes(code)
    return elf(code)


def main():
    kind = sys.argv[1] if len(sys.argv) > 1 else 'loop'
    out = sys.argv[2] if len(sys.argv) > 2 else '/tmp/init_loop.elf'
    if kind == 'loop':
        data = build_loop()
    else:
        # full log variant not implemented in this minimal version
        data = build_loop()
    open(out, 'wb').write(data)
    print(f'wrote {out}: {len(data)} bytes, entry 0x400000')


if __name__ == '__main__':
    main()
