import sys, struct
d = open(sys.argv[1], 'rb').read()
BASE = 0x140000000
def s(va):
    r = va - BASE
    if 0x1000 <= r < len(d):
        e = d.find(b'\0', r, r+80); t = d[r:e]
        if len(t) >= 2 and all(32 <= c < 127 for c in t): return t.decode()
    return None
code_lo, code_hi = BASE + 0x1000, BASE + 0x5d88000
out = []
a = 0x1792000
end = 0x1792000 + 0x2796a8
prev = None
while a < end - 32:
    n, f, fn, h = struct.unpack_from('<QQQQ', d, a)
    name = s(n) if BASE <= n < BASE + len(d) else None
    if name and code_lo <= fn < code_hi and h < 2**32 and f < 2**48:
        if prev is None or a - prev != 32: out.append('-- table @%x' % a)
        out.append('%x %s %x' % (a, name, fn)); prev = a; a += 32; continue
    a += 8
open(sys.argv[2], 'w').write('\n'.join(out))
print(len(out))
