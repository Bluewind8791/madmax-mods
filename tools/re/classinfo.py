# For each RTTI class: full vtables, methods unique to the class, and functions that reference the vtable (ctor/dtor).
import sys, struct, re, bisect, collections
d = open(sys.argv[1], 'rb').read()
BASE = 0x140000000
CODE_LO, CODE_HI = BASE + 0x1b38000, BASE + 0x5d54000
def u32(o): return struct.unpack_from('<I', d, o)[0]
def u64(o): return struct.unpack_from('<Q', d, o)[0]
# .pdata (RUNTIME_FUNCTION) for function starts
pd = []
o = 0x1a0c000
while o < 0x1a0c000 + 0x125600:
    b, e, _ = struct.unpack_from('<III', d, o)
    if b == 0: break
    pd.append((b, e)); o += 12
pd.sort(); starts = [p[0] for p in pd]
def func_of(va):
    r = va - BASE; i = bisect.bisect_right(starts, r) - 1
    return BASE + pd[i][0] if i >= 0 and pd[i][0] <= r < pd[i][1] else None
# count how many times each code pointer appears in data sections
cnt = collections.Counter()
for lo, hi in ((0x1228000, 0x1228000 + 0x569200), (0x1792000, 0x1792000 + 0x2796a8)):
    for o in range(lo, hi - 8, 8):
        q = u64(o)
        if CODE_LO <= q < CODE_HI: cnt[q] += 1
leas = [m.start() for m in re.finditer(rb'[\x48\x4c]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]', d)]
for cls in sys.argv[2:]:
    m = re.search(re.escape(('.?AV%s@@' % cls).encode() + b'\0'), d)
    td = m.start() - 0x10
    print('== %s' % cls)
    for c in re.finditer(re.escape(struct.pack('<I', td)), d):
        col = c.start() - 0x0C
        if col < 0 or u32(col) != 1 or u32(col + 0x14) != col: continue
        for v in re.finditer(re.escape(struct.pack('<Q', BASE + col)), d):
            vt = v.start() + 8
            fns = []
            while CODE_LO <= u64(vt + 8 * len(fns)) < CODE_HI: fns.append(u64(vt + 8 * len(fns)))
            print('  vtable %x this-offset %x, %d methods' % (BASE + vt, u32(col + 4), len(fns)))
            print('    unique: ' + ' '.join('[%d]%x' % (i, f) for i, f in enumerate(fns) if cnt[f] == 1))
            refs = set()
            for p in leas:
                if p + 7 + struct.unpack_from('<i', d, p + 3)[0] == vt: refs.add(func_of(BASE + p))
            print('    vtable referenced by functions: ' + ' '.join('%x' % f for f in refs if f))
