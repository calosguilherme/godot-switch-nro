/**************************************************************************/
/*  timing_alt.h                                                          */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/**************************************************************************/

// Os tipos dos temporizadores do mbedTLS com MBEDTLS_TIMING_ALT (mbedtls_config_switch.h): o mbedtls/timing.h
// inclui este arquivo; as funções estão em mbedtls_switch.cpp.

#pragma once

#include <stdint.h>

struct mbedtls_timing_hr_time {
	uint64_t inicio; // armGetSystemTick() quando o temporizador foi zerado
};

typedef struct mbedtls_timing_delay_context {
	struct mbedtls_timing_hr_time timer;
	uint32_t int_ms;
	uint32_t fin_ms;
} mbedtls_timing_delay_context;
