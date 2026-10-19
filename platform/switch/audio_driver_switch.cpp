/**************************************************************************/
/*  audio_driver_switch.cpp                                               */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/* Referência: o port do Godot 3.5 de Stary2001 e cpasjuste (MIT).        */
/**************************************************************************/

#include "audio_driver_switch.h"

#include "core/os/os.h"

#include <malloc.h>
#include <cstdio>
#include <cstring>

static const AudioRendererConfig CONFIG_AUDREN = {
	.output_rate = AudioRendererOutputRate_48kHz,
	.num_voices = 4,
	.num_effects = 0,
	.num_sinks = 1,
	.num_mix_objs = 1,
	.num_mix_buffers = 2,
};

Error AudioDriverSwitch::init() {
	mix_rate = 48000;
	speaker_mode = SPEAKER_MODE_STEREO;
	amostras_godot.resize(frames * CANAIS);

	Result r = audrenInitialize(&CONFIG_AUDREN);
	if (R_FAILED(r)) {
		printf("audrenInitialize falhou: 0x%x\n", r);
		return ERR_CANT_OPEN;
	}
	r = audrvCreate(&audren, &CONFIG_AUDREN, CANAIS);
	if (R_FAILED(r)) {
		printf("audrvCreate falhou: 0x%x\n", r);
		audrenExit();
		return ERR_CANT_OPEN;
	}

	// a memória que o sistema lê precisa estar alinhada a 4 KiB e ter tamanho múltiplo de 4 KiB
	bytes_por_buffer = sizeof(int16_t) * frames * CANAIS;
	size_t tamanho_pool = (bytes_por_buffer * BUFFERS + 0xFFF) & ~0xFFF;
	pool = memalign(0x1000, tamanho_pool);
	memset(pool, 0, tamanho_pool);
	armDCacheFlush(pool, tamanho_pool);
	for (int i = 0; i < BUFFERS; i++) {
		memset(&ondas[i], 0, sizeof(AudioDriverWaveBuf));
		ondas[i].data_raw = pool;
		ondas[i].size = tamanho_pool;
		ondas[i].start_sample_offset = i * frames;
		ondas[i].end_sample_offset = ondas[i].start_sample_offset + frames;
	}
	int id_pool = audrvMemPoolAdd(&audren, pool, tamanho_pool);
	audrvMemPoolAttach(&audren, id_pool);

	static const u8 canais_saida[] = { 0, 1 };
	audrvDeviceSinkAdd(&audren, AUDREN_DEFAULT_DEVICE_NAME, CANAIS, canais_saida);
	audrvUpdate(&audren);
	audrenStartAudioRenderer();

	audrvVoiceInit(&audren, 0, CANAIS, PcmFormat_Int16, mix_rate);
	audrvVoiceSetDestinationMix(&audren, 0, AUDREN_FINAL_MIX_ID);
	audrvVoiceSetMixFactor(&audren, 0, 1.0f, 0, 0);
	audrvVoiceSetMixFactor(&audren, 0, 0.0f, 0, 1);
	audrvVoiceSetMixFactor(&audren, 0, 0.0f, 1, 0);
	audrvVoiceSetMixFactor(&audren, 0, 1.0f, 1, 1);
	audrvVoiceStart(&audren, 0);
	audren_ok = true;
	printf("[switch] áudio pronto: audren 48 kHz, %u amostras por bloco\n", frames);

	sair.clear();
	ativo.clear();
	thread.start(AudioDriverSwitch::thread_func, this);
	return OK;
}

void AudioDriverSwitch::thread_func(void *p_udata) {
	AudioDriverSwitch *ad = static_cast<AudioDriverSwitch *>(p_udata);
	while (!ad->sair.is_set()) {
		// acha um buffer livre (o sistema já tocou) para encher
		int livre = -1;
		for (int i = 0; i < BUFFERS; i++) {
			if (ad->ondas[i].state == AudioDriverWaveBufState_Free || ad->ondas[i].state == AudioDriverWaveBufState_Done) {
				livre = i;
				break;
			}
		}
		if (livre < 0) {
			audrvUpdate(&ad->audren);
			audrenWaitFrame();
			continue;
		}

		int16_t *saida = (int16_t *)((uint8_t *)ad->pool + livre * ad->bytes_por_buffer);
		const int n = ad->frames * CANAIS;
		if (ad->ativo.is_set()) {
			ad->lock();
			ad->start_counting_ticks();
			ad->audio_server_process(ad->frames, ad->amostras_godot.ptrw());
			ad->stop_counting_ticks();
			ad->unlock();
			const int32_t *entrada = ad->amostras_godot.ptr();
			for (int i = 0; i < n; i++) {
				saida[i] = (int16_t)(entrada[i] >> 16);
			}
		} else {
			memset(saida, 0, ad->bytes_por_buffer);
		}
		armDCacheFlush(saida, ad->bytes_por_buffer);
		audrvVoiceAddWaveBuf(&ad->audren, 0, &ad->ondas[livre]);
		if (!audrvVoiceIsPlaying(&ad->audren, 0)) {
			audrvVoiceStart(&ad->audren, 0);
		}
		audrvUpdate(&ad->audren);
	}
}

void AudioDriverSwitch::start() {
	ativo.set();
}

void AudioDriverSwitch::finish() {
	sair.set();
	if (thread.is_started()) {
		thread.wait_to_finish();
	}
	if (audren_ok) {
		audrvClose(&audren);
		audrenExit();
		audren_ok = false;
	}
	if (pool) {
		free(pool);
		pool = nullptr;
	}
}
