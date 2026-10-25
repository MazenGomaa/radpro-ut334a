"""Build {address: 'PERIPH_REG'} (plus field info) from a CMSIS SVD. Usage as module: load(path)."""
import xml.etree.ElementTree as ET

def _int(s):
    return int(s.strip(), 0)

def load(path):
    root = ET.parse(path).getroot()
    periphs = {p.findtext('name').strip(): p for p in root.iter('peripheral')
               if (p.findtext('name') or '').strip()}  # the SVD has one empty <peripheral/>
    regs = {}
    for name, p in periphs.items():
        base = _int(p.findtext('baseAddress'))
        src = p
        if p.get('derivedFrom'):
            src = periphs[p.get('derivedFrom').strip()]
        rs = src.find('registers')
        if rs is None:
            continue
        for r in rs.iter('register'):
            off = _int(r.findtext('addressOffset'))
            fields = []
            fs = r.find('fields')
            if fs is not None:
                for f in fs.iter('field'):
                    if f.findtext('bitOffset') is not None:
                        lsb = _int(f.findtext('bitOffset')); w = _int(f.findtext('bitWidth'))
                    else:
                        msb, lsb = [int(x) for x in f.findtext('bitRange').strip('[]').split(':')]
                        w = msb - lsb + 1
                    fields.append((f.findtext('name'), lsb, w))
            regs[base + off] = (f'{name}_{r.findtext("name")}', fields)
    return regs

if __name__ == '__main__':
    import sys
    m = load(sys.argv[1])
    bases = {}
    for a, (n, _) in sorted(m.items()):
        bases.setdefault(n.split('_')[0], a)
    for n, a in sorted(bases.items(), key=lambda x: x[1]):
        print(f'0x{a:08X} {n}')
