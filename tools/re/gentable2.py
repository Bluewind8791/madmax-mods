import sys, re
d = open(sys.argv[1], 'rb').read()
BASE = 0x140000000
skip = {'CEconomyResource[2]', 'CSpawnPickup@10[12]'}
rows = []
def add(label, va):
    if label in skip: return
    r = va - BASE
    rows.append('    {"%s", 0x%x, {%s}},' % (label, r, ', '.join('0x%02x' % b for b in d[r:r + 8])))
# parse classinfo output files: "== Class", "  vtable X this-offset Y", "    unique: [i]addr ..."
for path in sys.argv[3:]:
    cls = None; off = None
    for line in open(path):
        m = re.match(r'== (\S+)', line)
        if m: cls = m.group(1); continue
        m = re.match(r'\s+vtable \S+ this-offset (\S+)', line)
        if m: off = m.group(1); continue
        m = re.match(r'\s+unique: (.*)', line)
        if m:
            for i, a in re.findall(r'\[(\d+)\]([0-9a-f]+)', m.group(1)):
                add('%s%s[%s]' % (cls, '' if off == '0' else '@' + off, i), int(a, 16))
# SetEconomyResourceAmount is hooked with argument logging in research.cpp (InstallArgHooks).
add('CCharacter::GiveObject', 0x141ccd230)
open(sys.argv[2], 'w').write('// Generated from a memory dump of MadMax.exe. {label, rva, first 8 bytes}\n' + '\n'.join(rows) + '\n')
print(len(rows))
