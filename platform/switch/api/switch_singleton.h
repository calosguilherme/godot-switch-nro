/**************************************************************************/
/*  switch_singleton.h                                                    */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/**************************************************************************/

#pragma once

#include "core/object/object.h"
#include "core/variant/variant.h"

// O que o jogo pode pedir ao console, pelo singleton "Switch" (só existe no Switch):
//   if Engine.has_singleton("Switch"): Engine.get_singleton("Switch").definir_turbo(true)
class SwitchHorizon : public Object {
	GDCLASS(SwitchHorizon, Object);

	static SwitchHorizon *singleton;

	bool turbo_pedido = false;
	bool turbo_aplicado = false;
	int modelo_cache = -1;

protected:
	static void _bind_methods();

public:
	static SwitchHorizon *get_singleton() { return singleton; }

	// Fonte compartilhada do sistema (0 padrão/japonês, 1 chinês simplificado, 2 chinês simplificado
	// estendido, 3 chinês tradicional, 4 coreano, 5 símbolos Nintendo): os bytes do TTF, ou vazio.
	PackedByteArray fonte_do_sistema(int p_tipo);

	// a versão do jogo neste .nro (romfs:/versao.txt, gravada pelo gsn.py; a mesma do .pck); "?" sem ela
	String versao();
	// a versão do godot-switch-nro (o motor) com que o .nro foi compilado
	String versao_do_motor();
	// "erista" (o Switch de 2017) ou "mariko" (V2, Lite e OLED: chip mais novo, esquenta e gasta menos).
	String modelo();
	bool na_base();
	bool carregando();

	// Turbo: o processador em 1785 MHz durante o jogo (o mesmo teto do modo oficial de carregamento),
	// sem baixar a placa de vídeo. No Erista só vale na base ou carregando. Desligado por padrão.
	void definir_turbo(bool p_ligado);
	// "desligado", "ligado" ou "esperando" (pedido, mas o Erista está fora da base e sem carregador).
	String estado_do_turbo();

	// Chamado pelo laço principal (OS_Switch::run) a cada ~1 s: o sistema volta o processador ao padrão
	// quando muda de modo (base ↔ portátil), então o turbo é reaplicado. `p_acelerado`: a aceleração de
	// carregamento (FastLoad) está ligada e já cuida do processador.
	void manter_turbo(bool p_acelerado);
	// Volta o processador ao padrão (ao sair).
	void soltar_turbo();

	// OpenGL ou Vulkan (docs/decisoes.md): "opengl" no <jogo>.nro, "vulkan" no <jogo>.vulkan.
	String renderizador();
	// O <jogo>.vulkan está na pasta do jogo (a escolha só aparece com ele).
	bool tem_vulkan();
	// Pede ao carregador do homebrew para abrir o <jogo>.nro quando o jogo fechar (ele aplica a escolha
	// gravada em dados/renderizador.txt); o jogo fecha em seguida. false: o carregador não troca de .nro.
	bool reabrir();

	SwitchHorizon();
	~SwitchHorizon();
};
