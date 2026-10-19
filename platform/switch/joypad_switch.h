/**************************************************************************/
/*  joypad_switch.h                                                       */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/* Referência: o port do Godot 3.5 de Stary2001 e cpasjuste (MIT).        */
/**************************************************************************/

#pragma once

#include "core/input/input.h"

#include "switch_wrapper.h"

// Joy-Cons (juntos, separados ou presos no console) e Pro Controller como controles do Godot.
// Jogador 1: o 1º controle ou o console no modo portátil; jogador 2: o 2º controle.
class JoypadSwitch {
	static const int JOGADORES = 2;

	Input *input = nullptr;
	PadState pads[JOGADORES];
	bool conectado[JOGADORES] = {};
	bool gatilho[JOGADORES][2] = {};

	// vibração: os pedidos do Input (Input.start_joy_vibration) vão aos motores do controle
	HidVibrationDeviceHandle motores[JOGADORES][2];
	u32 estilo_dos_motores[JOGADORES] = {};
	uint64_t pedido_visto[JOGADORES] = {};
	uint64_t vibra_ate_ms[JOGADORES] = {};
	bool vibrando[JOGADORES] = {};
	void _vibrar(int p_jogador, float p_fraco, float p_forte);
	void _cuidar_da_vibracao(int p_jogador);

public:
	void process();

	JoypadSwitch(Input *p_input);
	// ao sair, os motores que estavam vibrando param (o processo segue vivo no carregador)
	~JoypadSwitch();
};
