import ctypes, sys, struct, subprocess
k32 = ctypes.WinDLL('kernel32')
r = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq MadMax.exe', '/FO', 'CSV', '/NH'], capture_output=True, text=True).stdout
pid = int([l for l in r.splitlines() if 'MadMax.exe' in l][0].split('","')[1])
h = k32.OpenProcess(0x0410, False, pid)
def rd(a, n):
    b = ctypes.create_string_buffer(n); k32.ReadProcessMemory(h, ctypes.c_void_p(a), b, n, None); return b.raw
q = lambda a: struct.unpack('<Q', rd(a, 8))[0]
mgr = q(0x1417f5338); x = q(mgr + 0x37a8); player = q(x + 0x20); inv = q(player + 0x438)
rid = struct.unpack('<H', rd(inv + 0x40, 2))[0]
tbl = q(q(0x1417f6428) + 8); entry = tbl + rid * 24
print('mgr %x player %x inv %x id %d entry %x' % (mgr, player, inv, rid, entry))
print('entry bytes', rd(entry, 24).hex(), 'amount', struct.unpack('<f', rd(entry + 0xc, 4))[0])
