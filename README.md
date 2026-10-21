# GTA 2 - Nintendo 64 Tech Demo

Tech demo funcional de **Grand Theft Auto 2** para o **Nintendo 64** com suporte a **Expansion Pak (8 MB)** em resolução **320×240**, desenvolvida com **Libdragon** e **Tiny3D**.

O projeto utiliza conceitos, dados de blocos e tipos de ponto fixo derivados da engenharia reversa do código-fonte de PC ([`gta2_re`](https://github.com/CriminalRETeam/gta2_re) e [`gta2-browser`](https://github.com/M1sterMo/gta2-browser)), adaptados para o pipeline 3D acelerado por hardware no RSP (Reality Signal Processor) do N64.

---

## 📦 Identificação do Executável da ROM

| Campo | Valor |
|---|---|
| **Caminho Absoluto** | `C:\Desenvolvimento\Nintendo 64\GTA264\GTA2 N64 Tech Demo\gta2_n64_demo.z64`<br>*(espelhado em `C:\Desenvolvimento\Nintendo 64\GTA2 N64 Tech Demo\gta2_n64_demo.z64`)* |
| **Tamanho** | **737.280 bytes** (720 KB) |
| **Data / Hora de Compilação** | `20/10/2026 14:24:31` (atualizada com correções e logs de colisão) |
| **Hash SHA-256** | `86A34BE468BFFEA0131048612126D16BB27D3897C9545C7005A58CAB026B2AC7` |
| **Caminho no Emulador** | `..\Veil of the Damned 64\deps\ares\ares-v148\ares.exe` |

---

## 🎯 Distinção de Escopo: Implementado, Testado e Provisório

### 1. Funcionalidades Implementadas
* **Cenário 3D**: Recorte urbano de Downtown (20×20 blocos) com asfalto, calçadas, faixas de pedestre, sinalização e edifícios com parapeitos.
* **Veículo 3D**: Carro esportivo estilo GTA 2 estacionado na avenida principal.
* **Personagem**: Sprite 2D de Claude Speed com rotação e projeção de sombra elíptica.
* **Física e Colisão**:
  - Pedestre: Caminhada analógica/D-Pad, corrida com botão A, detecção de colisão com resposta deslizante (*sliding collision*) contra paredes.
  - Veículo: Aceleração progressiva com A, frenagem e ré com B, arrasto natural, esterçamento proporcional à velocidade e colisão com ricochete elástico contra prédios.
* **Câmera**: Modo de acompanhamento GTA 2 (ângulo superior característico) e modo livre de inspeção (pan analógico, zoom e inclinação).
* **Embarque / Desembarque**: Detecção de proximidade com prompt contextual no HUD, embarque e desembarque ao lado da porta do motorista com tecla Z / D-Pad Up.
* **Telemetria e HUD**: FPS real in-game, tempo bruto de quadro em milissegundos, total de RAM, código/dados estáticos, heap alocado e heap livre medidos pelo sistema.

### 2. Funcionalidades Efetivamente Testadas no Ares
* **Inicialização e Carga**: Boot completo sem falhas de assertion (`t3d_light_set_directional` corrigido para ser chamado após `t3d_viewport_attach`).
* **Carregamento DragonFS**: Leitura bem-sucedida do grid de colisão (`downtown_col.bin`), modelos 3D (`downtown_slice.t3dm`, `car.t3dm`) e sprites (`player.sprite`, `shadow.sprite`).
* **Jogabilidade Pedestre**: Caminhada, corrida e colisão deslizante testadas e validadas.
* **Condução Veicular**: Embarque, aceleração, frenagem, marcha a ré, curvas dinâmicas, ricochete contra edifícios e desembarque testados e validados.
* **Câmera Livre**: Alternância com botão R e inspeção livre de blocos testadas.
* **Estabilidade e Memória**: Execução contínua por mais de 30 segundos com registro em console ISViewer, comprovando estabilidade e ausência de vazamento de memória (heap estável em 1.229 KB).

### 3. Cenário e Assets Provisórios
* O cenário (20×20 blocos) e o modelo do carro foram gerados proceduralmente pela ferramenta [build_assets.py](file:///c:/Desenvolvimento/Nintendo%2064/GTA2%20N64%20Tech%20Demo/tools/build_assets.py). São **assets provisórios** com visual e proporções representativas para viabilizar a tech demo sem depender de arquivos comerciais protegidos por direitos autorais.

### 4. Componentes Comprovadamente Derivados de GTA 2
* **Aritmética de Ponto Fixo `Fix16`** (18.14, onde `1.0 = 16384`) e **Sistema Angular `Ang16`** (1440 unidades = 360°, 4 unidades/grau), preservando os tipos numéricos originais do motor DMA Design.
* *Nota Técnica*: O uso isolado desses formatos numéricos garante consistência matemática, mas **não comprova por si só a fidelidade ou o aproveitamento integral da física original** do jogo, que engloba sistemas complexos de tração, arrasto, atrito de pneus, curvas de torque, deformação e máquinas de estado de pedestres.
* Estrutura de grid de blocos (alturas e tipos) e perspectiva de câmera superior inclinada.

---

## 📊 Medições Reais de Desempenho e Memória

Todas as métricas foram extraídas de medições diretas no hardware virtual (`ares-v148` com Vulkan / paraLLEl-RDP):

| Métrica | Valor Medido | Descrição |
|---|---|---|
| **FPS Real In-Game** | **35.1 – 36.4 FPS** | Medido pelo loop de renderização do aplicativo |
| **Tempo de Quadro Real Bruto** | **26.59 – 28.99 ms** | Tempo total de processamento por frame (sem clamp) |
| **Apresentação do Emulador** | **59 – 60 VPS** | Taxa de atualização de vídeo NTSC do console |
| **RAM Total Detectada** | **8.192 KB** (8 MB) | Expansion Pak detectado via `get_memory_size()` |
| **Código Executável (.text)** | **244 KB** (`249.784 bytes`) | Linker map: `__text_start` até `__text_end` |
| **Dados Estáticos (.data + .rodata)** | **77 KB** (`79.276 bytes`) | Linker map: `__data_start` até `__data_end` |
| **BSS (Variáveis Globais)** | **10 KB** (`9.896 bytes`) | Linker map: `__bss_start` até `__bss_end` |
| **Total Binário Estático** | **331 KB** (`338.948 bytes`) | Ocupação total do binário na memória antes do heap |
| **Heap Total Alocado** | **1.229 KB** | Medido via `sys_get_heap_stats(&hstats)` -> `hstats.used` |
| **Heap Total Disponível** | **7.796 KB** | Medido via `sys_get_heap_stats(&hstats)` -> `hstats.total` |
| **Margem Real de Heap Livre** | **6.567 KB** | Medido via `sys_get_heap_stats(&hstats)` -> `hstats.free` |
| **Tamanho da ROM (.z64)** | **720 KB** (`737.280 bytes`) | Binário final com assets DragonFS embutidos |

---

## 🎮 Controles no Nintendo 64

| Ação | Controle / Tecla no Ares |
|---|---|
| **Mover / Dirigir** | Alavanca Analógica ou D-Pad / Setas do Teclado |
| **Correr (A pé)** | Botão **A** / Barra de Espaço |
| **Acelerar (No carro)** | Botão **A** / Barra de Espaço |
| **Frear / Marcha a Ré (No carro)** | Botão **B** / Tecla **Z** |
| **Entrar / Sair do Carro** | Gatilho **Z** ou **D-Pad Up** / Tecla **X** ou Seta Cima |
| **Alternar Câmera (Seguir / Livre)** | Botão **R** / Tecla **R** |
| **Zoom e Altura da Câmera** | Botões **C-Up** / **C-Down** / Teclas **I** / **K** |
| **Inclinação da Câmera** | Botões **C-Left** / **C-Right** / Teclas **J** / **L** |

---

## 🛠️ Instruções Reproduzíveis de Compilação

O projeto é compilado usando a toolchain GCC 16.2.0 do Libdragon instalada no WSL. O diretório no WSL é um symlink para a pasta do workspace:

```bash
# No terminal WSL / Bash:
cd /home/georges/gta2_n64
make clean
make
```

Ou execute o script automatizado:
```bash
wsl bash -lc "cd /home/georges/gta2_n64 && bash ./build.sh"
```

A compilação produz:
- `build/gta2_n64_demo.elf`: Executável MIPS ELF com símbolos e mapa.
- `build/gta2_n64_demo.dfs`: Sistema de arquivos DragonFS contendo malhas, colisão e sprites.
- `gta2_n64_demo.z64`: Imagem ROM final pronta para emuladores e flashcarts.

---

## 🕹️ Instruções Reproduzíveis de Execução

Execute no emulador de referência **ares-v148**:

```powershell
& "..\Veil of the Damned 64\deps\ares\ares-v148\ares.exe" "C:\Desenvolvimento\Nintendo 64\GTA264\GTA2 N64 Tech Demo\gta2_n64_demo.z64"
```

Para visualizar a saída de telemetria em tempo real no console:
```powershell
& "..\Veil of the Damned 64\deps\ares\ares-v148\ares.exe" --terminal "C:\Desenvolvimento\Nintendo 64\GTA264\GTA2 N64 Tech Demo\gta2_n64_demo.z64"
```
