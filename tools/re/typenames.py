import ctypes, sys, struct, subprocess, re
def rot(x, k): return ((x << k) | (x >> (32 - k))) & 0xffffffff
def hashlittle(data, initval=0):
    length = len(data); a = b = c = (0xdeadbeef + length + initval) & 0xffffffff
    i = 0
    while length > 12:
        a = (a + struct.unpack_from('<I', data, i)[0]) & 0xffffffff
        b = (b + struct.unpack_from('<I', data, i+4)[0]) & 0xffffffff
        c = (c + struct.unpack_from('<I', data, i+8)[0]) & 0xffffffff
        a = (a - c) & 0xffffffff; a ^= rot(c, 4); c = (c + b) & 0xffffffff
        b = (b - a) & 0xffffffff; b ^= rot(a, 6); a = (a + c) & 0xffffffff
        c = (c - b) & 0xffffffff; c ^= rot(b, 8); b = (b + a) & 0xffffffff
        a = (a - c) & 0xffffffff; a ^= rot(c, 16); c = (c + b) & 0xffffffff
        b = (b - a) & 0xffffffff; b ^= rot(a, 19); a = (a + c) & 0xffffffff
        c = (c - b) & 0xffffffff; c ^= rot(b, 4); b = (b + a) & 0xffffffff
        length -= 12; i += 12
    tail = data[i:] + b'\0' * 12
    if length == 0: return c
    a = (a + struct.unpack_from('<I', tail, 0)[0]) & 0xffffffff if length > 0 else a
    b = (b + struct.unpack_from('<I', tail, 4)[0]) & 0xffffffff if length > 4 else b
    c = (c + struct.unpack_from('<I', tail, 8)[0]) & 0xffffffff if length > 8 else c
    # masks for partial words
    if length < 4: a = (a - struct.unpack_from('<I', tail, 0)[0] + (struct.unpack_from('<I', tail, 0)[0] & ((1 << (8*length)) - 1))) & 0xffffffff
    elif 4 < length < 8: b = (b - struct.unpack_from('<I', tail, 4)[0] + (struct.unpack_from('<I', tail, 4)[0] & ((1 << (8*(length-4))) - 1))) & 0xffffffff
    elif 8 < length < 12: c = (c - struct.unpack_from('<I', tail, 8)[0] + (struct.unpack_from('<I', tail, 8)[0] & ((1 << (8*(length-8))) - 1))) & 0xffffffff
    c ^= b; c = (c - rot(b, 14)) & 0xffffffff
    a ^= c; a = (a - rot(c, 11)) & 0xffffffff
    b ^= a; b = (b - rot(a, 25)) & 0xffffffff
    c ^= b; c = (c - rot(b, 16)) & 0xffffffff
    a ^= c; a = (a - rot(c, 4)) & 0xffffffff
    b ^= a; b = (b - rot(a, 14)) & 0xffffffff
    c ^= b; c = (c - rot(b, 24)) & 0xffffffff
    return c
k32 = ctypes.WinDLL('kernel32')
r = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq MadMax.exe', '/FO', 'CSV', '/NH'], capture_output=True, text=True).stdout
pid = int([l for l in r.splitlines() if 'MadMax.exe' in l][0].split('","')[1])
h = k32.OpenProcess(0x0410, False, pid)
def rd(a, n):
    b = ctypes.create_string_buffer(n); k32.ReadProcessMemory(h, ctypes.c_void_p(a), b, n, None); return b.raw
q = lambda a: struct.unpack('<Q', rd(a, 8))[0]
types = q(q(0x1417f6418))
want = {}
for t in range(80):
    hsh = struct.unpack('<I', rd(types + t * 36, 4))[0]
    want[hsh] = t
names = set()
for line in open(sys.argv[1], encoding='utf-8', errors='ignore'):
    s = line.rstrip('\n').split(' ', 1)[-1]
    for w in re.findall(r'[A-Za-z0-9_]{3,40}', s): names.add(w); names.add(w.lower())
for extra in sys.argv[2:]:
    for line in open(extra, encoding='utf-8', errors='ignore'):
        for w in re.findall(r'[A-Za-z0-9_]{3,40}', line): names.add(w); names.add(w.lower())
assert hashlittle(b'') == 0xdeadbeef
found = {}
for n in names:
    hv = hashlittle(n.encode())
    if hv in want: found.setdefault(want[hv], set()).add(n)
for t in sorted(want.values()):
    hsh = [k for k, v in want.items() if v == t][0]
    print(t, '%08x' % hsh, ','.join(sorted(found.get(t, []))))
