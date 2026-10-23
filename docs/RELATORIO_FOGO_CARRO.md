# GTA 2 N64 (recomp): relatório do fogo do carro

Data: 21/10/2026
Projeto: `C:\Desenvolvimento\Nintendo 64\GTA264\GTA2 N64 Tech Demo\`
Status: **resolvido e validado no Ares**. A medição `[fogo]` ainda está no código e falta removê-la.

---

## 1. Problema

- Quando o carro pegava fogo, só o som tocava; a chama não aparecia.
- No PC/PS1 o fogo são partículas criadas pelo foco de fogo (`Explosion_30`, tipos 3/4/5/12/13/14 -> `EmitFireTrail_*`), com sprites `code_obj2` ids `def112 + 0, 96-102, 147-151`.

## 2. Hipóteses já descartadas (antes desta sessão)

- Fogo invisível por alfa zero: o jogo manda alfa 80 a 240 (`SetBlendAndAlpha`).
- Partículas desligadas: `skip_particles` não está setado, o serviço roda todo quadro e o pool tem 500, igual ao PC.

## 3. Medição aplicada

Build de medição com backups `.bak_fogo` em `sprite.cpp`, `Explosion_30.cpp`, `Particle_8.cpp`, `gbh_n64.cpp` e `render_n64.c`.
A linha `[fogo]` sai no log a cada 120 quadros:

| Campo | Significado |
|---|---|
| focos | atualizações de focos de fogo |
| criadas | partículas de chama criadas |
| falhas_pool | partículas não criadas por pool cheio |
| sprite_draw | sprites de chama que chegaram em `Sprite::Draw_59EFF0` |
| gbh_draw | chegaram na camada gbh (`gbh_n64.cpp`) |
| rejeitadas / tex_vazia | descartadas no gbh / textura vazia |
| enviadas | quads enviados ao RDP |
| alfa / blend / zoom / sem_tex | estado do último quad de chama |
| pos / z | posição do último quad |

Critério de leitura:
- `criadas`=0: o problema está na criação.
- `sprite_draw`=0: o sprite não chega ao desenho.
- `rejeitadas`/`tex_vazia` subindo: o problema está no gbh ou na conversão de textura.
- `enviadas`>0: o problema está no RDP ou na ordem de desenho.

## 4. Resultado da medição (antes da correção)

Quadro ~1320, carro em chamas:

```
[fogo] focos=101 criadas=63 falhas_pool=0 sprite_draw=0 gbh_draw=0 rejeitadas=0 tex_vazia=0 enviadas=0
```

As partículas eram criadas sem falha de pool, mas nenhuma chegava ao desenho.

## 5. Investigação (pesquisa de código, sem build)

Caminho de desenho conferido no `gta2_re` e igual ao PC:

1. `Explosion_30::EmitFireTrail_*` cria a partícula (`Particle_8::New_53E3C0`), define o sprite (`code_obj2`, `def112 + 96`) e coloca na `gSpriteGrid_3_679210`.
2. `Game_0x40::Draw_4B92D0` -> `SpriteGrid_400::DrawSpritesClipped_477A40` -> `AddToDrawList_478240` -> `SpriteRenderer_1C::DisplayAdd_495510` (camada por `ComputeZLayer_5A1BD0`).
3. `MapRenderer::Draw_4F6A20` -> `SpriteRenderer_1C::Draw_495560(camada)` -> `SpriteTree_4::Draw_5C5DF0` -> `Sprite::Draw_59EFF0` -> `pgbh_DrawQuad`.

Nenhum desses arquivos tem mudança `#ifdef N64`. O defeito estava na atualização da partícula:

- `Particle_4C::UpdateDirectedProjectile_state_3_12_5384C0` (estado 3, a chama do carro) faz:
  ```cpp
  Fix16 zpos = field_30_pNext->field_1C_zpos + dword_6FD540;
  if (zpos > dword_6FD28C) return true;   // mata a partícula
  ```
- Em `Particle_4C.cpp`, o limite é calculado a partir de valores de outro arquivo:
  ```cpp
  DEFINE_GLOBAL_INIT(Fix16, dword_6FD554, dword_6FD448, 0x6FD554);                    // dword_6FD448 está em Explosion_30.cpp
  DEFINE_GLOBAL_INIT(Fix16, dword_6FD28C, kFP16Eight_6FD4C0 - dword_6FD554, 0x6FD28C); // kFP16Eight_6FD4C0 está em Explosion_30.cpp
  ```

## 6. Causa raiz (confirmada no binário)

- No N64 todos esses globais ficam em `.sbss` (`gta2_recomp.map`), ou seja, começam zerados e só recebem valor quando o inicializador do arquivo roda.
- A libdragon roda os inicializadores globais (`.ctors`) **de trás para frente**. A função `__wrap___do_global_ctors` foi lida direto no `gta2_recomp.elf`: ela começa em `__CTOR_END__ - 4` e anda `-4` até `__CTOR_LIST__`.
- Ordem na `.ctors`: `Explosion_30.o` (0x801d288c) vem antes de `Particle_4C.o` (0x801d28b8). Por isso `Particle_4C` inicializa primeiro, quando os valores de `Explosion_30` ainda são zero.
- Resultado: `dword_6FD554 = 0` e `dword_6FD28C = 0 - 0 = 0` para sempre. Toda partícula de chama morria na primeira atualização, antes de ser desenhada.
- No PC funciona por causa da ordem de inicialização do MSVC.

### Varredura de casos parecidos

Foram verificados todos os `DEFINE_GLOBAL_INIT` que dependem de global de outro arquivo:

| Arquivo | Global | Depende de | Ordem no N64 |
|---|---|---|---|
| Char_Pool.cpp | gDummyH_678584 | gDummyW_678530 (Ped.cpp) | correta |
| Police_38.cpp | kFpEight_6FED48 | kFpOne64th_6FEB88 (Police_7B8.cpp) | correta |
| Particle_4C.cpp | dword_6FD554 | dword_6FD448 (Explosion_30.cpp) | **errada (corrigida)** |
| Particle_4C.cpp | dword_6FD28C | kFP16Eight_6FD4C0 (Explosion_30.cpp) | **errada (corrigida)** |

Atenção: se a lista de arquivos do Makefile mudar de ordem, os dois casos "corretos" podem quebrar do mesmo jeito.

## 7. Correção aplicada

Arquivo: `port/gta2_re/Source/Particle_4C.cpp` (backup `Particle_4C.cpp.bak_ctor`):

```cpp
#ifdef N64
// N64: a libdragon roda os inicializadores globais em ordem reversa; nao depender de Explosion_30.cpp
DEFINE_GLOBAL_INIT(Fix16, dword_6FD554, Fix16(0x100, 0), 0x6FD554);
DEFINE_GLOBAL_INIT(Fix16, dword_6FD28C, Fix16(8) - Fix16(0x100, 0), 0x6FD28C);
#else
DEFINE_GLOBAL_INIT(Fix16, dword_6FD554, dword_6FD448, 0x6FD554);
DEFINE_GLOBAL_INIT(Fix16, dword_6FD28C, kFP16Eight_6FD4C0 - dword_6FD554, 0x6FD28C);
#endif
```

Os valores são os mesmos do PC. Nenhum outro arquivo foi alterado.

## 8. Resultado da medição (depois da correção)

Quadro ~1320, carro em chamas:

```
[fogo] focos=212 criadas=142 falhas_pool=0 sprite_draw=2119 gbh_draw=2119 rejeitadas=0 tex_vazia=0 enviadas=2119 alfa=160 blend=2 zoom=0 sem_tex=0 pos=-231,-225 z=0.390
```

- Fogo visível no Ares (confirmado pelo Georges).
- Todos os sprites de chama chegam ao RDP: 2119 desenhados, 0 rejeitados, 0 sem textura.
- Alfa 160 e blend 2 batem com o esperado.

## 9. Desempenho durante o fogo

Mesma região, quadro ~1200 sem fogo contra quadro ~1320 com fogo:

| Métrica | Sem fogo (antes, q.1320) | Sem fogo (depois, q.1080) | Com fogo (depois, q.1320) |
|---|---|---|---|
| fps | 19.5 | 17.6 | 14.4 |
| período | 51.2 ms | 56.9 ms | 69.5 ms |
| conv_tex | 3.69 ms | 3.13 ms | **10.42 ms** |
| jogo~ | 37.2 ms | 38.7 ms | 50.3 ms |
| serviço partículas | 0.1 ms | 0.1 ms | 0.9 ms |
| objetos | 2.4 ms | 2.5 ms | 3.1 ms |
| tris | 215 | 329 | 272 |
| heap | 5647 KB | 5628 KB | 5654 KB |

Leitura: com o fogo aparecendo, o maior custo novo é a conversão de textura (`conv_tex` foi de ~3 para ~10 ms), provavelmente pela troca de quadros da animação da chama (ids 96-102). É um ponto para otimizar depois; um problema por vez.

## 10. Pendências

1. Remover a medição `[fogo]`: voltar dos `.bak_fogo` (`sprite.cpp`, `Explosion_30.cpp`, `Particle_8.cpp`, `gbh_n64.cpp`, `render_n64.c`) mantendo a correção do `Particle_4C.cpp`.
   - Conferido: `Explosion_30.cpp.bak_fogo` já contém a correção antiga do pool de explosões (`return lowest_idx`).
2. Avaliar o custo de `conv_tex` com fogo (cache dos quadros da chama).
3. O carro não mostra amassados de dano antes de explodir (próximo problema).
4. Seguir a lista de próximos passos do documento de passagem (peso da área da igreja, Beta etc.).

## 11. Como compilar e testar

Na pasta do projeto (PowerShell):

```powershell
powershell -ExecutionPolicy Bypass -File build_recomp.ps1
powershell -ExecutionPolicy Bypass -File tools\read_ares_recomp.ps1
```
