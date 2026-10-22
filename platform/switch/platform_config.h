/**************************************************************************/
/*  platform_config.h                                                     */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/**************************************************************************/

#pragma once

#include <alloca.h>

// a configuração do mbedTLS do Switch (sem os sockets dele; relógio da plataforma): mbedtls_config_switch.h
#define GODOT_MBEDTLS_INCLUDE_H "mbedtls_config_switch.h"
