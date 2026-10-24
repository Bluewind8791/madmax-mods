import re, sys, mmap
f = open(sys.argv[1], 'rb'); data = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
out = open(sys.argv[2], 'w')
for m in re.finditer(rb'[\x20-\x7e]{4,}', data):
    out.write('%08x %s\n' % (m.start(), m.group().decode()))
