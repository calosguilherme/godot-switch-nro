# Pendências do port

O que falta ou ainda não foi provado, do mais sentido ao menos. As do jogo Batida Capixaba ficam na
branch `batida-capixaba` (`jogos/batida-capixaba/pendencias.md`).

## Para qualquer jogo

1. **A primeira abertura em Vulkan é uma tela preta.** O Godot compila os shaders do renderizador
   Mobile antes de desenhar qualquer coisa (no Batida, minutos na primeira vez; menos de 5 s depois,
   com o cache). Caminho provável: desenhar a `romfs:/abertura.rgba` pelo RenderingDevice (ou por um
   framebuffer da libnx antes de o Vulkan pegar a janela). Precisa de teste no console.
2. **Rede:** o Godot não tem rede no port (`ERROR: Parameter "_create" is null` em `core/io/ip.cpp` no
   log). `HTTPRequest`, multiplayer, WebSocket e TLS não funcionam (os módulos estão desligados e falta a
   implementação de `IP` para a libnx).
3. **Modo applet** (aberto pelo álbum, ~400 MB): o jogo não cabe e cai. Falta uma tela avisando "abra
   segurando R" (o port 3.5 tinha uma).
4. **Módulos desligados** que um jogo pode usar (`detect.py`): CSG (a manifold choca com macros da
   newlib), vídeo Theora, Jolt (só a física padrão), `text_server_adv` (sem ele não há escrita
   complexa: árabe, hebraico, indianas; as latinas, o cirílico e o CJK funcionam), câmera, XR.
5. **C# e GDExtension:** só GDScript. O Switch homebrew não carrega bibliotecas avulsas; uma GDExtension
   teria de ser compilada junto com o motor (como módulo).
6. **Teclado virtual** (o swkbd da libnx) para `LineEdit`: não ligado.
7. **Mais de dois controles:** o port lê dois jogadores (o 1º controle ou o portátil, e o 2º).
8. **O Rust do NVK não está fixado:** o `Docker.rust` do `mesa-switch` instala o nightly do dia. O
   testado foi o `rustc 1.101.0-nightly (b6eeb93c9 2026-10-18)`; um nightly novo pode quebrar o NAK.
9. **Um único título no NACP:** o mesmo nome em todos os idiomas do hbmenu.
10. **O estudo de 2026-10-18** (`docs/estudo-nro-generico.md`) lista o resto do que um runtime
    genérico pediria (log só sob demanda, nomes da API em inglês, opções por projeto). Parte já foi
    feita (nomes de arquivo, romfs, empacotador); o estudo ficou como registro.

## Para provar no console

- O empacotamento deste repositório (ver `docs/testes.md`, "Ainda não testado no console").
- O "tudo solto" da conferência da saída e o log sem linhas cortadas (0.2.25+).
