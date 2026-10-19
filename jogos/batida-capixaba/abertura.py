"""A tela de abertura do Batida no Switch: o tambor do ícone e o nome do jogo, 1280×720.

Grava abertura_switch.png no projeto do jogo: a imagem de abertura do Godot no Switch (no lugar do logo
do Godot; project.godot, application/boot_splash/image.switch, PNG com import "keep"). O gsn.py usa a
mesma imagem para a abertura que o .nro desenha antes do Godot (romfs:/abertura.rgba).

Rode depois de mudar o ícone do jogo:
    PYTHONUTF8=1 python jogos/batida-capixaba/abertura.py <pasta do jogo>
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

LARGURA, ALTURA = 1280, 720
TAMBOR = 400  # altura do ícone na tela


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit("uso: abertura.py <pasta do jogo (a do project.godot)>")
    jogo = Path(sys.argv[1]).resolve()
    icone = Image.open(jogo / "icone.png").convert("RGBA")
    fundo = icone.getpixel((4, 4))
    tela = Image.new("RGBA", (LARGURA, ALTURA), fundo)
    tambor = icone.resize((TAMBOR, TAMBOR), Image.LANCZOS)
    topo = 70
    tela.paste(tambor, ((LARGURA - TAMBOR) // 2, topo), tambor)
    fonte = ImageFont.truetype(str(jogo / "fontes" / "NotoSans.ttf"), 64)
    fonte.set_variation_by_name("Bold")
    desenho = ImageDraw.Draw(tela)
    titulo = "Batida Capixaba"
    caixa = desenho.textbbox((0, 0), titulo, font=fonte)
    x = (LARGURA - (caixa[2] - caixa[0])) // 2 - caixa[0]
    desenho.text((x, topo + TAMBOR + 40), titulo, font=fonte, fill=(245, 227, 192, 255))
    tela.save(jogo / "abertura_switch.png")
    print("abertura: fundo", fundo)


if __name__ == "__main__":
    main()
