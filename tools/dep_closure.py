#!/usr/bin/env python3
"""Compute ELF dependency closure for all binaries under a root using llvm-readelf,
print libraries NOT in the closure (safe to delete)."""
import subprocess
import os
import sys

root = sys.argv[1]
KEEP_EXTRA = set(sys.argv[2].split(',')) if len(sys.argv) > 2 else set()

libdir = os.path.join(root, 'system/lib64')
bins = [os.path.join(root, 'system/bin', f) for f in os.listdir(os.path.join(root, 'system/bin'))]
libs = [os.path.join(libdir, f) for f in os.listdir(libdir)] if os.path.isdir(libdir) else []

def needed(path):
    try:
        out = subprocess.run(['llvm-readelf', '-d', path], capture_output=True, text=True, timeout=30).stdout
    except Exception:
        return set()
    deps = set()
    for line in out.splitlines():
        if 'NEEDED' in line:
            dep = line.split('[')[1].split(']')[0]
            deps.add(dep)
    return deps

dep_map = {}
for p in bins + libs:
    dep_map[p] = needed(p)

roots = bins  # all binaries are roots (binaries may dlopen too)
closure = set()
queue = list(bins)
while queue:
    p = queue.pop()
    for d in dep_map.get(p, set()):
        if d not in closure:
            closure.add(d)
            lp = os.path.join(libdir, d)
            if os.path.exists(lp):
                queue.append(lp)

print(f"# closure: {len(closure)} libs")
removable = []
for p in libs:
    base = os.path.basename(p)
    if base not in closure and base not in KEEP_EXTRA:
        removable.append(p)
print("### REMOVABLE LIBS:")
tot = 0
for p in sorted(removable):
    sz = os.path.getsize(p)
    tot += sz
    print(f"{sz//1024:5d} KB  {os.path.basename(p)}")
print(f"# total removable: {tot/1024/1024:.1f} MB")
with open('/tmp/removable_libs.txt', 'w') as f:
    for p in removable:
        f.write(p + '\n')
