# godot-switch-nro

**Leve um jogo do Godot 4.7.2 para o Nintendo Switch homebrew.** Você aponta a pasta do projeto e sai a
pasta do cartão SD, pronta para abrir pelo hbmenu no Atmosphère:

```text
python gsn.py gerar ../meu-jogo
  → saida/meu-jogo/switch/meu-jogo/meu-jogo.nro      (OpenGL, aparece no hbmenu)
                                    meu-jogo.vulkan   (Vulkan, o padrão; o .nro passa a vez a ele)
                                    meu-jogo.pck      (o jogo)
```

[English](README.md) · [Decisões](docs/decisoes.md) · [Testes](docs/testes.md) ·
[Pendências](docs/pendencias.md)

## Para que serve

Não existe Godot 4 oficial (nem não oficial) para o Switch homebrew: o último port conhecido parou no
Godot 3.5. Este repositório é uma plataforma `switch` para o Godot 4.7.2, escrita para o jogo
[Batida Capixaba](#de-onde-veio) e separada dele para servir a qualquer projeto.

- **Só homebrew** (`.nro` no Atmosphère, com o hbmenu). Não é para lojas nem para consoles sem
  desbloqueio.
- **Projetos em GDScript**, 2D ou 3D. O motor é o Godot inteiro; o que fica de fora está em
  [Limites](#limites).
- **Dois vídeos:** Vulkan (o NVK do Mesa 26, renderizador Mobile: o mesmo visual do PC e do celular, o
  padrão) e OpenGL ES 3.2 (Mesa 20.1, renderizador Compatibility: a reserva).

## Como funciona

```text
            uma vez                               a cada versão do jogo
 ┌────────────────────────────┐        ┌──────────────────────────────────────────┐
 │ gsn.py preparar            │        │ gsn.py gerar <projeto>                   │
 │  Godot 4.7.2 + remendos    │        │  1. lê o project.godot (+ configuração)  │
 │  imagem Docker devkitPro   │        │  2. exporta o .pck (editor do Godot)     │
 │  NVK do mesa-switch        │        │  3. ícone 256×256 e abertura 1280×720    │
 └─────────────┬──────────────┘        │  4. compila o motor (só o que mudou)     │
               ▼                       │  5. elf2nro: motor + NACP + romfs        │
 ┌────────────────────────────┐        │  6. monta a pasta do cartão              │
 │ gsn.py motor               │──ELF──▶└──────────────────────────────────────────┘
 │  godot-opengl.elf          │
 │  godot-vulkan.elf          │
 └────────────────────────────┘
```

- **O motor** (`platform/switch/`) é compilado uma vez por variante e não sabe nada do jogo. Os nomes de
  arquivo vêm do próprio executável: o `meu-jogo.nro` procura `meu-jogo.pck` e `meu-jogo.vulkan` na
  mesma pasta.
- **O empacotamento** (`gsn.py` + `scripts/empacotar.sh`) põe o título, o autor, a versão e o ícone no
  `.nro` (NACP) e, no romfs dele, a imagem de abertura (`abertura.rgba`), a versão (`versao.txt`) e,
  se você pedir, o próprio `.pck` (`jogo.pck`).
- **Na abertura**, o `meu-jogo.nro` lê `dados/renderizador.txt`. Sem arquivo ou com `vulkan`, passa a
  vez ao `meu-jogo.vulkan` pelo carregador do homebrew. Se o Vulkan não chegar a rodar 300 quadros (caiu,
  travou ou foi fechado), a abertura seguinte volta sozinha ao OpenGL.
- Os scripts de compilação rodam no Docker (a imagem oficial do devkitPro). No Windows, pelo Ubuntu do
  WSL; no Linux, direto.

## Passo a passo

### 1. O que instalar (uma vez)

| O quê | Para quê |
| --- | --- |
| **Windows 10/11 com WSL e Ubuntu** (ou um Linux) | onde o motor é compilado |
| **Docker** no Ubuntu: `sudo apt install docker.io` | a imagem do devkitPro; no WSL, os scripts rodam como root e sobem o `dockerd` sozinhos |
| **Python 3** com o **Pillow** (`python -m pip install pillow`) no sistema onde você roda o `gsn.py` | o ícone e a abertura |
| **O editor do Godot 4.7.2** (a mesma versão!) | exportar o `.pck` |
| **Git** | baixar o Godot e o mesa-switch |
| ~25 GB livres no Linux/WSL | o código do Godot (duas árvores), o mesa-switch e as imagens Docker |

No Windows, rode os comandos no PowerShell ou no Git Bash, na pasta deste repositório. A distribuição
do WSL padrão é `Ubuntu` (outra: variável `GSN_WSL`). O trabalho pesado fica em
`~/godot-switch-nro-trabalho` dentro do Linux (outra pasta: variável `GSN_TRABALHO`).

### 2. Preparar (uma vez, ~1 h)

```sh
python gsn.py preparar
```

Baixa o Godot 4.7.2, aplica os remendos de `patches/godot/`, monta a imagem Docker e compila o NVK do
[mesa-switch](https://github.com/danfromtico/mesa-switch) no commit testado (a parte demorada: 30–60
min e uma imagem de ~7 GB). Sem Vulkan: `python gsn.py preparar --sem-vulkan` (só o `.nro` OpenGL).

### 3. Gerar o jogo

```sh
python gsn.py info ../meu-jogo      # confere o que vai sair (título, ícone, abertura, preset...)
python gsn.py gerar ../meu-jogo --godot "C:/caminho/Godot_v4.7.2-stable_win64_console.exe"
```

Na primeira vez o motor compila do zero (~9 min por variante); depois, só o que mudou. Sai
`saida/meu-jogo/switch/meu-jogo/`. Opções: `--sem-vulkan`, `--sem-compilar` (usa o motor já
compilado), `--saida <pasta>`, `--config <arquivo>`.

### 4. Pôr no Switch

1. Copie a pasta `saida/meu-jogo/switch/` para a raiz do cartão (fica `sdmc:/switch/meu-jogo/`).
   **Troque sempre todos os arquivos juntos**: o `.nro`, o `.vulkan` e o `.pck` de versões diferentes
   não combinam.
2. Abra o hbmenu **segurando R ao abrir um jogo** (o modo título; pelo álbum o homebrew tem só ~400 MB).
3. Na primeira abertura em Vulkan a tela fica preta enquanto os shaders compilam (num jogo 3D grande,
   minutos). Não feche: da segunda vez em diante é rápido.

### 5. Testar e mandar o log

Ao lado do `.nro` aparecem `meu-jogo.log` (ou `meu-jogo.vulkan.log`), `meu-jogo.saida.txt` (a
conferência da saída: "tudo solto" é o esperado) e, no Vulkan, `vulkan_etapas.txt`. Se o console mostrar
a tela de erro, o relatório fica em `sdmc:/atmosphere/crash_reports/`. Com o `nxlink -s meu-jogo.nro`
no PC (mesma rede, o console no hbmenu), o log aparece no terminal.

## A configuração do jogo

Sem configuração nenhuma, tudo sai do `project.godot`:

| Chave (`[jogo]`) | Padrão | O quê |
| --- | --- | --- |
| `titulo` | `application/config/name` | o nome no hbmenu |
| `autor` | `Godot Engine (homebrew)` | o autor no hbmenu |
| `arquivo` | o título sem acentos, minúsculo, com hífens | o nome dos arquivos e da pasta no cartão |
| `versao` | `application/config/version` ou `1.0.0` | a versão (NACP, `romfs:/versao.txt`) |
| `versao_arquivo` | — | um arquivo com a versão (ganha da `versao`) |
| `subir_versao` | `não` | `sim`: soma 1 ao último número a cada `gerar` (e grava no `versao_arquivo`) |
| `escrever_versao_em` | — | ex.: `res://versao.txt`: grava a versão no projeto antes de exportar, para o jogo mostrá-la |
| `icone` | `application/config/icon` | PNG/JPG; vira JPG 256×256 |
| `abertura` | `boot_splash/image.switch` ou `boot_splash/image` | a imagem desenhada antes do Godot; cabe inteira em 1280×720 |
| `cor_de_fundo` | `boot_splash/bg_color(.switch)` | `#rrggbb`; o fundo da abertura e do ícone |
| `preset` | `Switch` | o preset de exportação; sem ele, vai um Linux arm64 temporário (o `export_presets.cfg` volta ao que era) |
| `vulkan` | `sim` | `não`: só o `.nro` OpenGL |
| `embutir_pck` | `não` | `sim`: o `.pck` vai dentro do `.nro` (um arquivo só no cartão) |
| `godot` | variável `GODOT` ou `godot` no PATH | o editor do Godot 4.7.2 |
| `projeto_nome` | — | só em `jogos/*/jogo.cfg`: o `application/config/name` do projeto a que a configuração serve |

A configuração é procurada em: `--config`; `godot-switch-nro.cfg` na raiz do projeto;
`jogos/<algum>/jogo.cfg` neste repositório com o `projeto_nome` igual ao nome do projeto (assim o jogo
não precisa ter nada do Switch dentro dele). Caminhos relativos são relativos ao arquivo de
configuração; `res://` é a pasta do projeto. Exemplo:

```ini
[jogo]
projeto_nome = Meu Jogo
titulo = Meu Jogo
autor = Meu Estúdio
arquivo = meujogo
versao_arquivo = VERSAO
subir_versao = sim
icone = icone.jpg
```

## Para o jogo

O jogo roda sem mudar nada. Para aproveitar o console:

- **`OS.has_feature("switch")`** é verdadeiro no Switch (também `nintendo_switch`). Use para o que só
  vale lá. Recomendação: o que **reduz** gráfico fica atrás desse teste; PC e celular continuam como
  estão.
- **`user://`** fica em `sdmc:/switch/<jogo>/dados/`.
- **O singleton `Switch`** (só existe no Switch):

  ```gdscript
  if Engine.has_singleton("Switch"):
      var switch := Engine.get_singleton("Switch")
      switch.definir_turbo(true)
  ```

  | Método | O quê |
  | --- | --- |
  | `versao()` | a versão do jogo neste `.nro` (`romfs:/versao.txt`) |
  | `versao_do_motor()` | a versão do godot-switch-nro |
  | `renderizador()` | `"opengl"` ou `"vulkan"` |
  | `tem_vulkan()` | o `<jogo>.vulkan` está na pasta |
  | `reabrir()` | pede ao carregador para abrir o `<jogo>.nro` quando o jogo fechar (aplica a troca de vídeo); feche o jogo logo depois (`get_tree().quit()`) |
  | `modelo()` | `"erista"` (2017) ou `"mariko"` (V2, Lite, OLED) |
  | `na_base()`, `carregando()` | o console está na TV / na tomada |
  | `definir_turbo(bool)`, `estado_do_turbo()` | o processador a 1785 MHz ("desligado", "ligado" ou "esperando": o Erista só aceita na base ou carregando) |
  | `fonte_do_sistema(tipo)` | os bytes do TTF compartilhado (0 padrão/japonês, 1 chinês simplificado, 2 estendido, 3 tradicional, 4 coreano, 5 símbolos Nintendo) |

- **Trocar de vídeo:** grave `opengl` ou `vulkan` em `user://renderizador.txt` e chame `reabrir()`.
  Se existir `user://vulkan_falhou`, a última tentativa com Vulkan não chegou a rodar e o jogo voltou ao
  OpenGL: avise o jogador e apague o arquivo.
- **O HOME** congela o processo sem aviso; na volta o jogo recebe `NOTIFICATION_APPLICATION_FOCUS_OUT` e
  `NOTIFICATION_APPLICATION_PAUSED`: pause aí.
- **Desempenho:** o processador é o gargalo (~10–15× mais lento que um PC para o mesmo script), e cada
  chamada de desenho custa caro no driver. Agrupe (MultiMesh), evite alocar por quadro e meça no console.
- **Cores no OpenGL:** no Compatibility o `ALBEDO` é tratado como sRGB; shaders próprios que fazem a
  conta em linear saem mais escuros no `.nro` OpenGL (no Vulkan, não).
- **Arquivos ao lado do `.nro`** para diagnóstico: `<jogo>.verbose` (o Godot em modo detalhado),
  `mesa_env.txt` (linhas `NOME=valor` viram variáveis do driver), `render_separado` (desenho em outra
  thread, experimental).

## Versões

| Componente | Versão testada |
| --- | --- |
| Godot | 4.7.2-stable (com `patches/godot/`) |
| devkitPro (imagem `devkitpro/devkita64`) | resumo `sha256:1fc388c3a0d3…` |
| devkitA64 (GCC) | r29.2 (GCC 15.2.0) |
| libnx | 4.12.0 |
| switch-mesa (OpenGL) | 20.1.0 (Mesa 20.1.0-rc3, nouveau, GLES 3.2) |
| switch-tools (`elf2nro`, `nacptool`) | 1.13.1 |
| mesa-switch (Vulkan, NVK) | commit `0ff52079` (Mesa 26.2.4), Vulkan 1.4.354 |
| Rust (para o NAK do NVK) | 1.101.0-nightly (2026-10-18) |
| Meson | 1.12.1 |
| godot-switch-nro | ver `VERSION` |

## Testes feitos

Resumo (detalhes em [docs/testes.md](docs/testes.md)): o motor rodou no console um jogo 3D completo
(o Batida Capixaba) nos dois vídeos: 3D, som em faixas sincronizadas, Joy-Con, toque, vibração, idioma e
fontes do sistema, HOME, fechar pelo sistema, troca OpenGL ↔ Vulkan com volta automática. No Vulkan,
~25% mais quadros que no OpenGL. O empacotamento genérico deste repositório ainda espera o primeiro teste
no console.

## Limites

Sem rede, sem C#, sem GDExtension, sem teclado virtual, até dois controles, alguns módulos desligados
(CSG, vídeo Theora, Jolt, escrita complexa), primeira abertura em Vulkan com tela preta. Lista e
motivos em [docs/pendencias.md](docs/pendencias.md).

## O que tem aqui

| Caminho | O quê |
| --- | --- |
| `gsn.py` | a ferramenta: `preparar`, `motor`, `gerar`, `info` |
| `platform/switch/` | a plataforma do Godot (C++): vídeo (EGL e Vulkan), som (audren), controles, toque, o singleton `Switch` |
| `patches/godot/` | remendos no código do Godot |
| `patches/mesa-switch/` | o build do NVK com o Rust para o ARM do Switch |
| `scripts/` | os passos em bash (Docker): preparar, compilar o motor, empacotar |
| `docker/Dockerfile` | a imagem do devkitPro + scons |
| `exemplo/` | um projeto mínimo para provar o motor (`python gsn.py gerar exemplo`) |
| `jogos/` | configurações de jogos (uma pasta por jogo, em branches próprias) |
| `docs/` | decisões, testes, pendências e o estudo de viabilidade |

## De onde veio

O port foi escrito em outubro de 2026 para o **Batida Capixaba**, um jogo de ritmo sobre o congo
capixaba. A configuração e o histórico do jogo no Switch estão na branch
[`batida-capixaba`](../../tree/batida-capixaba) (`jogos/batida-capixaba/`), que gera a mesma versão que
o jogo usa.

## Créditos e referências

- [Godot Engine](https://godotengine.org) (MIT): o motor.
- O port do **Godot 3.5 para o Switch** de [Stary2001](https://github.com/Stary2001) e
  [cpasjuste](https://github.com/cpasjuste) (MIT): a referência para as partes que falam com o console.
- [devkitPro](https://devkitpro.org) e [libnx](https://github.com/switchbrew/libnx): o compilador e a
  biblioteca do Switch homebrew.
- [danfromtico/mesa-switch](https://github.com/danfromtico/mesa-switch) (Mesa, MIT): o Vulkan (NVK) e o
  Mesa novo para o Horizon; usado também pelo [NetherSX2_nx](https://github.com/NaGaa95/NetherSX2_nx).
- [Atmosphère](https://github.com/Atmosphere-NX/Atmosphere): o firmware customizado onde o `.nro` roda.

Nintendo Switch é marca da Nintendo. Este projeto não tem ligação com a Nintendo.

## Licença

MIT ([LICENSE](LICENSE)), como o Godot. Os jogos gerados continuam com as licenças deles.
