import ctypes, sys, struct, subprocess, math
# Print economy entries near the player and whether CarAutoLoot's IsScrapPile filter accepts them.
# Usage: near.py [radius_m]   (default 40)
k32 = ctypes.WinDLL('kernel32')
r = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq MadMax.exe', '/FO', 'CSV', '/NH'], capture_output=True, text=True).stdout
pid = int([l for l in r.splitlines() if 'MadMax.exe' in l][0].split('","')[1])
h = k32.OpenProcess(0x0410, False, pid)
def rd(a, n):
    b = ctypes.create_string_buffer(n); k32.ReadProcessMemory(h, ctypes.c_void_p(a), b, n, None); return b.raw
q = lambda a: struct.unpack('<Q', rd(a, 8))[0]
radius = float(sys.argv[1]) if len(sys.argv) > 1 else 40.0
player = q(q(q(0x1417f5338) + 0x37a8) + 0x20)
px, py, pz = struct.unpack('<3f', rd(player + 0x208, 12))
scrap_id = struct.unpack('<H', rd(q(player + 0x438) + 0x40, 2))[0]
hdr = q(0x1417f6428); static, used = struct.unpack('<II', rd(hdr, 8))
ents = rd(q(hdr + 8), used * 24)
pos = rd(q(q(0x1417f6420)), used * 12)
types = q(q(0x1417f6418))
print('player %.1f %.1f %.1f  scrap id %d  static %d used %d' % (px, py, pz, scrap_id, static, used))
rows = []
for i in range(used):
    t, amount = ents[i*24+8], struct.unpack_from('<f', ents, i*24+12)[0]
    x, y, z = struct.unpack_from('<3f', pos, i*12)
    d3 = math.sqrt((x-px)**2 + (y-py)**2 + (z-pz)**2)
    if d3 > radius: continue
    v = struct.unpack('<II7f', rd(types + t * 36, 36))
    ok = v[1] == 3 and v[0] != 0xb31e209f and v[8] > 0 and 0 < v[4] <= 100
    rows.append((d3, i, t, v, amount, y - py, ok))
for d3, i, t, v, amount, dy, ok in sorted(rows):
    print('%5.1f m dy %+5.1f  id %4d type %2d cat %d amount %g  f: %s  %s' % (
        d3, dy, i, t, v[1], amount, ' '.join('%g' % x for x in v[2:]), 'ACCEPT' if ok else 'reject'))
