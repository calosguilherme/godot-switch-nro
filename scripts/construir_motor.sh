#!/bin/bash
# Compila o motor (o executável ELF do Godot para o Switch), sem nada de jogo dentro:
#   bash scripts/construir_motor.sh opengl   →  $TRABALHO/bin/godot-opengl.elf   (Mesa 20.1 do devkitPro)
#   bash scripts/construir_motor.sh vulkan   →  $TRABALHO/bin/godot-vulkan.elf   (NVK; precisa do preparar_nvk.sh)
# Do zero, cada variante leva ~9 min (LTO); depois, só o que mudou. Parâmetros a mais vão para o scons.
source "$(dirname "$0")/comum.sh"

VARIANTE="${1:-opengl}"
shift || true
case "$VARIANTE" in
	opengl)
		ARVORE="$TRABALHO/godot"
		EXTRA=()
		MONTAR=()
		;;
	vulkan)
		ARVORE="$TRABALHO/godot-vk"
		[ -f "$TRABALHO/nvk-libs/libvulkan.a" ] || { echo "erro: falta o NVK; rode scripts/preparar_nvk.sh" >&2; exit 1; }
		EXTRA=(vulkan=yes use_volk=no opengl3=no module_glslang_enabled=yes nvk=/nvk)
		MONTAR=(-v "$TRABALHO/nvk-libs":/nvk:ro)
		;;
	*)
		echo "uso: construir_motor.sh opengl|vulkan [opções do scons]" >&2
		exit 2
		;;
esac
[ -d "$ARVORE/.git" ] || { echo "erro: falta o Godot em $ARVORE; rode scripts/preparar_godot.sh" >&2; exit 1; }

imagem_pronta
aplicar_remendos "$ARVORE"
rm -rf "$ARVORE/platform/switch"
cp -r "$AQUI_GSN/platform/switch" "$ARVORE/platform/switch"
printf '// Gerado pelo scripts/construir_motor.sh a partir de VERSION.\n#pragma once\n#define GSN_VERSAO "%s"\n' "$GSN_VERSAO" > "$ARVORE/platform/switch/versao.gen.h"

echo "== motor $VARIANTE (godot-switch-nro $GSN_VERSAO)"
docker run --rm -v "$ARVORE":/godot "${MONTAR[@]}" -w /godot "$IMAGEM" \
	scons platform=switch target=template_release -j"$(nproc)" "${EXTRA[@]}" "$@"

mkdir -p "$TRABALHO/bin"
cp "$ARVORE/bin/godot.switch.template_release.arm64" "$TRABALHO/bin/godot-$VARIANTE.elf"
echo "$GSN_VERSAO" > "$TRABALHO/bin/godot-$VARIANTE.versao"
ls -la "$TRABALHO/bin/godot-$VARIANTE.elf"
