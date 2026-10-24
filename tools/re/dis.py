import sys, struct, capstone
d = open(sys.argv[1], 'rb').read()
BASE = 0x140000000
va = int(sys.argv[2], 16); n = int(sys.argv[3]) if len(sys.argv) > 3 else 60
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
def s(v):
    r = v - BASE
    if 0 <= r < len(d):
        e = d.find(b'\0', r, r+80); t = d[r:e]
        if len(t) >= 3 and all(32 <= c < 127 for c in t): return '"%s"' % t.decode()
    return ''
r = va - BASE
for i, ins in enumerate(md.disasm(d[r:r+n*15], va)):
    if i >= n: break
    extra = ''
    if 'rip' in ins.op_str:
        try:
            disp = int(ins.op_str.split('rip ')[1].split(']')[0].replace(' ', ''), 16)
            tgt = ins.address + ins.size + disp; extra = '; %x %s' % (tgt, s(tgt))
        except Exception: pass
    print('%x: %-8s %s %s' % (ins.address, ins.mnemonic, ins.op_str, extra))
    if ins.mnemonic == 'ret' and len(sys.argv) <= 4: pass
