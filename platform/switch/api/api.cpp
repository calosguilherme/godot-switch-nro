/**************************************************************************/
/*  api.cpp                                                               */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/**************************************************************************/

#include "api.h"

#ifdef HORIZON_ENABLED

#include "switch_singleton.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "core/os/os.h"

#include "../os_switch.h"
#include "../switch_wrapper.h"
#include "../versao.gen.h"

#include <cstdio>

static SwitchHorizon *switch_singleton = nullptr;

void register_switch_api() {
	GDREGISTER_ABSTRACT_CLASS(SwitchHorizon);
	switch_singleton = memnew(SwitchHorizon);
	Engine::get_singleton()->add_singleton(Engine::Singleton("Switch", switch_singleton));
}

void unregister_switch_api() {
	if (switch_singleton) {
		switch_singleton->soltar_turbo();
		memdelete(switch_singleton);
		switch_singleton = nullptr;
	}
}

SwitchHorizon *SwitchHorizon::singleton = nullptr;

// os valores oficiais do processador: 1020 MHz é o de todos os modos de jogo; 1785 MHz é o teto que o
// próprio sistema usa no modo de carregamento (FastLoad)
static const u32 CPU_PADRAO_HZ = 1020000000;
static const u32 CPU_TURBO_HZ = 1785000000;

static bool _definir_cpu(u32 p_hz) {
	if (hosversionAtLeast(8, 0, 0)) {
		if (R_FAILED(clkrstInitialize())) {
			return false;
		}
		ClkrstSession sessao;
		Result r = clkrstOpenSession(&sessao, PcvModuleId_CpuBus, 3);
		if (R_SUCCEEDED(r)) {
			r = clkrstSetClockRate(&sessao, p_hz);
			clkrstCloseSession(&sessao);
		}
		clkrstExit();
		return R_SUCCEEDED(r);
	}
	if (R_FAILED(pcvInitialize())) {
		return false;
	}
	Result r = pcvSetClockRate(PcvModule_CpuBus, p_hz);
	pcvExit();
	return R_SUCCEEDED(r);
}

void SwitchHorizon::_bind_methods() {
	ClassDB::bind_method(D_METHOD("fonte_do_sistema", "tipo"), &SwitchHorizon::fonte_do_sistema);
	ClassDB::bind_method(D_METHOD("modelo"), &SwitchHorizon::modelo);
	ClassDB::bind_method(D_METHOD("na_base"), &SwitchHorizon::na_base);
	ClassDB::bind_method(D_METHOD("carregando"), &SwitchHorizon::carregando);
	ClassDB::bind_method(D_METHOD("definir_turbo", "ligado"), &SwitchHorizon::definir_turbo);
	ClassDB::bind_method(D_METHOD("estado_do_turbo"), &SwitchHorizon::estado_do_turbo);
	ClassDB::bind_method(D_METHOD("versao"), &SwitchHorizon::versao);
	ClassDB::bind_method(D_METHOD("versao_do_motor"), &SwitchHorizon::versao_do_motor);
	ClassDB::bind_method(D_METHOD("renderizador"), &SwitchHorizon::renderizador);
	ClassDB::bind_method(D_METHOD("tem_vulkan"), &SwitchHorizon::tem_vulkan);
	ClassDB::bind_method(D_METHOD("reabrir"), &SwitchHorizon::reabrir);
}

PackedByteArray SwitchHorizon::fonte_do_sistema(int p_tipo) {
	PackedByteArray bytes;
	if (p_tipo < 0 || p_tipo >= PlSharedFontType_Total) {
		return bytes;
	}
	if (R_FAILED(plInitialize(PlServiceType_User))) {
		return bytes;
	}
	PlFontData fonte = {};
	if (R_SUCCEEDED(plGetSharedFontByType(&fonte, (PlSharedFontType)p_tipo)) && fonte.address && fonte.size > 0) {
		bytes.resize(fonte.size);
		memcpy(bytes.ptrw(), fonte.address, fonte.size);
	}
	plExit();
	return bytes;
}

String SwitchHorizon::versao() {
	return String::utf8(OS_Switch::versao_do_jogo);
}

String SwitchHorizon::versao_do_motor() {
	return GSN_VERSAO;
}

String SwitchHorizon::renderizador() {
#if defined(GLES3_ENABLED)
	return "opengl";
#else
	return "vulkan";
#endif
}

bool SwitchHorizon::tem_vulkan() {
	const String caminho = OS::get_singleton()->get_executable_path().get_base_dir().path_join(OS::get_singleton()->get_executable_path().get_file().get_basename() + ".vulkan");
	FILE *f = fopen(caminho.utf8().get_data(), "rb");
	if (!f) {
		return false;
	}
	fclose(f);
	return true;
}

bool SwitchHorizon::reabrir() {
	const String nro = OS::get_singleton()->get_executable_path().get_base_dir().path_join(OS::get_singleton()->get_executable_path().get_file().get_basename() + ".nro");
	if (!envHasNextLoad()) {
		printf("[switch] reabrir: o carregador não troca de .nro\n");
		return false;
	}
	const CharString caminho = nro.utf8();
	const CharString linha = ("\"" + nro + "\"").utf8();
	const Result r = envSetNextLoad(caminho.get_data(), linha.get_data());
	printf("[switch] reabrir pelo %s (0x%x)\n", caminho.get_data(), r);
	return R_SUCCEEDED(r);
}

String SwitchHorizon::modelo() {
	if (modelo_cache < 0) {
		modelo_cache = 0;
		if (R_SUCCEEDED(splInitialize())) {
			u64 tipo = 0;
			if (R_SUCCEEDED(splGetConfig(SplConfigItem_HardwareType, &tipo))) {
				// 0 Icosa e 1 Copper são o chip antigo (Erista); 2 Hoag (Lite), 3 Iowa (V2), 4 Calcio e 5 Aula (OLED), o novo
				modelo_cache = tipo >= 2 ? 1 : 0;
			}
			splExit();
		}
	}
	return modelo_cache == 1 ? "mariko" : "erista";
}

bool SwitchHorizon::na_base() {
	return appletGetOperationMode() == AppletOperationMode_Console;
}

bool SwitchHorizon::carregando() {
	bool sim = false;
	if (R_SUCCEEDED(psmInitialize())) {
		PsmChargerType tipo = PsmChargerType_Unconnected;
		sim = R_SUCCEEDED(psmGetChargerType(&tipo)) && tipo != PsmChargerType_Unconnected;
		psmExit();
	}
	return sim;
}

void SwitchHorizon::definir_turbo(bool p_ligado) {
	turbo_pedido = p_ligado;
	manter_turbo(false);
	printf("[switch] turbo %s: %s, %s, %s -> %s\n", p_ligado ? "pedido" : "desligado pelo jogador", modelo().utf8().get_data(),
			na_base() ? "na base" : "fora da base", carregando() ? "carregando" : "sem carregador", estado_do_turbo().utf8().get_data());
}

String SwitchHorizon::estado_do_turbo() {
	if (!turbo_pedido) {
		return "desligado";
	}
	return turbo_aplicado ? "ligado" : "esperando";
}

void SwitchHorizon::manter_turbo(bool p_acelerado) {
	if (p_acelerado) {
		return; // a aceleração de carregamento já está no teto
	}
	// vale em qualquer modelo e modo (pedido do criador, 2026-10-18): 1785 MHz é o mesmo teto que o
	// próprio sistema usa nas telas de carregamento, também no portátil; o custo é a bateria
	const bool pode = turbo_pedido;
	if (pode) {
		// reaplica sempre: o sistema volta ao padrão quando o modo muda
		if (_definir_cpu(CPU_TURBO_HZ) && !turbo_aplicado) {
			printf("[switch] turbo ligado (1785 MHz)\n");
		}
		turbo_aplicado = true;
	} else if (turbo_aplicado) {
		soltar_turbo();
	}
}

void SwitchHorizon::soltar_turbo() {
	if (turbo_aplicado) {
		_definir_cpu(CPU_PADRAO_HZ);
		turbo_aplicado = false;
		printf("[switch] turbo desligado\n");
	}
}

SwitchHorizon::SwitchHorizon() {
	singleton = this;
}

SwitchHorizon::~SwitchHorizon() {
	singleton = nullptr;
}

#else

void register_switch_api() {}
void unregister_switch_api() {}

#endif // HORIZON_ENABLED
