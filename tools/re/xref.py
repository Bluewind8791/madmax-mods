import sys, re, struct
d = open(sys.argv[1], 'rb').read()
BASE = 0x140000000
names = sys.argv[2:]
leas = [(m.start()) for m in re.finditer(rb'[\x48\x4c]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]', d)]
print('lea count', len(leas))
for name in names:
    for m in re.finditer(re.escape(name.encode()) + b'\x00', d):
        rva = m.start()
        if d[rva-1] != 0: continue
        print('== %s string rva %x' % (name, rva))
        va = struct.pack('<Q', BASE + rva)
        for p in re.finditer(re.escape(va), d):
            print('  abs ptr at rva %x' % p.start())
        for p in leas:
            disp = struct.unpack_from('<i', d, p+3)[0]
            if p + 7 + disp == rva:
                print('  lea at rva %x' % p)
