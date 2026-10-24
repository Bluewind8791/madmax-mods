# Compare code bytes of the dump with the running game and print C arrays.
import ctypes, subprocess, sys
d = open(r'E:/mods/Mad Max/re/MadMax_dump.exe', 'rb').read()
k32 = ctypes.WinDLL('kernel32')
r = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq MadMax.exe', '/FO', 'CSV', '/NH'], capture_output=True, text=True).stdout
pid = int([l for l in r.splitlines() if 'MadMax.exe' in l][0].split('","')[1])
h = k32.OpenProcess(0x0410, False, pid)
for arg in sys.argv[1:]:
    va, n = [int(x, 16) for x in arg.split(':')]
    b = ctypes.create_string_buffer(n); k32.ReadProcessMemory(h, ctypes.c_void_p(va), b, n, None)
    dump = d[va - 0x140000000: va - 0x140000000 + n]
    print('%x len %d match %s' % (va, n, b.raw == dump))
    print(', '.join('0x%02x' % x for x in dump))
