import sys, struct, os
data = open(sys.argv[1], 'rb').read()
out = sys.argv[2]
i = 0; n = 0
while True:
    i = data.find(b' FDA', i)
    if i < 0: break
    ver, ic, io, tc, to, uc, uo, nc, no, size = struct.unpack_from('<10I', data, i+4)
    if ver == 4 and tc > 0 and 0x40 < size < 0x1000000 and uc == 0:
        open(os.path.join(out, '%08x.adf' % i), 'wb').write(data[i:i+size]); n += 1
    i += 4
print(n)
