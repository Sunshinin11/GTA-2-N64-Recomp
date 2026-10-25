#!/usr/bin/env python3
"""Gera o p.gxt (portugues do Brasil) a partir do e.gxt do jogo + tools/pt_br.tsv.

uso: make_pt_gxt.py <e.gxt> <p.gxt de saida> [pt_br.tsv]

O formato do .gxt do GTA 2 (PC, little-endian): 'GBL'+idioma+versao(u16=100), depois os chunks
TKEY (12 bytes por texto: offset u32 + chave de 8 chars) e TDAT (textos UTF-16LE terminados em 0).
Letras acentuadas ficam gravadas como CODIGOS DE GLIFO da fonte (128..176), como no s.gxt; a fonte nao tem ã/õ,
entao eles usam os glifos â/ô.
"""
import struct, sys, os

# Latin-1 -> codigo de glifo da fonte (mesma tabela do engine: kExtendedWesternCharMap)
GLYPH = {}
for i, ch in enumerate("ÀÁÂÄÆÇÈÉÊËÌÍÎÏÒÓÔÖÙÚÛÜß"):          # 128..150
    GLYPH[ch] = 128 + i
for i, ch in enumerate("àáâäæçèéêëìíîïòóôöùúûü"):          # 151..172
    GLYPH[ch] = 151 + i
GLYPH.update({'Ñ': 173, 'ñ': 174, '¿': 175, '¡': 176})
GLYPH.update({'Ã': GLYPH['Â'], 'ã': GLYPH['â'], 'Õ': GLYPH['Ô'], 'õ': GLYPH['ô']})   # sem til na fonte
GLYPH.update({'º': ord('o'), 'ª': ord('a'), '°': ord('o')})


def enc(t):
    out = []
    for ch in t:
        o = ord(ch)
        if o < 128 or o >= 0x2000:
            out.append(ch)
        elif ch in GLYPH:
            g = GLYPH[ch]
            out.append(chr(g))
        else:
            sys.stderr.write("AVISO: caractere sem glifo %r em %r\n" % (ch, t[:40]))
            out.append('?')
    return ''.join(out)


def main():
    src, dst = sys.argv[1], sys.argv[2]
    tsv = sys.argv[3] if len(sys.argv) > 3 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pt_br.tsv')
    d = open(src, 'rb').read()
    assert d[:3] == b'GBL' and struct.unpack('<H', d[4:6])[0] == 100, "nao parece um .gxt do GTA 2"
    o = 6
    ch = {}
    while o + 8 <= len(d):
        t = d[o:o + 4]
        n = struct.unpack('<I', d[o + 4:o + 8])[0]
        ch[t] = d[o + 8:o + 8 + n]
        o += 8 + n
    tkey, tdat = ch[b'TKEY'], ch[b'TDAT']
    tr = {}
    for ln in open(tsv, encoding='utf-8'):
        ln = ln.rstrip('\n')
        if not ln or ln.startswith('#'):
            continue
        k, t = ln.split('\t', 1)
        tr[k] = t
    keys, strs = [], []
    for i in range(0, len(tkey), 12):
        off = struct.unpack('<I', tkey[i:i + 4])[0]
        key = tkey[i + 4:i + 12].split(b'\0')[0].decode('ascii')
        e = off
        while tdat[e:e + 2] != b'\0\0' or (e - off) % 2:
            e += 1
        s = tdat[off:e].decode('utf-16le')
        keys.append(key)
        strs.append(enc(tr[key]) if key in tr else s)
    data = bytearray()
    ktab = bytearray()
    for key, s in zip(keys, strs):
        ktab += struct.pack('<I', len(data)) + key.encode('ascii').ljust(8, b'\0')
        data += s.encode('utf-16le') + b'\0\0'
    out = b'GBLPd\x00' if False else b'GBLP' + struct.pack('<H', 100)
    out += b'TKEY' + struct.pack('<I', len(ktab)) + bytes(ktab)
    out += b'TDAT' + struct.pack('<I', len(data)) + bytes(data)
    open(dst, 'wb').write(out)
    print("p.gxt: %d textos, %d bytes (%d traduzidos)" % (len(keys), len(out), sum(1 for k in keys if k in tr)))


if __name__ == '__main__':
    main()
