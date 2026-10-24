# List strongholds and their project statuses from the running game (read-only).
import ctypes, struct, subprocess, pickle, os
k32 = ctypes.WinDLL('kernel32')
r = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq MadMax.exe', '/FO', 'CSV', '/NH'], capture_output=True, text=True).stdout
pid = int([l for l in r.splitlines() if 'MadMax.exe' in l][0].split('","')[1])
h = k32.OpenProcess(0x0410, False, pid)
def rd(a, n):
    b = ctypes.create_string_buffer(n); k32.ReadProcessMemory(h, ctypes.c_void_p(a), b, n, None); return b.raw
q = lambda a: struct.unpack('<Q', rd(a, 8))[0]
u = lambda a: struct.unpack('<I', rd(a, 4))[0]
DB = pickle.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'hashdb.pkl'), 'rb'))
mgr = q(0x1417f5fd8)
n = u(mgr + 0x7c)
print('mgr %x count %d' % (mgr, n))
for i in range(n):
    p, c = struct.unpack('<QQ', rd(mgr + 0x18 + i * 16, 16))
    uses = u(c + 8) if c else 0
    cid = u(mgr + 0x68 + i * 4)
    print('#%d sh %x ctrl %x uses %d cacheid %08x' % (i, p, c, uses, cid))
    if not p or not uses: continue
    print('   vtbl %x territory %08x (%s)' % (q(p), u(p + 0x128), DB.get(u(p + 0x128), '?')))
    b, e = q(p + 0x1c0), q(p + 0x1c8)
    for a in range(b, e, 0x48):
        ph = u(a); st = rd(a + 0x46, 1)[0]
        print('     %08x %-28s status %d' % (ph, DB.get(ph, '?'), st))
