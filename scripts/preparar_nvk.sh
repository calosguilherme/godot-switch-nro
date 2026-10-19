#!/bin/bash
# Prepara o Vulkan (NVK do Mesa 26, do danfromtico/mesa-switch), uma vez: baixa o commit testado, põe os
# arquivos de patches/mesa-switch/ (o Rust do NAK compilado para o ARM do Switch; docs/decisoes.md, D-16),
# compila no Docker dele (imagem devkitpro-mesa-rust, ~7 GB; leva de 30 a 60 minutos) e junta as
# bibliotecas estáticas em $TRABALHO/nvk-libs, que a variante Vulkan do motor liga.
#   bash scripts/preparar_nvk.sh
source "$(dirname "$0")/comum.sh"

MESA="$TRABALHO/mesa-switch"
LIBS="$TRABALHO/nvk-libs"
mkdir -p "$TRABALHO"
imagem_pronta
if [ ! -d "$MESA/.git" ]; then
	echo "== baixando o mesa-switch ($MESA_SWITCH_COMMIT)"
	git clone -q "$MESA_SWITCH_URL" "$MESA"
fi
git -C "$MESA" checkout -q "$MESA_SWITCH_COMMIT"
cp "$AQUI_GSN/patches/mesa-switch/build-switch-gsn.sh" "$MESA/"
cp "$AQUI_GSN/patches/mesa-switch/switch_cross_file_gsn.txt" "$MESA/"
cp "$AQUI_GSN/patches/mesa-switch/rustc-switch-wrapper-gsn.sh" "$MESA/"
chmod +x "$MESA/build-switch-gsn.sh" "$MESA/rustc-switch-wrapper-gsn.sh"

echo "== compilando o NVK (demora)"
(cd "$MESA" && bash ./build-switch-gsn.sh)

echo "== juntando as bibliotecas em $LIBS"
rm -rf "$LIBS" && mkdir -p "$LIBS"
B="$MESA/builddir-switch"
for f in \
	src/nouveau/vulkan/libnvk.a src/nouveau/vulkan/libvulkan.a src/vulkan/util/libvulkan_util.a \
	src/compiler/nir/libnir.a src/compiler/libcompiler.a src/compiler/spirv/libvtn.a src/util/libxmlconfig.a \
	src/nouveau/compiler/libnak.a src/nouveau/compiler/libnak_rs.a src/nouveau/nil/libnil.a \
	src/nouveau/nil/liblibnil_format_table.a src/nouveau/mme/libnouveau_mme.a src/nouveau/winsys/libnouveau_ws.a \
	src/nouveau/headers/libnvidia_headers_c.a src/compiler/rust/libcompiler_c_helpers.a src/util/libmesa_util.a \
	src/util/libmesa_util_simd.a src/util/blake3/libblake3.a src/c11/impl/libmesa_util_c11.a; do
	cp "$B/$f" "$LIBS/"
done
# o Rust pede -ldl -lrt -lutil, que a libnx não tem: bibliotecas vazias (o mesa-switch traz os símbolos)
docker run --rm -v "$LIBS":/nvk "$IMAGEM" bash -c '
	set -e
	cd /tmp
	echo "void __mesa_switch_stub_lib(void) {}" > stub.c
	/opt/devkitpro/devkitA64/bin/aarch64-none-elf-gcc -c stub.c -o stub.o
	for L in dl rt util; do /opt/devkitpro/devkitA64/bin/aarch64-none-elf-ar rcs /nvk/lib${L}.a stub.o; done
	N=$(/opt/devkitpro/devkitA64/bin/aarch64-none-elf-nm -g --defined-only /nvk/libvulkan.a 2>/dev/null | grep -c " T vkCreateViSurfaceNN$" || true)
	[ "$N" = "1" ] || { echo "erro: o libvulkan.a não tem o vkCreateViSurfaceNN"; exit 1; }
'
echo "== NVK pronto em $LIBS"
