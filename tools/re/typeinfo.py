import ctypes, struct, subprocess, collections
k32 = ctypes.WinDLL('kernel32')
r = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq MadMax.exe', '/FO', 'CSV', '/NH'], capture_output=True, text=True).stdout
pid = int([l for l in r.splitlines() if 'MadMax.exe' in l][0].split('","')[1])
h = k32.OpenProcess(0x0410, False, pid)
def rd(a, n):
    b = ctypes.create_string_buffer(n); k32.ReadProcessMemory(h, ctypes.c_void_p(a), b, n, None); return b.raw
q = lambda a: struct.unpack('<Q', rd(a, 8))[0]
types = q(q(0x1417f6418))
hdr = q(0x1417f6428); n1, n2 = struct.unpack('<II', rd(hdr, 8)); data = q(hdr + 8); ents = rd(data, n2 * 24)
cnt = collections.Counter(ents[i*24+8] for i in range(n2))
for t in range(80):
    d = rd(types + t * 36, 36)
    v = struct.unpack('<II7f', d)
    print('type %2d hash %08x cat %d  f: %s  n=%d' % (t, v[0], v[1], ' '.join('%g' % x for x in v[2:]), cnt[t]))
