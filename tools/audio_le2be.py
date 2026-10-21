#!/usr/bin/env python3
"""audio_le2be.py - bancos de som do GTA 2 (PC, little-endian) para o N64 (big-endian).
   .raw: PCM 16 bits mono -> troca os 2 bytes de cada sample.
   .sdt: 320 entradas de 6 x s32 (offset, tamanho, taxa, deslocamento, inicio/fim do loop) -> troca 4 bytes.
uso: audio_le2be.py <entrada> <saida>"""
import sys
src, dst = sys.argv[1], sys.argv[2]
d = bytearray(open(src, 'rb').read())
w = 4 if src.lower().endswith('.sdt') else 2
assert len(d) % w == 0, "%s: tamanho %d nao e multiplo de %d" % (src, len(d), w)
for i in range(0, len(d), w):
    d[i:i + w] = d[i:i + w][::-1]
open(dst, 'wb').write(d)
