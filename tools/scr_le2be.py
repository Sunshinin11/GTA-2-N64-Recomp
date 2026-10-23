#!/usr/bin/env python3
"""Converte scripts .scr do GTA 2 (PC, little-endian) para o N64 (big-endian).

O motor le o .scr como blocos crus de memoria. Cada comando do script e' uma estrutura diferente
(92 tipos no gta2_re); a tabela abaixo diz, por opcode, quais campos de 16/32 bits o motor le.
Ela foi gerada a partir do codigo do gta2_re (tools/scr_tabela.json); campos de 8 bits nao mudam.
Bytes do comando fora da tabela sao trocados como 16 bits (o caso comum), exceto os de 8 bits conhecidos.

uso: scr_le2be.py <entrada> <saida>     (detecta: principal = 82656 bytes; missao = o resto)
"""
import json, os, struct, sys

TAB = {int(k): v for k, v in json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'scr_tabela.json'))).items()}

def swap16(buf, o):
    buf[o], buf[o + 1] = buf[o + 1], buf[o]

def swap32(buf, o):
    buf[o:o + 4] = buf[o:o + 4][::-1]

def convert_commands(data, offs):
    """data: bytearray com os comandos; offs: offsets (no proprio data) de todos os comandos"""
    so = sorted(set(offs))
    nxt = {so[i]: (so[i + 1] if i + 1 < len(so) else len(data)) for i in range(len(so))}
    unknown = set()
    for o in so:
        typ = struct.unpack_from('<H', data, o + 2)[0]       # le o tipo ANTES de trocar
        size = nxt[o] - o
        for k in range(0, 8, 2):                             # header: 4 campos de 16 bits
            swap16(data, o + k)
        ent = TAB.get(typ)
        if ent is None:
            unknown.add(typ)
            ent = {'p': [], 'b': []}
        done = set()
        for fo, fs in ent['p']:
            if fo + fs > size: continue
            (swap32 if fs == 4 else swap16)(data, o + fo)
            done.update(range(fo, fo + fs))
        u8 = set(ent['b'])
        for k in range(8, size - 1, 2):                      # resto: 16 bits, sem mexer em bytes de 8 bits
            if k in done or k + 1 in done or k in u8 or k + 1 in u8: continue
            swap16(data, o + k)
    return unknown

def convert_main(b):
    out = bytearray(b)
    bp = struct.unpack_from('<6000H', b, 0)
    for i in range(6000): swap16(out, i * 2)
    data = out[12000:12000 + 65536]
    unk = convert_commands(data, [bp[i] for i in range(1, 6000) if bp[i]])
    out[12000:12000 + 65536] = data
    t = 12000 + 65536
    tsz = struct.unpack_from('<H', b, t)[0]
    swap16(out, t)
    p = t + 2; end = p + tsz
    while p < end:                                           # textos: id, zona, tipo, campo 6 (16 bits) + tamanho (8 bits)
        ln = out[p + 8]
        for k in range(0, 8, 2): swap16(out, p + k)
        p += 9 + ln
    assert p == end, 'tabela de textos inconsistente'
    return bytes(out), unk

def convert_mission(b):
    out = bytearray(b)
    base, start, rem = struct.unpack_from('<HHI', b, 0)
    swap16(out, 0); swap16(out, 2); swap32(out, 4)
    bp = struct.unpack_from('<1536H', b, 8)
    for i in range(1536): swap16(out, 8 + i * 2)
    data = out[8 + 0xC00:]
    o0 = bp[0]
    unk = convert_commands(data, [bp[k] - o0 for k in range(1536) if bp[k]])
    out[8 + 0xC00:] = data
    return bytes(out), unk

def convert(b):
    return convert_main(b) if len(b) == 82656 else convert_mission(b)

if __name__ == '__main__':
    src, dst = sys.argv[1], sys.argv[2]
    b = open(src, 'rb').read()
    out, unk = convert(b)
    if unk: print('AVISO %s: opcodes sem tabela: %s' % (src, sorted(unk)))
    open(dst, 'wb').write(out)
