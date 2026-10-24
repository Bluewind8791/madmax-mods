# snap.py save|diff : snapshot economy table (amount, type, pos) and diff against saved snapshot
import ctypes, sys, struct, subprocess, math, pickle, os
k32 = ctypes.WinDLL('kernel32')
r = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq MadMax.exe', '/FO', 'CSV', '/NH'], capture_output=True, text=True).stdout
pid = int([l for l in r.splitlines() if 'MadMax.exe' in l][0].split('","')[1])
h = k32.OpenProcess(0x0410, False, pid)
def rd(a, n):
    b = ctypes.create_string_buffer(n); k32.ReadProcessMemory(h, ctypes.c_void_p(a), b, n, None); return b.raw
q = lambda a: struct.unpack('<Q', rd(a, 8))[0]
hdr = q(0x1417f6428); n1, n2 = struct.unpack('<II', rd(hdr, 8)); data = q(hdr + 8); pos = q(q(0x1417f6420))
ents = rd(data, n2 * 24); P = rd(pos, n2 * 12)
player = q(q(q(0x1417f5338) + 0x37a8) + 0x20); pp = struct.unpack('<3f', rd(player + 0x208, 12))
cur = [(ents[i*24:(i+1)*24], struct.unpack_from('<3f', P, i*12)) for i in range(n2)]
f = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'snap.pkl')
print('n1 %d n2 %d player pos %s' % (n1, n2, tuple(round(v, 1) for v in pp)))
if sys.argv[1] == 'save':
    pickle.dump(cur, open(f, 'wb')); print('saved')
else:
    old = pickle.load(open(f, 'rb'))
    for i in range(max(len(old), len(cur))):
        o = old[i] if i < len(old) else None; c = cur[i] if i < len(cur) else None
        if o != c:
            def fmt(x):
                if not x: return '-'
                e, p = x
                return 'type %d amt %.2f raw %s pos %s' % (e[8], struct.unpack_from('<f', e, 0xc)[0], e.hex(), tuple(round(v, 1) for v in p))
            dist = math.dist(c[1], pp) if c else -1
            print('id %x dist %.1f\n   old %s\n   new %s' % (i, dist, fmt(o), fmt(c)))
