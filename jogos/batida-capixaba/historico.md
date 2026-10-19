# O Batida Capixaba no Switch: histórico

A entrada DEC-102 do devlog do jogo ("O jogo no Switch homebrew: o motor, as cores e os primeiros ganhos
de desempenho", 2026-10-18 e 19), trazida para cá quando o port saiu do projeto do jogo e virou o
godot-switch-nro. O texto é o da época: os comandos citados (`bash switch/entregar.sh`, `VULKAN=1 ...`,
`switch/construir.sh`) hoje são `python gsn.py gerar <pasta do jogo>` (ver `LEIA-ME.md` desta pasta),
e as decisões genéricas estão em `docs/decisoes.md`. No devlog do jogo ficou um resumo do que mudou
**no jogo** por causa do Switch, com um ponteiro para cá.

---

- **Pedido:** o jogo no Switch com CFW, só como `.nro` no Atmosphère. Não existe Godot 4 para homebrew
  (o port de Stary2001/cpasjuste parou no 3.5), então portamos o 4.7.2: a plataforma ficava em `switch/`
  na raiz do workspace do jogo e hoje é este repositório (godot-switch-nro), compilada com devkitPro
  (libnx) numa imagem Docker no WSL.
- **Premissa do criador para toda mudança feita por causa do Switch:** o que piora gráfico ou algo do
  gênero só vale quando o jogo detecta o Switch (`OS.has_feature("switch")`; nos shaders,
  `CURRENT_RENDERER == RENDERER_COMPATIBILITY`); fora isso, só melhorias de desempenho gerais, sem mudar
  visual nem jogabilidade em nenhuma versão. Cada mudança é conferida no PC comparando capturas.
- **Etapa 1 (motor):** provada no console com uma cena de teste: 3D (OpenGL ES 3.2 do Mesa 20.1), as
  faixas da Madalena sincronizadas, Joy-Con e toque.
- **Etapa 2 (o jogo no renderizador Compatibility, o único do Switch):**
  - `renderer/rendering_method.switch="gl_compatibility"` no projeto e o preset de exportação "Switch"
    (só o `.pck`, ~428 MB). PC e celular seguem no Mobile;
  - **cores:** no Compatibility o Godot trata o `ALBEDO` como sRGB e as cores/texturas de material chegam
    em sRGB; os shaders do palco fazem a conta em linear, e o toon saía escuro e saturado. O
    `palco/shaders/cor.gdshaderinc` (`cor_entrada`/`cor_saida`) converte só no Compatibility; no Mobile
    as funções não fazem nada (captura do PC idêntica). Dez cenas conferidas nos dois renderizadores;
  - no Switch: gráfico "leve" por padrão e as opções de janela/resolução de PC escondidas.
- **Desempenho (o processador do Switch é o gargalo; ~10× mais lento que o PC):**
  - a lista de músicas recalculava a dificuldade de todas as 42 a cada quadro: 15,1 ms → 0,57 ms no PC
    (`TelaDoMenu._nota_da_musica`); o menu do ato montava todas as lições a cada quadro (a de Canto lê a
    gravação do disco): 3,0 ms → 0,18 ms (`Licoes.todas` guarda o resultado);
  - o HUD reaproveita um StyleBoxFlat em vez de criar um por caixa a cada quadro e lembra as medidas
    de texto (ganho pequeno; o desenho continua idêntico);
  - roteiro novo `custo-menus` (custo por quadro de cada menu, com tudo liberado).
- **Mais desempenho (todas as versões, desenho idêntico):** as notas da pista e as faíscas viraram
  MultiMesh (um desenho por malha/material em vez de um nó por nota e por faísca): 222 → 161 chamadas
  de desenho na rua e 270 → 198 na floresta (gráfico leve). No PC quase não muda o tempo; no Switch
  cada chamada custa caro no driver. Roteiros novos `custo-cenas` (o peso de HUD, roda, palco e 3D
  desligando um de cada vez) e `contar-objetos`.
- **No .nro:** aceleração do processador (FastLoad) **só na abertura**: desliga de vez no começo do laço
  principal (a 0.2.4 esperava 180 quadros abaixo de 100 ms, que com a placa no mínimo nunca vinham, e o
  jogo inteiro ficou a 5 FPS). Ela baixa a placa de vídeo ao mínimo, e religada no jogo (até a
  0.2.2, mesmo com a trava pela espera da placa, que ficava em 0,2 ms) derrubava o desafio do machado
  a 4 FPS por ~40 s; o singleton `Switch` (fontes do sistema, turbo, modelo, base, carregador); o idioma do
  console (`get_locale`); o HOME pausa a partida. O aviso de foco do sistema não chega (0.2.5: modo de foco aceito, nenhum
  aviso no HOME), e o HOME congela o processo inteiro, todas as threads (0.2.7: o pulso do log parou
  157 s e um quadro durou 155 s); a 0.2.6 esperava a parada na troca de quadro, que não é onde ela
  acontece. Desde a 0.2.8, uma thread sentinela (prioridade 0x2B, acima da principal) dorme 100 ms de
  cada vez; se acordar mais de 1,5 s depois, o processo esteve congelado e, na volta, a partida pausa e o fechar pelo sistema
  avisa a janela;
  a vibração do `Input` vai aos Joy-Cons.
- **No jogo, só no Switch:** fontes próprias (Noto Sans + recortes da Math e da Symbols 2, OFL; CJK das
  fontes do console) porque não há fontes do sistema (as setas e ✕ □ △ ○ não apareciam); o **modo
  turbo** na Configuração (desligado; 1785 MHz; desde a 0.2.4 em qualquer modelo e modo, também no portátil fora do carregador), nos 9 idiomas.
- **Onde o machado gasta (PC, Compatibility, gráficos leves, 0.2.6):** 3,5 ms por quadro; o `_draw` do
  HUD (script) 0,87 ms, o palco 1,28 ms (a roda 0,5), o 2D no motor ~0,5 ms; texto 0,3 e caixas 0,5.
  No Switch, com turbo, o mesmo quadro leva 54 ms (18 FPS), ~15 vezes mais. O motor já sai com -O3.
  `custo-desafios` ganhou `--so_ato=N`, as variantes "sem texto" e "sem caixas" e o tempo do `_draw`.
- **0.2.7, otimizações sem mudança visual (todas as versões):**
  - **HUD em folhas** (`hud/pincel.gd`): os pincéis desenham pelo RenderingServer num item escolhido
    (`_alvo`), com os mesmos comandos dos `draw_*` do 4.7.2. A partida desenha em folhas filhas, na ordem
    de antes; as guardadas só se redesenham quando a chave muda: rótulos das faixas (0,099 → 0,014 ms),
    placa e corpo do tambor (o salto vira a escala da folha) e o título. Capturas antes/depois iguais.
  - **Faíscas:** o laço passa só pelo trecho aceso da roda de 160 (0,17 → 0,14 ms).
  - **Roda:** a raiz de cada pessoa recebe posição e giro uma vez por quadro, só se mudaram (o Godot
    recalcula o boneco a cada atribuição); o custo do script não mudou (0,18 ms).
  - Machado no PC (A/B, duas rodadas cada): 3,48 → 3,23 ms por quadro (−7%).
  - **LTO** no motor do Switch (`detect.py`, `"lto": "full"`): .nro de 61,4 para 54,5 MB.
  - O .nro mede na abertura quanto custa um malloc/free (a newlib trava um mutex a cada pedido) e uma
    conta de inteiros, para saber se o gerenciador de memória é parte dos ~15× de diferença para o PC.
    Resultado (0.2.7): malloc+free 499 ns (~25× o PC), conta de inteiros 1,87 ns. Pelas contas, a
    memória pesa uns 5% do quadro: não é a causa principal.
- **0.2.7 no console:** o machado foi a 18-19 FPS (54 ms); uma música comum fica em 29-31 ms. No PC, a
  música comum na mata custa 2,0 ms e o desafio 3,2-3,5 ms: o desafio dobra quase tudo (roda, efeitos,
  notas, faíscas), sem um ponto único.
- **0.2.8:** o pulso mostra a média por quadro do script (process) e da física; o desenho em thread
  separada fica atrás do arquivo `render_separado` na pasta do .nro (o `DisplayServerSwitch` passa o
  contexto do EGL entre as threads). No PC, `--render-thread separate` com OpenGL cai na abertura
  (erro no motor), por isso não é o padrão. No console (0.2.8) o menu ficou igual com ele (30,6 ms) e a
  espera da placa segue em 0,2 ms: nem a placa nem o desenho seguram o quadro; serrilhado e polígonos não
  mudariam o FPS.
- **0.2.9:** no Switch, o jogo escreve no log a cada 5 s a média por quadro de cada parte do script
  (`[jogo] ...`: lógica, estado da tela, palco por pedaço, HUD e `_draw`); o resto do quadro é o motor.
  No PC, o machado e uma música comum na mata custam o mesmo (2,15 e 2,07 ms) e o script é ~1,1 ms de
  ~4 ms; no Switch o machado leva quase o dobro da música, e é isso que o log vai explicar.
- **O log da 0.2.9 (console, turbo):** o script custa o mesmo no machado e na música da rua (~12 ms:
  palco 7,4, dos quais a roda ~5; `_draw` do HUD ~4); o quadro vai de 30 para 53 ms. Os ~23 ms a mais
  são trabalho do motor na cena da mata, fora do script e com a placa folgada. A pausa no HOME
  funcionou (`pausa: o jogo ficou congelado (HOME)`).
- **0.2.10:** com o arquivo `diagnostico` na pasta do .nro, a partida esconde por 5 s, uma de cada
  vez, os lenhadores (modelos com esqueleto, carregados mesmo nos gráficos leves), a árvore, a
  floresta, os galhos, as faíscas, a roda, o HUD e o 3D, e o log mostra o quadro em cada estado
  (`palco/diagnostico.gd`).
- **Resultado (console):** normal 52,6 ms; **sem a floresta 31-32 ms**; sem lenhadores, árvore, galhos,
  faíscas, roda ou HUD, nada muda (~52); sem o 3D, 21-24 ms. A floresta custa ~21 ms. Ela tem 170 mil
  triângulos nos gráficos leves, todos no toon das duas faces com `discard` (o recorte das folhas);
  128 mil são folhas e flores com transparência de verdade. A "espera da GPU" do pulso só mede a troca
  de quadro: o driver segura o processador em outros pontos, então a placa pode ser o gargalo aqui.
- **0.2.11:** troncos, pedras, grama, chão e cogumelos (texturas sem transparência, conferido pixel a
  pixel) vão para `toon_dupla.gdshader`, o mesmo toon sem o `discard` (`#ifndef SEM_RECORTE` em
  `toon_corpo.gdshaderinc`), em todas as versões; captura igual. O diagnóstico mede agora o 3D a 75%,
  sem árvores e flores, sem chão/pedras/grama, metade das árvores e sem a floresta.
- **Resultado (console, fora da base):** normal 51 ms; 3D a 75% 41; sem árvores e flores 34; sem
  chão/pedras/grama 49,8; metade das árvores 45; sem floresta 31. As copas (folhas com recorte, perto
  da câmera, várias camadas) custam ~17 ms: é a placa de vídeo pintando pixels. Mesmo sem floresta, o
  piso de todas as cenas é ~30 ms (script ~12 + motor ~18).
- **0.2.12:** no Switch fora da base (a placa roda mais devagar que na TV) e só nos cenários de
  folhagem (`FOLHAGEM`, hoje a mata), o 3D vai a 75% (`principal.gd`, `_escala_no_switch`, conferido a
  cada segundo); na base e nas outras cenas, 100%. Capturas da comparação em `comparacao/` (nesta pasta).
- **Na base (0.2.11, relato do criador):** o machado bate 30 FPS. Confirma que fora da base é a placa
  de vídeo (mais lenta no portátil) que segura a mata; na base sobra o piso de ~30 ms do processador.
- **0.2.12 no console, fora da base:** o machado com o 3D a 75% foi a 40-41 ms (antes 51; ~24 FPS); sem
  árvores e flores 27,5; sem floresta 26. No começo da partida, um quadro de 2,7 s e a primeira janela a
  129 ms por quadro: a mata montando e compilando shaders com a música já tocando (o jogador perdia
  notas).
- **0.2.13, a partida espera o cenário (todas as versões):** `tela_da_partida.gd` começa em `preparando`:
  o relógio fica parado na antecedência (`som.pausar`) até o palco não estar carregando
  (`Palco3D.carregando()`: mata, guarda, modelos da roda) e vierem 10 quadros seguidos abaixo de 100 ms
  (os shaders novos compilam nos primeiros); teto de 20 s. Passando de 0,3 s, o HUD mostra
  "Carregando…" (nos 9 idiomas, `lote13.py`). No PC passa em poucos quadros.
- **0.2.14, a abertura no Switch:** a tela preta dos primeiros ~11 s e o logo do Godot viram a imagem do
  jogo (o tambor do ícone e o nome, `jogos/batida-capixaba/abertura.py`, NotoSans Bold). O .nro desenha a imagem pelo
  OpenGL assim que o vídeo existe (~4 s; antes o Godot ainda compila os shaders dele por ~7 s com a
  tela preta), embutida com `.incbin` (`abertura.rgba`); depois o Godot mostra a mesma imagem como
  abertura (`application/boot_splash/image.switch`, PNG com import "keep"). No PC nada muda. Os ~4 s
  antes do vídeo continuam pretos.
- **0.2.15:** as cópias de cada malha da floresta (um MultiMesh por malha) vão da mais perto da câmera
  para a mais longe. O Compatibility já ordena os objetos opacos em 16 faixas de distância, mas cada
  MultiMesh é um objeto só; dentro dele, a ordem era a do arquivo. Com as folhas da frente primeiro, a
  placa pode descartar as de trás sem pintar. Mesma imagem, todas as versões.
- **0.2.15 no console:** desenhar da frente para trás não rendeu nada (normal ~40 ms): com o recorte
  das folhas a placa pinta mesmo o que fica atrás. Tirar o que ninguém vê passa a valer.
- **Floresta leve (preparada, ainda não ligada):** a conferência `floresta-visivel` renderiza só as
  árvores e plantas com folhas, cada cópia com uma cor de identificação (canal próprio da cópia, sem a
  cor de vértice), nos quatro enquadramentos (keys, touch, duoKeys, duoTouch), e conta os pixels de cada
  uma com as outras na frente e sozinha (a 1/4 da resolução). Escondida: aparece menos de 10% do que
  ocuparia sozinha em todos os enquadramentos: 26 de 96, gravadas em `palco/mata_escondidas.json` (pelo
  caminho do nó no mata.glb). A primeira tentativa, pelo retângulo da caixa na tela, tirava 65 e
  esvaziava a mata. Capturas com e sem as 26 (`comparacao/floresta-*`): iguais à vista.
  A 0.2.16 mede o ganho no estado "floresta leve" do diagnóstico.
- **0.2.16 no console, fora da base:** floresta leve 39,1/40,7/40,6 ms contra normal 40,0/40,9/41,8: menos
  de 1 ms. Não foi ligada (a lista e a conferência ficam). O custo está nas folhas visíveis: metade das
  árvores ~36 ms, 3D a 56% ~35 ms, sem árvores e flores ~28 ms. Captura do 3D a 60% em
  `comparacao/mata-3d-60.png`.
- **0.2.17, folhas mais leves no Switch:** `toon_folha_leve.gdshader` (`#define FOLHA_LEVE` no
  `toon_corpo.gdshaderinc`): a cor do vértice com a cor base, a névoa e a cor dela saem por vértice, e a
  textura vai para linear pela aproximação polinomial do próprio Compatibility (`cor_entrada_rapida`).
  `Materiais.folha_leve` liga no Switch (`--folha_leve` força no PC). Captura no PC, Compatibility:
  diferença máxima de 3/255 na floresta. Ganho no console ainda não medido.
- **Pausa da otimização do Switch (pedido do criador, 2026-10-18).** Estado: fora da base o machado
  está em ~40 ms (24 FPS) com o 3D a 75% na mata; na base, 30 FPS. Opções medidas e não ligadas: 3D a 60%
  (~34 ms), metade das árvores (~36 ms), a combinação 3D a ~70% + um terço a menos de copas (estimada em
  ~33 ms); a floresta leve das árvores escondidas rende menos de 1 ms. O diagnóstico (`diagnostico` na
  pasta do .nro) e a conferência `floresta-visivel` continuam no projeto.
- **Mesa 26.2 no lugar do 20.1 (passo 1 de 3: Mesa novo no OpenGL, depois Vulkan, depois a escolha no
  menu).** Existe Vulkan (NVK) e Mesa novo para homebrew no Horizon: `danfromtico/mesa-switch` (Mesa
  26.2.4, MIT, camada própria `src/nouveau/horizon`, sem `libdrm_nouveau`), usado pelo NetherSX2_nx. Só
  EGL/OpenGL/GLES compilado (`build-opengl.sh` no `Docker.mesa` dele, sem xmlconfig/expat), em
  `/root/switch/terceiros/mesa-switch` no WSL. O Godot do Switch ganhou a opção `mesa=<prefixo>`
  (`detect.py`; liga o grupo EGL, GLESv2, glapi, mesa_util*, blake3, xmlconfig, expat, zstd, z duas
  vezes) e `MESA=<prefixo> bash switch/entregar.sh` a usa; sem `MESA`, continua o 20.1. A 0.2.18 é a
  primeira com o Mesa 26 (.nro de 69,7 MB). O EGL dele anuncia as configurações como não conformes (o
  nosso pedido não exige). Opção a experimentar depois: `MESA_SWITCH_GLTHREAD=1` (o trabalho do
  driver numa thread).
- **0.2.18 no console (Mesa 26.2):** a abertura ficou bem mais rápida (motor pronto antes de 5 s, antes
  ~12; menu em ~25 s, antes ~35), mas os quadros ficaram ~13 ms mais caros no driver: menu 45 ms
  (antes 30), música comum 42-44 (antes ~30), machado fora da base 53 (antes 40); o script igual.
  O glthread vem ligado por padrão no Mesa 26 (`switch_glthread_requested`), e o Godot espera respostas
  do driver todo quadro. A 0.2.19 desliga o glthread por padrão e lê `mesa_env.txt` ao lado do .nro
  (linhas NOME=valor viram variáveis de ambiente antes do vídeo), para testar opções sem recompilar.
- **0.2.19 no console:** sem o glthread, o Mesa 26 ficou igual (menu ~45 ms, música 41-44, machado 52-53):
  o custo a mais é do driver mesmo. A 0.2.20 voltou ao Mesa 20.1 (o .nro de 58,2 MB de antes).
- **Etapas 2 e 3 (Vulkan e a escolha no jogo), em andamento:**
  - Dois .nro: `batida.nro` (OpenGL, Mesa 20.1) e `batida.vulkan` (Vulkan pelo NVK do Mesa 26, renderizador
    Mobile; o menu do homebrew não lista a extensão .vulkan). Os dois Mesa não cabem no mesmo binário
    (bibliotecas estáticas com os mesmos nomes internos).
  - O jogo grava a escolha em `user://renderizador.txt`; o batida.nro lê na abertura e passa a vez ao
    batida.vulkan pelo carregador do homebrew (`envSetNextLoad`). O batida.vulkan marca
    `dados/vulkan_abrindo` e a apaga depois de 300 quadros; se a marca sobra (falhou ou travou), a próxima
    abertura volta ao OpenGL e grava `dados/vulkan_falhou`, que o jogo avisa ("O Vulkan não abriu").
  - Motor: `rendering_context_driver_vulkan_switch` (superfície `VK_NN_vi_surface` sobre o NWindow), o
    display server com os dois caminhos (contexto e RenderingDevice como no Linux), `detect.py` com
    `vulkan=yes use_volk=no opengl3=no module_glslang_enabled=yes nvk=<pasta>`; `construir.sh` com
    `VARIANTE=vulkan` (num Godot à parte, `/root/switch/godot-vk`) e `VULKAN=1 bash switch/entregar.sh`.
  - Singleton `Switch`: `renderizador()`, `tem_vulkan()`, `reabrir()`. Configuração: "Vídeo do Switch"
    (OpenGL / Vulkan (experimental)), só no Switch com o batida.vulkan na pasta; trocar fecha e reabre o
    jogo. Textos nos 9 idiomas (`lote14.py`); o Modo DEV mostra o vídeo em uso.
  - NVK: o `build-switch.sh` do mesa-switch compila a parte em Rust (o compilador NAK) para x86-64 no
    Docker em Linux (o wrapper só passa o alvo com `MESA_SWITCH_RUST_TARGET`): o `libvulkan.a` saiu com
    365 objetos de PC e o ligador recusou. Cópia local `build-switch-batida.sh` com
    `MESA_SWITCH_RUST_TARGET=aarch64-unknown-linux-gnu` e `rustup target add`. O `posix_memalign` da
    camada de compatibilidade ficou fraco (o NVK traz o dele).
  - Para o Rust sair para o ARM sem quebrar as macros procedurais (que rodam no PC): o alvo
    `--target=aarch64-unknown-linux-gnu` só no cross file (`patches/mesa-switch/switch_cross_file_gsn.txt`), o alvo
    instalado no rustup, e um wrapper próprio do rustc (`patches/mesa-switch/rustc-switch-wrapper-gsn.sh`) que tira
    o `--target` e o `-C linker=` do teste inicial do Meson. Tudo no `build-switch-batida.sh` (cópia local
    do `build-switch.sh`). O libvulkan.a saiu com 922 objetos ARM.
  - Ligação do batida.vulkan: o libvulkan.a (NVK, NIR, NAK, runtime de Vulkan) + `nak_rs` (a biblioteca do
    Rust) + os utilitários do Mesa (`detect.py`). A 0.2.21 é a primeira com os dois: batida.nro 58,2 MB e
    batida.vulkan 69,9 MB, na mesma versão (`VULKAN=1 bash switch/entregar.sh`).
  - **A 0.2.21 no console:** ao escolher Vulkan, a mensagem de erro padrão do sistema, e o jogo não
    abria mais (cada abertura passava a vez ao Vulkan, que caía antes de gravar a marca). O relatório do
    Atmosphère mostrou que quem caiu foi o carregador de homebrew (hbl), com 2001-0106 (InvalidCurrentMemory):
    a thread do pulso do log (`_pulso`, solta com `pthread_detach`, dormindo 5 s) continuava viva quando
    o batida.nro saía. A libnx não encerra threads que sobram, e a pilha dela (1 MiB do heap) ficava presa;
    o batida.vulkan, maior que o batida.nro, precisa daquele pedaço do heap e o carregador não conseguiu
    mapeá-lo. Conserto (0.2.23): o pulso para e a thread é esperada antes de o `main` sair
    (`_parar_pulso`). Também: a marca `vulkan_abrindo` é gravada antes de passar a vez (se o Vulkan cair
    de novo, a próxima abertura volta ao OpenGL), marcas de passagem em `vulkan_etapas.txt` e a medida do
    cache de shaders (formatos binários do driver, arquivos de `shader_cache` e `mesa_cache` no log).
  - **Auditoria do que sobra na saída (0.2.23):** no código, cada abertura tem o fechamento (áudio:
    thread, audren e pool; vídeo: EGL ou Vulkan; sentinela; rede; romfs; fontes do sistema; relógios).
    Faltavam: parar a vibração dos controles e devolver o modo de foco do applet ao padrão (feitos). Como o
    driver de vídeo (Mesa/NVK) não dá para auditar pelo código, a saída agora se confere sozinha: o
    `pthread_create` embrulhado anota cada thread viva, e no fim (`userAppExit`, depois dos destrutores)
    o heap do .nro é percorrido com `svcQueryMemory` atrás de trechos presos (tipo diferente de heap ou
    com atributo). Resultado em `batida.saida.txt` / `batida.vulkan.saida.txt` e no log ("saída
    (...)"). O pulso do log mostrava a memória do processo, sempre 3185/3189 MiB (o carregador reserva o
    heap inteiro), e não servia para ver vazamento durante o jogo: agora mostra o malloc em uso e o pico.
  - **Com o conserto, o batida.vulkan abre** (o carregador passou): o NVK achou a placa ("Vulkan 1.4.354 -
    Forward Mobile - NVIDIA Tegra X1 (GM20B) (NVK GM20B)", "Tela: 1280x720, Vulkan (NVK)"), e a tela ficou
    preta, sem erro. O pulso calou depois de 5 s, mas o cache de shaders passou de 32 para 40 arquivos:
    estava compilando shaders. O pulso calava porque toda pthread da libnx nasce com prioridade 59 (abaixo
    da principal, 44; `__syscall_thread_create` desmontado) e as threads de trabalho do Godot ocupavam os
    três núcleos. 0.2.24: o pulso é uma thread da libnx com prioridade 0x2B, como a sentinela. A trava do
    Vulkan funcionou: a abertura seguinte voltou sozinha ao OpenGL.
  - **Cache de shaders no OpenGL: não há.** O Mesa 20.1 do devkitPro informa 0 formatos binários
    (`GL_NUM_PROGRAM_BINARY_FORMATS`), então o Godot não consegue guardar shader pronto, e o cache do
    próprio Mesa (`mesa_cache`) não grava nada. No OpenGL os shaders são recompilados a cada abertura; o
    cache só funciona no Vulkan (SPIR-V no `shader_cache`).
  - **A troca sumia sem avisar:** ao escolher Vulkan, o jogo fechava e reabria na hora, e a primeira abertura
    (compilando) era uma tela preta; o criador fechou achando que travou, e a trava voltou ao OpenGL com
    "O Vulkan não abriu". Agora a troca mostra "Abrindo com Vulkan" (ou OpenGL) por 5 s antes de fechar,
    avisando da tela preta na primeira vez; a dica da Configuração diz o mesmo; e a volta diz "O Vulkan não
    terminou de abrir" (fechado ou travado: a marca não distingue). Textos nos 9 idiomas (`lote15.py`).
  - **0.2.24: o Vulkan roda no console.** Com o cache da tentativa anterior (77 arquivos), o `Main::setup`
    terminou antes de 5 s (no OpenGL, ~15 s, e ele recompila tudo a cada abertura). Fora da base:

    | Cena | OpenGL (0.2.17–0.2.19) | Vulkan (0.2.24) |
    | --- | --- | --- |
    | menu | ~26–35 FPS | 25–38 FPS |
    | partida na rua | 28–31 FPS (~33 ms) | 38–40 FPS (25 ms) |
    | partida na mata (3D a 75%) | ~24 FPS (~40 ms) | 30–31 FPS (32 ms) |

    Ao entrar na mata pela primeira vez, dois quadros de 1,5–1,8 s (shaders novos, que ficam no cache). A
    "espera da GPU" do pulso só é medida no OpenGL (sai 0,0 no Vulkan). Memória do malloc: 618 MiB no
    menu, 634 na rua, 677 na mata, estável dentro de cada cena. Linhas do log cortadas com lixo do cartão
    no meio: o stdout e o stderr (dois FILE com travas próprias, o mesmo arquivo) e o pulso em prioridade
    alta escreviam ao mesmo tempo no mesmo deslocamento. 0.2.25: `_write_r` embrulhado
    (`-Wl,--wrap=_write_r`), uma trava única para o stdout, o stderr e o fsync do pulso.
  - **0.2.26: o Vulkan é o padrão no Switch** (o criador: "gráficos 100%, vale deixar o Vulkan como
    padrão"). Sem `dados/renderizador.txt`, o batida.nro passa a vez ao batida.vulkan; a preferência
    `video` nasce "vulkan"; a opção chama "Vulkan" (sem "experimental"), e a dica diz que o OpenGL é o
    mais lento, para quando o Vulkan der problema. Limite: a primeira abertura de todas em Vulkan compila
    os shaders antes de o Godot desenhar qualquer coisa, e a tela fica preta (a cortina não alcança essa
    fase); a abertura seguinte usa o cache (<5 s).
  - **A cortina do carregamento** (pedido do criador: o "Carregando…" era texto solto e o cenário piscava
    por trás): `hud/desenho_da_cortina.gd`, por cima de tudo na `Tela`. Opaca: o céu da festa, dois varais
    de bandeirinhas, um tambor de congo (fitas dos tocadores, cordas em V) batendo a 108 BPM com uma onda
    por batida e cinco contas nas cores das faixas acendendo em sequência. Entra de uma vez e sai
    esmaecendo em 450 ms (`principal.gd` `_cortina`). Cobre a partida enquanto prepara (no Switch desde o
    primeiro quadro; no PC só depois de 300 ms, para o preparo de poucos quadros não piscar) e a espera
    antes de reabrir com outro vídeo. "Movimento reduzido": sem ondas nem pulo. Roteiro de conferência
    `cortina` (capturas em pt-BR, de, ja). Textos novos em `lote16.py`.
- **Ideia do criador, guardada para depois:** trocar o fundo distante da mata por uma imagem pronta,
  igual ao que o 3D desenha (o jogador vê a mesma coisa). Só se render ganho e não houver outro
  caminho; o que pesa hoje são as copas perto da câmera, que uma imagem não substitui sem mudar o
  visual.
- **Estudo do NRO genérico:** `docs/estudo-nro-generico.md` (viável depois do Batida redondo).
- **Errado e desfeito:** a escala 3D automática do celular aplicada ao Switch desceu até 40% da
  resolução e deixou tudo serrilhado, sem ganho de FPS (o gargalo não é a placa de vídeo).
- **No console hoje:** menu ~26 FPS, partida ~28–31 FPS; a primeira vez que um modelo ou efeito aparece
  o Switch compila os shaders e para por até 2 min (depois vai para o cache); abertura ~1,5–2 min.
- **Não observado:** o ganho dos menus no console (próximo teste).

---

## 2026-10-19 — O port sai do jogo e vira o godot-switch-nro (0.2.27)

- **Pedido do criador:** deixar o repositório do jogo só com o jogo e levar o port para um repositório
  próprio e público, genérico para outros jogos, com uma branch do Batida que gere a mesma entrega.
- **O que mudou no motor para ser genérico:** os nomes de arquivo saem do próprio executável
  (`batida.nro` → `<jogo>.nro`); a abertura saiu do binário (`.incbin`) para o romfs de cada `.nro`; a
  versão do jogo vem de `romfs:/versao.txt` (antes era compilada no motor) e a do motor é a do
  `VERSION`; o `.nro` OpenGL passa `gl_compatibility` sozinho; o `.pck` dentro do `.nro` virou
  `romfs:/jogo.pck`. O ELF OpenGL ficou 3,6 MB menor (a abertura) e o `.nro` com o mesmo tamanho.
- **A entrega antiga** (`switch/entregar.sh`: subir a versão, compilar no WSL, exportar, copiar) virou
  `python gsn.py gerar <pasta do jogo>` com `jogos/batida-capixaba/jogo.cfg`. A pasta `switch/` saiu do
  repositório do jogo (guardada em `.legado/` deste repositório, fora do git, com as entregas antigas).
- **0.2.27**, a primeira entrega pelo `gsn.py`: `batida.nro` 55,6 MiB, `batida.vulkan` 66,7 MiB,
  `batida.pck` 409,4 MiB, os mesmos tamanhos da 0.2.26; a abertura e o ícone idênticos byte a byte. Falta
  o teste no console (ver `pendencias.md`).
- No devlog do jogo, a DEC-102 ficou com o resumo do que mudou **no jogo** e um ponteiro para cá.
