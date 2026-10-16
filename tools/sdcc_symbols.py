"""Merge the linker map (global symbols, classified by area) with the
per-module .sym files (all symbols, module-relative) into one 'addr name'
list for the emulator's profile command."""
import re, sys, os

build = sys.argv[1] if len(sys.argv) > 1 else '/home/zeal8bit/Zeal/Zeal-GameGear/build'
out_path = sys.argv[2] if len(sys.argv) > 2 else '/tmp/gg_symbols.txt'

# 1. Map: keep TEXT-area globals per module.
globals_by_mod = {}
area = None
for line in open(os.path.join(build, 'gamegear.map'), errors='ignore'):
    m = re.match(r'^(\w+)\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})\s+=', line)
    if m:
        area = m.group(1)
        continue
    m = re.match(r'\s*([0-9A-Fa-f]{8})\s+(_\S+)\s+(\S+)\s*$', line)
    if m and area == '_TEXT':
        globals_by_mod.setdefault(m.group(3), {})[m.group(2)] = int(m.group(1), 16)

syms, bases = {}, {}
for mod, gl in globals_by_mod.items():
    for cand in (f'{mod}.c.sym', f'{mod}.asm.sym'):
        path = os.path.join(build, 'CMakeFiles/gamegear.dir/src', cand)
        if os.path.exists(path):
            break
    else:
        # No .sym for assembly modules (nmi): the map's globals are enough.
        for name, addr in gl.items():
            syms[addr] = name
        bases[mod] = min(gl.values())
        continue

    entries = []
    for line in open(path, errors='ignore'):
        m = re.match(r'\s*(\d+)\s+(_\w+)\s+([0-9A-Fa-f]{6})\s+([A-Z]+)\s*$', line)
        if m:
            entries.append((int(m.group(1)), m.group(2), int(m.group(3), 16)))

    base = text_idx = None
    for idx, name, off in entries:
        if name in gl:
            base, text_idx = gl[name] - off, idx
            break
    if base is None:
        for name, addr in gl.items():
            syms[addr] = name
        bases[mod] = min(gl.values())
        continue

    bases[mod] = base
    for idx, name, off in entries:
        if idx == text_idx and 0x8000 <= base + off < 0xC000:
            syms[base + off] = name

with open(out_path, 'w') as f:
    for addr, name in sorted(syms.items()):
        f.write(f'{addr:08x} {name}\n')
print(f'{len(syms)} TEXT symbols from {len(bases)} modules -> {out_path}')
for mod, b in sorted(bases.items(), key=lambda kv: kv[1]):
    print(f'  {mod:14s} base 0x{b:04x}')
