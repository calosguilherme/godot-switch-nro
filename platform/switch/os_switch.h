/**************************************************************************/
/*  os_switch.h                                                           */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/* Referência: o port do Godot 3.5 de Stary2001 e cpasjuste (MIT).        */
/**************************************************************************/

#pragma once

#include "audio_driver_switch.h"
#include "joypad_switch.h"

#include "drivers/unix/os_unix.h"
#include "servers/audio/audio_server.h"

class OS_Switch : public OS_Unix {
	MainLoop *main_loop = nullptr;
	AudioDriverSwitch audio_driver_switch;
	JoypadSwitch *joypad = nullptr;

protected:
	void initialize() override;
	void initialize_joypads() override;
	void set_main_loop(MainLoop *p_main_loop) override;
	void delete_main_loop() override;
	void finalize() override;
	bool _check_internal_feature_support(const String &p_feature) override;

public:
	String get_name() const override { return "Switch"; }
	String get_distribution_name() const override { return "Horizon"; }
	String get_version() const override;
	MainLoop *get_main_loop() const override { return main_loop; }

	String get_executable_path() const override;
	String get_data_path() const override;
	String get_config_path() const override;
	String get_cache_path() const override;
	using OS_Unix::get_user_data_dir;
	String get_user_data_dir(const String &p_user_dir) const override;

	Error shell_open(const String &p_uri) override { return ERR_UNAVAILABLE; }
	// o idioma do console (o OS_Unix lê a variável LANG, que o Switch não tem: saía sempre "en")
	String get_locale() const override;
	Error get_entropy(uint8_t *r_buffer, int p_bytes) override;
	// o homebrew roda em 3 dos 4 núcleos (o 4º é do sistema)
	int get_processor_count() const override { return 3; }

	void run();

	// Cache de shaders (docs/decisoes.md): conta os arquivos e os bytes de dados/shader_cache (o do Godot) e de
	// dados/mesa_cache (o do driver) e escreve no log, com `p_quando` ("na abertura", "com o menu"...).
	static void contar_cache(const char *p_quando);

	// Botão HOME e fechar pelo sistema: o hook do applet avisa o jogo (pausa ao sair de foco; pedido de
	// fechar antes de sair). Chamados de dentro do appletMainLoop, na thread principal.
	bool focado = true;
	void foco_mudou(int p_tipo);
	void pausar_o_jogo(const char *p_motivo);
	// a sentinela do HOME (ver OS_Switch::run): `suspenso` vira verdadeiro quando ela percebe que o processo
	// esteve congelado; o laço principal pausa a partida e o apaga
	static volatile bool suspenso;
	static volatile bool sentinela_ligada;
	static void _sentinela(void *p_nada);

	// Aceleração do processador na abertura (o FastLoad do sistema, que os jogos usam nas telas de
	// carregamento): o processador vai a 1785 MHz e a placa de vídeo ao mínimo. Liga antes do Main::setup
	// e desliga no começo do laço principal, de vez: com a placa no mínimo os quadros do jogo caem a
	// 4-8 FPS (versões 0.2.1 a 0.2.4). Só no modo título (pelo álbum o sistema não deixa).
	// laço principal parado mais que isto entre dois quadros: o sistema suspendeu o jogo (HOME)
	static const int SUSPENSO_MS = 1500;
	static bool acelerado;
	static void acelerar(bool p_ligar);

	// números da renderização, guardados pelo laço principal a cada segundo para o pulso do log
	struct Medidas {
		bool prontas = false;
		double processo_ms = 0.0; // script e cenas (o process do Godot), média por quadro
		double fisica_ms = 0.0; // os passos de física do quadro, média
		double desenhos = 0.0;
		double objetos = 0.0;
		double video_mib = 0.0;
		double quadro_ms = 0.0; // a iteração inteira do motor, em média
		double troca_ms = 0.0; // dentro dela, esperando a placa de vídeo (eglSwapBuffers; só o OpenGL)
		double gpu_ms = 0.0; // a placa de vídeo desenhando a tela (timestamps do Godot; 0 se o driver não mede)
		double desenho_ms = 0.0; // o motor montando o desenho da tela na CPU (cena, culling, listas)
	};
	static Medidas medidas;

	// caminho do .nro (argv[0]), guardado antes do Main::setup (godot_switch.cpp)
	static const char *caminho_nro;
	// Rede (docs/decisoes.md, D-18; rede_switch.cpp): escreve no log o endereço do console e o tipo de
	// conexão (ou "sem conexão"); `p_sockets` é o resultado do socketInitializeDefault
	static void relatar_rede(uint32_t p_sockets);
	// a versão do jogo, que o gsn.py grava em romfs:/versao.txt (a mesma do .pck); "?" sem o arquivo
	static char versao_do_jogo[32];
	static void ler_versao_do_jogo();
	// <jogo>.vulkan: a marca da abertura (dados/vulkan_abrindo), apagada depois de QUADROS_RODANDO quadros
	static const char *marca_vulkan;
	static const int QUADROS_RODANDO = 300;

	OS_Switch();
};
