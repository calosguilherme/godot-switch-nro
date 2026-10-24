# Godot 4 para o Nintendo Switch homebrew (Atmosphère, .nro), com devkitPro (devkitA64 + libnx).
# Plataforma escrita para o Batida Capixaba e generalizada no godot-switch-nro; a referência foi o port do Godot 3.5 de Stary2001 e
# cpasjuste (MIT). Renderizador: só o Compatibility (OpenGL ES 3 do mesa/nouveau); não há Vulkan.
import os
from typing import TYPE_CHECKING

from platform_methods import validate_arch

if TYPE_CHECKING:
    from SCons.Script.SConscript import SConsEnvironment


def get_name():
    return "Switch"


def can_build():
    dkp = os.environ.get("DEVKITPRO")
    if not dkp:
        return False
    return os.path.isdir(dkp + "/devkitA64") and os.path.isdir(dkp + "/libnx")


def get_opts():
    from SCons.Variables import BoolVariable

    return [
        BoolVariable("nxlink", "Manda o stdout/stderr para o nxlink no PC", True),
        # outro Mesa no lugar do switch-mesa 20.1 do devkitPro (docs/decisoes.md): o prefixo do SDK com include/ e
        # lib/, como o build-opengl.sh do danfromtico/mesa-switch (Mesa 26.2) o instala. Vazio: o de sempre.
        ("mesa", "Prefixo de um Mesa para o Switch (vazio: o switch-mesa do devkitPro)", ""),
        # o Vulkan do <jogo>.vulkan (docs/decisoes.md): a pasta com as bibliotecas estáticas do NVK do mesa-switch
        # (Mesa 26.2, sem carregador). Vale com vulkan=yes use_volk=no opengl3=no.
        ("nvk", "Pasta com as bibliotecas do NVK (Vulkan) do mesa-switch", ""),
    ]


def get_doc_classes():
    return []


def get_doc_path():
    return "doc_classes"


def get_flags():
    return {
        "arch": "arm64",
        "target": "template_release",
        "vulkan": False,
        "opengl3": True,
        "d3d12": False,
        "metal": False,
        "sdl": False,
        "accesskit": False,
        "builtin_pcre2_with_jit": False,
        # LTO (otimização na hora de juntar, docs/decisoes.md): o motor fica menor e mais rápido no processador do
        # Switch; a compilação leva ~9 min do zero
        "lto": "full",
        # o que o Switch homebrew não usa (ou só pesa na compilação)
        "module_mono_enabled": False,
        "module_openxr_enabled": False,
        "module_webxr_enabled": False,
        "module_mobile_vr_enabled": False,
        "module_camera_enabled": False,
        "module_theora_enabled": False,
        # rede (docs/decisoes.md, D-18): o ENet (UDP, multijogador) e o mbedTLS (o Crypto: bytes aleatórios,
        # chaves, hash; o TLS em si ainda sem uso) ligados; UPnP, WebRTC e WebSocket fora
        "module_upnp_enabled": False,
        "module_enet_enabled": True,
        "module_webrtc_enabled": False,
        "module_websocket_enabled": False,
        "module_mbedtls_enabled": True,
        # sem o pacote de certificados embutido: com o mbedTLS, todo jogo leria ~140 certificados na abertura
        # (Crypto::load_default_certificates) sem usar HTTPS. Quem precisar de TLS liga de volta ou aponta o
        # network/tls/certificate_bundle_override do projeto
        "builtin_certs": False,
        "module_raycast_enabled": False,
        "module_lightmapper_rd_enabled": False,
        "module_betsy_enabled": False,
        "module_glslang_enabled": False,
        "module_jolt_physics_enabled": False,
        # a biblioteca manifold (CSG) usa nomes que a newlib define como macro (_S, _U...); o jogo não usa CSG
        "module_csg_enabled": False,
        "module_text_server_adv_enabled": False,
        "module_text_server_fb_enabled": True,
        "supported": [],
    }


def configure(env: "SConsEnvironment"):
    validate_arch(env["arch"], get_name(), ["arm64"])

    dkp = os.environ["DEVKITPRO"]
    ferramenta = dkp + "/devkitA64/bin/aarch64-none-elf-"
    env["CC"] = ferramenta + "gcc"
    env["CXX"] = ferramenta + "g++"
    env["LINK"] = ferramenta + "g++"
    env["AS"] = ferramenta + "as"
    env["AR"] = ferramenta + "gcc-ar"
    env["RANLIB"] = ferramenta + "gcc-ranlib"
    env["ENV"]["DEVKITPRO"] = dkp
    env["ENV"]["PATH"] = dkp + "/portlibs/switch/bin:" + dkp + "/devkitA64/bin:" + env["ENV"]["PATH"]

    arquitetura = ["-march=armv8-a+crc+crypto", "-mtune=cortex-a57", "-mtp=soft", "-fPIE"]
    env.Append(CCFLAGS=arquitetura + ["-ffunction-sections", "-fdata-sections"])
    env.Append(
        LINKFLAGS=arquitetura
        + ["-specs=" + dkp + "/libnx/switch.specs", "-Wl,--gc-sections", "-Wl,--wrap=pthread_create", "-Wl,--wrap=_write_r"]
    )

    if env["lto"] == "auto":
        env["lto"] = "full"
    if env["lto"] != "none":
        env.Append(CCFLAGS=["-flto"])
        env.Append(LINKFLAGS=["-flto=auto"])

    if env["mesa"]:
        # o Mesa escolhido vem antes dos portlibs: os cabeçalhos e as bibliotecas dele valem
        env.Append(CPPPATH=[env["mesa"] + "/include"])
        env.Append(LIBPATH=[env["mesa"] + "/lib"])
    env.Append(CPPPATH=["#platform/switch", "#platform/switch/compat", dkp + "/portlibs/switch/include", dkp + "/libnx/include"])
    env.Append(LIBPATH=[dkp + "/portlibs/switch/lib", dkp + "/libnx/lib"])
    env.Append(
        CPPDEFINES=[
            "__SWITCH__",
            # HORIZON_ENABLED também guia o remendo 03-rede-horizon.patch (docs/decisoes.md, D-18): sem AF_UNIX,
            # sem IPv6 e sem getifaddrs no drivers/unix; o resto da rede (IP, sockets UDP/TCP) é o do Unix
            "HORIZON_ENABLED",
            "UNIX_ENABLED",
            # a newlib não tem pthread_setname_np
            "PTHREAD_NO_RENAME",
        ]
    )
    if env["nxlink"]:
        env.Append(CPPDEFINES=["NXLINK_ENABLED"])
    if env["vulkan"]:
        # Vulkan pelo NVK (docs/decisoes.md): sem carregador (use_volk=no), ligado direto; a superfície é a VK_NN_vi_surface
        env.Append(CPPDEFINES=["VULKAN_ENABLED", "RD_ENABLED", "VK_USE_PLATFORM_VI_NN"])
        # a espera pelas cercas e o envio medidos no pulso (rendering_context_driver_vulkan_switch.cpp, D-15)
        env.Append(LINKFLAGS=["-Wl,--wrap=vkWaitForFences", "-Wl,--wrap=vkQueueSubmit"])
        # o tamanho da troca de quadros pelo modo (na base, 1920×1080; rendering_context_driver_vulkan_switch.cpp, D-20)
        env.Append(LINKFLAGS=["-Wl,--wrap=vkGetInstanceProcAddr"])
        if env["nvk"]:
            env.Append(LIBPATH=[env["nvk"]])
        # do build-switch.sh do mesa-switch: o libvulkan.a já junta o NVK, o NIR, o NAK (com o Rust) e o runtime
        # de Vulkan; faltam os utilitários (ligar o libnvk.a, o libnir.a etc. junto duplicaria definições).
        # Se chamam uns aos outros: o grupo vai duas vezes. dl, rt e util são vazias (o Rust as pede)
        # nak_rs: a biblioteca do Rust (core, alloc, std) que o código em Rust de dentro do libvulkan.a chama
        nvk = ["vulkan", "nak_rs", "nouveau_ws", "nvidia_headers_c", "xmlconfig", "mesa_util", "mesa_util_simd",
               "blake3", "mesa_util_c11", "expat", "zstd", "z"]
        env.Append(LIBS=nvk + nvk + ["dl", "rt", "util"])
    if not env["opengl3"]:
        env.Append(LIBS=["nx", "stdc++", "m"])
        return
    env.Append(CPPDEFINES=["GLES3_ENABLED"])
    if env["mesa"]:
        # Mesa 26 (danfromtico/mesa-switch): o driver vai dentro do libEGL, e a camada do Switch dispensa o
        # drm_nouveau. As bibliotecas se chamam umas às outras: o grupo vai duas vezes (a OpenGLConfig.cmake
        # dele faz o mesmo, um grupo com nova varredura)
        grupo = ["EGL", "GLESv2", "glapi", "mesa_util_c11", "blake3", "mesa_util", "mesa_util_simd", "xmlconfig", "expat", "zstd", "z"]
        env.Append(LIBS=grupo + grupo + ["nx", "stdc++", "m"])
    else:
        # a ordem importa: o mesa (EGL, GLES) depende do glapi e do nouveau, e tudo da libnx
        env.Append(LIBS=["EGL", "GLESv2", "glapi", "drm_nouveau", "nx", "m"])
