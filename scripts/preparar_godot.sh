#!/bin/bash
# Prepara o código do Godot (uma vez; rodar de novo não estraga nada): baixa a tag testada, aplica os
# remendos de patches/godot/ e faz a cópia da variante Vulkan (uma árvore à parte, para trocar de
# variante sem recompilar tudo). Também monta a imagem Docker.
#   bash scripts/preparar_godot.sh
source "$(dirname "$0")/comum.sh"

mkdir -p "$TRABALHO"
imagem_pronta
for arvore in "$TRABALHO/godot" "$TRABALHO/godot-vk"; do
	if [ ! -d "$arvore/.git" ]; then
		echo "== baixando o Godot $GODOT_TAG em $arvore"
		git clone -q --depth 1 --branch "$GODOT_TAG" "$GODOT_URL" "$arvore"
	fi
	aplicar_remendos "$arvore"
done
echo "== Godot pronto em $TRABALHO (godot e godot-vk)"
