import sys, struct, bisect
d = open(sys.argv[1], 'rb').read(); BASE = 0x140000000
pd = []
o = 0x1a0c000
while o < 0x1a0c000 + 0x125600:
    b, e, _ = struct.unpack_from('<III', d, o)
    if b == 0: break
    pd.append((b, e)); o += 12
pd.sort(); starts = [p[0] for p in pd]
nat = {}
for line in open(sys.argv[2]):
    p = line.split()
    if len(p) == 3 and p[0] != "--": nat.setdefault(int(p[2], 16), []).append(p[1])
for a in sys.argv[3:]:
    va = int(a, 16); r = va - BASE; i = bisect.bisect_right(starts, r) - 1
    f = BASE + pd[i][0] if i >= 0 and pd[i][0] <= r < pd[i][1] else None
    print('%s -> func %s %s' % (a, '%x' % f if f else '?', ','.join(nat.get(f, [])) if f else ''))
