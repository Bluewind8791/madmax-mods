# GraphScript (.gsrc) printer: gs.py <file.gsrc>
import sys, struct, pickle, os
D = os.path.dirname(os.path.abspath(__file__))
DB = pickle.load(open(os.path.join(D, 'hashdb.pkl'), 'rb'))
def HN(v): return DB.get(v, '%08x' % v)
b = open(sys.argv[1], 'rb').read()
ic, io = struct.unpack_from('<II', b, 8)
nh, th, off, sz = struct.unpack_from('<IIII', b, io)
d = b[off:off+sz]
def arr(o): return struct.unpack_from('<qq', d, o)
def gsdata(o):
    name, typ = struct.unpack_from('<II', d, o)
    vo, vc = arr(o + 8)
    ref = d[o + 24]
    return name, typ, d[vo:vo+vc], ref
no, nc = arr(0)
bo, bc = arr(16 + 8)
blob = d[bo:bo+bc]
def slot(typ, raw):
    if len(raw) < 4: return raw.hex()
    p = struct.unpack_from('<I', raw)[0]
    s = blob[p:p+16]
    if len(s) < 8: return 'off %x' % p
    t = HN(typ)
    q = struct.unpack_from('<Q', s)[0]
    if t == 'string':
        so = q & 0xffffffff
        e = blob.find(b'\0', so)
        return repr(blob[so:e].decode('latin1')) if 0 <= so < len(blob) and e >= 0 else 'str@%x' % so
    if t == 'float': return '%g' % struct.unpack_from('<f', s)[0]
    if t == 'bool': return str(s[0])
    if t in ('uint32',): return HN(struct.unpack_from('<I', s)[0]) + ' (%d)' % struct.unpack_from('<I', s)[0]
    a, c = struct.unpack_from('<ii', s)
    return '%d,%d [%s]' % (a, c, HN(a & 0xffffffff))
def dset(o, ind):
    name = struct.unpack_from('<I', d, o)[0]
    dO, dC = arr(o + 8); sO, sC = arr(o + 24)
    out = [ind + HN(name)]
    for i in range(dC):
        n, t, raw, ref = gsdata(dO + i * 32)
        out.append(ind + '  .' + HN(n) + ' : ' + HN(t) + ' = ' + slot(t, raw) + (' REF' if ref else ''))
    for i in range(sC):
        out += dset(sO + i * 48, ind + '  ')
    return out
for i in range(nc):
    o = no + i * 64
    cls = struct.unpack_from('<I', d, o)[0]
    print('#%d %s' % (i, HN(cls)))
    print('\n'.join(dset(o + 16, '  ')))
print('blob', len(blob))
