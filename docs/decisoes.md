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
- **[medido] O log perdia o fim:** sem buffer, cada linha vai ao cartão, mas o tamanho do arquivo só se
  grava no `fsync` (o do pulso, a cada 5 s). Até o port 0.3.2 os logs terminavam no último pulso, sem
  "fim do laço", "saiu" nem a saída do jogo; desde o 0.3.3 o `main` faz `fsync` antes de voltar. E cada
  abertura guarda o log da anterior em `<jogo>.anterior.log` (`<jogo>.vulkan.anterior.log`): reabrir o
  jogo apagava o log da sessão de teste.
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
- **Módulos desligados:** mono, XR, câmera, vídeo (Theora), UPnP, WebRTC, WebSocket, raycast,
  lightmapper, betsy, Jolt, `text_server_adv` e CSG (a biblioteca manifold choca com macros da newlib).
  O ENet e o mbedTLS voltaram com a rede (D-18). A lista está no `detect.py`; ver `docs/pendencias.md`
  para o que um jogo genérico pode sentir falta.

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
- **[código] `Thread.Priority` do jogo (0.3.2):** o `drivers/unix` não aplica a prioridade pedida
  (`Thread::PlatformFunctions::set_priority` vazio); no Switch, `PRIORITY_HIGH` passa a 0x2B
  (`OS_Switch::initialize`, `svcSetThreadPriority` na própria thread), e `NORMAL`/`LOW` ficam em 59. Para
  trabalho curto e periódico que não pode esperar a principal (o Batida mistura o trecho da música numa
  thread para um `AudioStreamGenerator`); uma thread alta que não dorme toma o núcleo da principal.
- **[medido]** Com o pulso em prioridade alta, linhas do log saíram cortadas com lixo do cartão no meio:
  o stdout e o stderr (dois `FILE` com travas próprias, o mesmo arquivo) e o pulso escreviam ao mesmo
  tempo no mesmo deslocamento. Desde a 0.2.25 o `_write_r` da newlib é embrulhado
  (`-Wl,--wrap=_write_r`): uma trava única para o stdout, o stderr e o `fsync` do pulso.
- O pulso (a cada 5 s) mostra FPS, `malloc` em uso e o pico (a memória do processo era inútil: sempre
  3185/3189 MiB, porque o carregador reserva o heap inteiro), o quadro, a espera da placa (só no
  OpenGL), desenhos, objetos e memória de vídeo.
- **GPU e desenho na CPU no pulso (0.3.1):** no Vulkan a troca não espera a placa, e a "espera da GPU"
  saía sempre 0,0 (Batida 0.2.28: no Ato 7 o script somava ~22 ms de um quadro de 46,5 ms, sem dizer de
  quem era o resto). O laço liga `viewport_set_measure_render_time` no viewport raiz e o pulso mostra a
  média de `viewport_get_measured_render_time_gpu` (a placa, por timestamps) e `_cpu` (o motor montando
  o desenho: cena, culling, listas). No OpenGL os timestamps só existem sobre GL de desktop; sem eles, a
  GPU sai 0 e vale a espera da troca.
- **[medido] "processo" no pulso (0.3.4; até o 0.3.3 o rótulo era "pior script do segundo", errado):** é a
  média do `Performance::TIME_PROCESS`, o passo de processo inteiro (todos os `_process`, animações,
  esqueletos). No Batida 0.2.32 ele bate com o quadro em todas as telas: o jogo é limitado pela CPU no
  processo, com a placa em 16–28 ms. O que o processo tem a mais que os custos medidos pelo jogo é o
  motor processando a cena (no Ato 7, ~26 ms: as animações com esqueleto da multidão, suspeita).
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

## D-18 — Rede: IPv4, UDP/TCP, DNS e ENet (port 0.3.2)

- **Contexto:** o modo online do Batida é P2P: um cliente da DHT do BitTorrent (BEP 5/43) num
  `PacketPeerUDP` acha o outro jogador (DNS dos nós de partida pela fila do `IP`, `bind` em
  `0.0.0.0`, broadcast na rede local, furo de NAT), e o ENet sobe na mesma porta
  (`ENetMultiplayerPeer.create_server`/`create_client` com porta local, `ENetConnection.socket_send`).
  Usa também `IP.get_local_addresses` e `Crypto.generate_random_bytes`. Até aqui o
  `UNIX_SOCKET_UNAVAILABLE` desligava **toda** a rede: sem o `IPUnix::make_default()`, o `IP` era nulo
  (`Parameter "_create" is null` em `core/io/ip.cpp`), o `register_core_types` registrava o singleton
  `IP` nulo, e o GDScript compilava `IP.get_local_addresses()` como chamada num objeto nulo (16
  `Parameter "method" is null` na abertura). Sem o mbedTLS, `Crypto.new()` devolvia nulo
  ("Crypto is not available when the mbedtls module is disabled").
- **Decisão:** o `detect.py` não define mais o `UNIX_SOCKET_UNAVAILABLE`: valem o `IPUnix` e o
  `NetSocketUnix` do `drivers/unix`, sobre os sockets BSD da libnx. O `OS_Switch::initialize` chama o
  `OS_Unix::initialize_core`, que faz o `make_default` dos dois **antes** do `register_core_types`
  (no `Main::setup`): o singleton `IP` passa a existir.
- **O que a libnx não tem** (`patches/godot/03-rede-horizon.patch`, tudo atrás do `HORIZON_ENABLED`, que
  só o `detect.py` do Switch define; as outras plataformas compilam igual):
  - sem `sys/un.h`: as partes AF_UNIX do `NetSocketUnix` devolvem `ERR_UNAVAILABLE`;
  - sem IPv6 (nem o `ipv6_mreq`): todo socket abre em IPv4, como no OpenBSD; o `htons` vem do
    `arpa/inet.h`, como no Web;
  - sem `getifaddrs`: o `IPUnix::get_local_interfaces` fica na plataforma (`rede_switch.cpp`): a volta
    (`lo`, 127.0.0.1) e o endereço do console (`nifm`), do `nifmGetCurrentIpConfigInfo`. O nifm abre e
    fecha a cada pedido (o jogo pede ao abrir uma sala): nada fica aberto (D-14).
- **O que a libnx faz sozinha:** `getaddrinfo` (pelo serviço sfdnsres, uma sessão por pedido, só IPv4),
  `poll`, `ioctl(FIONREAD)` e o `fcntl` só com `O_NONBLOCK` (o que o Godot usa para não bloquear; o
  `FD_CLOEXEC` que o Godot pede é recusado sem efeito: não há outros processos).
- **DNS e threads (regras 3 e 6):** o `IP` do Godot resolve nomes numa thread própria (criada no
  `IP::IP`, no `register_core_types`; esperada no `IP::~IP`, no `unregister_core_types`, dentro do
  `Main::cleanup`, antes do `socketExit` e da conferência da saída). É uma pthread: prioridade 59, abaixo
  da principal (D-15). Ela quase só espera (o semáforo, depois a resposta do sfdnsres); com o Vulkan
  compilando shaders nos três núcleos a resposta pode atrasar **[estimativa]**, sem travar o jogo (a
  fila é consultada a cada quadro). A libnx não tem prazo para o `getaddrinfo`: um nome sendo resolvido
  na hora de sair atrasa a saída até o sfdnsres responder.
- **Sockets da libnx:** o `socketInitializeDefault()` de sempre. Cada socket UDP recebe até 42 KiB
  (`0xA500`) e manda 9 KiB (`0x2400`); um pacote do ENet tem até ~1400 bytes, então ~30 cabem entre dois
  quadros (o jogo consulta o ENet a cada quadro, sem esperar). As respostas da DHT têm centenas de bytes.
  3 sessões do bsd:u = 3 chamadas ao mesmo tempo, e só a thread principal usa sockets (o nxlink, quando
  ligado, usa um). Aumentar os buffers só gastaria heap (a memória dos sockets sai do heap do `.nro` e
  volta no `socketExit`).
- **Módulos:** `module_enet_enabled` e `module_mbedtls_enabled` ligados. O mbedTLS dá o `Crypto`
  (`generate_random_bytes`, chaves, HMAC), o `HashingContext` já vinha do núcleo. Os bytes aleatórios
  vêm do `OS::get_entropy` (o `randomGet` da libnx). O mbedTLS não se reconhece no devkitA64 (só Unix e
  Windows): a configuração dele vem de `platform/switch/mbedtls_config_switch.h` (o gancho
  `GODOT_MBEDTLS_INCLUDE_H` do Godot, sem remendo): a do Godot, sem os sockets do próprio mbedTLS
  (`MBEDTLS_NET_C`, que o Godot não usa) e com o relógio e os temporizadores do DTLS pela plataforma
  (`mbedtls_switch.cpp`, `armGetSystemTick`). `builtin_certs` desligado: sem ele, a abertura não lê os
  ~140 certificados do pacote embutido (o `Crypto::load_default_certificates` roda em todo jogo).
- **Fica fora:** HTTPS/TLS de verdade (sem certificados; um jogo pode apontar
  `network/tls/certificate_bundle_override`, não testado), WebSocket, WebRTC, UPnP (o Batida não usa no
  Switch), IPv6, multicast IPv6 e sockets AF_UNIX. TCP (`StreamPeerTCP`, `TCPServer`) compila mas não
  foi pensado para o console.
- **Na abertura** o log diz o estado da rede: `[switch] rede: 192.168.0.7/24 (Wi-Fi, 3 barras, internet
  ok)` ou `[switch] rede: sem conexão`.
- **Custo:** o `godot-opengl.elf` foi de 54,60 MB para 55,33 MB (+0,73 MB, +1,3%); o
  `godot-vulkan.elf`, de 69,94 MB para 70,75 MB (+0,81 MB); a maior parte deve ser o mbedTLS
  **[estimativa]**.
- **[estimativa]** Nada disto rodou no console ainda: o plano está em `docs/testes.md` ("Rede"). O ponto
  mais incerto é se a conexão do sistema fica de pé para um homebrew sem um pedido ao nifm
  (`nifmCreateRequest`), como fazem os jogos oficiais; se o log disser "sem conexão" com o Wi-Fi ligado,
  é o próximo passo (e o pedido teria de ser fechado na saída).
