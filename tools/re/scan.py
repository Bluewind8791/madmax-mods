# Value scanner for MadMax.exe. usage: scan.py first <value> | scan.py next <value>
import ctypes, ctypes.wintypes as wt, sys, struct, subprocess, array, os
k32 = ctypes.WinDLL('kernel32', use_last_error=True)
state = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'scan_state.bin')
mode, value = sys.argv[1], float(sys.argv[2])
r = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq MadMax.exe', '/FO', 'CSV', '/NH'], capture_output=True, text=True).stdout
pid = int([l for l in r.splitlines() if 'MadMax.exe' in l][0].split('","')[1])
h = k32.OpenProcess(0x0400 | 0x0010, False, pid)
class MBI(ctypes.Structure):
    _fields_ = [('BaseAddress', ctypes.c_void_p), ('AllocationBase', ctypes.c_void_p), ('AllocationProtect', wt.DWORD), ('PartitionId', wt.WORD),
                ('RegionSize', ctypes.c_size_t), ('State', wt.DWORD), ('Protect', wt.DWORD), ('Type', wt.DWORD)]
def read(addr, size):
    buf = ctypes.create_string_buffer(size); n = ctypes.c_size_t()
    if k32.ReadProcessMemory(h, ctypes.c_void_p(addr), buf, size, ctypes.byref(n)): return buf.raw[:n.value]
    return None
ib = struct.pack('<i', int(value)); fb = struct.pack('<f', value)
if mode == 'first':
    found = []
    addr = 0; mbi = MBI()
    while k32.VirtualQueryEx(h, ctypes.c_void_p(addr), ctypes.byref(mbi), ctypes.sizeof(mbi)):
        base = mbi.BaseAddress or 0
        if mbi.State == 0x1000 and mbi.Protect in (0x04, 0x40) and mbi.Type in (0x20000, 0x1000000):
            data = read(base, mbi.RegionSize)
            if data:
                for pat, kind in ((ib, 0), (fb, 1)):
                    i = data.find(pat)
                    while i != -1:
                        if i % 4 == 0: found.append((base + i) * 2 + kind)
                        i = data.find(pat, i + 1)
        addr = base + mbi.RegionSize
        if addr >= 0x7fffffffffff: break
else:
    old = array.array('Q'); old.frombytes(open(state, 'rb').read())
    found = []
    for e in old:
        a, kind = e >> 1, e & 1
        d = read(a, 4)
        if d == (fb if kind else ib): found.append(e)
array.array('Q', found).tofile(open(state, 'wb'))
print('candidates:', len(found))
for e in found[:20]: print('  %x %s' % (e >> 1, 'float' if e & 1 else 'int'))
