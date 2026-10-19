#!/bin/bash
# Wrapper do rustc para o build do NVK do mesa-switch no Docker em Linux (godot-switch-nro, docs/decisoes.md D-16).
# O alvo ARM (--target=aarch64-unknown-linux-gnu) vem do cross file só para os crates do Switch; as
# macros procedurais e as dependências delas rodam no PC e saem sem ele. O teste inicial do Meson
# (sanity check) precisa compilar e rodar no PC: ali saem o --target e o ligador do Switch (-C linker=).
set -e

RUSTC_BIN=/root/.cargo/bin/rustc
[ -x "$RUSTC_BIN" ] || RUSTC_BIN="$(command -v rustc)"

teste=0
for arg in "$@"; do
    case "$arg" in
        *sanity_check_for_rust.rs*|*sanitycheckrs.rs*) teste=1 ;;
    esac
done

if [ "$teste" = 1 ]; then
    args=()
    pular=0
    lista=("$@")
    for ((i = 0; i < ${#lista[@]}; i++)); do
        arg="${lista[$i]}"
        case "$arg" in
            --target=*|-Clinker=*) continue ;;
            -C)
                proximo="${lista[$((i + 1))]:-}"
                case "$proximo" in
                    linker=*) i=$((i + 1)); continue ;;
                esac
                ;;
        esac
        args+=("$arg")
    done
    exec "$RUSTC_BIN" "${args[@]}"
fi
exec "$RUSTC_BIN" "$@"
