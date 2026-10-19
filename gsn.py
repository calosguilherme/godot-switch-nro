#!/usr/bin/env python3
"""godot-switch-nro: um jogo do Godot 4.7.2 no Nintendo Switch homebrew (.nro no Atmosphère).

    python gsn.py preparar [--sem-vulkan]          # uma vez: Godot, remendos, imagem Docker e o NVK
    python gsn.py motor [opengl|vulkan|todos]      # compila o motor (o ELF de cada variante)
    python gsn.py gerar <pasta-do-projeto>         # .pck + <jogo>.nro + <jogo>.vulkan, prontos para o cartão
    python gsn.py info <pasta-do-projeto>          # mostra a configuração que o `gerar` usaria

No Windows, os passos de compilação rodam no Ubuntu do WSL (como root, com o docker.io); no Linux, direto.
A exportação do .pck usa o editor do Godot 4.7.2 do seu sistema (--godot, `godot` na configuração, a
variável GODOT ou `godot` no PATH). Detalhes no README.
"""
from __future__ import annotations

import argparse
import configparser
import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
import unicodedata
from dataclasses import dataclass, field
from pathlib import Path

AQUI = Path(__file__).resolve().parent
NO_WINDOWS = platform.system() == "Windows"
DISTRO = os.environ.get("GSN_WSL", "Ubuntu")
ABERTURA = (1280, 720)
ICONE = 256
PRESET_TEMPORARIO = "Switch (godot-switch-nro)"
CONFIG_NO_PROJETO = "godot-switch-nro.cfg"


def tamanho(arquivo: Path) -> str:
    bytes_ = arquivo.stat().st_size
    return f"{bytes_ / 1048576:.1f} MiB" if bytes_ >= 1048576 else f"{bytes_ / 1024:.0f} KiB"


def falhar(texto: str) -> None:
    print("erro: " + texto, file=sys.stderr)
    sys.exit(1)


# --- Rodar os scripts (WSL no Windows, bash no Linux) ------------------------------------------------

def caminho_linux(caminho: Path) -> str:
    """C:\\Users\\x → /mnt/c/Users/x no WSL; no Linux, o próprio caminho."""
    texto = str(caminho.resolve())
    if not NO_WINDOWS:
        return texto
    unidade, resto = texto[0].lower(), texto[2:].replace("\\", "/")
    return f"/mnt/{unidade}{resto}"


def rodar_script(script: str, *args: str) -> None:
    trabalho = os.environ.get("GSN_TRABALHO")
    prefixo = f"GSN_TRABALHO={shlex.quote(trabalho)} " if trabalho else ""
    linha = f"cd {shlex.quote(caminho_linux(AQUI))} && {prefixo}bash scripts/{script} " + " ".join(shlex.quote(a) for a in args)
    if NO_WINDOWS:
        comando = ["wsl.exe", "-d", DISTRO, "-u", "root", "--", "bash", "-lc", linha]
    else:
        comando = ["bash", "-lc", linha]
    print(f"== {script} {' '.join(args)}", flush=True)
    processo = subprocess.Popen(comando, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    assert processo.stdout is not None
    for bruto in processo.stdout:
        sys.stdout.write(bruto.decode("utf-8", errors="replace"))
        sys.stdout.flush()
    if processo.wait() != 0:
        falhar(f"{script} terminou com erro ({processo.returncode})")


def saida_do_script(linha: str) -> str:
    """Roda um comando curto no ambiente dos scripts e devolve a saída (ex.: onde está o TRABALHO)."""
    comando = ["wsl.exe", "-d", DISTRO, "-u", "root", "--", "bash", "-lc", linha] if NO_WINDOWS else ["bash", "-lc", linha]
    return subprocess.run(comando, capture_output=True).stdout.decode("utf-8", errors="replace").strip()


def trabalho_linux() -> str:
    return os.environ.get("GSN_TRABALHO") or saida_do_script('echo "$HOME/godot-switch-nro-trabalho"')


# --- O projeto e a configuração ------------------------------------------------------------------------

def ler_project_godot(projeto: Path) -> dict[str, str]:
    """As chaves do project.godot como "seção/chave" → valor cru (sem as aspas)."""
    arquivo = projeto / "project.godot"
    if not arquivo.is_file():
        falhar(f"{projeto} não é um projeto do Godot (falta o project.godot)")
    valores: dict[str, str] = {}
    secao = ""
    for linha in arquivo.read_text(encoding="utf-8").splitlines():
        linha = linha.strip()
        if not linha or linha.startswith(";"):
            continue
        if linha.startswith("[") and linha.endswith("]"):
            secao = linha[1:-1]
            continue
        if "=" in linha:
            chave, valor = linha.split("=", 1)
            valor = valor.strip()
            if valor.startswith('"') and valor.endswith('"'):
                valor = valor[1:-1]
            valores[f"{secao}/{chave.strip()}"] = valor
    return valores


def slug(texto: str) -> str:
    texto = unicodedata.normalize("NFKD", texto).encode("ascii", "ignore").decode()
    texto = re.sub(r"[^a-zA-Z0-9]+", "-", texto).strip("-").lower()
    return texto or "jogo"


def cor_do_godot(valor: str | None) -> tuple[int, int, int]:
    """Color(r, g, b, a) do project.godot (ou #rrggbb) → (r, g, b) de 0 a 255."""
    if not valor:
        return (0, 0, 0)
    if valor.startswith("#"):
        return tuple(int(valor[i:i + 2], 16) for i in (1, 3, 5))  # type: ignore[return-value]
    numeros = re.findall(r"[-0-9.]+", valor)
    if len(numeros) >= 3:
        return tuple(round(max(0.0, min(1.0, float(n))) * 255) for n in numeros[:3])  # type: ignore[return-value]
    return (0, 0, 0)


def sim(valor: str | None, padrao: bool) -> bool:
    if valor is None or valor == "":
        return padrao
    return valor.strip().lower() in ("sim", "s", "yes", "y", "true", "1")


@dataclass
class Jogo:
    projeto: Path
    titulo: str
    autor: str
    arquivo: str
    versao: str
    icone: Path | None
    abertura: Path | None
    cor_de_fundo: tuple[int, int, int]
    preset: str
    vulkan: bool
    embutir_pck: bool
    godot: str | None
    config: Path | None = None
    versao_arquivo: Path | None = None
    subir_versao: bool = False
    escrever_versao_em: str | None = None
    extras: dict[str, str] = field(default_factory=dict)


def achar_config(projeto: Path, nome: str, pedido: str | None) -> Path | None:
    if pedido:
        caminho = Path(pedido)
        if not caminho.is_file():
            falhar(f"configuração {pedido} não existe")
        return caminho.resolve()
    no_projeto = projeto / CONFIG_NO_PROJETO
    if no_projeto.is_file():
        return no_projeto.resolve()
    # as configurações guardadas neste repositório (jogos/<jogo>/jogo.cfg), pelo nome do projeto
    for candidato in sorted((AQUI / "jogos").glob("*/jogo.cfg")):
        cfg = configparser.ConfigParser()
        cfg.read(candidato, encoding="utf-8")
        if cfg.get("jogo", "projeto_nome", fallback="") == nome:
            return candidato.resolve()
    return None


def caminho_de(valor: str | None, projeto: Path, base: Path | None) -> Path | None:
    """res://x → <projeto>/x; caminho relativo → relativo à configuração; vazio → None."""
    if not valor:
        return None
    if valor.startswith("res://"):
        return (projeto / valor[len("res://"):]).resolve()
    caminho = Path(os.path.expanduser(valor))
    if not caminho.is_absolute() and base is not None:
        caminho = base / caminho
    return caminho.resolve()


def montar_jogo(pasta: str, config_pedida: str | None, godot_pedido: str | None) -> Jogo:
    projeto = Path(pasta).resolve()
    pg = ler_project_godot(projeto)
    nome = pg.get("application/config/name", projeto.name)
    config = achar_config(projeto, nome, config_pedida)
    cfg = configparser.ConfigParser()
    if config:
        cfg.read(config, encoding="utf-8")
    c = cfg["jogo"] if cfg.has_section("jogo") else {}
    base = config.parent if config else None

    versao_arquivo = caminho_de(c.get("versao_arquivo"), projeto, base)
    versao = c.get("versao") or ""
    if versao_arquivo and versao_arquivo.is_file():
        versao = versao_arquivo.read_text(encoding="utf-8").strip()
    versao = versao or pg.get("application/config/version", "") or "1.0.0"

    abertura_padrao = pg.get("application/boot_splash/image.switch") or pg.get("application/boot_splash/image")
    cor_padrao = pg.get("application/boot_splash/bg_color.switch") or pg.get("application/boot_splash/bg_color")
    titulo = c.get("titulo") or nome
    return Jogo(
        projeto=projeto,
        titulo=titulo,
        autor=c.get("autor") or "Godot Engine (homebrew)",
        arquivo=c.get("arquivo") or slug(titulo),
        versao=versao,
        icone=caminho_de(c.get("icone") or pg.get("application/config/icon"), projeto, base),
        abertura=caminho_de(c.get("abertura") or abertura_padrao, projeto, base),
        cor_de_fundo=cor_do_godot(c.get("cor_de_fundo") or cor_padrao),
        preset=c.get("preset") or "Switch",
        vulkan=sim(c.get("vulkan"), True),
        embutir_pck=sim(c.get("embutir_pck"), False),
        godot=os.path.expanduser(godot_pedido or c.get("godot") or os.environ.get("GODOT") or shutil.which("godot") or "") or None,
        config=config,
        versao_arquivo=versao_arquivo,
        subir_versao=sim(c.get("subir_versao"), False),
        escrever_versao_em=c.get("escrever_versao_em"),
    )


# --- Imagens -------------------------------------------------------------------------------------------

def pillow():
    try:
        from PIL import Image  # noqa: F401
    except ImportError:
        falhar("o gsn.py precisa do Pillow para o ícone e a abertura: python -m pip install pillow")
    from PIL import Image
    return Image


def fazer_icone(jogo: Jogo, destino: Path) -> None:
    """O ícone do hbmenu: JPG 256×256 (a transparência vira a cor de fundo)."""
    Image = pillow()
    if jogo.icone and jogo.icone.is_file():
        imagem = Image.open(jogo.icone).convert("RGBA")
        if jogo.icone.suffix.lower() in (".jpg", ".jpeg") and imagem.size == (ICONE, ICONE):
            shutil.copyfile(jogo.icone, destino)
            return
        imagem = imagem.resize((ICONE, ICONE), Image.LANCZOS)
    else:
        print("aviso: sem ícone no projeto; vai um quadrado na cor de fundo")
        imagem = Image.new("RGBA", (ICONE, ICONE), jogo.cor_de_fundo + (255,))
    fundo = Image.new("RGBA", imagem.size, jogo.cor_de_fundo + (255,))
    Image.alpha_composite(fundo, imagem).convert("RGB").save(destino, "JPEG", quality=92)


def fazer_abertura(jogo: Jogo, destino: Path) -> bool:
    """A abertura que o .nro desenha antes do Godot: RGBA 1280×720 crus (a imagem cabe inteira, centrada,
    sobre a cor de fundo, como o boot splash do Godot)."""
    if not jogo.abertura or not jogo.abertura.is_file():
        print("aviso: sem imagem de abertura; a tela fica preta até o Godot abrir")
        return False
    Image = pillow()
    imagem = Image.open(jogo.abertura).convert("RGBA")
    if imagem.size != ABERTURA:
        escala = min(ABERTURA[0] / imagem.width, ABERTURA[1] / imagem.height)
        imagem = imagem.resize((round(imagem.width * escala), round(imagem.height * escala)), Image.LANCZOS)
    tela = Image.new("RGBA", ABERTURA, jogo.cor_de_fundo + (255,))
    tela.alpha_composite(imagem, ((ABERTURA[0] - imagem.width) // 2, (ABERTURA[1] - imagem.height) // 2))
    destino.write_bytes(tela.tobytes())
    return True


# --- O .pck --------------------------------------------------------------------------------------------

def preset_existe(projeto: Path, nome: str) -> bool:
    arquivo = projeto / "export_presets.cfg"
    return arquivo.is_file() and f'name="{nome}"' in arquivo.read_text(encoding="utf-8")


def preset_temporario(projeto: Path) -> str:
    """Um preset Linux arm64 só para o .pck, acrescentado ao export_presets.cfg (o original volta no fim)."""
    arquivo = projeto / "export_presets.cfg"
    texto = arquivo.read_text(encoding="utf-8") if arquivo.is_file() else ""
    indices = [int(n) for n in re.findall(r"(?m)^\[preset\.(\d+)\]$", texto)]
    n = max(indices, default=-1) + 1
    return texto + f"""
[preset.{n}]

name="{PRESET_TEMPORARIO}"
platform="Linux"
runnable=false
dedicated_server=false
custom_features="switch"
export_filter="all_resources"
include_filter=""
exclude_filter=""
export_path=""
script_export_mode=2

[preset.{n}.options]

binary_format/embed_pck=false
texture_format/s3tc_bptc=true
texture_format/etc2_astc=true
binary_format/architecture="arm64"
"""


def exportar_pck(jogo: Jogo, destino: Path) -> None:
    if not jogo.godot:
        falhar("não achei o editor do Godot 4.7.2: use --godot, `godot` na configuração ou a variável GODOT")
    arquivo = jogo.projeto / "export_presets.cfg"
    original = arquivo.read_text(encoding="utf-8") if arquivo.is_file() else None
    preset = jogo.preset
    if not preset_existe(jogo.projeto, preset):
        print(f"o projeto não tem o preset \"{preset}\": exporto com um preset temporário (o export_presets.cfg volta ao que era)")
        arquivo.write_text(preset_temporario(jogo.projeto), encoding="utf-8")
        preset = PRESET_TEMPORARIO
    destino.parent.mkdir(parents=True, exist_ok=True)
    registro = destino.with_suffix(".exportar.log")
    try:
        with open(registro, "w", encoding="utf-8", errors="replace") as log:
            r = subprocess.run([jogo.godot, "--headless", "--path", str(jogo.projeto), "--export-pack", preset, str(destino)],
                               stdout=log, stderr=subprocess.STDOUT)
    finally:
        if preset != jogo.preset:
            if original is None:
                arquivo.unlink(missing_ok=True)
            else:
                arquivo.write_text(original, encoding="utf-8")
    if r.returncode != 0 or not destino.is_file():
        falhar(f"a exportação do .pck falhou (veja {registro})")
    print(f".pck: {destino} ({tamanho(destino)})")


# --- Comandos ------------------------------------------------------------------------------------------

def cmd_preparar(args: argparse.Namespace) -> None:
    rodar_script("preparar_godot.sh")
    if not args.sem_vulkan:
        rodar_script("preparar_nvk.sh")


def cmd_motor(args: argparse.Namespace) -> None:
    variantes = ["opengl", "vulkan"] if args.variante == "todos" else [args.variante]
    for v in variantes:
        rodar_script("construir_motor.sh", v)


def mostrar(jogo: Jogo) -> None:
    print(f"projeto      {jogo.projeto}")
    print(f"configuração {jogo.config or '(nenhuma: só o project.godot)'}")
    print(f"título       {jogo.titulo}")
    print(f"autor        {jogo.autor}")
    print(f"arquivos     {jogo.arquivo}.nro, {jogo.arquivo}.vulkan, {jogo.arquivo}.pck  (sdmc:/switch/{jogo.arquivo}/)")
    print(f"versão       {jogo.versao}" + ("  (sobe a cada geração)" if jogo.subir_versao else ""))
    print(f"ícone        {jogo.icone or '(nenhum)'}")
    print(f"abertura     {jogo.abertura or '(nenhuma)'}  fundo {jogo.cor_de_fundo}")
    print(f"preset       {jogo.preset}" + ("" if preset_existe(jogo.projeto, jogo.preset) else " (não existe: vai um temporário)"))
    print(f"vulkan       {'sim' if jogo.vulkan else 'não'}   .pck dentro do .nro: {'sim' if jogo.embutir_pck else 'não'}")
    print(f"editor       {jogo.godot or '(não achado)'}")


def cmd_info(args: argparse.Namespace) -> None:
    mostrar(montar_jogo(args.projeto, args.config, args.godot))


def cmd_gerar(args: argparse.Namespace) -> None:
    jogo = montar_jogo(args.projeto, args.config, args.godot)
    if args.sem_vulkan:
        jogo.vulkan = False
    if not jogo.godot or not (Path(jogo.godot).is_file() or shutil.which(jogo.godot)):
        falhar(f"não achei o editor do Godot 4.7.2 ({jogo.godot or 'nenhum'}): use --godot, `godot` na configuração ou a variável GODOT")
    # os arquivos de versão mudam antes da exportação (a versão vai para o .pck); se algo falhar no meio,
    # eles voltam ao que eram
    guardados: dict[Path, str | None] = {}
    alvo_versao = caminho_de(jogo.escrever_versao_em, jogo.projeto, jogo.projeto) if jogo.escrever_versao_em else None
    for arquivo in (jogo.versao_arquivo, alvo_versao):
        if arquivo is not None:
            guardados[arquivo] = arquivo.read_text(encoding="utf-8") if arquivo.is_file() else None
    try:
        _gerar(jogo, args, alvo_versao)
    except BaseException:
        for arquivo, texto in guardados.items():
            if texto is None:
                arquivo.unlink(missing_ok=True)
            else:
                arquivo.write_text(texto, encoding="utf-8")
        print("a geração parou no meio: os arquivos de versão voltaram ao que eram", file=sys.stderr)
        raise


def _gerar(jogo: Jogo, args: argparse.Namespace, alvo_versao: Path | None) -> None:
    if jogo.subir_versao and jogo.versao_arquivo:
        partes = jogo.versao.split(".")
        partes[-1] = str(int(partes[-1]) + 1) if partes[-1].isdigit() else partes[-1]
        jogo.versao = ".".join(partes)
        jogo.versao_arquivo.write_text(jogo.versao + "\n", encoding="utf-8")
    mostrar(jogo)

    trabalho = AQUI / "saida" / ".trabalho" / jogo.arquivo
    romfs = trabalho / "romfs"
    shutil.rmtree(trabalho, ignore_errors=True)
    romfs.mkdir(parents=True)
    cartao = Path(args.saida).resolve() if args.saida else AQUI / "saida" / jogo.arquivo
    pasta = cartao / "switch" / jogo.arquivo
    pasta.mkdir(parents=True, exist_ok=True)

    # a versão vai ao .nro (romfs:/versao.txt) e, se o jogo quiser mostrá-la, a um arquivo do projeto
    (romfs / "versao.txt").write_text(jogo.versao + "\n", encoding="utf-8")
    if alvo_versao is not None:
        alvo_versao.write_text(jogo.versao + "\n", encoding="utf-8")
    fazer_abertura(jogo, romfs / "abertura.rgba")
    fazer_icone(jogo, trabalho / "icone.jpg")

    if not args.sem_compilar:
        cmd_motor(argparse.Namespace(variante="todos" if jogo.vulkan else "opengl"))

    pck = trabalho / f"{jogo.arquivo}.pck"
    exportar_pck(jogo, pck)
    if jogo.embutir_pck:
        shutil.copyfile(pck, romfs / "jogo.pck")
    else:
        shutil.copyfile(pck, pasta / f"{jogo.arquivo}.pck")
        (romfs / "jogo.pck").unlink(missing_ok=True)

    # o romfs do .vulkan sem a abertura (só o OpenGL a desenha; seriam 3,6 MB a mais no cartão)
    romfs_vulkan = trabalho / "romfs-vulkan"
    romfs_vulkan.mkdir()
    for f in romfs.iterdir():
        if f.name != "abertura.rgba":
            shutil.copyfile(f, romfs_vulkan / f.name)

    bin_ = trabalho_linux() + "/bin"
    variantes = [("opengl", ".nro", romfs)] + ([("vulkan", ".vulkan", romfs_vulkan)] if jogo.vulkan else [])
    for variante, extensao, pasta_romfs in variantes:
        rodar_script("empacotar.sh", f"{bin_}/godot-{variante}.elf", caminho_linux(pasta) + "/" + jogo.arquivo + extensao,
                     jogo.titulo, jogo.autor, jogo.versao, caminho_linux(trabalho / "icone.jpg"), caminho_linux(pasta_romfs))
    if not jogo.vulkan:
        # um .vulkan de outra versão nunca fica na pasta (o .nro passaria a vez a ele)
        (pasta / f"{jogo.arquivo}.vulkan").unlink(missing_ok=True)
    if jogo.embutir_pck:
        (pasta / f"{jogo.arquivo}.pck").unlink(missing_ok=True)

    print()
    for f in sorted(pasta.iterdir()):
        print(f"  {f.name:<32} {tamanho(f):>10}")
    print(f"\npronto: versão {jogo.versao}. Copie a pasta {cartao / 'switch'} para a raiz do cartão")
    print(f"(fica sdmc:/switch/{jogo.arquivo}/) e troque TODOS os arquivos dela juntos.")


def main() -> None:
    p = argparse.ArgumentParser(description="Godot 4.7.2 no Switch homebrew (.nro).")
    sub = p.add_subparsers(dest="comando", required=True)
    a = sub.add_parser("preparar", help="baixa e prepara o Godot, a imagem Docker e o NVK (uma vez)")
    a.add_argument("--sem-vulkan", action="store_true", help="pula o NVK (só a variante OpenGL)")
    a.set_defaults(func=cmd_preparar)
    a = sub.add_parser("motor", help="compila o motor")
    a.add_argument("variante", nargs="?", default="todos", choices=["opengl", "vulkan", "todos"])
    a.set_defaults(func=cmd_motor)
    for nome, func, ajuda in (("gerar", cmd_gerar, "gera os arquivos do cartão para um projeto"),
                              ("info", cmd_info, "mostra a configuração resolvida de um projeto")):
        a = sub.add_parser(nome, help=ajuda)
        a.add_argument("projeto", help="a pasta do projeto do Godot (a do project.godot)")
        a.add_argument("--config", help=f"arquivo de configuração (padrão: {CONFIG_NO_PROJETO} no projeto ou jogos/*/jogo.cfg)")
        a.add_argument("--godot", help="o executável do editor do Godot 4.7.2")
        if nome == "gerar":
            a.add_argument("--saida", help="pasta de saída (padrão: saida/<jogo>)")
            a.add_argument("--sem-vulkan", action="store_true", help="só o .nro (OpenGL)")
            a.add_argument("--sem-compilar", action="store_true", help="usa o motor já compilado")
        a.set_defaults(func=func)
    args = p.parse_args()
    args.func(args)


if __name__ == "__main__":
    if sys.stdout.encoding and sys.stdout.encoding.lower() != "utf-8":
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")  # type: ignore[attr-defined]
    main()
