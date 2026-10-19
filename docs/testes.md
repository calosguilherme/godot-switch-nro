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

## Ainda não testado no console

- O empacotamento genérico deste repositório (`.nro` montado pelo `gsn.py`, abertura e versão no
  romfs, nomes tirados do executável). O motor é o mesmo da 0.2.26 do Batida, mas os arquivos são
  montados de outro jeito: o primeiro teste de cada jogo no console vale como prova.
- O `exemplo/` com o som gerado no código.
- O `.pck` dentro do `.nro` (`embutir_pck = sim`).
