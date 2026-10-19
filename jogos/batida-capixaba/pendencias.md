# Pendências do Batida no Switch

As do port (valem para qualquer jogo) estão em `docs/pendencias.md`.

## Para provar no console

1. **A primeira entrega pelo `gsn.py`** (0.2.27): mesmo motor da 0.2.26, montado pelo empacotamento
   novo. Conferir: abre no hbmenu com o ícone e o nome de sempre; a abertura do tambor aparece no
   OpenGL; o Modo DEV mostra ".nro 0.2.27 · .pck 0.2.27"; a troca OpenGL ↔ Vulkan funciona.
2. **0.2.26:** a cortina de carregamento no console (tambor batendo até a mata montar, saindo
   esmaecida); o log sem linhas cortadas; `batida.vulkan.saida.txt` terminando em "tudo solto".
3. O ganho dos menus no console (as otimizações de menu nunca foram medidas lá).

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
