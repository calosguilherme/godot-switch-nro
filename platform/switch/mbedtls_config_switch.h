/**************************************************************************/
/*  mbedtls_config_switch.h                                               */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/**************************************************************************/

// A configuração do mbedTLS no Switch (docs/decisoes.md, D-18). O godot_module_mbedtls_config.h do Godot deixa a
// plataforma trocar a configuração (GODOT_MBEDTLS_INCLUDE_H, em platform_config.h): esta é a do Godot, com o que
// a libnx não tem. O mbedTLS só reconhece Unix e Windows para o relógio e os sockets; o devkitA64 não se diz Unix.

#pragma once

// a configuração padrão do mbedTLS e os ajustes do Godot (godot_module_mbedtls_config.h, 4.7.2)
#include <mbedtls/mbedtls_config.h>

#undef MBEDTLS_KEY_EXCHANGE_DHE_PSK_ENABLED
#undef MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED
#undef MBEDTLS_DES_C
#undef MBEDTLS_DHM_C

#ifdef THREADS_ENABLED
#define MBEDTLS_THREADING_C
#define MBEDTLS_THREADING_ALT
#define GODOT_MBEDTLS_THREADING_ALT
#endif

// os bytes aleatórios vêm do OS::get_entropy (o randomGet da libnx, OS_Switch::get_entropy)
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT

// o AES do processador: o mbedTLS só sabe detectá-lo no Linux (como o Godot faz fora do Linux)
#undef MBEDTLS_AESCE_C

#define MBEDTLS_DEPRECATED_REMOVED

// --- o que muda no Switch -------------------------------------------------------------------------------
// Os sockets do mbedTLS (net_sockets.c): o Godot não os usa (o TLS dele passa pelos StreamPeer/PacketPeer).
#undef MBEDTLS_NET_C
// O relógio e os temporizadores (do DTLS) vêm da plataforma, pelo contador do processador (mbedtls_switch.cpp,
// timing_alt.h).
#define MBEDTLS_PLATFORM_MS_TIME_ALT
#define MBEDTLS_TIMING_ALT
