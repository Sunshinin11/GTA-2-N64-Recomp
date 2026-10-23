# GTA 2 N64 (recomp) - documento de passagem

Uso estritamente pessoal. Repositorio GitHub privado, so para backup.
Projeto: `C:\Desenvolvimento\Nintendo 64\GTA264\GTA2 N64 Tech Demo\`
Dados do jogo: `C:\Desenvolvimento\Nintendo 64\GTA264\gta2_data\data`
Base: decomp `gta2_re` (PC) + libdragon preview, testado no Ares (unico emulador confiavel para libdragon).

## Regras de trabalho (do Georges)
- Responder em portugues do Brasil, direto, sem enrolar.
- Nunca editar arquivos do projeto nem preparar build de diagnostico sem propor antes e receber autorizacao explicita.
- Um problema por vez. Pesquisar e comparar codigo antes de cada build (nada de "tiro no escuro").
- Nunca repetir hipoteses ja descartadas. Nao discutir com o Georges.
- Fazer backup `.bak_<nome>` de cada arquivo antes de mudar.
- Nao desligar o filtro (botao L).

## Como compilar e testar (PowerShell, na pasta do projeto)
```powershell
cd "C:\Desenvolvimento\Nintendo 64\GTA264\GTA2 N64 Tech Demo"
powershell -ExecutionPolicy Bypass -File build_recomp.ps1
powershell -ExecutionPolicy Bypass -File tools\read_ares_recomp.ps1
```
- O primeiro compila no WSL (`build.sh -f Makefile.recomp -j4 all`) e grava `build_recomp_log.txt`.
- O segundo abre o Ares e grava o console em `ares_console_recomp.txt` (e o log que deve ser lido).
- ROM gerada: `gta2_recomp.z64`.

## Meta: V0.0.1 Beta (jogavel)
Menu com save, missoes, distritos com audio, splash com logo do N64, layout de controle do PS1.

## Estado atual
- Missoes funcionam com o script real (`bil.scr` convertido por `tools/scr_le2be.py` + `tools/scr_tabela.json`, validado em 70 arquivos / 30.719 comandos, 0 erros).
- Tela 640x240 (console_init). Populacao 40% (`n64_population.h`, `N64_POP_PCT`). Camera com 15% do afastamento por velocidade (`Camera.cpp`, `N64_CAM_SPEED_PCT`).
- Luzes dinamicas desligadas (`gbh_n64.cpp`, `N64_LUZES 0`): ganho de 8 a 15 ms por quadro.
- Audio 22050 Hz, radios ULC 12 kHz, cache de efeitos em RAM (256 KB).
- Desempenho: area inicial (igreja) ~530 triangulos, 11-15 fps. Fora dela ~220 triangulos, 22-26 fps.
- Heap ~5,5-5,7 MB.

## Correcoes ja feitas (nao refazer)
- Crash da policia: retorno faltando em `PedGroup.cpp` (MergeWithOtherGroup) -> `return 0` sob N64.
- Pool de explosoes: retorno faltando em `Explosion_30.cpp` (FreeLowestPriority) -> `return lowest_idx`.
- Assert do mixer em 50292 Hz -> `MAX_FREQ 66150` + clamp em `audio_n64.c`.
- Crash no pool de missoes (preload estourava buffers de 16 bytes) -> preload desligado e caminho das missoes corrigido em `ScriptManager_C1EA8.cpp`.
- Explosao "arco-iris": o RDP estoura a soma aditiva. Agora aditivo vira transparencia de 75% e os texels quase pretos de texturas aditivas viram transparentes (`render_n64.c`, `ADD_CUT 48`; backups `.bak_aditivo`). Resultado aceito pelo Georges.

## Hipoteses ja descartadas (nao repetir)
- Modo "reuso" de estado do RDP (sem ganho). Triple buffering (sem ganho; a espera era o audio).
- Camera a 10% (o carro sai da tela).
- Sprites totalmente transparentes como causa de lentidao (0 contados).
- Fogo invisivel por alfa zero: o jogo manda alfa 80 a 240 (SetBlendAndAlpha). Nao era isso.
- Particulas desligadas: `skip_particles` nao esta setado, o servico roda todo quadro, pool de 500 igual ao PC.

## Em andamento: fogo do carro nao aparece (so o som)
- No PC/PS1 o fogo sao particulas criadas pelo foco de fogo (Explosion_30, tipos 3/4/5/12/13/14 -> EmitFireTrail_*), sprites code_obj2 ids def112 + 0, 96-102, 147-151.
- Build de MEDICAO autorizada e aplicada (backups `.bak_fogo` em sprite.cpp, Explosion_30.cpp, Particle_8.cpp, gbh_n64.cpp, render_n64.c). Linha `[fogo]` no log a cada 120 quadros:
  focos, criadas, falhas_pool, sprite_draw, gbh_draw, rejeitadas, tex_vazia, enviadas, alfa, blend, zoom, sem_tex, pos/z.
- Teste: atirar no carro ate pegar fogo, esperar ~5 s antes de explodir, mandar o log.
- Leitura: se `criadas`=0 o problema e na criacao; se `sprite_draw`=0 o sprite nao chega ao desenho; se `rejeitadas`/`tex_vazia` subirem, e no gbh/conversao; se `enviadas`>0, e no RDP/ordem de desenho.
- Depois de corrigir: remover a medicao (voltar dos `.bak_fogo` + aplicar a correcao).
- Observado tambem: o carro nao mostra amassados de dano antes de explodir (avaliar depois, um problema por vez).
- Pasta `tmp_cmp/` na raiz: copia temporaria dos fontes, pode ser apagada.

## Proximos passos
1. Fogo do carro (acima).
2. Peso da area inicial perto da igreja (~300 sprites a mais visiveis).
3. Beta: menu + save (SRAM), distritos wil/ste com audio (auditoria de espaco), splash N64 (arte do Georges), controle estilo PS1, limpar prints/medicoes de debug, tag + backup no GitHub.
4. Pendente do Georges: se o visual sem luzes dinamicas e aceitavel.
5. Depois: Driver 2 (REDRIVER2), medir dados.

## Arquivos principais
- `port/n64/render_n64.c` (renderer, cache de texturas, medicoes [perf2]/[audio]/[fogo])
- `port/n64/gbh_n64.cpp` (camada gbh: Draw, texturas, luzes)
- `port/n64/audio_n64.c`, `port/n64/main_n64.c`, `port/n64/game_entry.cpp` (registro/flags de debug)
- `port/gta2_re/Source/*` (motor, mudancas sob `#ifdef N64`)
- `Makefile.recomp`, `tools/stage_data.sh`, `tools/scr_le2be.py`
