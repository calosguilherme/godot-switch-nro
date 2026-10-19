/**************************************************************************/
/*  joypad_switch.cpp                                                     */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/* Referência: o port do Godot 3.5 de Stary2001 e cpasjuste (MIT).        */
/**************************************************************************/

#include "joypad_switch.h"

// O Godot 4 nomeia os botões pela posição: A é o de baixo, B o da direita, X o da esquerda e Y o
// de cima. No controle da Nintendo, o de baixo é o B, o da direita o A, e assim por diante.
struct Botao {
	u64 nx;
	JoyButton godot;
};

static const Botao BOTOES[] = {
	{ HidNpadButton_B, JoyButton::A },
	{ HidNpadButton_A, JoyButton::B },
	{ HidNpadButton_Y, JoyButton::X },
	{ HidNpadButton_X, JoyButton::Y },
	{ HidNpadButton_Minus, JoyButton::BACK },
	{ HidNpadButton_Plus, JoyButton::START },
	{ HidNpadButton_StickL, JoyButton::LEFT_STICK },
	{ HidNpadButton_StickR, JoyButton::RIGHT_STICK },
	{ HidNpadButton_L, JoyButton::LEFT_SHOULDER },
	{ HidNpadButton_R, JoyButton::RIGHT_SHOULDER },
	{ HidNpadButton_Up, JoyButton::DPAD_UP },
	{ HidNpadButton_Down, JoyButton::DPAD_DOWN },
	{ HidNpadButton_Left, JoyButton::DPAD_LEFT },
	{ HidNpadButton_Right, JoyButton::DPAD_RIGHT },
};

JoypadSwitch::JoypadSwitch(Input *p_input) {
	input = p_input;
	padConfigureInput(JOGADORES, HidNpadStyleSet_NpadStandard);
	padInitialize(&pads[0], HidNpadIdType_No1, HidNpadIdType_Handheld);
	padInitialize(&pads[1], HidNpadIdType_No2);
}

JoypadSwitch::~JoypadSwitch() {
	for (int i = 0; i < JOGADORES; i++) {
		if (vibrando[i]) {
			_vibrar(i, 0.0f, 0.0f);
		}
	}
}

// Os dois motores de cada controle: o forte vai na faixa grave (160 Hz) e o fraco na aguda (320 Hz).
void JoypadSwitch::_vibrar(int p_jogador, float p_fraco, float p_forte) {
	PadState &pad = pads[p_jogador];
	u32 estilo = padGetStyleSet(&pad);
	HidNpadIdType id = p_jogador == 0 ? (padIsHandheld(&pad) ? HidNpadIdType_Handheld : HidNpadIdType_No1) : HidNpadIdType_No2;
	HidNpadStyleTag tag = (estilo & HidNpadStyleTag_NpadHandheld) ? HidNpadStyleTag_NpadHandheld : ((estilo & HidNpadStyleTag_NpadFullKey) ? HidNpadStyleTag_NpadFullKey : HidNpadStyleTag_NpadJoyDual);
	if (estilo_dos_motores[p_jogador] != (u32)tag) {
		if (R_FAILED(hidInitializeVibrationDevices(motores[p_jogador], 2, id, tag))) {
			return;
		}
		estilo_dos_motores[p_jogador] = (u32)tag;
	}
	HidVibrationValue v = {};
	v.amp_low = CLAMP(p_forte, 0.0f, 1.0f);
	v.freq_low = 160.0f;
	v.amp_high = CLAMP(p_fraco, 0.0f, 1.0f);
	v.freq_high = 320.0f;
	HidVibrationValue valores[2] = { v, v };
	hidSendVibrationValues(motores[p_jogador], valores, 2);
	vibrando[p_jogador] = p_fraco > 0.0f || p_forte > 0.0f;
}

void JoypadSwitch::_cuidar_da_vibracao(int p_jogador) {
	const uint64_t agora_ms = armTicksToNs(armGetSystemTick()) / 1000000;
	const uint64_t pedido = input->get_joy_vibration_timestamp(p_jogador);
	if (pedido != pedido_visto[p_jogador]) {
		pedido_visto[p_jogador] = pedido;
		const Vector2 forca = input->get_joy_vibration_strength(p_jogador);
		const float duracao = input->get_joy_vibration_duration(p_jogador);
		_vibrar(p_jogador, forca.x, forca.y);
		vibra_ate_ms[p_jogador] = duracao > 0.0f ? agora_ms + (uint64_t)(duracao * 1000.0f) : UINT64_MAX;
	} else if (vibrando[p_jogador] && agora_ms >= vibra_ate_ms[p_jogador]) {
		_vibrar(p_jogador, 0.0f, 0.0f);
	}
}

void JoypadSwitch::process() {
	for (int j = 0; j < JOGADORES; j++) {
		PadState &pad = pads[j];
		padUpdate(&pad);

		bool agora = padIsConnected(&pad);
		if (agora != conectado[j]) {
			conectado[j] = agora;
			Dictionary info;
			info["vendor_id"] = itos(0x057E);
			info["product_id"] = itos(padIsHandheld(&pad) ? 0x2008 : 0x2009);
			input->joy_connection_changed(j, agora, agora ? "Nintendo Switch Controller" : "", "", info);
		}
		if (!agora) {
			continue;
		}
		_cuidar_da_vibracao(j);

		u64 desceu = padGetButtonsDown(&pad);
		u64 subiu = padGetButtonsUp(&pad);
		if (desceu || subiu) {
			for (const Botao &b : BOTOES) {
				if (desceu & b.nx) {
					input->joy_button(j, b.godot, true);
				}
				if (subiu & b.nx) {
					input->joy_button(j, b.godot, false);
				}
			}
		}

		// ZL e ZR são digitais no Switch; o Godot os lê como gatilhos (eixos de 0 a 1)
		u64 segurados = padGetButtons(&pad);
		bool zl = (segurados & HidNpadButton_ZL) != 0;
		bool zr = (segurados & HidNpadButton_ZR) != 0;
		if (zl != gatilho[j][0]) {
			gatilho[j][0] = zl;
			input->joy_axis(j, JoyAxis::TRIGGER_LEFT, zl ? 1.0f : 0.0f);
		}
		if (zr != gatilho[j][1]) {
			gatilho[j][1] = zr;
			input->joy_axis(j, JoyAxis::TRIGGER_RIGHT, zr ? 1.0f : 0.0f);
		}

		HidAnalogStickState esquerdo = padGetStickPos(&pad, 0);
		HidAnalogStickState direito = padGetStickPos(&pad, 1);
		input->joy_axis(j, JoyAxis::LEFT_X, esquerdo.x / 32767.0f);
		input->joy_axis(j, JoyAxis::LEFT_Y, -esquerdo.y / 32767.0f);
		input->joy_axis(j, JoyAxis::RIGHT_X, direito.x / 32767.0f);
		input->joy_axis(j, JoyAxis::RIGHT_Y, -direito.y / 32767.0f);
	}
}
