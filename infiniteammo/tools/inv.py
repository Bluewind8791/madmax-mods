# Print the player's inventory entries from the running game.
# Usage: inv.py <dump.exe>            print entries (names from strings in the dump)
#        inv.py <dump.exe> save|diff  snapshot counts / show what changed since save
#        inv.py <dump.exe> set <item name> <count>  write an item count (test only; a save game keeps it)
import ctypes, sys, struct, subprocess, re, os, json

HERE = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.join(HERE, 'hashnames.json')
SNAP = os.path.join(HERE, 'inv_snap.json')

def rot(x, k): return ((x << k) | (x >> (32 - k))) & 0xffffffff
def hashlittle(data, initval=0):
    # Jenkins lookup3 hashlittle, little-endian byte reads.
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
    if length == 0: return c
    tail = data[i:] + b'\0' * 12
    a = (a + struct.unpack_from('<I', tail, 0)[0]) & 0xffffffff
    if length > 4: b = (b + struct.unpack_from('<I', tail, 4)[0]) & 0xffffffff
    if length > 8: c = (c + struct.unpack_from('<I', tail, 8)[0]) & 0xffffffff
    c ^= b; c = (c - rot(b, 14)) & 0xffffffff
    a ^= c; a = (a - rot(c, 11)) & 0xffffffff
    b ^= a; b = (b - rot(a, 25)) & 0xffffffff
    c ^= b; c = (c - rot(b, 16)) & 0xffffffff
    a ^= c; a = (a - rot(c, 4)) & 0xffffffff
    b ^= a; b = (b - rot(a, 14)) & 0xffffffff
    c ^= b; c = (c - rot(b, 24)) & 0xffffffff
    return c

def names(dump):
    if os.path.exists(CACHE):
        return {int(k): v for k, v in json.load(open(CACHE)).items()}
    d = open(dump, 'rb').read()
    m = {}
    for s in set(re.findall(rb'[A-Za-z0-9_.\-/]{3,80}', d)):
        m[hashlittle(s)] = s.decode()
    json.dump(m, open(CACHE, 'w'))
    return m

# Item names that are not stored as separate strings in the dump.
EXTRA = {0x101e5fcd: 'thunderstick', 0x97f610e7: 'shiv'}

k32 = ctypes.WinDLL('kernel32')
r = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq MadMax.exe', '/FO', 'CSV', '/NH'], capture_output=True, text=True).stdout
pid = int([l for l in r.splitlines() if 'MadMax.exe' in l][0].split('","')[1])
h = k32.OpenProcess(0x0438, False, pid)
def rd(a, n):
    b = ctypes.create_string_buffer(n); k32.ReadProcessMemory(h, ctypes.c_void_p(a), b, n, None); return b.raw
q = lambda a: struct.unpack('<Q', rd(a, 8))[0]

player = q(q(q(0x1417f5338) + 0x37a8) + 0x20)
inv = q(player + 0x438)
beg, end = q(inv), q(inv + 8)
print('player %x inventory %x entries %x-%x (%d)' % (player, inv, beg, end, (end - beg) // 8))
nm = {**names(sys.argv[1]), **EXTRA}
cur = {}
raw = rd(beg, end - beg) if 0 < end - beg < 0x10000 else b''
for i in range(0, len(raw), 8):
    hsh, cnt, b6, b7 = struct.unpack_from('<IHBB', raw, i)
    cur[hsh] = cnt
    if len(sys.argv) < 3:
        print('%3d @%x  %08x  count %5d  b6 %02x b7 %02x  %s' % (i // 8, beg + i, hsh, cnt, b6, b7, nm.get(hsh, '?')))
if len(sys.argv) > 2 and sys.argv[2] == 'save':
    json.dump(cur, open(SNAP, 'w')); print('saved', len(cur))
elif len(sys.argv) > 4 and sys.argv[2] == 'set':
    want = hashlittle(sys.argv[3].encode())
    for i in range(0, len(raw), 8):
        if struct.unpack_from('<I', raw, i)[0] == want:
            k32.WriteProcessMemory(h, ctypes.c_void_p(beg + i + 4), struct.pack('<H', int(sys.argv[4])), 2, None)
            print('set %s to %s' % (sys.argv[3], sys.argv[4]))
            break
    else:
        print('%s is not in the inventory' % sys.argv[3])
elif len(sys.argv) > 2 and sys.argv[2] == 'diff':
    old = {int(k): v for k, v in json.load(open(SNAP)).items()}
    for k in sorted(set(old) | set(cur)):
        if old.get(k) != cur.get(k):
            print('%08x %s: %s -> %s' % (k, nm.get(k, '?'), old.get(k), cur.get(k)))
