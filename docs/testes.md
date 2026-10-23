# Testes feitos

O que já foi provado, onde e com que resultado. Os testes no console foram feitos pelo autor do jogo
Batida Capixaba num Switch de 2017 (Erista) com Atmosphère, abrindo pelo hbmenu no modo título (segurando
R). As versões "0.2.x" são as entregas do Batida; o histórico completo, com as medições do jogo, está
na branch `batida-capixaba` (`jogos/batida-capixaba/historico.md`).

## No console

| Teste | Versão | Resultado |
| --- | --- | --- |
| Prova do motor (o `exemplo/` de hoje, com áudio de uma gravação): 3D com sombra, duas faixas sincronizadas, B cala uma faixa, Joy-Con, toque | etapa 1 (2026-10-18) | **passou** (OpenGL ES 3.2 do Mesa 20.1) |
| Um jogo 3D inteiro no renderizador Compatibility (o Batida: menus, campanha, 9 idiomas, 2 jogadores) | 0.2.1–0.2.20 | **passou**; menu ~26 FPS, partida 28–31 FPS, cena de folhagem ~24 FPS fora da base |
| Abertura com a imagem do jogo antes do Godot (OpenGL) | 0.2.14 | **passou**; ~4 s de tela preta antes do vídeo existir |
| HOME: o processo congela sem aviso; a sentinela pausa na volta | 0.2.7–0.2.9 | **passou** ("pausa: o jogo ficou congelado (HOME)") |
| Fechar pelo sistema (HOME → fechar o software) | 0.2.9 | **passou** (o jogo recebe o pedido de fechar) |
| Idioma do console, fontes do sistema (CJK), vibração | 0.2.x | **passou** |
| FastLoad ligado durante o jogo | 0.2.2–0.2.4 | **falhou** (4–5 FPS): ficou só na abertura (D-06) |
| Turbo (1785 MHz) no Erista fora da base e sem carregador | 0.2.4+ | o sistema aceita em qualquer modo desde a 0.2.4 |
| `--render-thread separate` | 0.2.8 | sem ganho (quadro igual) |
| Mesa 26.2 no OpenGL | 0.2.18–0.2.19 | abre mais rápido, quadros ~13 ms mais caros: **desfeito** (D-09) |
| Cache de shaders no OpenGL | 0.2.22–0.2.23 | **não existe** (0 formatos binários) (D-10) |
| Vulkan (NVK) abrindo | 0.2.21 | **caiu o carregador** (2001-0106): thread do log viva na saída (D-14) |
| Vulkan com a thread do log encerrada | 0.2.23 | **abriu**; tela preta compilando shaders; fechado no meio, a trava voltou ao OpenGL (D-12) |
| Vulkan rodando o jogo inteiro | 0.2.24 | **passou**: visual completo, ~25% mais FPS que o OpenGL, cache de shaders funcionando (D-13) |
| Log sem linhas cortadas (trava única de escrita) | 0.2.25–0.2.26 | aguardando o log do console |
| Conferência da saída ("tudo solto" no `<jogo>.saida.txt`) | 0.2.23+ | aguardando o arquivo do console |
| Conferência da saída, Vulkan | 0.2.28, 0.2.30 | **passou**: "0 threads vivas, 0 trechos do heap presos: tudo solto" (na troca do `.nro` e no fim) |
| Rede: abertura com Wi-Fi (port 0.3.2, D-18) | 0.2.30 | **passou**: `[switch] rede: 192.168.0.151/24 (Wi-Fi, 3 barras, internet ok)`; sumiram o `"_create" is null` e os 16 `"method" is null` |
| Rede: partida online do Batida, Switch ↔ PC na mesma rede (port 0.3.2, D-18) | 0.2.30 | **passou** (relato do autor: "funcionou beleza"); o log da sessão se perdeu na reabertura. Pela internet (DHT, furo de NAT) e sair no meio ainda a provar |
| GPU e desenho na CPU no pulso do Vulkan (port 0.3.1) | 0.2.30 | **passou**: GPU 17 ms no menu, 19–21 ms nas partidas; desenho na CPU 1,5–3 ms. O Batida é limitado pela CPU (script), não pela placa |
| O fim do log na saída | 0.2.28, 0.2.30 | **falhou**: o log parava no último pulso (sem "fim do laço"/"saiu"); corrigido no port 0.3.3 (`fsync` no fim do `main`), a provar |
| O fim do log na saída, com o `fsync` (port 0.3.3) | 0.2.32 | **ainda sem o fim**: o log para no último pulso. A conferir como o jogo foi fechado (HOME → fechar o software para abrir o DBI: o sistema pode encerrar o processo antes do fim do `main`, e o `saida.txt` seria o da sessão anterior) |
| O log da sessão anterior (`<jogo>.anterior.log`, port 0.3.3) | 0.2.32 | **passou**: `batida.anterior.log` e `batida.vulkan.anterior.log` no cartão |
| Espera pelas cercas e envio do Vulkan (port 0.3.5) | 0.2.34 | **a placa e a CPU não estão em série**: `vkWaitForFences` 0,0–0,2 ms e `vkQueueSubmit` 0,3 ms por quadro. Com 30 FPS fixos, o pior quadro do segundo é 28–35 ms sem turbo e 18–22 ms com turbo (Batida, Ato 3 e Ato 7) |
| GPU por quadro no Batida (port 0.3.3) | 0.2.32 | menu 16 ms, Ato 3 17–19 ms, Ato 7 27,7 ms; o quadro ≈ os scripts do jogo + a GPU (hipótese: a placa e a CPU em série no Vulkan, a medir com o port 0.3.5) |

## Medições de referência (console)

- `malloc`+`free`: 441–499 ns; conta de inteiros: 1,68–1,87 ns.
- O mesmo quadro do Batida: 3,5 ms no PC, 54 ms no Switch com turbo (OpenGL, 0.2.6).
- Memória do `malloc` no Batida (Vulkan, 0.2.24): 618 MiB no menu, 634 numa partida, 677 na cena de
  folhagem, estável dentro de cada cena (o processo tem ~3,1 GiB no modo título).

## Neste repositório (sem console)

Feitos em 2026-10-19, num Windows 11 com o WSL (Ubuntu) e o `docker.io`:

| Teste | Resultado |
| --- | --- |
| `python gsn.py preparar` numa pasta vazia: Godot 4.7.2 baixado e remendado (duas árvores), NVK compilado do commit fixado do mesa-switch, bibliotecas juntadas e conferidas (`vkCreateViSurfaceNN` presente) | **passou** (os avisos `bfd plugin: LLVM gold plugin has failed to create LTO module` vêm da indexação das bibliotecas do Rust e não afetam nada) |
| `python gsn.py motor todos` do zero | **passou**: `godot-opengl.elf` 54,6 MB, `godot-vulkan.elf` 69,9 MB. Recompilar sem mudanças: ~5 s por variante |
| `python gsn.py gerar exemplo`, sem configuração nenhuma | **passou**: preset temporário usado e o `export_presets.cfg` devolvido (o exemplo não tinha um: continua sem); título, autor, versão, ícone e abertura saíram do `project.godot` (conferidos dentro do `.nro`) |
| O motor não tem nome de jogo dentro | **passou**: nenhuma ocorrência de "batida" no `.nro` do exemplo |
| `python gsn.py gerar <pasta do Batida>`, só a pasta (a configuração achada pelo `projeto_nome`) | **passou**: `batida.nro` 55,6 MiB, `batida.vulkan` 66,7 MiB, `batida.pck` 409,4 MiB, os mesmos tamanhos da entrega 0.2.26 do jeito antigo; a abertura (`abertura.rgba`) e o ícone idênticos byte a byte aos antigos; a versão subiu para 0.2.27 nos três arquivos e no `versao_switch.txt` do jogo |
| A geração falha no meio (exportação com erro) | **passou**: os arquivos de versão voltam ao que eram; a pasta de entrega anterior fica intacta |
| Editor do Godot inexistente | **passou**: para antes de mexer em qualquer arquivo |
| Rede (D-18, 2026-10-22): `python gsn.py motor todos` com o `03-rede-horizon.patch`, ENet e mbedTLS | **passou**: `godot-opengl.elf` 55,3 MB (+0,73), `godot-vulkan.elf` 70,7 MB (+0,81); `ENetMultiplayerPeer`, `ENetConnection`, `CryptoMbedTLS` e a linha `[switch] rede:` presentes nos dois |

## Rede (D-18): plano do teste no console

O que provar com o modo online do Batida Capixaba (P2P: DHT do BitTorrent para o encontro, furo de NAT
em UDP, depois ENet na mesma porta). Cada passo nas duas variantes (`<jogo>.nro` OpenGL e `<jogo>.vulkan`);
depois de cada um, guardar `<jogo>.log` / `<jogo>.vulkan.log` e `<jogo>.saida.txt` /
`<jogo>.vulkan.saida.txt`, e o relatório mais novo de `sdmc:/atmosphere/crash_reports/` se cair.

1. **Abertura com Wi-Fi conectado.** No log, logo depois da linha `[switch] jogo ...`:
   `[switch] rede: 192.168.x.y/24 (Wi-Fi, N barras, internet ok)`. Não devem aparecer
   `Parameter "_create" is null` (`core/io/ip.cpp`), `Parameter "method" is null` (`variant_call.cpp`)
   nem `Crypto is not available`. O jogo mostra o modo online disponível (ele confere
   `ClassDB.can_instantiate("ENetMultiplayerPeer")`).
2. **Abertura sem rede** (modo avião): `[switch] rede: sem conexão (...)`; o jogo abre igual e, ao
   tentar o online, diz "sem rede" sem travar nem cair.
3. **Mesma rede, Switch convidado:** o PC abre a sala; o Switch entra pelo código. Prova o broadcast
   (`255.255.255.255` e o `/24` montado do `IP.get_local_addresses`) e o ENet (`create_client` com a
   porta local). Jogar uma partida até o fim.
4. **Mesma rede, Switch anfitrião:** o Switch abre a sala (porta 7480–7495 em `0.0.0.0`), o PC entra.
   Prova o `bind` nas portas fixas, o `create_server` e o `socket_send` (o pacote cru do anfitrião).
5. **Pela internet:** o PC noutra rede (ex.: o roteador do celular). Prova o DNS dos nós de partida
   (`router.bittorrent.com`, `dht.transmissionbt.com`, `router.utorrent.com`, pela fila do `IP` em outra
   thread), a DHT e o furo de NAT. Se não achar a sala, anotar se o PC (com o mesmo código, PC ↔ PC)
   acha: separa o que é do port do que é do roteador.
6. **Sair no meio:** com a partida online aberta, fechar pelo HOME → fechar o software; e, noutra vez,
   sair pelo menu do jogo. O `<jogo>.saida.txt` deve dizer "tudo solto" (0 threads vivas: o
   resolvedor de nomes do `IP` foi esperado; nenhum trecho do heap preso: os sockets e o nifm fechados).
   Abrir o jogo de novo em seguida (o `.nro` passa a vez ao `.vulkan`: o 2001-0106 de D-14 apareceria
   aqui).
7. **Trocar de rede com o jogo aberto** (desligar o Wi-Fi do roteador no meio da sala): o jogo deve
   cair com o motivo dele ("a conexão caiu"), sem travar.

Linhas a procurar se falhar: `Unable to change non-block mode` ou `Unable to change broadcast setting`
(o socket da libnx recusou a opção), `Failed to bind socket` (só com `<jogo>.verbose`), `getaddrinfo
failed` (só com `<jogo>.verbose`: o DNS), e o pulso do log parado (alguma chamada de rede bloqueou a
thread principal).

## Ainda não testado no console

- O empacotamento genérico deste repositório (`.nro` montado pelo `gsn.py`, abertura e versão no
  romfs, nomes tirados do executável). O motor é o mesmo da 0.2.26 do Batida, mas os arquivos são
  montados de outro jeito: o primeiro teste de cada jogo no console vale como prova.
- O `exemplo/` com o som gerado no código.
- O `.pck` dentro do `.nro` (`embutir_pck = sim`).
- A rede (D-18): o plano acima.
