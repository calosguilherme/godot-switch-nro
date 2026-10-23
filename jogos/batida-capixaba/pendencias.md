# Pendências do Batida no Switch

As do port (valem para qualquer jogo) estão em `docs/pendencias.md`.

## Para provar no console

1. **A primeira entrega pelo `gsn.py`** (0.2.27): mesmo motor da 0.2.26, montado pelo empacotamento
   novo. Conferir: abre no hbmenu com o ícone e o nome de sempre; a abertura do tambor aparece no
   OpenGL; o Modo DEV mostra ".nro 0.2.27 · .pck 0.2.27"; a troca OpenGL ↔ Vulkan funciona.
2. **0.2.26:** a cortina de carregamento no console (tambor batendo até a mata montar, saindo
   esmaecida); o log sem linhas cortadas; `batida.vulkan.saida.txt` terminando em "tudo solto".
3. O ganho dos menus no console (as otimizações de menu nunca foram medidas lá).
4. **0.2.28** (2026-10-22): mesmo motor da 0.2.27, `.pck` novo com as mudanças do jogo ainda sem commit
   no repositório dele (online P2P, povo em VAT com silhuetas, menu novo). O `.pck` passou de ~428 MB
   para ~712 MB (679 MiB): ver se a abertura e os carregamentos ficaram mais lentos.
5. **0.2.29** (motor 0.3.1: GPU e desenho na CPU no pulso; o jogo igual ao da 0.2.28, 6d450e6). Guardada
   como reserva; passou para o cartão e foi substituída pela 0.2.30 antes de rodar.
6. **0.2.30** (motor 0.3.2: rede D-18, GPU no pulso, `PRIORITY_HIGH`; jogo da cópia de trabalho sem commit
   sobre 6d450e6: montagem do cenário em etapas com a cortina (DEC-206), menu guardado e trecho em
   segundo plano (DEC-207), anexos dos esqueletos (DEC-210), Completo por padrão no Switch (DEC-211),
   `SegundoPlano.soltar_tudo` na saída (DEC-212)). Conferir: clique → cortina sem travar e tambor
   batendo; menu do Jogo rápido; a volta ao menu sem os erros do `Skeleton3D`; `[jogo] saída: ... 0 no
   ar` e `tudo solto`; a linha `[switch] rede:` e sem os erros do `IP`; o Online Switch ↔ PC (o PC da
   mesma cópia, pelo editor ou pelo `Godot_v4.7.2-stable_win64.exe`, não o `_console.exe`, que o
   firewall bloqueia); o Ato 7 e a guarda com a GPU no pulso.
   **Resultado (log da última sessão; a reabertura apagou o da sessão de teste):** nenhum erro (IP,
   `"method" is null`, `Skeleton3D`), rede ok, "tudo solto". GPU 17–21 ms: o limite é a CPU. Clique →
   cortina: o maior quadro 1,66 s (antes 7–13 s). Menu 26–36 FPS (antes 13). Mata: 20 FPS nos primeiros
   ~40 s (`_draw` 24 ms), depois 30. Guarda 24–27 FPS (a roda 17 ms; o degrau menor). **Bug:** pausar
   durante a cortina trava o carregamento até despausar (pedido ao agente do jogo). **Online Switch ↔ PC:
   funcionou** (relato do autor). **Jogo rápido, em parte:** enquanto a thread descomprime as músicas, o
   menu fica um pouco lento e o trecho não toca; quando termina, o FPS volta, mas demora. Ato 3 e Ato 7
   não foram jogados.
9. **0.2.34** (motor 0.3.5: a espera pelas cercas e o envio do Vulkan no pulso; jogo = commit b92ef4a,
   exportado da cópia de trabalho antes do commit, sem mudança depois: notas e músicas prontas no conteúdo (DEC-215), 30 FPS fixos no Switch
   (DEC-216), a festa mais leve na placa (DEC-217: o povo econômico ligado no Switch, MSAA 2×; `--povo_cheio`
   em `user://args.txt` desliga a economia), a montagem com 25 ms por quadro e o log por bloco (DEC-218)).
   No cartão, conferida por hash. A 0.2.33 (DEC-215/216) foi gerada e não chegou ao cartão.
   **Resultado:** "tudo bem melhor" (relato). Ato 3 e Ato 7 a 30 FPS (GPU 13 e 18 ms); sem turbo, o pior
   quadro do segundo bate 28–35 ms e dá a travadinha 30 → 29 (no Ato 3 e no Ato 5); com turbo, 18–22 ms e
   nenhuma queda. A série placa/CPU descartada (cercas 0,0–0,2 ms). Cortina: blocos ≤ 125 ms, mas os
   quadros com a cortina ficam em ~67 ms mesmo sem montagem e com os scripts em ~1 ms (a investigar: as
   leituras em thread disputando o .pck, ou envios à placa). Abertura: a rua montada num quadro (3,3 s).
7. **0.2.31:** gerada (motor 0.3.3, jogo sem commit com a roda, a pausa e o trecho pronto), mas a cópia
   para o cartão falhou (o DBI travou) e foi substituída pela 0.2.32 sem rodar.
8. **0.2.32** (motor 0.3.3: `fsync` no fim do log e o `<jogo>.anterior.log`; jogo do commit 4ee627d:
   DEC-206 a 214, os itens 1–5, a pausa no carregamento e o trecho pronto por música). No cartão,
   conferida por hash. Conferir: o Jogo rápido (o trecho toca logo, sem o menu lento), a pausa ignorada
   na cortina, a roda (o palco no `[jogo]`), a guarda do Ato 3 e o Ato 7 com a GPU no pulso.
   **Resultado:** Jogo rápido: trava ao entrar na lista e fica a 12–20 FPS sem som por ~25 s (as notas de
   dificuldade calculadas no console; `_draw` 36–40 ms até elas chegarem), depois 36 FPS. Ato 3: 31–35
   FPS (a roda 6–7 ms, antes 17), oscilando: pedido `Engine.max_fps = 30` no Switch. Ato 7: 22 FPS,
   GPU 27,7 ms, quadro 46 ms ≈ os scripts do jogo (~20 ms) + a GPU: a placa e a CPU parecem em série
   no Vulkan do port (a leitura de "o motor animando a cena" estava errada: o censo do jogo, DEC-217, achou
   0,1 ms de esqueletos; o "processo" do pulso é o pior quadro do segundo, com o desenho). Pedidos ao
   agente do jogo: notas prontas no conteúdo (DEC-215), 30 FPS fixos no Switch (DEC-216), aliviar a GPU
   da festa (DEC-217: o povo econômico ligado no Switch e MSAA 2×). No port 0.3.5: a espera pelas cercas e
   o envio do Vulkan no pulso, para provar a série.

## Relatos da 0.2.28 (2026-10-22, no console, ainda sem log)

Para fechar os quatro de uma vez: o arquivo vazio `diagnostico` ao lado do `batida.nro`; Jogo rápido,
uns 10 s na lista, uma música do Ato 3 até passar algumas quedas, uma do Ato 7; sair pelo menu. Pedir
`batida.log` (e `batida.vulkan.log`, o mesmo trajeto no outro vídeo), `batida.saida.txt`, se foi na base
ou no portátil e com o turbo ou sem. No log: `[switch] quadro de N ms` (os quadros longos),
`[switch] pulso` (FPS e espera da GPU: GPU ou CPU) e `[jogo] ...` (o custo de cada parte a cada 5 s).
Perguntar também se já era assim na 0.2.27.

1. **O menu do Jogo rápido a ~13 FPS.** Suspeita: o desenho do menu novo (`hud/desenho_do_menu.gd`,
   `_desenhar_em_colunas`, o estandarte largo) ou o 3D por trás dele; o `_draw` do HUD no `[jogo]`
   separa. À parte: a cada música selecionada, `Som.tocar_trecho` (`palco/som.gd`) carrega todas as
   faixas dela com `load()` na hora (base, canto e os quatro instrumentos, ~4 MB de MP3 do `.pck` no
   cartão): engasgo ao andar pela lista, não os 13 FPS constantes.
2. **Escolher a música trava o menu antes do tambor.** `TelaDaPartida.comecar()`
   (`telas/tela_da_partida.gd`) faz tudo no quadro do clique: monta os jogadores, `Som.comecar` carrega
   as faixas (`palco/faixas.gd`, um `load()` por faixa) e só no fim liga `preparando`, que pede a cortina.
   O menu fica parado na tela até esse quadro acabar. Caminho provável (no jogo): pôr a cortina antes e
   carregar as faixas em segundo plano (`ResourceLoader.load_threaded_request`).
3. **O tambor da cortina travando.** A cortina é desenhada pelo jogo, na mesma thread que monta o
   cenário (agora com o povo em VAT): o que a montagem fizer de uma vez congela o tambor. Também os
   shaders que compilam nos primeiros quadros (no OpenGL a cada abertura). Medir a duração dos quadros
   longos entre o clique e a cortina sair.
4. **A guarda do Ato 3: quedas de ~28 para ~24 FPS, abruptas, com um engasgo.** A estrada
   (`palco/guarda.gd`) recicla trechos; cada um tem as beiras da mata, da roça e da vila já montadas e
   escondidas, e `_trecho_reposto` mostra a da parte do caminho em que a procissão está. Duas
   suspeitas: (a) shader compilando quando uma parte aparece pela primeira vez (engasgo só na primeira
   vez; no Vulkan some na segunda abertura); (b) degrau de custo, porque a roça e a vila têm mais
   objetos que a mata (a queda volta a cada trecho que troca, com a espera da GPU subindo no pulso).
5. **O Ato 7 inteiro a ~20 FPS.** A festa (`palco/festa.gd`): a rua cheia de gente nas duas calçadas,
   pulando no ritmo, mais o povo da procissão (`palco/multidao.gd`, VAT, malha leve ao fundo; no Switch
   uma fileira só e mais rala). Os dois arquivos mudaram nesta versão. Suspeita: o povo (quantidade e
   a animação por vértice) e o dia claro com o casario inteiro; o pulso diz se é GPU ou CPU.

### O primeiro log (0.2.28, Vulkan, portátil sem carregador, turbo quase todo desligado)

**Gráficos no Completo** (o save do console: `"graphics":"completo"`, `"resolution":"nativa"`,
`"prioridade":"fluidez"`), não no Leve, que era o padrão no Switch: os números abaixo são do Completo.
Decisão do criador depois disso (DEC-211 do jogo): o Switch passa a começar no Completo (não conta mais
como pouca memória em `Jogo.pouca_memoria`), e o alvo das correções é o Completo rodar bem no console; as
reduções só do Switch (folha leve etc.) continuam.

Com o cache de shaders vazio (0 arquivos na abertura): os quadros longos estão inflados pela compilação.
O log colado veio cortado (não chegou ao Ato 3 nem ao Ato 7).

- **O menu do Jogo rápido (1) é a CPU, e o maior pedaço é o HUD.** 13 FPS, quadro de ~77–81 ms: `_draw`
  do HUD 34–38 ms, estado 12,5 ms, palco 10,3 ms (a roda 8,8), o resto (~20 ms) o motor desenhando 463
  chamadas e ~1220 objetos. Em outro menu, no mesmo log, 28–29 FPS: `_draw` ~15 ms, estado 2,6 ms, 235
  chamadas e ~590 objetos. Picos de estado de 30–87 ms (pior script do segundo até 437 ms) ao andar pela
  lista: o trecho da música carregando as faixas.
- **O clique (2) e o tambor (3) são o cenário montado num quadro só.** No quadro do clique,
  `principal.gd:_process` → `cenarios.usar(qual)` (`palco/cenarios.gd`) faz `Palco3D.new(...)` +
  `add_child` do cenário do ato inteiro, na parte medida como "estado": `play: 8 quadros em 5 s;
  estado 598,3 ms`, depois `quadro de 7267 ms`. Na mata: `quadro de 8177 ms` e `quadro de 5550 ms` (o
  malloc de 859 para 1124 MiB). A cortina não aparece porque o próprio quadro que a pediria monta o
  cenário. E `usar()` solta o cenário do ato ao voltar ao menu: cada música monta tudo de novo.
- **Na partida, 29–33 FPS travados na CPU:** o palco 19 ms por quadro, quase todo da roda (15–16 ms).
- **Ao sair de cada partida, ~40 `ERROR: Attempt to disconnect a nonexistent connection from
  'Skeleton3D' ... 'skeleton_updated'`** (os bonecos com esqueleto soltos com o cenário). No Switch cada
  linha de erro vai ao log no cartão; junto com o desmontar, `quadro de 3107 ms` / `2624 ms` na volta ao menu.
- **Erros conhecidos, não são deste problema:** `ip.cpp:336 "_create" is null` (o port não tem rede:
  o **online P2P não funciona no Switch**, `docs/pendencias.md` 2); `"method" is null` em
  `variant_call.cpp` (16 linhas): **é o `IP` nulo** (sem rede, `Engine::add_singleton("IP", nullptr)`),
  e o GDScript compila `IP.get_local_addresses()` (`nucleo/rede.gd:959, 1089`) e as 4 chamadas do
  `nucleo/dht.gd` (75, 210, 216, 221) como métodos de um OBJECT nulo. Hoje é só ruído, porque tudo fica
  atrás do `Rede.disponivel()`; com a rede no port, some. O online também usa
  `Crypto.new().generate_random_bytes` (`dht.gd:63`, `encontro.gd:29, 76`), que precisa do mbedTLS ou de
  um substituto no port.
- **O log inteiro (copiado do cartão por MTP):** a partida que começa com `quadro de 13293 ms` é, pelo
  custo do cenário (1,9 ms) e pela queda, a guarda do Ato 3 (o log não diz o cenário; conferir).
  **Ato 3 (4):** 27–29 FPS (quadro ~35 ms; script ~28 ms: palco 19,7 com a roda 15, `_draw` ~8); aos
  ~560 s cai de uma vez para 24–25 FPS (quadro 41 ms) e fica: 236 → 268 chamadas, 406 → 507 objetos e
  o `_draw` do HUD de ~8 para 13–14 ms. Degrau de custo (as beiras da parte nova do caminho e algo a
  mais no HUD), não shader. **Ato 7 provável (5)** (a última partida, cenário 0,2 ms): 21 FPS, quadro
  46,5 ms, mas o script soma só ~22 ms (palco 15, a roda 12,5, `_draw` 6): **metade do quadro está
  fora do GDScript** (GPU ou o motor animando: esqueletos, VAT), e no Vulkan o pulso não separa.
  208 linhas de erro `skeleton_updated` no log inteiro. Malloc no pico: 1417 MiB.
- **Port:** a "espera da GPU" do pulso só é medida no OpenGL (`DisplayServerSwitch::troca_us`, na troca
  do EGL); no Vulkan sai sempre 0,0 ms. Desde o port 0.3.1 o pulso mostra também "GPU" e "desenho na
  CPU" (timestamps do Godot no viewport raiz, D-15): o Ato 7 se resolve com um log do Vulkan.

## Desempenho

- No Vulkan, o desafio do machado fora da base está em 30–31 FPS (32 ms). No OpenGL, ~24 FPS (40 ms).
- Medidas e não ligadas (OpenGL): 3D a 60% (~34 ms), metade das árvores (~36 ms), 3D a ~70% + um terço
  a menos de copas (estimado ~33 ms); a floresta sem as árvores escondidas rende menos de 1 ms.
- **Ideia do criador, guardada:** trocar o fundo distante da mata por uma imagem pronta, igual ao que o
  3D desenha. Só se render ganho e não houver outro caminho: o peso está nas copas perto da câmera, que
  uma imagem não substitui sem mudar o visual.

## Experiência

- A primeira abertura de todas em Vulkan fica com a tela preta enquanto compila os shaders (a cortina
  do jogo não alcança essa fase). Depende da pendência 1 de `docs/pendencias.md`.
- Na primeira vez que um modelo ou efeito aparece, o Switch compila os shaders dele (no Vulkan uma vez
  só; no OpenGL a cada abertura, porque lá não há cache).
