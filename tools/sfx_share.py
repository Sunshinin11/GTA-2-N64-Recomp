#!/usr/bin/env python3
"""sfx_share.py - um unico banco de efeitos sonoros para TODOS os mapas (bil, ste, wil).

Os bancos .raw dos mapas sao ~99% identicos. Em vez de ~6 MB por mapa na ROM, grava-se UM banco (audio/sfx.raw):
o banco do 1o mapa (intacto) + so os trechos que nao existem nele. Cada mapa ganha o proprio .sdt apontando
para o lugar certo e um <mapa>.raw de 4 bytes (placeholder: o motor so mede o tamanho dele).

Equivalencia EXATA com o pipeline antigo (troca global de 2 bytes por sample de todo o arquivo):
 - cada "unidade" (um efeito, ou a faixa inteira 69..89 / 115..135 que o jogo toca de uma vez como bloco continuo)
   e expandida para limites PARES [a,b), entao o parceiro de byte de cada sample vem junto;
 - a unidade e procurada no banco em posicao PAR (mesma paridade) -> o resultado apos a troca global e identico;
 - o layout relativo dentro da unidade (distancia entre efeitos) e preservado -> PlaySampleRange continua valido.
Tudo e conferido byte a byte (apos a troca) contra o banco original de cada mapa; aborta se algo diferir.

uso: sfx_share.py <pasta_data_do_jogo> <pasta_saida_data> <mapa1> [mapa2 ...]   (o 1o mapa e a base)
"""
import os, struct, sys

RANGES = [(69, 89), (115, 135)]       # UpdateRadioChatterLoop_41FCA0 -> PlaySampleRange_58E8C0(idx, a3)

def find_ci(folder, name):
    for f in os.listdir(folder):
        if f.lower() == name.lower():
            return os.path.join(folder, f)
    return None

def swap2(b):
    a = bytearray(b)
    a[0::2], a[1::2] = b[1::2], b[0::2]
    return bytes(a)

def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    src, out, maps = sys.argv[1], sys.argv[2], sys.argv[3:]
    adir = find_ci(src, 'audio')
    if not adir:
        sys.exit('ERRO: sem pasta audio em %s' % src)
    banks = {}
    for m in maps:
        rp, sp = find_ci(adir, m + '.raw'), find_ci(adir, m + '.sdt')
        if not rp or not sp:
            print("AVISO: banco '%s' nao existe em %s (ignorado)" % (m, adir)); continue
        raw = open(rp, 'rb').read(); sdt = open(sp, 'rb').read()
        assert len(sdt) == 320 * 24 and len(raw) % 2 == 0, (m, len(sdt), len(raw))
        ents = [list(struct.unpack('<6i', sdt[i * 24:i * 24 + 24])) for i in range(320)]
        for e in ents:
            assert e[0] >= 0 and e[1] >= 0 and e[0] + e[1] <= len(raw), (m, e)
        banks[m] = (raw, ents)
    names = [m for m in maps if m in banks]
    if not names:
        sys.exit('ERRO: nenhum banco encontrado')
    base = names[0]
    bank = bytearray(banks[base][0])
    new_sdt = {base: [list(e) for e in banks[base][1]]}
    stats = {}
    for m in names[1:]:
        raw, ents = banks[m]
        groups = [[i] for i in range(320) if not any(a <= i <= b for a, b in RANGES)]
        groups += [list(range(a, b + 1)) for a, b in RANGES]
        res = [list(e) for e in ents]
        reused = added = 0
        for g in groups:
            live = [i for i in g if ents[i][1] > 0]
            if not live:
                continue
            lo = min(ents[i][0] for i in live) & ~1
            hi = max(ents[i][0] + ents[i][1] for i in live)
            hi += hi & 1
            chunk = raw[lo:hi]
            pos = -1
            p = bank.find(chunk)
            while p >= 0:
                if p % 2 == 0:
                    pos = p; break
                p = bank.find(chunk, p + 1)
            if pos < 0:
                pos = len(bank); bank += chunk; added += len(chunk)
            else:
                reused += len(chunk)
            for i in live:
                res[i][0] = pos + (ents[i][0] - lo)
        new_sdt[m] = res
        stats[m] = (reused, added)
    # conferencia no dominio ja trocado (o que o N64 le)
    sb = swap2(bytes(bank))
    for m in names:
        raw, ents = banks[m]
        sr = swap2(raw)
        for i, (e, n) in enumerate(zip(ents, new_sdt[m])):
            assert e[1] == n[1] and e[2:] == n[2:], (m, i)
            if e[1] == 0: continue
            assert sr[e[0]:e[0] + e[1]] == sb[n[0]:n[0] + e[1]], (m, i, e, n)
        for a, b in RANGES:
            live = [i for i in range(a, b + 1) if ents[i][1] > 0]
            if not live: continue
            lo_i = min(live, key=lambda i: ents[i][0])
            hi_i = max(live, key=lambda i: ents[i][0] + ents[i][1])
            lo_e, lo_n, hi_e = ents[lo_i], new_sdt[m][lo_i], ents[hi_i]
            span = hi_e[0] + hi_e[1] - lo_e[0]
            assert sr[lo_e[0]:lo_e[0] + span] == sb[lo_n[0]:lo_n[0] + span], (m, 'faixa', a, b)
    odir = os.path.join(out, 'audio'); os.makedirs(odir, exist_ok=True)
    def put(path, data):
        if os.path.exists(path) and open(path, 'rb').read() == data: return
        open(path, 'wb').write(data)
    put(os.path.join(odir, 'sfx.raw'), sb)
    for m in names:
        put(os.path.join(odir, m.lower() + '.sdt'), b''.join(struct.pack('>6i', *e) for e in new_sdt[m]))
        put(os.path.join(odir, m.lower() + '.raw'), b'SFX\0')
    tot = sum(len(banks[m][0]) for m in names)
    print('    [SFX] banco unico sfx.raw = %d bytes (antes: %d bytes em %d bancos; economia %.2f MB)' %
          (len(bank), tot, len(names), (tot - len(bank)) / 1048576.0))
    for m, (r, a) in stats.items():
        print('          %s: %d bytes reaproveitados, %d bytes novos' % (m, r, a))

main()
