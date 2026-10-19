# Estudo de viabilidade: o .nro do Batida como runtime genérico de Godot 4.7 para Switch homebrew

> **Registro histórico.** Escrito antes deste repositório existir, quando o port ainda vivia dentro do
> projeto do Batida Capixaba. Parte do que ele propõe já foi feita (o motor sem nomes de jogo, a
> abertura e a versão no romfs, o empacotador `gsn.py`); o que falta está em `docs/pendencias.md`. Os
> caminhos e números de linha citados abaixo são da época.

Data: 2026-10-18. Escopo: só pesquisa. Nenhum código foi alterado, nada foi compilado e não houve commit.

Convenção usada no texto:

- **[medido]**: número medido no console pelo criador (vem do pedido e do devlog, DEC-102);
- **[código]**: conferido no código (o port em `switch/platform/switch/` ou o Godot 4.7.2 em `/root/switch/godot`);
- **[estimativa]**: leitura minha, ainda sem teste.

As linhas citadas são da versão de 18/10 às 04:50. O port estava sendo editado durante o estudo (a pasta
`api/` e o turbo apareceram no meio dele), então as linhas podem andar um pouco.

---

## 0. Resposta curta

**Dá para fazer, e a parte de "runtime + empacotar" custa pouco** [estimativa: 6–9 dias]. O próprio
Godot 4.7 já procura um `.pck` anexado ao fim do executável [código: `core/io/file_access_pack.cpp:257-281`],
e o .nro é o executável. Então o empacotamento não precisa de devkitPro: basta reescrever o bloco de
ícone e NACP do .nro e anexar o .pck no fim.

O que custa caro é a **compatibilidade com jogos dos outros**: rede, teclado virtual, idioma, HOME e
suspender, vibração, mais controles, módulos desligados e o desempenho. Isso soma de 25 a 45 dias, e
depois ainda vem a manutenção a cada versão do Godot.

**Recomendação: não fazer agora.** Primeiro, terminar o Batida no Switch. Boa parte do que um runtime
genérico precisa (idioma do sistema, pausa no HOME, vibração, salvar ao fechar) **o Batida também
precisa** e hoje não tem. Depois, publicar um runtime "experimental" com o empacotador, sem prometer
suporte (seção 8).

---

## 1. O que hoje é do Batida no .nro e como generalizar

| # | Onde | O que é do Batida | Como generalizar | Custo |
|---|---|---|---|---|
| 1 | `SCsub:21-23` | título e autor padrão "Batida Capixaba", versão "0.1.0" (pelas variáveis `SWITCH_TITULO/AUTOR/VERSAO`) | padrão neutro (ex.: "Godot 4.7 Runtime (homebrew)", veja a marca na seção 5); o empacotador troca por jogo | 0,1 d |
| 2 | `SCsub:24`, `icone.jpg` | o ícone do Batida | ícone neutro no runtime; o empacotador põe o do jogo | 0,1 d |
| 3 | `SCsub:26,31`, `construir.sh:2` | as saídas `bin/batida.nacp` e `bin/batida.nro` | `bin/godot_switch_runtime.nro` (e um `.elf` para quem quiser usar o romfs) | 0,1 d |
| 4 | `godot_switch.cpp:131` | o caminho de reserva `sdmc:/switch/batida/batida.nro` quando falta o argv[0] | sem caminho fixo; avisa na tela se o argv[0] vier vazio (ex.: forwarder) | 0,2 d |
| 5 | `godot_switch.cpp:152-159` | procura `romfs:/batida.pck` | ordem: (a) `.pck` anexado ao próprio .nro (o Godot já faz, [código] `project_settings.cpp:707-714`); (b) `romfs:/game.pck` ou o primeiro `*.pck` do romfs; (c) `<nome>.pck` ao lado do .nro (o Godot já faz, `project_settings.cpp:737`) | 0,3 d |
| 6 | `os_switch.cpp:105-119` | `user://` = `sdmc:/switch/<config/name>/dados`: a subpasta "dados" é em português | `sdmc:/switch/<nome>/data` (ou a raiz da pasta); uma opção de projeto `switch/user_dir_suffix` para o Batida manter os saves onde estão | 0,3 d |
| 7 | `godot_switch.cpp:46-71, 138-140` | o log `<nome>.log` é **sempre** criado e truncado a cada abertura, sem buffer (mais lento) | o log só liga se existir `<nome>.log.on` (ou por opção de projeto); no padrão, nada é gravado no cartão | 0,3 d |
| 8 | `godot_switch.cpp:73-92, 142` | `_sondar_pck` (sonda de depuração) | remover | 0,1 d |
| 9 | `godot_switch.cpp:94-128, 143` | a thread de "pulso" grava no log a cada 5 s e chama `fsync` | só no modo diagnóstico (o mesmo marcador do log) | 0,2 d |
| 10 | `godot_switch.cpp:144,150,172,178-193`, `os_switch.cpp:39`, `display_server_switch.cpp:200`, `audio_driver_switch.cpp:71`, `api/api.cpp:140,152` | os `printf("[switch] …")` de depuração, com os testes do logger (`:178-179`) | os de diagnóstico passam por `print_verbose`; ficam só 2 ou 3 linhas de abertura (versão, modo título/applet) | 0,3 d |
| 11 | `api/api.cpp:22-26, 65-72`; `api/switch_singleton.h` | o singleton "Switch" com métodos em português (`fonte_do_sistema`, `definir_turbo`, `na_base`…). O Batida já usa (`hud/fontes_switch.gd:54-57`) | nomes em inglês (`get_shared_font`, `set_cpu_boost`, `is_docked`, `is_charging`, `get_model`) e os nomes em português mantidos como apelidos, para não quebrar o Batida | 0,5 d |
| 12 | `os_switch.cpp:146-164`, `os_switch.h` | as regras de aceleração (FastLoad) foram afinadas com o Batida (o "desafio do machado") | ficam, mas com uma opção de projeto para desligar (`switch/fastload_on_slow_frames`) | 0,2 d |
| 13 | `detect.py:2`, cabeçalhos | comentários "escrita para o Batida" | trocar o texto | 0,1 d |
| 14 | `detect.py:51-71` | módulos desligados porque o **Batida** não usa (CSG, vídeo, rede, Jolt, text_server_adv) | veja a seção 3: alguns precisam voltar para um runtime genérico | 6–15 d |
| 15 | `/root/switch/godot/main/main.cpp` | as "sondas" do `Main::setup` (`sondas_setup.py`) | restaurar `/root/switch/main.cpp.original` antes de qualquer build público | 0,1 d |
| 16 | `godot_switch.cpp` (falta) | o projeto tem de ter `rendering_method.switch="gl_compatibility"`; sem isso o Godot procura o driver "vulkan" e **aborta** ("Unknown rendering driver 'vulkan'", [código] `main.cpp.original:2556-2583`) | o runtime passa `--rendering-method gl_compatibility --rendering-driver opengl3` quando o projeto não diz nada | 0,3 d |
| 17 | (falta) | no modo applet (aberto pelo álbum, ~400 MB) o jogo simplesmente não cabe | tela de aviso "abra segurando R" (o port 3.5 fazia isso com `romfs:/applet_splash.rgba.gz`) | 0,5 d |

**Subtotal da generalização: ~3–4 dias** [estimativa], sem contar o item 14.

---

## 2. Empacotar sem compilar nada

### Como o .nro é por dentro [código + formato público do switchbrew]

```
[NroStart 0x10][NroHeader "NRO0" … size][segmentos de código]  ← o que o hbloader carrega na memória
[AssetHeader "ASET": ícone{offset,tamanho}, NACP{offset,tamanho}, romfs{offset,tamanho}]
[ícone JPEG 256×256][NACP 0x4000 bytes][romfs (opcional)]
```

O hbmenu lê o nome, o autor, a versão e o ícone do bloco ASET. O `romfsInit()` acha o romfs pelo mesmo
bloco. O port 3.5 já reescrevia esse bloco na exportação, com `create_nro`/`create_nacp`
[código: `/root/switch/ref35/platform/switch/export/export.cpp:442-556`].

### O atalho: o .pck anexado no fim do .nro

O Godot 4.7, quando não acha um `.pck` solto, abre o próprio executável, lê os 4 últimos bytes
(`GDPC`), volta 12 bytes para ler o tamanho e carrega o pack dali [código:
`core/io/file_access_pack.cpp:257-281`; o `--main-pack` é dispensável porque `project_settings.cpp:714`
tenta o `exec_path` primeiro]. O `OS_Switch::get_executable_path()` já devolve o caminho do .nro
(`os_switch.cpp:93-102`). E o editor já sabe gravar o pack assim: o
`EditorExportPlatform.save_pack(preset, debug, path, embed=true)` abre o arquivo, anexa no fim com
alinhamento de 8 bytes e escreve o trailer [código: `editor/export/editor_export_platform.cpp:2289-2345`,
exposto ao GDScript na linha 2764].

Então: **.nro genérico + ícone/NACP novos no ASET + .pck colado no fim = um .nro do jogo.** Sem romfs,
sem elf2nro, sem build_romfs.

[estimativa] Os dados anexados depois do ASET não atrapalham o hbmenu nem o hbloader, que carrega só os
segmentos de código (`header.size`), mas **isso precisa ser confirmado no console** antes de valer como
caminho principal. No cartão em FAT32 o arquivo final tem de ficar abaixo de 4 GB.

### As opções

| Opção | Como | Quem usa precisa de | Prós | Contras | Custo |
|---|---|---|---|---|---|
| A. Só renomear | `jogo.nro` (genérico) + `jogo.pck` lado a lado | nada | zero trabalho; o Godot já acha o `<nome>.pck` | todos os jogos aparecem no hbmenu com o mesmo nome e ícone | 0 d |
| **B. Ferramenta de linha de comando (Python puro)** | `empacotar.py --nro runtime.nro --pck jogo.pck --titulo … --autor … --versao … --icone icone.png -o jogo.nro`: reescreve o ASET, monta a NACP (16 idiomas × nome 0x200/autor 0x100, versão), converte o PNG para JPEG 256×256 e anexa o .pck com o trailer | Python 3 (Pillow só para converter o ícone) | Windows/Linux/macOS; nada do devkitPro; ~200 linhas; testável no PC | linha de comando; o .pck sai de outra etapa (o editor) | **1,5–2 d** |
| C. romfs + elf2nro/build_romfs (como o 3.5 fazia) | distribui o `.elf` (61 MB) e chama as ferramentas do devkitPro | devkitPro (switch-tools) instalado | formato "canônico", o pck fica como arquivo `romfs:/game.pck` | exige o devkitPro de quem só quer exportar; o devkitPro não gosta que reempacotem as ferramentas dele | 1 d |
| **D. Plugin do editor em GDScript** (addon) | uma `EditorExportPlatformExtension` registrada por `EditorPlugin.add_export_platform()` [código: `editor/export/editor_export_platform_extension.h:51-159`, `editor/plugins/editor_plugin.cpp:473`]: aparece "Switch (homebrew)" em Exportar, com opções (título, autor, versão, ícone, texturas), checagens e o mesmo código de B em GDScript (`FileAccess` + `save_pack(embed=true)`) | só o editor 4.7 oficial | um clique; avisa do que não funciona (C#, GDExtension, Forward+ sem a versão `.switch`, CSG, Theora); define as features `switch`/`etc2`/`astc`; dá para pôr na Asset Library | `EditorExportPlatformExtension` é API nova (4.3+) e pouco usada; o runtime .nro (61 MB) vai junto do addon ou é baixado | **3–5 d** (+1–2 d para "rodar no console" via o protocolo do nxlink) |
| E. Exportador em C++ dentro de um editor próprio (como o 3.5) | `platform/switch/export/` + um editor compilado para Windows, Linux e macOS | baixar um editor não oficial | integração total | manter 3 builds de editor por versão; ninguém quer trocar de editor | 8–12 d + manutenção alta |

**Recomendação: B primeiro, D depois, reaproveitando a mesma lógica.** E não vale a pena. A fica como
"modo rápido" documentado.

O `export_presets.cfg` de hoje (preset "Switch" = plataforma Linux, só o .pck) continua servindo como
etapa 1 de B.

---

## 3. Compatibilidade com outros jogos

Legenda: ✅ funciona · ⚠️ funciona com ressalva · ❌ não funciona.

| Item | Estado | Detalhe | Custo de resolver |
|---|---|---|---|
| **Versão do .pck** | ⚠️ | O leitor aceita os formatos de pack V2–V4 e recusa pack de versão **mais nova** que 4.7 [código: `file_access_pack.cpp:295-296`]. Recursos binários, cenas e o GDScript em tokens (`script_export_mode=2`) mudam entre versões menores. Regra prática: **exportar com o editor 4.7.x**; 4.7.0/4.7.1 provavelmente funcionam [estimativa]; 4.6 é arriscado; 4.8 é recusado. | o runtime mostra uma mensagem clara se recusar (0,3 d) |
| **Renderizador** | ⚠️ | Só Compatibility (GLES 3.2, Mesa 20.1 nouveau). Projeto em Forward+/Mobile sem a versão `.switch` **aborta** hoje (item 16 da seção 1). Forçado para Compatibility, ele abre, mas sem o que o Compatibility não tem: SDFGI/VoxelGI, névoa volumétrica, SSAO/SSR, partículas por GPU com colisão avançada, shaders de compute; a luz e as cores mudam (o Batida precisou do `cor.gdshaderinc` para isso). | forçar: 0,3 d. Diferença visual: por conta de quem fez o jogo |
| **Formatos de textura** | ⚠️ | No GLES o Godot dá o ETC2 como sempre disponível, e o S3TC/BPTC só se o driver anunciar a extensão [código: `drivers/gles3/storage/config.cpp:87-111`]. Um pack exportado só com `s3tc_bptc` (o padrão do preset Linux) pode ficar sem texturas 3D. Exigir `import_etc2_astc=true` e o preset com ETC2/ASTC. (O Tegra X1 decodifica ASTC no hardware.) | o plugin D avisa (incluso). Conferir as extensões do Mesa no log: 0,2 d |
| **C#** | ❌ | `module_mono_enabled=False` (`detect.py:52`); não há .NET para homebrew. | fora de alcance |
| **GDExtension / bibliotecas nativas** | ❌ | não há `dlopen` (`compat/dlfcn.h:15-18` devolve nulo). Plugins como godot-steam, FMOD, Spine-cpp e Terrain3D não carregam. Saída: compilar a extensão **estaticamente** dentro de um runtime próprio (módulo), um build por jogo. | por extensão: 1–3 d e um .nro específico |
| **CSG** | ❌ | desligado: a biblioteca manifold choca com macros da newlib (`detect.py:68-69`). Cena com `CSGBox3D` etc. carrega com erro ou sem esses nós. | 0,5–2 d (`#undef` num cabeçalho de compatibilidade ou um remendo no thirdparty) |
| **Física 3D (Jolt)** | ⚠️ | `module_jolt_physics_enabled=False`. Desde o 4.6 o Jolt é o padrão dos **projetos 3D novos** ([notas do 4.6](https://godotengine.org/releases/4.6/)). Sem ele, o Godot cai em silêncio no GodotPhysics3D [código: `main.cpp.original:362-366`]; empilhamento, juntas e colisões se comportam diferente. | ligar e testar: 0,5–2 d [estimativa] |
| **Texto complexo** | ⚠️ | `text_server_adv` desligado: só o `text_server_fb`. Sem árabe/hebraico (RTL), devanágari, tailandês, ligaduras/recursos OpenType nem quebra de linha por dicionário. Latim e CJK aparecem (o Batida mostra ja/zh/ko). | 1–3 d (ICU + HarfBuzz; +~5–10 MB) |
| **Fontes do sistema** | ⚠️ | `SystemFont` não acha nada no Switch e cai na fonte embutida: sem CJK, sem setas e símbolos (achado do Batida, `hud/fontes_switch.gd:2-11`). O singleton já entrega as fontes compartilhadas (`api.cpp:74-89`); um runtime genérico pode ligá-las como **reserva automática** das fontes, sem o jogo pedir. | 1 d |
| **Rede e sockets** | ❌ | `UNIX_SOCKET_UNAVAILABLE` (`detect.py:112`) desliga **toda** a rede, não só os sockets Unix [código: `drivers/unix/os_unix.cpp:179-182`, `ip_unix.cpp:31`]. Sem o `IPUnix::make_default()`, o `IP::create()` falha no `ERR_FAIL_NULL_V(_create…)` [código: `core/io/ip.cpp:334-337`]: é o erro que aparece no log. Não funcionam HTTPRequest, ENet, WebSocket, multijogador nem o **depurador remoto**. A libnx tem sockets BSD (o port já chama `socketInitializeDefault()`, `godot_switch.cpp:132`) e o port 3.5 tinha rede e mbedTLS. Falta separar o `AF_UNIX` (`net_socket_unix.cpp:99-110, 319, 389…`) e criar um substituto para o `getifaddrs`. | TCP/UDP/HTTP: 2–4 d; HTTPS (mbedTLS + certificados): +1–2 d; ENet/WebSocket depois disso: +0,5–1 d |
| **Vídeo (Theora)** | ❌ | `module_theora_enabled=False`. `VideoStreamPlayer` com `.ogv` não toca. O Batida esconde o vídeo no Switch (`palco/video.gd:22`). | 0,5–1 d para ligar; decodificar 720p no A57 a 1 GHz é pesado [estimativa: até 480p] |
| **Resolução / TV** | ⚠️ | 1280×720 fixo (`display_server_switch.h:23`); na base o sistema amplia para 1080p. Jogos com `stretch` `canvas_items`/`viewport` ficam certos; com `stretch=disabled` e base 1920×1080 saem cortados. `window_set_size`/`mode` não fazem nada. | 1080p na base (trocar `nwindowSetDimensions` na mudança de modo): 1–2 d. A placa de vídeo está ociosa [medido], então pode valer |
| **Teclado virtual** | ❌ | `has_feature` só responde toque e troca de quadros (`display_server_switch.cpp:40-47`): `LineEdit` no console não recebe texto. O port 3.5 tinha o swkbd embutido (`ref35/.../os_switch.cpp:332-488`). | 1,5–2 d |
| **Mouse / teclado USB** | ⚠️ | Não há mouse; o toque vira mouse pelo `emulate_mouse_from_touch` do Godot (padrão ligado) [estimativa: conferir]. Mouse e teclado USB/Bluetooth não são lidos. | 1 d (`hidGetMouseStates`/`hidGetKeyboardStates`) |
| **Toque** | ✅ | `InputEventScreenTouch/Drag` (`display_server_switch.cpp:135-175`). | — |
| **`user://`** | ✅ | `sdmc:/switch/<config/name>/dados` (o nome vem do projeto ou de `custom_user_dir_name`). Funciona; só trocar o "dados" (seção 1, item 6). | 0,3 d |
| **Salvar ao fechar** | ⚠️ | Quando o sistema manda fechar (HOME → fechar), o `appletMainLoop()` fica falso e o laço sai direto (`os_switch.cpp:131`): **não chega `NOTIFICATION_WM_CLOSE_REQUEST`**. Jogo que salva nesse aviso perde o save. | 0,5 d (`appletLockExit` + mandar o aviso + esperar o jogo) |
| **HOME / suspender / foco** | ❌ | Nenhum `NOTIFICATION_APPLICATION_FOCUS_OUT/PAUSED/RESUMED`. O Batida espera esses avisos para pausar (`palco/principal.gd:396`) e **hoje não pausa no Switch**. | 1 d (mensagens do applet → avisos do Godot; o áudio para no suspender) |
| **Vibração** | ❌ | `Input.start_joy_vibration` não faz nada; o driver não implementa. O Batida usa (`telas/tela_da_partida.gd:412`). | 1 d (`hidSendVibrationValues`) |
| **Vários controles** | ⚠️ | Dois jogadores (`joypad_switch.h:71`); Joy-Con avulso na horizontal sem remapeamento; sem o applet de "escolher controles"; sem giroscópio/acelerômetro. | 4–8 controles + Joy-Con avulso: 1–1,5 d; applet de controles: 0,5 d; giroscópio: 1 d |
| **Idioma do sistema** | ❌ | O `OS_Unix::get_locale` lê a variável `LANG`; no Switch não existe, e sai sempre "en" [código: `drivers/unix/os_unix.cpp:1031-1042`]. O Batida usa `OS.get_locale_language()` (`nucleo/idioma.gd:39`): **no console ele abre sempre em inglês**. | 0,5 d (`setGetSystemLanguage` → código do Godot) |
| **Bateria** | ❌ | `OS.get_power_*` sem implementação (o 3.5 tinha `power_switch.cpp`). | 0,5 d |
| **Áudio** | ✅ | audren 48 kHz estéreo, latência ~43 ms (`audio_driver_switch.h:49`). Só estéreo (sem 5.1); sem microfone. | — |
| **Threads** | ✅ | 3 núcleos (`os_switch.h:45`), pilha de 1 MiB pelo `--wrap` (`godot_switch.cpp:26-44`). | — |
| **Memória** | ⚠️ | Modo título ~3,2 GB [medido]; pelo álbum (applet) ~400 MB: jogo médio não abre. | aviso (seção 1, item 17) |
| **Processos/shell** | ❌ | `OS.execute`, `create_process`, `shell_open` devolvem "indisponível" (`compat/posix_switch.cpp`, `os_switch.h:42`). Normal num console. | — |

**O que um projeto precisa respeitar para rodar (a lista que vai no LEIA-ME do runtime):**
editor 4.7.x; renderizador Compatibility (ou aceitar a troca automática); texturas ETC2/ASTC; só
GDScript; sem GDExtension; sem CSG nem Theora (até serem ligados); sem rede (até a etapa 4);
`stretch` ligado; base de 1280×720 ou proporção 16:9; nada de `SystemFont` sem reserva; aguentar um
processador ~10× mais lento.

---

## 4. Desempenho

### O que foi medido no Batida [medido]

| Medida | Valor |
|---|---|
| Abertura na 1ª vez | 1,5–2 min (o Mesa compila os shaders no processador) |
| Abertura com o cache do Godot | ~35 s |
| Gargalo | o processador; a placa de vídeo fica ociosa |
| GDScript | ~10–16× mais lento que um PC moderno |
| FPS (DEC-102) | menu ~26, partida ~28–31 |
| 1ª vez que um modelo ou efeito aparece | trava até 2 min (compilação) |

### O que um jogo genérico deve esperar [estimativa]

- **2D simples** (plataforma, puzzle, visual novel): 60 FPS é provável, se o GDScript por quadro for leve.
- **3D leve** (low-poly, poucas luzes): 30 FPS com cuidado; a primeira abertura longa é igual para todos.
- **Lógica pesada em GDScript** (muitos nós com `_process`, IA, geração procedural): é o que mais sofre.
  Um jogo que gasta 3 ms por quadro no PC gasta 30–50 ms no Switch.
- **Travadas de shader**: todo material, partícula ou efeito novo trava na primeira vez. O "shader
  baker" do 4.7 **não ajuda**: ele só existe para Vulkan, D3D12 e Metal [código:
  `editor/shader/shader_baker/shader_baker_export_plugin_platform_{vulkan,d3d12,metal}.h`].

### O que o runtime pode oferecer

| Recurso | Estado | Custo |
|---|---|---|
| **Aceleração nos carregamentos (FastLoad)** | já existe (`os_switch.cpp:33-41, 146-164`); 1785 MHz enquanto o quadro é lento por causa do processador | 0 d (só a opção para desligar) |
| **Turbo durante o jogo** | já existe e o jogo pede (`api.cpp:120-154`); no Erista só na base ou carregando. Para um runtime genérico: **desligado por padrão**, ligado por opção de projeto | 0,3 d |
| **Cache de shaders pré-gerado** | **viável**. A chave do cache do Godot é o código do shader mais `GL_VENDOR/RENDERER/VERSION` [código: `drivers/gles3/shader_gles3.cpp:140-148`]. Como o Mesa vai **dentro do .nro**, essas strings são iguais em todo console com o mesmo .nro e a mesma placa (Erista e Mariko usam a mesma GM20B) [estimativa: conferir nos dois]. Plano: um "modo gravação" (rodar o jogo no console, passar pelas cenas), copiar `user://shader_cache` para o pack ou o romfs e, na 1ª abertura, o runtime copia para `user://` (ou aponta `Engine::set_shader_cache_path`, `rasterizer_gles3.cpp:339`). Cobre só o que foi visto na gravação. | 2–3 d |
| **Pré-aquecer shaders** | do lado do jogo: instanciar materiais e partículas atrás da tela de carregamento (receita no LEIA-ME) | 0,5 d (documentação) |
| **1080p na base** | a placa de vídeo sobra [medido], então dá para ligar sem perder FPS [estimativa] | 1–2 d |

---

## 5. Licenças e distribuição

Isto **não é parecer jurídico**. Fato = o que o texto da licença ou a documentação dizem; leitura = minha
interpretação.

| Componente | Licença | O que precisa ir junto | Fonte |
|---|---|---|---|
| Godot 4.7.2 | MIT | o `LICENSE.txt` e o aviso de copyright; a documentação sugere enviar o `COPYRIGHT.txt` como `GODOT_COPYRIGHT.txt` (**fato**) | [Complying with licenses](https://docs.godotengine.org/en/stable/about/complying_with_licenses.html), [godotengine.org/license](https://godotengine.org/license/) |
| Terceiros do Godot | variadas: Expat/MIT, BSD, Zlib, Apache-2.0 (mbedTLS, manifold), FTL (FreeType), Unicode (ICU), HarfBuzz… O `COPYRIGHT.txt` tem 107 blocos `Files:` (**fato**) | o `COPYRIGHT.txt` inteiro cobre todos. **FreeType (FTL)** exige o crédito "Portions of this software are copyright © <ano> The FreeType Project" na documentação (**fato**, `COPYRIGHT.txt:1605`). Se o mbedTLS voltar: Apache-2.0 | `/root/switch/godot/COPYRIGHT.txt` |
| Port 3.5 de Stary2001/cpasjuste (referência) | MIT (**fato**, [repositório](https://github.com/Stary2001/godot)) | o código do port foi escrito **tomando-o como referência**. Leitura: se algum trecho foi adaptado de perto (o mapa de botões, a configuração do audren, o swkbd se vier), manter o aviso de copyright deles num `CREDITS`. Custa nada e evita discussão | |
| libnx | ISC (**fato**: `dkp-pacman` diz "custom" e o texto é ISC, "Copyright 2017 libnx Authors") | o aviso ISC | `/opt/devkitpro/licenses/switch-tools/COPYING` (mesmo texto) |
| Mesa 20.1 (switch-mesa) | MIT (**fato**, `dkp-pacman -Qi`) | o aviso MIT do Mesa (o pacote não traz o arquivo de licença; copiar do Mesa 20.1) | |
| libdrm_nouveau | MIT (**fato**) | o aviso MIT | |
| newlib (libc) | várias licenças BSD (**leitura**: o pacote não declara; o `COPYING.NEWLIB` lista avisos que pedem crédito na documentação) | incluir o `COPYING.NEWLIB` | |
| libstdc++/libgcc | GPLv3 + GCC Runtime Library Exception | **leitura**: a exceção cobre o binário; nada a enviar além de um crédito | |
| Ferramentas do devkitPro (elf2nro, nacptool) | ISC (switch-tools) | na opção B elas **não vão junto** (o empacotador é próprio). **Leitura**: o devkitPro pede que não reempacotem as ferramentas dele, mais um motivo para a opção B | |
| Fontes do sistema (`plGetSharedFont`) | da Nintendo | **leitura**: usar em tempo de execução é o que o homebrew faz; **nunca** gravar no `user://` nem pôr dentro de um pack. Escrever isso no LEIA-ME | |

**Marcas.**

- **Nintendo** (leitura): não usar logotipo, ícone, fonte ou imagem da Nintendo. Escrever "para
  Nintendo Switch com homebrew (CFW)" só como descrição, com o aviso "não afiliado à Nintendo; Nintendo
  Switch é marca da Nintendo". Não distribuir nada do SDK oficial, chaves ou arquivos do sistema (o port
  só usa a libnx, que é engenharia reversa limpa). Não fazer forwarder NSP, que é a parte mais
  sensível. Não ligar o projeto a pirataria. Homebrew de motor (RetroArch, SDL, o próprio Godot 3.5)
  existe há anos sem ação conhecida contra ele; isso é histórico, não garantia.
- **Godot** (leitura): a marca é da Godot Foundation. A página da licença não traz política de marca
  (**fato**). Usar um nome próprio com "para Godot 4.7" na descrição, e não "Godot Switch Official"; não
  usar o logo do Godot como ícone padrão sem conferir as regras do logo (`LOGO_LICENSE.md`, CC BY 4.0
  para o logo, mas a marca é outra coisa).

**O que vai junto do runtime:** `LICENSE.txt` (o seu, MIT), `GODOT_COPYRIGHT.txt`, `THIRDPARTY.md` (libnx,
Mesa, libdrm_nouveau, newlib, crédito do FreeType, port 3.5) e o aviso de marca. Custo: 0,5–1 d.

---

## 6. Comunidade, upstream e manutenção

### O que existe hoje (pesquisa de 18/10/2026)

| Projeto | Versão | Estado | Fonte |
|---|---|---|---|
| Stary2001/godot (cpasjuste et al.) | 3.5.x (ramo `3.5.2-stable_switch`); exportador C++ dentro do editor; .nro + .pck | parado; o GameBrew mostra a última versão 3.5.1 (03/2023) | [GitHub](https://github.com/Stary2001/godot), [GameBrew](https://www.gamebrew.org/wiki/Godot_Engine_Switch) |
| Forks: PuntoScene/godot-nx, redthing1, bitingraphics, jakemchugh1… | 3.5.x | cópias; nenhuma com 4.x | [godot-nx](https://github.com/PuntoScene/godot-nx), [redthing1](https://github.com/redthing1/godot_switch), [bitingraphics](https://github.com/bitingraphics/godot-switch), [jakemchugh1](https://github.com/jakemchugh1/godot-switch) |
| fhidalgosola/godot-switch | antigo, "sem som, sem rede, um Joy-Con" | abandonado | [GitHub](https://github.com/fhidalgosola/godot-switch) |
| halotroop2288, ramo `4.x-switch` | 4.x | "development has stopped" (2023) | [Wololo, 09/2023](https://wololo.net/2023/09/18/the-state-of-godot-engine-on-homebrew-consoles/) |
| RAWRLAB | 3.5.x e 4.1.x | **oficial**, só para desenvolvedor licenciado pela Nintendo (portal); não é homebrew | [GameFromScratch, 01/2024](https://gamefromscratch.com/godot-gets-free-nintendo-switch-port/) |
| W4 Games | Switch/Switch 2 | comercial, oficial | [W4 Games](https://www.w4games.com/blog/w4-games-news-1/tag/108) |

**Fato:** não achei nenhum port de Godot 4 para Switch homebrew ativo em 2025–2026. Este seria, até onde
a busca alcança, o primeiro que roda um jogo 4.x completo. Isso tem valor para a comunidade; também
quer dizer que **não há com quem dividir a manutenção**.

### Upstream

- A documentação do Godot trata plataformas próprias como algo **distribuído à parte**: uma pasta em
  `platform/`, clonada sobre o Godot (exemplo: FRT), sem mexer no resto (**fato**:
  [Custom platform ports](https://docs.godotengine.org/en/4.6/engine_details/engine_api/custom_platform_ports.html)).
  O Godot nunca recebeu console nenhum no repositório principal.
- **Leitura:** a plataforma `switch` em si não entra no upstream. O que **pode** entrar são remendos
  pequenos e genéricos: os 6 trechos de `remendos/01-drivers-unix.patch` reescritos como opções
  (`UNIX_XATTR_UNAVAILABLE`, `UNIX_SYSLOG_UNAVAILABLE`) e a separação "sem sockets Unix ≠ sem rede"
  (`UNIX_DOMAIN_SOCKET_UNAVAILABLE`). Com isso o port vira **pasta pura**, sem remendo. Custo: 1–2 d
  mais o tempo de revisão deles (semanas a meses), sem garantia.
- **Forma recomendada:** um repositório próprio só com `platform/switch/` + o empacotador + o plugin,
  construído sobre a tag oficial (como já é hoje com `construir.sh`). Não é fork do Godot inteiro.

### Custo de acompanhar o Godot [estimativa]

| Evento | Frequência | Custo |
|---|---|---|
| Versão de correção (4.7.3…) | a cada 1–2 meses | 0,5 d (compilar, testar 3 demos) |
| Versão menor (4.8, 4.9) | ~2 por ano | 2–5 d: o 4.7 já mudou API de `DisplayServer` (os enums foram para `DisplayServerEnums`, visível no `display_server_switch.h`); OS, áudio e entrada mudam um pouco a cada versão |
| devkitPro (libnx, newlib, gcc) | algumas vezes por ano | 0,5–2 d quando algo quebra (a newlib já exigiu `compat/`) |
| Mesa 20.1 | parado desde 2020 | **risco**: se o Godot passar a exigir algo do GLES que o Mesa 20.1 não tem, não há para onde correr. A alternativa (deko3d, a API nativa da comunidade) seria um renderizador inteiro novo: meses |
| Pedidos e defeitos da comunidade | contínuo | 1–3 d/mês se o projeto pegar |

E cada .pck só roda no runtime da mesma versão menor. Quem publica precisa manter **um runtime por
versão menor** (4.7, 4.8…) ao mesmo tempo, ou os jogos antigos param de rodar.

---

## 7. Plano em etapas

| Etapa | O quê | Esforço [estimativa] |
|---|---|---|
| 0. Limpeza | restaurar `main.cpp`; tirar sondas e prints; log e pulso só por marcador (seção 1, itens 7–10, 15) | 0,5–1 d |
| 1. Runtime neutro | nome, ícone, saída, busca do .pck (anexado/romfs/ao lado), `user://`, forçar Compatibility, aviso de modo applet, singleton em inglês com apelidos (itens 1–6, 11–13, 16–17) | 2–3 d |
| 2. Empacotador (opção B) | `empacotar.py`: ASET + NACP + ícone + .pck anexado; **confirmar no console** que o .pck anexado abre; documentação | 1,5–2 d |
| **= MVP "experimental"** | **roda jogos 2D/3D simples de 4.7 sem rede** | **4–6 d** |
| 3. Plataforma completa | idioma (0,5), HOME/foco/pausa/fechar (1,5), vibração (1), 4–8 controles + Joy-Con avulso (1,5), teclado virtual (2), mouse/teclado USB (1), bateria (0,5), 1080p na base (1–2), fontes do sistema como reserva (1) | 10–12 d (**~5 d disso o Batida aproveita**) |
| 4. Módulos | rede TCP/UDP/HTTP (2–4), HTTPS (1–2), ENet/WebSocket (0,5–1), Jolt (0,5–2), CSG (0,5–2), Theora (0,5–1), text_server_adv (1–3) | 6–15 d |
| 5. Desempenho | cache de shaders pré-gerado (modo gravação + cópia), turbo opcional, guia de otimização | 3–4 d |
| 6. Plugin do editor (opção D) | exportação "Switch (homebrew)" em GDScript com checagens; opcional: rodar no console via nxlink (+1–2) | 3–5 d (+1–2) |
| 7. Teste de aceitação | abaixo, com as correções que ele pedir | 3–5 d |
| 8. Publicação | licenças, LEIA-ME em inglês e português, CI (GitHub Actions com a imagem `devkitpro/devkita64`), página de lançamento | 2–3 d |
| **Total completo** | | **~35–55 d** (MVP 4–6 d) |
| Manutenção | por versão menor do Godot | 2–5 d, ~2× por ano, + suporte |

### Teste de aceitação

Projetos de [godot-demo-projects](https://github.com/godotengine/godot-demo-projects), exportados com o
editor 4.7.2 e empacotados pela ferramenta da etapa 2 (os nomes das pastas a conferir no ramo do 4.7):

| Projeto | Tipo | O que exercita |
|---|---|---|
| `2d/dodge_the_creeps` | 2D | o básico: cenas, áudio, entrada, `user://` (pontuação) |
| `2d/platformer` | 2D física | física 2D, animação, câmera |
| `3d/platformer` | 3D | GLES3, sombras, física 3D (GodotPhysics; com o Jolt ligado, repetir) |
| `3d/material_testers` | 3D shaders | estresse de compilação de shader (mede o cache pré-gerado) |
| `gui/control_gallery` | UI | temas, `LineEdit` (teclado virtual), foco por controle, toque como mouse |
| `audio/generator` | áudio | `AudioStreamGenerator`, latência, estalos |
| `misc/joypads` | entrada | mapeamento dos botões, 2+ controles, vibração |
| `gui/translation` + `misc/os_test` | sistema | idioma do sistema, `OS.*`, features |

O que medir em cada um, no modo título, em Erista e Mariko, portátil e na base:

| Medida | Critério para passar |
|---|---|
| abre e chega na cena principal | 100% (nenhum trava ou fecha); 1ª abertura ≤ 60 s nos 2D, ≤ 120 s nos 3D; 2ª abertura ≤ 50% da 1ª |
| FPS médio e mínimo (pelo pulso, 2 min de jogo) | 2D ≥ 55 médio; 3D ≥ 30 médio; nenhum quadro > 250 ms depois do aquecimento |
| travadas de shader (quadros > 100 ms) | contar; com o cache pré-gerado, cair ≥ 80% |
| memória (pulso) | sem crescer em 10 min |
| áudio | 5 min sem estalo nem atraso perceptível |
| entrada | todos os botões batendo com o `misc/joypads`; o toque clica em botão de UI; o teclado virtual digita |
| `user://` | salva, fecha pelo HOME, reabre e o dado está lá (testa o aviso de fechar) |
| HOME e volta | o jogo pausa (aviso de foco) e volta sem perder o áudio nem a imagem |
| idioma | console em pt-BR/ja → `OS.get_locale()` certo |
| saída | "sair" do jogo volta ao hbmenu sem travar |

Mais o **Batida** como 9º caso, já que é o jogo mais pesado disponível.

---

## 8. Recomendação

**Não fazer o runtime genérico agora. Fazer depois de o Batida estar redondo no Switch, em duas fases.**

Por quê:

1. **O port ainda está mudando** (hoje mesmo apareceram o turbo e o singleton). Generalizar algo que
   ainda muda dobra o trabalho.
2. **Metade do trabalho "genérico" é dívida do próprio Batida no Switch.** O jogo abre sempre em inglês
   (idioma), não pausa no HOME, não vibra e pode perder dados se fechar pelo HOME. Esses ~5 dias da
   etapa 3 valem a pena **agora, pelo Batida**, e já deixam o runtime meio pronto.
3. **O MVP é barato (4–6 d)**, então esperar não custa oportunidade. Não há concorrência ativa em
   Godot 4 homebrew.
4. **O caro é o depois**: manutenção a cada versão do Godot, um Mesa parado desde 2020 e uma
   comunidade que vai pedir C#, GDExtension e rede, e reclamar do desempenho. Isso só vale se o criador
   quiser manter um projeto público. É uma decisão dele, não técnica.

**Ordem sugerida:**

1. **Agora (pelo Batida):** etapa 0 + idioma, HOME/fechar, vibração (etapa 3 em parte). ~6 d.
2. **Depois do Batida pronto no Switch:** etapas 1 e 2 e o teste de aceitação em versão reduzida
   (3 demos). Publicar como "experimental, sem suporte, só 4.7". ~6–8 d.
3. **Só se houver procura** (issues, gente usando): etapas 4, 5 e 6. ~15–25 d, mais a manutenção.

### As 3 maiores barreiras

1. **Desempenho e shaders**: processador ~10× mais lento para GDScript e aberturas de 1,5–2 min na
   primeira vez [medido]. Jogos feitos para PC vão decepcionar sem otimizar; o cache pré-gerado só
   ameniza.
2. **Escopo de compatibilidade**: só Compatibility, só GDScript, sem GDExtension (sem `dlopen`) e, por
   ora, sem rede, CSG, Theora, Jolt e texto complexo. Isso corta uma fatia grande dos projetos.
3. **Manutenção sem ninguém para dividir**: um runtime por versão menor do Godot, a API interna do Godot
   mudando, o Mesa 20.1 sem saída e o cuidado com as marcas Nintendo e Godot.
