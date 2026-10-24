# Build {jenkins hashlittle: string} from printable strings in given files -> hashdb.pkl
import sys, re, pickle
M = 0xffffffff
def rot(x, k): return ((x << k) | (x >> (32 - k))) & M
def hashlittle(data, init=0):
    n = len(data); a = b = c = (0xdeadbeef + n + init) & M; i = 0
    while n > 12:
        a = (a + int.from_bytes(data[i:i+4], 'little')) & M
        b = (b + int.from_bytes(data[i+4:i+8], 'little')) & M
        c = (c + int.from_bytes(data[i+8:i+12], 'little')) & M
        a = (a - c) & M; a ^= rot(c, 4); c = (c + b) & M
        b = (b - a) & M; b ^= rot(a, 6); a = (a + c) & M
        c = (c - b) & M; c ^= rot(b, 8); b = (b + a) & M
        a = (a - c) & M; a ^= rot(c, 16); c = (c + b) & M
        b = (b - a) & M; b ^= rot(a, 19); a = (a + c) & M
        c = (c - b) & M; c ^= rot(b, 4); b = (b + a) & M
        i += 12; n -= 12
    if n == 0: return c
    t = data[i:] + b'\0' * (12 - n)
    a = (a + int.from_bytes(t[0:4], 'little')) & M
    b = (b + int.from_bytes(t[4:8], 'little')) & M
    c = (c + int.from_bytes(t[8:12], 'little')) & M
    c ^= b; c = (c - rot(b, 14)) & M
    a ^= c; a = (a - rot(c, 11)) & M
    b ^= a; b = (b - rot(a, 25)) & M
    c ^= b; c = (c - rot(b, 16)) & M
    a ^= c; a = (a - rot(c, 4)) & M
    b ^= a; b = (b - rot(a, 14)) & M
    c ^= b; c = (c - rot(b, 24)) & M
    return c
if __name__ == '__main__':
    db = {}
    files = sys.argv[2:]
    if files and files[0].startswith("@"): files = open(files[0][1:]).read().splitlines() + files[1:]
    for f in files:
        for m in re.finditer(rb'[\x20-\x7e]{2,200}', open(f, 'rb').read()):
            s = m.group(0)
            for w in {s, *re.findall(rb'[A-Za-z0-9_.]{2,}', s)}:
                db.setdefault(hashlittle(w), w.decode())
    pickle.dump(db, open(sys.argv[1], 'wb')); print(len(db))
