import sys, struct
d = open(sys.argv[1], 'rb').read()
BASE = 0x140000000
start = int(sys.argv[2], 16); n = int(sys.argv[3])
def s(va):
    r = va - BASE
    if 0 <= r < len(d):
        e = d.find(b'\0', r, r+80)
        t = d[r:e]
        if e > r and all(32 <= c < 127 for c in t): return t.decode()
    return None
for i in range(n):
    q = struct.unpack_from('<Q', d, start + i*8)[0]
    print('%x: %016x %s' % (start + i*8, q, s(q) or ''))
