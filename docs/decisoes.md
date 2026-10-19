# Decisões do port

As decisões de projeto do godot-switch-nro, com o porquê e o que foi medido. O port nasceu para o jogo
**Batida Capixaba** (outubro de 2026), e as versões citadas como "0.2.x" são as entregas do Batida em que
cada coisa foi observada no console. O histórico do jogo está na branch `batida-capixaba`
(`jogos/batida-capixaba/historico.md`).

Convenção: **[medido]** = medido num Switch real; **[código]** = conferido no código; **[estimativa]** =
leitura ainda sem teste.

---

## D-01 — Portar o Godot 4.7.2 do zero

- **Contexto:** não existe Godot 4 para homebrew. O port de Stary2001 e cpasjuste parou no Godot 3.5
  (MIT) e foi a referência para as partes que falam com o console (libnx: vídeo, som, controles).
- **Decisão:** uma plataforma nova, `platform/switch`, no Godot **4.7.2** (versão fixa, tag
  `4.7.2-stable`), compilada com o devkitPro (devkitA64 + libnx).
- **Só `.nro` no Atmosphère.** Android/Linux do Switchroot ficaram de fora a pedido do autor do jogo.
- **Modo título:** o jogo precisa ser aberto pelo hbmenu **segurando R ao abrir um jogo**. Pelo álbum
  (modo applet) o homebrew tem só ~400 MB, e um jogo 3D não cabe.

## D-02 — O ambiente de compilação

- O `apt`/`pacman` do devkitPro respondem 403 (Cloudflare) fora do navegador: a saída foi a imagem
  oficial `devkitpro/devkita64` no Docker, fixada por resumo (sha256) em `docker/Dockerfile`.
- No Windows: Ubuntu do WSL **como root**, com o `docker.io` do Ubuntu (sem Docker Desktop). O
  `scripts/comum.sh` sobe o `dockerd` se ele não estiver rodando (o WSL sem systemd não sobe sozinho).
- LTO completo (`"lto": "full"` no `detect.py`): o motor ficou ~11% menor (61,4 → 54,5 MB) e mais
  rápido no processador do Switch. Do zero, ~9 min por variante.

## D-03 — Armadilhas do Godot 4 na libnx (resolvidas)

- O `Main::setup` procura o `.pck` antes de guardar o caminho do executável: o
  `OS_Switch::get_executable_path` devolve o `argv[0]` (guardado antes do `Main::setup`).
- O cartão não abre o mesmo arquivo duas vezes para escrita: o log (`<jogo>.log`) é aberto uma vez e o
  stdout e o stderr apontam para ele (`dup2`).
- Nada de `FileAccess` antes do `StringName::setup`; sem `/dev/urandom` (`randomGet` da libnx).
- A newlib não tem `sys/xattr.h` nem `pthread_setname_np`: remendo `patches/godot/01-drivers-unix.patch`
  e `PTHREAD_NO_RENAME`.
- Pilha das threads: a pthread da newlib nasce com pilha pequena demais para o GDScript; o ligador troca
  o `pthread_create` (`-Wl,--wrap=pthread_create`) por um que dá 1 MiB a quem não pediu tamanho.
- `patches/godot/02-main-diagnostico.patch`: cada saída de erro do `Main::setup` imprime a linha. Só
  diagnóstico (custa nada); ajudou a achar as falhas da abertura.

## D-04 — Plataforma: som, controles, toque, arquivos

- **Som:** audren, estéreo, 48 kHz, blocos de 1024 amostras em dois buffers.
- **Controles:** jogador 1 = 1º controle ou o console no modo portátil; jogador 2 = 2º controle. Os
  botões seguem a posição do Godot 4 (o B da Nintendo é o "A" do Godot); ZL/ZR são gatilhos (0 ou 1). A
  vibração do `Input.start_joy_vibration` vai aos Joy-Cons.
- **Toque:** vira `InputEventScreenTouch`/`InputEventScreenDrag`.
- **Arquivos do jogo (`user://`):** em `dados/`, dentro da pasta do `.nro` (ex.:
  `sdmc:/switch/jogo/dados`): apagar a pasta do jogo apaga tudo. A pasta antiga do Godot
  (`sdmc:/switch/godot_app_userdata_<projeto>/`) é movida para lá na primeira abertura.
- **Idioma:** o do console (`OS.get_locale()`; o `OS_Unix` lia a variável `LANG`, que o Switch não tem).
- **Processador:** o homebrew roda em 3 dos 4 núcleos (`get_processor_count` = 3).
- **Módulos desligados:** mono, XR, câmera, vídeo (Theora), UPnP, ENet, WebRTC, WebSocket, mbedTLS,
  raycast, lightmapper, betsy, Jolt, `text_server_adv` e CSG (a biblioteca manifold choca com macros da
  newlib). A lista está no `detect.py`; ver `docs/pendencias.md` para o que um jogo genérico pode sentir
  falta.

## D-05 — O botão HOME

- **[medido]** O aviso de foco do sistema não chega (0.2.5: modo de foco aceito, nenhum aviso no HOME).
  O HOME congela o processo inteiro, todas as threads (0.2.7: o pulso do log parou 157 s, e um quadro
  durou 155 s). Esperar a parada na troca de quadro (0.2.6) não funciona: não é ali que ela acontece.
- **Decisão (desde a 0.2.8):** uma thread **sentinela** com prioridade 0x2B (acima da principal) dorme
  100 ms de cada vez. Se acordar mais de 1,5 s depois, o processo esteve congelado: na volta, o motor
  manda ao jogo `NOTIFICATION_APPLICATION_PAUSED`/foco perdido (o jogo decide se pausa). O fechar pelo
  sistema avisa a janela (`WINDOW_EVENT_CLOSE_REQUEST`).
- Na saída, o modo de foco do applet volta ao padrão (o processo continua vivo no carregador).

## D-06 — Aceleração (FastLoad) só na abertura

- O FastLoad do sistema (o modo dos jogos oficiais nas telas de carregamento) põe o processador a
  1785 MHz e a **placa de vídeo no mínimo**.
- **[medido]** Ligado durante o jogo (até a 0.2.2), um desafio pesado caiu a 4 FPS por ~40 s; a 0.2.4
  esperava 180 quadros leves para desligar, que com a placa no mínimo nunca vinham, e o jogo inteiro
  ficou a 5 FPS.
- **Decisão:** liga antes do `Main::setup` e desliga de vez no começo do laço principal. Só no modo
  título (pelo álbum o sistema não deixa).
- **Turbo** (opcional, pelo jogo): o processador a 1785 MHz sem baixar a placa. No Erista (o Switch de
  2017) o sistema só aceita na base ou carregando; reaplicado a cada ~1 s (o sistema volta ao padrão ao
  mudar de modo).

## D-07 — O singleton `Switch`

O que o jogo pode pedir ao console (só existe no Switch: `Engine.has_singleton("Switch")`). Lista e uso
no README ("Para o jogo"). Nomes em português, como o resto do port.

## D-08 — Onde o tempo vai (o processador é o gargalo)

- **[medido]** O processador do Switch é ~10–15× mais lento que um PC de mesa para o mesmo script
  (Batida 0.2.6: o mesmo quadro de 3,5 ms no PC levou 54 ms no console).
- **[medido]** `malloc`+`free` na newlib: 441–499 ns (~25× o PC); uma conta de inteiros 1,68–1,87 ns.
  Pesa ~5% do quadro: não é a causa principal. O `.nro` mede e escreve no log na abertura.
- Cada chamada de desenho custa caro no driver: agrupar (MultiMesh) rende no Switch mesmo quando no PC
  não muda nada.
- `--render-thread separate` (o desenho em outra thread) fica atrás do arquivo `render_separado` ao lado
  do `.nro`: no console não mudou o quadro (0.2.8), e no PC o OpenGL com ele cai na abertura (erro do
  motor). Não é o padrão.

## D-09 — Mesa 26 no OpenGL: testado e desfeito

- O `danfromtico/mesa-switch` (Mesa 26.2.4, MIT, camada própria `src/nouveau/horizon`) também compila o
  OpenGL/GLES. O `detect.py` aceita `mesa=<prefixo>` para ligá-lo no lugar do 20.1.
- **[medido]** Abertura bem mais rápida (motor pronto antes de 5 s, antes ~12), mas cada quadro ~13 ms
  mais caro no driver (menu 30 → 45 ms; desafio pesado 40 → 53 ms). Sem o glthread (padrão do Mesa 26,
  desligado por `MESA_SWITCH_GLTHREAD=0`), igual. **Decisão:** o OpenGL segue no Mesa 20.1 do devkitPro.
- `mesa_env.txt` ao lado do `.nro`: cada linha `NOME=valor` vira variável de ambiente antes do vídeo,
  para testar opções do driver sem recompilar.

## D-10 — Cache de shaders

- **[medido]** No OpenGL **não há cache**: o Mesa 20.1 informa 0 formatos binários
  (`GL_NUM_PROGRAM_BINARY_FORMATS`). O Godot grava os arquivos assim mesmo (quase vazios) e recompila tudo
  a cada abertura. O cache em disco do próprio Mesa (`MESA_SHADER_CACHE_DIR`, apontado para
  `dados/mesa_cache`) não grava nada.
- **[medido]** No Vulkan o cache funciona: o SPIR-V vai para `dados/shader_cache` (`*.vulkan.cache`) e
  os pipelines prontos para `dados/vulkan/pipelines.*.cache`. A segunda abertura usa o cache.
- O Godot nunca apaga cache velho: a pasta cresce de versão em versão; apagar é seguro (recompila).
- O log conta os arquivos na abertura e com 600 quadros (`[switch] cache ...`).

## D-11 — A abertura antes do Godot

- Nos primeiros segundos (o vídeo existe ~4 s depois de abrir; o Godot leva mais ~7 s compilando os
  shaders dele antes da própria imagem de abertura) a tela era preta.
- **Decisão:** o `.nro` OpenGL desenha uma imagem assim que o vídeo existe: `romfs:/abertura.rgba`
  (RGBA crus 1280×720), que o `gsn.py` gera da imagem de abertura do projeto
  (`boot_splash/image.switch` ou `boot_splash/image`). No Batida ela era embutida com `.incbin`; no
  repositório genérico vem do romfs de cada jogo.
- **Limite:** no Vulkan a imagem ainda não aparece (ver pendências).

## D-12 — Dois executáveis: OpenGL e Vulkan

- **Contexto:** existe Vulkan para homebrew no Horizon: o NVK do `danfromtico/mesa-switch` (usado pelo
  NetherSX2_nx). Os dois Mesa (20.1 para o OpenGL, 26 para o Vulkan) **não cabem no mesmo binário**:
  bibliotecas estáticas com os mesmos nomes internos.
- **Decisão:** dois executáveis na mesma pasta: `<jogo>.nro` (OpenGL, renderizador Compatibility) e
  `<jogo>.vulkan` (Vulkan, renderizador Mobile, o mesmo do PC e do celular). O hbmenu só lista o `.nro`.
- **A escolha:** o jogo grava `user://renderizador.txt` (`opengl` ou `vulkan`). Na abertura, o
  `<jogo>.nro` lê o arquivo e, com Vulkan, passa a vez ao `<jogo>.vulkan` pelo carregador do homebrew
  (`envSetNextLoad`) antes de iniciar o Godot. O singleton tem `reabrir()` para aplicar a troca na hora.
- **A trava:** o `<jogo>.nro` grava `dados/vulkan_abrindo` **antes** de passar a vez; o `<jogo>.vulkan`
  só a apaga depois de rodar 300 quadros. Se a marca sobra (falhou, travou ou foi fechado no meio), a
  próxima abertura volta ao OpenGL, grava `renderizador.txt` = `opengl` e `dados/vulkan_falhou` (o jogo
  pode avisar). **[medido]** Funcionou na 0.2.23 e na 0.2.24.
- O nome `<jogo>` é o do próprio executável sem a extensão: o motor não conhece o nome de nenhum jogo.

## D-13 — O Vulkan é o padrão

- **[medido]** 0.2.24, fora da base (portátil): menu 25–38 FPS (OpenGL ~26–35); partida comum 38–40 FPS,
  25 ms (OpenGL 28–31, ~33 ms); a cena mais pesada (folhagem, 3D a 75%) 30–31 FPS, 32 ms (OpenGL ~24,
  40 ms). Abertura com cache: `Main::setup` em menos de 5 s (OpenGL ~15 s). O visual saiu inteiro.
- **Decisão (0.2.26):** sem `renderizador.txt`, o `<jogo>.nro` passa a vez ao `<jogo>.vulkan` (se ele
  existir). O OpenGL fica como reserva.
- **Limite:** a primeira abertura de todas em Vulkan compila os shaders antes de o Godot desenhar
  qualquer coisa: tela preta por um tempo (minutos, num jogo grande). Daí em diante, o cache.

## D-14 — O que sobra na saída derruba o próximo `.nro`

- **[medido]** 0.2.21: ao passar a vez ao Vulkan, o **carregador de homebrew** caiu com 2001-0106
  (`InvalidCurrentMemory`), sem log, e o jogo não abria mais. O relatório do Atmosphère
  (`sdmc:/atmosphere/crash_reports/`) mostrou uma segunda thread viva, dormindo 5 s: o pulso do log,
  solto com `pthread_detach`. A libnx não encerra threads que sobram, e a pilha dela (1 MiB do heap) ficou
  presa; o `.vulkan`, maior que o `.nro`, precisa daquele pedaço do heap.
- **Regra:** toda thread criada pelo port é esperada antes de o `main` sair, e todo serviço aberto é
  fechado. A vibração para e o modo de foco volta ao padrão.
- **Conferência automática:** o `pthread_create` embrulhado anota cada thread viva; no fim
  (`userAppExit`, depois dos destrutores) o heap do `.nro` é percorrido com `svcQueryMemory` atrás de
  trechos presos (tipo diferente de heap ou com atributo). Resultado em `<jogo>.saida.txt` (ou
  `<jogo>.vulkan.saida.txt`) e no fim do log: "tudo solto" é o esperado.

## D-15 — O log e as prioridades das threads

- **[código]** Toda pthread da libnx nasce com prioridade 59 (abaixo da principal, 44;
  `__syscall_thread_create` desmontado), em qualquer núcleo permitido. **[medido]** Com o Vulkan
  compilando shaders nos três núcleos, o pulso do log (uma pthread) ficou mudo (0.2.22). Desde a 0.2.24 o
  pulso é uma thread da libnx com prioridade 0x2B, como a sentinela.
- **[medido]** Com o pulso em prioridade alta, linhas do log saíram cortadas com lixo do cartão no meio:
  o stdout e o stderr (dois `FILE` com travas próprias, o mesmo arquivo) e o pulso escreviam ao mesmo
  tempo no mesmo deslocamento. Desde a 0.2.25 o `_write_r` da newlib é embrulhado
  (`-Wl,--wrap=_write_r`): uma trava única para o stdout, o stderr e o `fsync` do pulso.
- O pulso (a cada 5 s) mostra FPS, `malloc` em uso e o pico (a memória do processo era inútil: sempre
  3185/3189 MiB, porque o carregador reserva o heap inteiro), o quadro, a espera da placa (só no
  OpenGL), desenhos, objetos e memória de vídeo.
- **Marcas de passagem do Vulkan:** `vulkan_etapas.txt` ao lado do executável, uma linha por etapa
  vencida antes do `main` (userAppInit, construtores, main, sockets, log). Antes do `main` o caminho vem
  da linha de comando crua do carregador (`envGetArgv`).

## D-16 — O NVK compilado para o Switch

- O `build-switch.sh` do `mesa-switch` compila a parte em Rust (o compilador NAK) para x86-64 quando
  roda no Docker em Linux (o wrapper dele só passa o alvo com `MESA_SWITCH_RUST_TARGET`): o
  `libvulkan.a` saiu com 365 objetos de PC, e o ligador recusou.
- Pôr o alvo numa variável global quebrou as macros procedurais (que rodam no PC); pôr no cross file
  quebrou o teste inicial do Meson.
- **Decisão (`patches/mesa-switch/`):** o alvo `--target=aarch64-unknown-linux-gnu` só no cross file
  (`switch_cross_file_gsn.txt`), o alvo instalado no rustup, e um wrapper do rustc
  (`rustc-switch-wrapper-gsn.sh`) que tira o `--target` e o `-C linker=` só do teste inicial do Meson.
  Resultado: o `libvulkan.a` com 922 objetos ARM.
- **Ligação** (`detect.py`): o `libvulkan.a` (NVK, NIR, NAK, runtime de Vulkan) + `nak_rs` (a biblioteca
  do Rust) + os utilitários do Mesa, o grupo duas vezes (se chamam uns aos outros); `dl`, `rt` e `util`
  vazias (o Rust as pede). Não ligar o `libnvk.a`/`libnir.a` junto (duplica definições). O
  `posix_memalign` da camada de compatibilidade é fraco: vale o do NVK.
- A superfície é a `VK_NN_vi_surface` sobre o `NWindow` padrão
  (`rendering_context_driver_vulkan_switch`); o display server tem os dois caminhos (EGL e
  RenderingDevice), como o do Linux.

## D-17 — Um motor para qualquer jogo

- O motor sai como **ELF** (`godot-opengl.elf`, `godot-vulkan.elf`), sem nada de jogo dentro. O `.nro` de
  cada jogo é montado pelo `gsn.py` com o `nacptool` e o `elf2nro`: título, autor e versão (NACP), ícone
  (JPG 256×256) e o romfs (`abertura.rgba`, `versao.txt` e, se pedido, `jogo.pck`). Trocar de jogo não
  recompila nada.
- O `.pck` é exportado pelo editor do Godot 4.7.2 com o preset do projeto (padrão "Switch"); sem ele, o
  `gsn.py` põe um preset Linux arm64 temporário no `export_presets.cfg` e devolve o arquivo como era.
- A variante OpenGL passa `--rendering-driver opengl3 --rendering-method gl_compatibility` (um projeto
  sem `rendering_method.switch` abortaria procurando o Vulkan); a Vulkan passa `vulkan`/`mobile`.
- A configuração do jogo vem do `project.godot`; o que faltar ou precisar mudar vai num
  `godot-switch-nro.cfg` no projeto ou num `jogos/<jogo>/jogo.cfg` neste repositório.
