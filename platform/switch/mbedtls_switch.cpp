/**************************************************************************/
/*  mbedtls_switch.cpp                                                    */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/**************************************************************************/

// O relógio do mbedTLS no Switch (mbedtls_config_switch.h, docs/decisoes.md D-18): o mbedTLS só tem o de Unix e o do
// Windows. Tudo em milissegundos, pelo contador do processador (armGetSystemTick, 19,2 MHz). As regras são as do
// timing.c do mbedTLS: o DTLS espera `int_ms` (1) e desiste em `fin_ms` (2); fin_ms = 0 desliga (-1).

#include "timing_alt.h"

#include "switch_wrapper.h"

static uint64_t _ms_desde(uint64_t p_tick) {
	return armTicksToNs(armGetSystemTick() - p_tick) / 1000000ULL;
}

extern "C" {

int64_t mbedtls_ms_time(void) {
	return (int64_t)(armTicksToNs(armGetSystemTick()) / 1000000ULL);
}

unsigned long mbedtls_timing_get_timer(struct mbedtls_timing_hr_time *val, int reset) {
	if (reset) {
		val->inicio = armGetSystemTick();
		return 0;
	}
	return (unsigned long)_ms_desde(val->inicio);
}

void mbedtls_timing_set_delay(void *data, uint32_t int_ms, uint32_t fin_ms) {
	mbedtls_timing_delay_context *ctx = (mbedtls_timing_delay_context *)data;
	ctx->int_ms = int_ms;
	ctx->fin_ms = fin_ms;
	if (fin_ms != 0) {
		mbedtls_timing_get_timer(&ctx->timer, 1);
	}
}

int mbedtls_timing_get_delay(void *data) {
	mbedtls_timing_delay_context *ctx = (mbedtls_timing_delay_context *)data;
	if (ctx->fin_ms == 0) {
		return -1;
	}
	const unsigned long passou = mbedtls_timing_get_timer(&ctx->timer, 0);
	if (passou >= ctx->fin_ms) {
		return 2;
	}
	if (passou >= ctx->int_ms) {
		return 1;
	}
	return 0;
}

uint32_t mbedtls_timing_get_final_delay(const mbedtls_timing_delay_context *data) {
	return data->fin_ms;
}

} // extern "C"
