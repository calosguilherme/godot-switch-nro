"""Desenha o ícone (256×256) e a abertura (1280×720) do exemplo: formas simples, sem marca de ninguém.

    python exemplo/gerar_imagens.py
"""
from pathlib import Path

from PIL import Image, ImageDraw

AQUI = Path(__file__).resolve().parent
FUNDO = (27, 35, 51, 255)
AZUL = (71, 140, 191, 255)
CLARO = (230, 238, 245, 255)


def cubo(desenho: ImageDraw.ImageDraw, cx: float, cy: float, r: float) -> None:
    """Um cubo em perspectiva isométrica: três faces em tons de azul."""
    topo = [(cx, cy - r), (cx + r * 0.87, cy - r / 2), (cx, cy), (cx - r * 0.87, cy - r / 2)]
    esq = [(cx - r * 0.87, cy - r / 2), (cx, cy), (cx, cy + r), (cx - r * 0.87, cy + r / 2)]
    dir_ = [(cx + r * 0.87, cy - r / 2), (cx, cy), (cx, cy + r), (cx + r * 0.87, cy + r / 2)]
    desenho.polygon(topo, fill=(120, 180, 225, 255))
    desenho.polygon(esq, fill=AZUL)
    desenho.polygon(dir_, fill=(45, 95, 140, 255))


def main() -> None:
    icone = Image.new("RGBA", (256, 256), FUNDO)
    cubo(ImageDraw.Draw(icone), 128, 128, 92)
    icone.save(AQUI / "icone.png")

    abertura = Image.new("RGBA", (1280, 720), FUNDO)
    d = ImageDraw.Draw(abertura)
    cubo(d, 640, 330, 150)
    d.rectangle((540, 560, 740, 566), fill=CLARO)
    abertura.save(AQUI / "abertura.png")
    print("icone.png e abertura.png prontos")


if __name__ == "__main__":
    main()
