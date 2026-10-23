# GTA 2 N64 (recomp): relatório geral do projeto

Atualizado em: 21/10/2026
Uso estritamente pessoal. Repositório GitHub privado (`Sunshinin11/GTA-2-N64-Recomp`), usado só como backup e organização.

---

## 1. Identificação

| Item | Valor |
|---|---|
| Pasta do projeto | `C:\Desenvolvimento\Nintendo 64\GTA264\GTA2 N64 Tech Demo\` |
| Dados do jogo | `C:\Desenvolvimento\Nintendo 64\GTA264\gta2_data\data` (versão freeware) |
| Base do motor | decomp `gta2_re` (GTA 2 PC), em `port/gta2_re/Source` |
| Camada N64 | `port/n64` (plataforma, renderer, áudio, entrada) |
| SDK | libdragon preview, GCC 16.2.0 (mips64-elf), compilado no WSL |
| Emulador de teste | Ares (único confiável para libdragon), paraLLEl-RDP via Vulkan |
| Console alvo | N64 com Expansion Pak (8 MB) |
| ROM gerada | `gta2_recomp.z64` (31.784.960 bytes, ~30,3 MB) |
| ELF | `build_recomp/gta2_recomp.elf`: text 1.737.176, data 208.377, bss 318.180 (total ~2,2 MB) |

Observação: o `README.md` da raiz descreve a **tech demo antiga** (cenário procedural 20x20, `gta2_n64_demo.z64`, Tiny3D). O trabalho atual é o **recomp** (motor real do GTA 2), descrito neste documento.

## 2. Meta

**V0.0.1 Beta (jogável)**: menu com save, missões, distritos com áudio, splash com logo do N64 e layout de controle do PS1.

Planos de longo prazo:
- Colocar os 3 mapas (Downtown, Residential, Industrial) na ROM, com som e rádios.
- Primeiro deixar o jogo funcional e com todo o conteúdo; compressão de som e otimização (resolução, população) depois.
- Cartucho repro físico para uso próprio.
- Depois do GTA 2: Driver 2 (REDRIVER2), começando por medir os dados.

## 3. Como compilar e testar

Na pasta do projeto (PowerShell):

```powershell
powershell -ExecutionPolicy Bypass -File build_recomp.ps1
powershell -ExecutionPolicy Bypass -File tools\read_ares_recomp.ps1
```

- O primeiro compila no WSL (`build.sh -f Makefile.recomp -j4 all`) e grava `build_recomp_log.txt`.
- O segundo abre o Ares e grava o console em `ares_console_recomp.txt` (é o log que deve ser lido).

Compilação (`Makefile.recomp`):
- Motor em `-Os` (a RAM é o gargalo); arquivos quentes em `-O2`: `MapRenderer`, `sprite`, `SpriteRenderer_1C`, `SpriteGrid_400`, `Draw`, `map_0x370`, `collide`, `CarPhysics_B0`, `Fix16_Point`, `fix16`, `ang16`, `angle`.
- Flags do motor: `-DGTA2_PORT=1 -DN64 -fshort-wchar -fms-extensions -fpermissive -fno-exceptions -fno-rtti`.
- `--wrap=malloc/calloc/realloc` para medir alocações.
- Dados copiados para o DragonFS por `tools/stage_data.sh` (lista `DATA_ENTRIES`: `bil.gmp/sty/scr`, `fstyle.sty`, textos `e.gxt`/`s.gxt`, áudio `bil.raw/sdt`, rádios `gtaudio/*.wav`).

## 4. Arquivos principais

| Arquivo | Papel |
|---|---|
| `port/n64/render_n64.c` | renderer RDP, cache de texturas, medições `[perf]`, `[perf2]`, `[audio]`, `[fogo]` |
| `port/n64/gbh_n64.cpp` | camada gbh (Draw, texturas, luzes) |
| `port/n64/audio_n64.c` | mixer, rádios, cache de efeitos |
| `port/n64/main_n64.c` | boot, vídeo, loop |
| `port/n64/game_entry.cpp` | registro e flags de debug do motor |
| `port/n64/miles_n64.cpp` | substituto do Miles Sound System |
| `port/gta2_re/Source/*` | motor (mudanças sempre sob `#ifdef N64`) |
| `port/gta2_re/Source/n64_population.h` | população (`N64_POP_PCT`) |
| `Makefile.recomp`, `tools/stage_data.sh` | build e dados |
| `tools/scr_le2be.py` + `tools/scr_tabela.json` | conversão dos scripts de missão para big-endian |

## 5. Estado atual

- **Missões** funcionam com o script real (`bil.scr` convertido por `tools/scr_le2be.py`; validado em 70 arquivos e 30.719 comandos, 0 erros).
- **Vídeo**: tela 640x240 (`console_init`). Filtro de textura ligado (botão L; não desligar).
- **População**: 40% (`n64_population.h`, `N64_POP_PCT`).
- **Câmera**: 15% do afastamento por velocidade (`Camera.cpp`, `N64_CAM_SPEED_PCT`).
- **Luzes dinâmicas**: desligadas (`gbh_n64.cpp`, `N64_LUZES 0`), ganho de 8 a 15 ms por quadro. Falta o Georges decidir se o visual sem elas é aceitável.
- **Áudio**: 22050 Hz, rádios ULC a 12 kHz, cache de efeitos em RAM (256 KB).
- **Pool de sprites**: 2000 no N64 (5031 no PC), em `sprite.hpp` (`N64_SPRITE_POOL`).
- **Fogo do carro**: corrigido e validado nesta sessão (seção 7.6).

## 6. Medições

### 6.1 Memória (log de 21/10/2026)

| Momento | Heap usado | Arena | Observação |
|---|---|---|---|
| Antes do init | 608 KB | 616 KB | fim_bss=80228D38 |
| Depois do init | 784 KB | 792 KB | |
| Loop, quadro ~600 | 5.522 KB | 5.544 KB | |
| Loop, quadro ~1200 | 5.628 KB | 5.632 KB | |
| Com fogo, quadro ~1320 | 5.654 KB | | |

- RAM total: 8.192 KB (Expansion Pak). Teste de `malloc` de 64 KB a 1 MB passa em todos os pontos.
- Alocações grandes no boot somam ~2,7 MB (maior bloco: 356 KB).
- `[fix] desalinhados=144`: acessos desalinhados corrigidos pelo tratador de exceção, estável (não cresce).

### 6.2 Desempenho por área

| Área | Triângulos | FPS |
|---|---|---|
| Área inicial (igreja) | ~480 a 550 | 11 a 15 |
| Fora da área inicial | ~215 a 330 | 17 a 26 |

### 6.3 Detalhe por quadro (log de 21/10/2026, depois da correção do fogo)

| Quadro | FPS | Período | Desenho | conv_tex | Jogo | tris | Situação |
|---|---|---|---|---|---|---|---|
| 120 | 11.7 | 85.6 ms | 26.3 ms | 9.52 ms | 58.0 ms | 538 | igreja, carregando texturas |
| 360 | 13.7 | 72.9 ms | 27.0 ms | 2.47 ms | 43.1 ms | 545 | igreja |
| 600 | 14.3 | 70.0 ms | 25.2 ms | 2.46 ms | 40.7 ms | 501 | igreja |
| 840 | 15.6 | 64.0 ms | 23.6 ms | 2.00 ms | 38.6 ms | 484 | saindo da igreja |
| 1080 | 17.6 | 56.9 ms | 15.9 ms | 3.13 ms | 38.7 ms | 329 | fora |
| 1320 | 14.4 | 69.5 ms | 13.6 ms | 10.42 ms | 50.3 ms | 272 | fora, carro em chamas |

`espera=0.0 ms` em todos os quadros: o limite é CPU (lógica + desenho), não o vsync.

### 6.4 Custo dos serviços do motor (ms por quadro, quadro ~1320)

| jogador | peds | carros | objetos | partículas | HUD | guindastes | scripts | resto |
|---|---|---|---|---|---|---|---|---|
| 0.5 | 5.3 | 7.6 | 3.1 | 0.9 | 0.7 | 0.1 | 0.0 | 0.0 |

Peds e carros são os serviços mais caros (juntos ~13 ms).

### 6.5 Áudio

- `poll1` de 0,8 a 4,4 ms, `poll2` de 0,4 a 1,5 ms.
- Leituras de 9 a 64 por quadro (1 a 10 KB/quadro), cache de efeitos até ~242 KB de 256 KB, sem saídas do cache.

### 6.6 Binário

| Seção | Bytes |
|---|---|
| text | 1.737.176 |
| data | 208.377 |
| bss | 318.180 |
| ROM `.z64` | 31.784.960 |

## 7. Correções feitas (não refazer)

1. **Crash da polícia**: retorno faltando em `PedGroup.cpp` (`MergeWithOtherGroup`) -> `return 0` sob N64. Backup `.bak_ret`.
2. **Pool de explosões**: retorno faltando em `Explosion_30.cpp` (`FreeLowestPriority`) -> `return lowest_idx`. Backup `.bak_ret`.
3. **Assert do mixer em 50292 Hz**: `MAX_FREQ 66150` + clamp em `audio_n64.c`.
4. **Crash no pool de missões**: o preload estourava buffers de 16 bytes; preload desligado e caminho das missões corrigido em `ScriptManager_C1EA8.cpp`.
5. **Explosão "arco-íris"**: o RDP estourava a soma aditiva. Aditivo virou transparência de 75% e os texels quase pretos de texturas aditivas viraram transparentes (`render_n64.c`, `ADD_CUT 48`; backups `.bak_aditivo`). Aceito pelo Georges.
6. **Fogo do carro invisível (21/10/2026)**:
   - Causa: a libdragon roda os inicializadores globais (`.ctors`) de trás para frente (confirmado na `__wrap___do_global_ctors` do ELF). `Particle_4C.o` inicializa antes de `Explosion_30.o`, e por isso `dword_6FD554` e `dword_6FD28C` ficavam 0. Com o limite de altura em 0, toda partícula de chama morria na primeira atualização (`UpdateDirectedProjectile_state_3_12_5384C0`).
   - Correção: valores fixos sob `#ifdef N64` em `Particle_4C.cpp` (`Fix16(0x100, 0)` e `Fix16(8) - Fix16(0x100, 0)`), iguais aos do PC. Backup `.bak_ctor`.
   - Medição antes: `focos=101 criadas=63 sprite_draw=0`. Depois: `focos=212 criadas=142 sprite_draw=2119 enviadas=2119 rejeitadas=0 sem_tex=0`. Fogo visível.
   - Detalhes em `RELATORIO_FOGO_CARRO.md`.

### Risco conhecido: ordem de inicialização

Todos os `DEFINE_GLOBAL_INIT` que dependem de valor de outro arquivo foram varridos:

| Arquivo | Depende de | Situação |
|---|---|---|
| Char_Pool.cpp (`gDummyH_678584`) | Ped.cpp | correto pela ordem atual |
| Police_38.cpp (`kFpEight_6FED48`) | Police_7B8.cpp | correto pela ordem atual |
| Particle_4C.cpp (2 valores) | Explosion_30.cpp | corrigido |

Se a ordem dos arquivos em `port/game_srcs.txt` mudar, os dois primeiros podem quebrar do mesmo jeito.

## 8. Hipóteses descartadas (não repetir)

- Modo "reuso" de estado do RDP: sem ganho.
- Triple buffering: sem ganho (a espera era o áudio).
- Câmera a 10%: o carro sai da tela.
- Sprites totalmente transparentes como causa de lentidão: 0 contados.
- Fogo invisível por alfa zero: o jogo manda alfa 80 a 240.
- Partículas desligadas: `skip_particles` não está setado, o serviço roda todo quadro, pool de 500 igual ao PC.
- Fogo: falha de pool, conversão de textura, gbh e RDP (a medição mostrou que o problema era antes do desenho).

## 9. Medições temporárias ainda no código

| Medição | Arquivos | Backup para remover |
|---|---|---|
| `[fogo]` | `sprite.cpp`, `Explosion_30.cpp`, `Particle_8.cpp`, `gbh_n64.cpp`, `render_n64.c` | `.bak_fogo` (o de `Explosion_30.cpp` já contém o `return lowest_idx`) |
| `[servicos ms]` | `Game_0x40.cpp` | `.bak_med` |
| `[perf]`, `[perf2]`, `[audio]`, `[alloc]`, `[mem]`, `[w]` | `render_n64.c`, `audio_n64.c`, `main_n64.c` e outros | limpar antes do Beta |

## 10. Próximos passos

1. Remover a medição `[fogo]` mantendo a correção do `Particle_4C.cpp` (aguardando autorização).
2. Custo de `conv_tex` com fogo (~3 para ~10 ms): cache dos quadros da animação da chama.
3. O carro não mostra amassados de dano antes de explodir.
4. Peso da área inicial perto da igreja (~300 sprites a mais visíveis, 11 a 15 fps).
5. Beta: menu + save (SRAM), distritos wil/ste com áudio (auditoria de espaço), splash N64 (arte do Georges), controle estilo PS1, limpeza de prints e medições, tag + backup no GitHub.
6. Decisão pendente do Georges: visual sem luzes dinâmicas.
7. Depois: Driver 2 (REDRIVER2), medir dados.

## 11. Regras de trabalho

- Português do Brasil, direto.
- Nunca editar arquivos nem preparar build de diagnóstico sem propor antes e receber autorização explícita.
- Um problema por vez. Pesquisar e comparar código antes de cada build.
- Nunca repetir hipóteses descartadas.
- Backup `.bak_<nome>` de cada arquivo antes de mudar.
- Não desligar o filtro (botão L).
- Sempre terminar pedidos de teste com os dois comandos da seção 3.
