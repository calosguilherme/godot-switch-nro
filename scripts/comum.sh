#!/bin/bash
# Definições comuns dos scripts (rodam no Linux ou no Ubuntu do WSL, com o Docker).
set -e

AQUI_GSN=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

# Onde ficam o código do Godot, o mesa-switch, as bibliotecas do NVK e os executáveis (fora do
# repositório: são gigabytes).
TRABALHO="${GSN_TRABALHO:-$HOME/godot-switch-nro-trabalho}"

# As versões testadas (README, "Versões"). Trocar qualquer uma pede refazer os testes de docs/testes.md.
GODOT_URL="https://github.com/godotengine/godot.git"
GODOT_TAG="4.7.2-stable"
MESA_SWITCH_URL="https://github.com/danfromtico/mesa-switch.git"
MESA_SWITCH_COMMIT="0ff52079ca9e9e837ed27eb95a8d239f439a1864"

IMAGEM="godot-switch-nro"
GSN_VERSAO=$(tr -d ' \r\n' < "$AQUI_GSN/VERSION")

# O Docker do Ubuntu no WSL não sobe sozinho (sem systemd): sobe aqui, se precisar.
docker_pronto() {
	if ! docker info >/dev/null 2>&1; then
		if command -v dockerd >/dev/null 2>&1; then
			(nohup dockerd >/var/log/dockerd.log 2>&1 &)
			for _ in $(seq 1 30); do
				docker info >/dev/null 2>&1 && break
				sleep 1
			done
		fi
	fi
	docker info >/dev/null 2>&1 || {
		echo "erro: o Docker não está rodando (no WSL: rode como root; ou instale o docker.io)" >&2
		exit 1
	}
}

imagem_pronta() {
	docker_pronto
	docker build -q -t "$IMAGEM" "$AQUI_GSN/docker" >/dev/null
}

# Aplica os remendos de patches/godot/ a uma árvore do Godot (pula os que já estão aplicados).
aplicar_remendos() {
	local arvore="$1"
	for r in "$AQUI_GSN"/patches/godot/*.patch; do
		[ -e "$r" ] || continue
		if git -C "$arvore" apply --reverse --check "$r" 2>/dev/null; then
			continue
		fi
		git -C "$arvore" apply "$r"
		echo "remendo aplicado: $(basename "$r") em $arvore"
	done
}
