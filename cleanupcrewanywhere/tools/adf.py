# Minimal ADF v4 reader: adf.py <file> [typelib dir]  -> prints instance tree
import sys, struct, glob, os
PRIM = {0x580D0A62:('b',1),0x0CA2821D:('B',1),0xD13FCF93:('h',2),0x86D152BD:('H',2),
        0x192FE633:('i',4),0x075E4E4F:('I',4),0xAF41354F:('q',8),0xA139E01F:('Q',8),
        0x7515A207:('f',4),0xC609F663:('d',8)}
def parse(b):
    ver, ic, io, tc, to, uc, uo, nc, no, size = struct.unpack_from('<10I', b, 4)
    lens = b[no:no+nc]; p = no+nc; names = []
    for L in lens: names.append(b[p:p+L].decode('latin1')); p += L+1
    types = {}; p = to
    for _ in range(tc):
        t, sz, al, nh, ni, fl, eh, el, cnt = struct.unpack_from('<4Iq4I', b, p); p += 40
        mem = []
        if t == 8:
            for _ in range(cnt):
                mi, v = struct.unpack_from('<qi', b, p); p += 12; mem.append((names[mi], v))
        else:
            for _ in range(cnt):
                mi, th, msz, off, u1, u2 = struct.unpack_from('<qIIqII', b, p); p += 32
                mem.append((names[mi], th, msz, off & 0xffffff))
        types[nh] = dict(t=t, size=sz, name=names[ni], eh=eh, el=el, mem=mem)
    insts = []
    for i in range(ic):
        nh, th, off, sz, ni = struct.unpack_from('<IIIIq', b, io+i*24)
        insts.append((names[ni], th, off, sz))
    return types, insts
types = {}
for f in glob.glob(os.path.join(sys.argv[2], '*.adf')) if len(sys.argv) > 2 else []:
    try: types.update(parse(open(f,'rb').read())[0])
    except Exception as e: print('typelib fail', f, e, file=sys.stderr)
b = open(sys.argv[1], 'rb').read()
t2, insts = parse(b); types.update(t2)
import pickle
DB = pickle.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'hashdb.pkl'), 'rb'))
def HN(v): return DB.get(v, '%08x' % v)
out = sys.stdout
def val(d, off, th, ind, depth=0):
    if th in PRIM:
        c, n = PRIM[th]; v = struct.unpack_from('<'+c, d, off)[0]
        return HN(v) if c == 'I' and v > 0xffff else repr(v)
    T = types.get(th)
    if T is None: return '?type %08x' % th
    t = T['t']
    if t == 1:
        s = T['name'] + ' {\n'
        for (mn, mth, msz, moff) in T['mem']:
            s += ind+'  '+mn+' = '+val(d, off+moff, mth, ind+'  ', depth+1)+'\n'
        return s + ind + '}'
    if t in (3,):
        o, c = struct.unpack_from('<qq', d, off)
        E = T['eh']
        if E == 0x0CA2821D:
            raw = d[o:o+c]
            txt = raw.rstrip(bytes(1))
            if txt and all(32 <= x < 127 for x in txt): return 'str ' + repr(txt.decode())
            return 'hex ' + raw[:64].hex() + (' i32=%d f=%g h=%s' % (struct.unpack_from('<i', raw)[0], struct.unpack_from('<f', raw)[0], HN(struct.unpack_from('<I', raw)[0])) if c == 4 else '')
        if c > 2000: return '[%d items]' % c
        es = PRIM[E][1] if E in PRIM else types[E]['size'] if E in types else 0
        return '[' + ''.join('\n'+ind+'  '+val(d, o+i*es, E, ind+'  ', depth+1) for i in range(c)) + '\n'+ind+']'
    if t == 4:
        E = T['eh']; es = PRIM[E][1] if E in PRIM else types[E]['size']
        return '<' + ', '.join(val(d, off+i*es, E, ind+'  ', depth+1) for i in range(T['el'])) + '>'
    if t == 5:
        o = struct.unpack_from('<q', d, off)[0]
        e = d.index(b'\0', o); return repr(d[o:e].decode('latin1'))
    if t == 8:
        v = struct.unpack_from('<i', d, off)[0]
        for n, ev in T['mem']:
            if ev == v: return n
        return 'enum %d' % v
    if t == 9:
        return 'hash %08x' % struct.unpack_from('<I', d, off)[0]
    if t == 2:
        return 'ptr %x' % struct.unpack_from('<q', d, off)[0]
    if t == 7:
        return 'bits %x' % int.from_bytes(d[off:off+T['size']], 'little')
    return '?t%d' % t
for (n, th, off, sz) in insts:
    print(n, '=', val(b[off:off+sz], 0, th, ''))
