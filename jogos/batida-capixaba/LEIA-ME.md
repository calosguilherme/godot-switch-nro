# Batida Capixaba no Switch

A configuração que gera a entrega do Batida Capixaba para o Switch homebrew, a mesma que o jogo usava
quando o port vivia dentro dele (versão 0.2.26 em diante).

## Gerar uma entrega

Com o repositório do jogo ao lado deste (`Desktop/batidacapixaba` e `Desktop/godot-switch-nro`):

```sh
python gsn.py gerar ../batidacapixaba/games/batida-capixaba-godot
```

O `gsn.py` acha esta configuração pelo nome do projeto (`projeto_nome = Batida Capixaba`) e:

1. sobe a versão em `VERSAO` (0.2.26 → 0.2.27...) e grava a mesma em `res://versao_switch.txt` do jogo;
2. compila o motor (só o que mudou) e exporta o `.pck` com o preset "Switch" do jogo;
3. monta `saida/batida/switch/batida/` com `batida.nro`, `batida.vulkan` e `batida.pck`.

No console: copiar `saida/batida/switch/` para a raiz do cartão (`sdmc:/switch/batida/`) e **trocar os
três arquivos juntos**. No Modo DEV o jogo mostra ".nro X · .pck Y" e "≠" se as versões não batem.

O editor do Godot é o de `godot` no `jogo.cfg` (`~/Desktop/Godot_v4.7.2-stable_win64.exe/...`); em
outra máquina, use `--godot`.

## O que tem aqui

| Arquivo | O quê |
| --- | --- |
| `jogo.cfg` | a configuração (título, autor, nome dos arquivos, versão, ícone, abertura, preset) |
| `VERSAO` | a versão da última entrega (sobe a cada `gerar`) |
| `icone.jpg` | o ícone do hbmenu (o tambor) |
| `abertura.py` | redesenha `abertura_switch.png` no jogo a partir do ícone (rode depois de mudar o ícone) |
| `historico.md` | tudo o que foi feito e medido no Switch (a antiga DEC-102 do devlog do jogo) |
| `pendencias.md` | o que falta do Batida no Switch |
| `comparacao/` | capturas das comparações de gráfico citadas no histórico |

## Premissa do criador para mudanças por causa do Switch

1. O que **piora o gráfico ou algo do gênero** só vale quando o jogo detecta que está no Switch
   (`OS.has_feature("switch")` no GDScript; nos shaders, `CURRENT_RENDERER == RENDERER_COMPATIBILITY`
   para o que só o OpenGL precisa). PC, celular e web continuam como estão.
2. Fora isso, só entram **melhorias de desempenho gerais**, sem mudar gráfico nem jogabilidade em
   nenhuma versão. Cada mudança é conferida no PC comparando capturas antes e depois.

## O que no jogo existe por causa do Switch

Fica no repositório do jogo (é código do jogo), listado aqui para quem mexe no port saber onde olhar:

- `project.godot`: `rendering_method.switch="gl_compatibility"`, `boot_splash/image.switch` e
  `bg_color.switch`; `export_presets.cfg`: o preset "Switch" (só o `.pck`).
- `abertura_switch.png`, `versao_switch.txt` (gravado pelo `gsn.py`), `fontes/` + `hud/fontes_switch.gd`
  (Noto Sans e recortes, CJK das fontes do console).
- `palco/shaders/cor.gdshaderinc` (as cores no Compatibility), `toon_dupla.gdshader`,
  `toon_folha_leve.gdshader` e `Materiais.folha_leve` (folhas mais leves no Switch).
- `palco/principal.gd`: a escala do 3D na mata fora da base (`_escala_no_switch`), o turbo, a troca de
  vídeo (`_trocar_video_do_switch`, `_conferir_vulkan_no_switch`), a cortina de carregamento e os custos
  `[jogo]` no log.
- `telas/tela_do_menu.gd`: "Vídeo do Switch" e "Modo turbo" na Configuração; `nucleo/preferencias.gd`:
  `video` (padrão "vulkan") e `turbo`.
- `palco/diagnostico.gd` (o arquivo `diagnostico` ao lado do `.nro`) e as conferências `custo-*`,
  `floresta-visivel` e `cortina` (`ferramentas/conferencia.gd`).
