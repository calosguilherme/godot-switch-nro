/**************************************************************************/
/*  audio_driver_switch.h                                                 */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/* Referência: o port do Godot 3.5 de Stary2001 e cpasjuste (MIT).        */
/**************************************************************************/

#pragma once

#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/templates/safe_refcount.h"
#include "servers/audio/audio_server.h"

#include "switch_wrapper.h"

// Áudio pelo audren (o renderizador de áudio do sistema), estéreo a 48 kHz. Uma thread mistura um
// bloco do Godot por vez e entrega ao sistema em dois buffers que se revezam.
class AudioDriverSwitch : public AudioDriver {
	Thread thread;
	Mutex mutex;

	AudioDriver::SpeakerMode speaker_mode = AudioDriver::SPEAKER_MODE_STEREO;
	int mix_rate = 48000;
	static const int CANAIS = 2;
	static const int BUFFERS = 2;
	unsigned int frames = 1024;

	Vector<int32_t> amostras_godot;

	LibnxAudioDriver audren;
	AudioDriverWaveBuf ondas[BUFFERS];
	void *pool = nullptr;
	size_t bytes_por_buffer = 0;
	bool audren_ok = false;

	SafeFlag ativo;
	SafeFlag sair;

	static void thread_func(void *p_udata);

public:
	const char *get_name() const override { return "Switch"; }

	Error init() override;
	void start() override;
	int get_mix_rate() const override { return mix_rate; }
	SpeakerMode get_speaker_mode() const override { return speaker_mode; }
	float get_latency() override { return float(frames * BUFFERS) / mix_rate; }

	void lock() override { mutex.lock(); }
	void unlock() override { mutex.unlock(); }
	void finish() override;
};
