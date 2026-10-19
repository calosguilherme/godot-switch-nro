#!/bin/bash
# Monta um .nro a partir do motor (ELF), com os dados do jogo: título, autor e versão (NACP), ícone
# (JPG 256×256) e a pasta do romfs (abertura.rgba, versao.txt e, se pedido, jogo.pck). Quem chama é o
# gsn.py; à mão:
#   bash scripts/empacotar.sh <motor.elf> <saida.nro> "<título>" "<autor>" "<versão>" <icone.jpg> <pasta-romfs>
source "$(dirname "$0")/comum.sh"

[ $# -eq 7 ] || { echo "uso: empacotar.sh <motor.elf> <saida.nro> <título> <autor> <versão> <icone.jpg> <pasta-romfs>" >&2; exit 2; }
ELF=$(realpath "$1")
SAIDA_DIR=$(realpath "$(dirname "$2")")
SAIDA_NOME=$(basename "$2")
TITULO="$3"
AUTOR="$4"
VERSAO="$5"
ICONE=$(realpath "$6")
ROMFS=$(realpath "$7")

imagem_pronta
docker run --rm \
	-v "$ELF":/e/motor.elf:ro -v "$ICONE":/e/icone.jpg:ro -v "$ROMFS":/e/romfs:ro -v "$SAIDA_DIR":/s \
	-e TITULO="$TITULO" -e AUTOR="$AUTOR" -e VERSAO="$VERSAO" -e NOME="$SAIDA_NOME" \
	"$IMAGEM" bash -c '
		set -e
		T=/opt/devkitpro/tools/bin
		$T/nacptool --create "$TITULO" "$AUTOR" "$VERSAO" /tmp/jogo.nacp >/dev/null
		$T/elf2nro /e/motor.elf "/s/$NOME" --icon=/e/icone.jpg --nacp=/tmp/jogo.nacp --romfsdir=/e/romfs >/dev/null
	'
ls -la "$SAIDA_DIR/$SAIDA_NOME"
