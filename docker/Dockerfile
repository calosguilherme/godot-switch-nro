# A imagem oficial do devkitPro (devkitA64, libnx, switch-mesa, ferramentas do Switch) com o scons, para
# compilar o Godot e montar os .nro. Fixada no resumo testado (devkitA64 r29.2, libnx 4.12.0, switch-mesa
# 20.1.0, switch-tools 1.13.1; ver README, "Versões"). Para outra versão do devkitPro, troque o resumo e
# refaça os testes de docs/testes.md.
FROM devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282
RUN apt-get update && apt-get install -y --no-install-recommends scons && rm -rf /var/lib/apt/lists/*
