import re, sys
for p in sys.argv[1:]:
    data = open(p, 'rb').read()
    seen = []
    for m in re.finditer(rb'[\x20-\x7e]{4,}', data):
        s = m.group().decode()
        if s not in seen:
            seen.append(s)
    print('\n'.join(seen))
