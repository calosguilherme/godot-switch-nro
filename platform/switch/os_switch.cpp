/**************************************************************************/
/*  os_switch.cpp                                                         */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/* Referência: o port do Godot 3.5 de Stary2001 e cpasjuste (MIT).        */
/**************************************************************************/

#include "os_switch.h"

#include "display_server_switch.h"
#include "api/switch_singleton.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/os/main_loop.h"
#include "core/os/thread.h"
#include "core/input/input.h"
#include "main/main.h"
#include "main/performance.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/rendering/rendering_server.h"

#include "switch_wrapper.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>

OS_Switch::OS_Switch() {
	AudioDriverManager::add_driver(&audio_driver_switch);
	DisplayServerSwitch::register_switch_driver();
}

const char *OS_Switch::caminho_nro = nullptr;
char OS_Switch::versao_do_jogo[32] = "?";

void OS_Switch::ler_versao_do_jogo() {
	FILE *f = fopen("romfs:/versao.txt", "r");
	if (!f) {
		return;
	}
	char linha[32] = {};
	if (fgets(linha, sizeof(linha), f)) {
		linha[strcspn(linha, "\r\n ")] = 0;
		if (linha[0]) {
			snprintf(versao_do_jogo, sizeof(versao_do_jogo), "%s", linha);
		}
	}
	fclose(f);
}

static void _contar_pasta(const String &p_pasta, int &r_arquivos, uint64_t &r_bytes, int &r_vazios) {
	DIR *d = opendir(p_pasta.utf8().get_data());
	if (!d) {
		return;
	}
	struct dirent *e;
	while ((e = readdir(d)) != nullptr) {
		if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
			continue;
		}
		const String caminho = p_pasta.path_join(String::utf8(e->d_name));
		struct stat st = {};
		if (stat(caminho.utf8().get_data(), &st) != 0) {
			continue;
		}
		if (S_ISDIR(st.st_mode)) {
			_contar_pasta(caminho, r_arquivos, r_bytes, r_vazios);
		} else {
			r_arquivos++;
			r_bytes += st.st_size;
			// um .cache do Godot sem nenhum shader binário tem só o cabeçalho e as contagens (< 64 bytes)
			if (st.st_size < 64) {
				r_vazios++;
			}
		}
	}
	closedir(d);
}

void OS_Switch::contar_cache(const char *p_quando) {
	if (!caminho_nro) {
		return;
	}
	const String dados = String::utf8(caminho_nro).get_base_dir().path_join("dados");
	for (const char *nome : { "shader_cache", "mesa_cache" }) {
		int arquivos = 0;
		int vazios = 0;
		uint64_t bytes = 0;
		_contar_pasta(dados.path_join(nome), arquivos, bytes, vazios);
		printf("[switch] cache %s %s: %d arquivos (%d quase vazios), %llu KiB\n", nome, p_quando, arquivos, vazios, (unsigned long long)(bytes / 1024));
	}
}
const char *OS_Switch::marca_vulkan = nullptr;
bool OS_Switch::acelerado = false;

void OS_Switch::acelerar(bool p_ligar) {
	if (p_ligar == acelerado || appletGetAppletType() != AppletType_Application) {
		return;
	}
	if (R_SUCCEEDED(appletSetCpuBoostMode(p_ligar ? ApmCpuBoostMode_FastLoad : ApmCpuBoostMode_Normal))) {
		acelerado = p_ligar;
		printf("[switch] aceleração %s\n", p_ligar ? "ligada" : "desligada");
	}
}
OS_Switch::Medidas OS_Switch::medidas;

// A prioridade das threads do Godot (Thread.Priority no GDScript; docs/decisoes.md, D-15): o drivers/unix
// não a aplica, e toda pthread da libnx nasce em 59, abaixo da principal (44). PRIORITY_HIGH passa a 0x2B,
// acima da principal, como o pulso e a sentinela: para trabalho curto e periódico que não pode esperar a
// principal (o som misturado à frente); uma thread alta que não dorme toma o núcleo da principal. NORMAL e
// LOW ficam em 59.
static Error _nome_da_thread(const String &) {
	return ERR_UNAVAILABLE; // a newlib não tem pthread_setname_np (PTHREAD_NO_RENAME)
}

static void _prioridade_da_thread(Thread::Priority p_prioridade) {
	if (p_prioridade == Thread::PRIORITY_HIGH) {
		svcSetThreadPriority(threadGetCurHandle(), 0x2B);
	}
}

void OS_Switch::initialize() {
	OS_Unix::initialize_core();
	// depois do initialize_core, que registra as do drivers/unix (só o nome)
	Thread::_set_platform_functions({ .set_name = _nome_da_thread, .set_priority = _prioridade_da_thread });
}

void OS_Switch::initialize_joypads() {
	joypad = memnew(JoypadSwitch(Input::get_singleton()));
}

void OS_Switch::set_main_loop(MainLoop *p_main_loop) {
	main_loop = p_main_loop;
}

void OS_Switch::delete_main_loop() {
	if (main_loop) {
		memdelete(main_loop);
	}
	main_loop = nullptr;
}

void OS_Switch::finalize() {
	if (joypad) {
		memdelete(joypad);
		joypad = nullptr;
	}
	delete_main_loop();
}

bool OS_Switch::_check_internal_feature_support(const String &p_feature) {
	return p_feature == "switch" || p_feature == "nintendo_switch" || p_feature == "arm64";
}

// Sem /dev/urandom: os bytes aleatórios vêm do gerador do próprio console (libnx).
Error OS_Switch::get_entropy(uint8_t *r_buffer, int p_bytes) {
	randomGet(r_buffer, p_bytes);
	return OK;
}

String OS_Switch::get_locale() const {
	String local = "en";
	if (R_SUCCEEDED(setInitialize())) {
		u64 codigo = 0;
		if (R_SUCCEEDED(setGetSystemLanguage(&codigo))) {
			// o código é o nome em ASCII ("pt-BR", "en-US", "zh-Hans"...), até 8 letras
			char nome[9] = {};
			memcpy(nome, &codigo, 8);
			local = String(nome);
		}
		setExit();
	}
	if (local == "zh-Hans" || local == "zh-CN") {
		return "zh_CN";
	}
	if (local == "zh-Hant" || local == "zh-TW") {
		return "zh_TW";
	}
	return local.replace("-", "_");
}

static void _hook_do_applet(AppletHookType p_tipo, void *p_os) {
	static_cast<OS_Switch *>(p_os)->foco_mudou((int)p_tipo);
}

// Todo aviso do sistema vai para o log (o HOME não pausava e o log não mostrava aviso nenhum).
void OS_Switch::foco_mudou(int p_tipo) {
	const AppletFocusState estado = appletGetFocusState();
	printf("[switch] aviso do sistema: tipo %d, foco %d\n", p_tipo, (int)estado);
	if (p_tipo != AppletHookType_OnFocusState && p_tipo != AppletHookType_OnResume) {
		return;
	}
	const bool agora = estado == AppletFocusState_InFocus;
	if (!main_loop) {
		focado = agora;
		return;
	}
	if (!agora && focado) {
		focado = false;
		pausar_o_jogo("foco saiu (HOME)");
	} else if (agora && !focado) {
		focado = true;
		printf("[switch] foco voltou\n");
		main_loop->notification(MainLoop::NOTIFICATION_APPLICATION_RESUMED);
		main_loop->notification(MainLoop::NOTIFICATION_APPLICATION_FOCUS_IN);
	}
}

// O jogo pausa a partida ao receber estes avisos (palco/principal.gd, _notification); a volta é com o
// jogador, no menu de pausa.
void OS_Switch::pausar_o_jogo(const char *p_motivo) {
	printf("[switch] pausa: %s\n", p_motivo);
	main_loop->notification(MainLoop::NOTIFICATION_APPLICATION_FOCUS_OUT);
	main_loop->notification(MainLoop::NOTIFICATION_APPLICATION_PAUSED);
}

String OS_Switch::get_version() const {
	SetSysFirmwareVersion versao;
	if (R_SUCCEEDED(setsysInitialize())) {
		Result r = setsysGetFirmwareVersion(&versao);
		setsysExit();
		if (R_SUCCEEDED(r)) {
			return String(versao.display_version);
		}
	}
	return "";
}

String OS_Switch::get_executable_path() const {
	// O Main::setup só guarda o caminho (set_cmdline) depois de procurar o .pck, e a busca do .pck
	// pergunta o caminho do executável: sem isto ela recebia "" e nem começava. O hbmenu passa o
	// caminho do .nro em argv[0] (ex.: sdmc:/switch/jogo/<jogo>.nro).
	String caminho = OS::get_executable_path();
	if (caminho.is_empty() && caminho_nro) {
		caminho = String::utf8(caminho_nro);
	}
	return caminho;
}

// Os dados do jogo ficam no cartão, ao lado dos outros homebrew: sdmc:/switch/<nome>/.
String OS_Switch::get_data_path() const {
	return "sdmc:/switch";
}

String OS_Switch::get_config_path() const {
	return "sdmc:/switch";
}

String OS_Switch::get_cache_path() const {
	return "sdmc:/switch";
}

// Os dados do jogo (user://: o save, as preferências, o cache de shaders) ficam em "dados", dentro da
// pasta do próprio .nro (ex.: sdmc:/switch/jogo/dados): apagar a pasta do jogo apaga tudo.
// Até 2026-10-18 ficavam em sdmc:/switch/<nome do projeto>/dados; na primeira vez, a pasta antiga é movida.
String OS_Switch::get_user_data_dir(const String &p_user_dir) const {
	const String pasta_do_nro = get_executable_path().get_base_dir();
	if (pasta_do_nro.is_empty()) {
		return get_data_path().path_join(p_user_dir.validate_filename()).path_join("dados");
	}
	const String novo = pasta_do_nro.path_join("dados");
	static bool conferido = false;
	if (!conferido) {
		conferido = true;
		const String antiga = get_data_path().path_join(p_user_dir.validate_filename());
		const String antigo = antiga.path_join("dados");
		struct stat st = {};
		if (stat(antigo.utf8().get_data(), &st) == 0 && stat(novo.utf8().get_data(), &st) != 0) {
			if (rename(antigo.utf8().get_data(), novo.utf8().get_data()) == 0) {
				rmdir(antiga.utf8().get_data()); // só sai se ficou vazia
				printf("[switch] dados movidos de %s para %s\n", antigo.utf8().get_data(), novo.utf8().get_data());
			} else {
				printf("[switch] não deu para mover %s (errno %d)\n", antigo.utf8().get_data(), errno);
			}
		}
	}
	return novo;
}

volatile bool OS_Switch::suspenso = false;
volatile bool OS_Switch::sentinela_ligada = false;

void OS_Switch::_sentinela(void *) {
	u64 antes = armGetSystemTick();
	while (sentinela_ligada) {
		svcSleepThread(100000000ULL);
		const u64 agora = armGetSystemTick();
		const u64 ms = armTicksToNs(agora - antes) / 1000000;
		if (ms > (u64)SUSPENSO_MS) {
			printf("[switch] o jogo ficou congelado %d ms\n", (int)ms);
			suspenso = true;
		}
		antes = agora;
	}
}

// O ritmo dos quadros pela tela (docs/decisoes.md, D-19): com o Engine.max_fps em 30 (20, 15), a janela mostra um
// quadro a cada 2 (3, 4) atualizações da tela de 60 Hz. Só com o limitador do Godot, que dorme pelo relógio, os
// quadros saíam a ~33,0 ms contra os 33,3 da tela, a diferença se acumulava e de tempos em tempos um quadro
// esperava uma atualização a mais (o contador ia a 29 com folga de sobra). O NVK e o EGL voltam o intervalo a 1
// quando recriam a troca de quadros: a cada quadro o laço confere e reaplica.
void OS_Switch::_manter_o_ritmo() {
	const int max_fps = Engine::get_singleton()->get_max_fps();
	const u32 intervalo = (max_fps > 0 && max_fps <= 30) ? CLAMP((u32)((60 + max_fps / 2) / max_fps), 2u, 4u) : 1u;
	NWindow *janela = nwindowGetDefault();
	if (janela->swap_interval != intervalo) {
		nwindowSetSwapInterval(janela, intervalo);
	}
	if (intervalo != intervalo_avisado) {
		intervalo_avisado = intervalo;
		printf("[switch] ritmo: um quadro a cada %u atualização(ões) da tela (Engine.max_fps %d)\n", intervalo, max_fps);
	}
}

void OS_Switch::run() {
	if (!main_loop) {
		return;
	}
	// a aceleração da abertura (ligada antes do Main::setup) acaba aqui: com ela a placa de vídeo fica no
	// mínimo, e nos quadros do jogo isso dá 5 FPS (0.2.4, que esperava quadros leves para desligar e
	// nunca os tinha)
	acelerar(false);
	main_loop->initialize();
	AppletHookCookie gancho;
	appletHook(&gancho, _hook_do_applet, this);
	const Result r_foco = appletSetFocusHandlingMode(AppletFocusHandlingMode_SuspendHomeSleepNotify);
	printf("[switch] modo de foco: %s (0x%x)\n", R_SUCCEEDED(r_foco) ? "aceito" : "recusado", r_foco);
	// O HOME congela o processo inteiro sem aviso (0.2.7: nenhum aviso de foco, e um quadro de 155 s).
	// A sentinela é uma thread que dorme 100 ms de cada vez, com prioridade acima da principal (um quadro
	// pesado não a atrasa): se acordar SUSPENSO_MS depois, o processo esteve congelado, e na volta a
	// partida pausa.
	sentinela_ligada = true;
	LibnxThread sentinela;
	const bool com_sentinela = R_SUCCEEDED(threadCreate(&sentinela, _sentinela, nullptr, nullptr, 0x4000, 0x2B, -2)) && R_SUCCEEDED(threadStart(&sentinela));
	uint64_t quadros = 0;
	uint64_t iteracao_us = 0;
	double processo_us = 0.0;
	double fisica_us = 0.0;
	// o tempo da placa de vídeo e o do motor montando o desenho, medidos pelo Godot no viewport raiz: no Vulkan
	// a troca não espera a placa (a "espera da GPU" sai 0) e, sem isto, o pulso não dizia se o quadro pesado
	// era da placa ou da CPU
	RID tela;
	SceneTree *arvore = Object::cast_to<SceneTree>(get_main_loop());
	if (arvore && arvore->get_root() && RenderingServer::get_singleton()) {
		tela = arvore->get_root()->get_viewport_rid();
		RenderingServer::get_singleton()->viewport_set_measure_render_time(tela, true);
	}
	double gpu_us = 0.0;
	double desenho_us = 0.0;
	// appletMainLoop() fica falso quando o sistema pede para fechar (HOME → fechar o software)
	while (appletMainLoop()) {
		if (joypad) {
			joypad->process();
		}
		DisplayServer::get_singleton()->process_events();
		const u64 antes = armGetSystemTick();
		const bool sair = Main::iteration();
		const u64 depois = armGetSystemTick();
		const u64 este_us = armTicksToNs(depois - antes) / 1000;
		iteracao_us += este_us;
		if (sair) {
			break;
		}
		_manter_o_ritmo();
		// o cache de shaders depois da abertura (com o menu já desenhado): comparado com o da abertura, diz
		// se o Godot e o driver gravaram (docs/decisoes.md)
		if (quadros == 600) {
			contar_cache("com o menu (600 quadros)");
		}
		// o Vulkan rodou: a marca da abertura sai (se sobrasse, a próxima abertura voltaria ao OpenGL)
		if (marca_vulkan && quadros == (uint64_t)QUADROS_RODANDO) {
			remove(marca_vulkan);
			printf("[switch] o Vulkan rodou %d quadros: marca da abertura retirada\n", QUADROS_RODANDO);
			marca_vulkan = nullptr;
		}
		if (suspenso) {
			suspenso = false;
			pausar_o_jogo("o jogo ficou congelado (HOME)");
		} else if (este_us > (u64)SUSPENSO_MS * 1000) {
			printf("[switch] quadro de %d ms\n", (int)(este_us / 1000));
		}
		// o TIME_PROCESS (o pior quadro do segundo, processo + desenho; D-15) e o da física, somados para a média
		if (Performance::get_singleton()) {
			processo_us += Performance::get_singleton()->get_monitor(Performance::TIME_PROCESS) * 1000000.0;
			fisica_us += Performance::get_singleton()->get_monitor(Performance::TIME_PHYSICS_PROCESS) * 1000000.0;
		}
		if (tela.is_valid()) {
			gpu_us += RenderingServer::get_singleton()->viewport_get_measured_render_time_gpu(tela) * 1000.0;
			desenho_us += RenderingServer::get_singleton()->viewport_get_measured_render_time_cpu(tela) * 1000.0;
		}
		// a cada ~segundo: o turbo (se o jogo pediu) é reaplicado, e os números da renderização são
		// guardados para o pulso do log (godot_switch.cpp)
		if (quadros % 60 == 59 && SwitchHorizon::get_singleton()) {
			SwitchHorizon::get_singleton()->manter_turbo(acelerado);
		}
		if (++quadros % 60 == 0 && Performance::get_singleton()) {
			Performance *perf = Performance::get_singleton();
			Medidas m;
			m.processo_ms = processo_us / 60.0 / 1000.0;
			m.fisica_ms = fisica_us / 60.0 / 1000.0;
			processo_us = 0.0;
			fisica_us = 0.0;
			m.gpu_ms = gpu_us / 60.0 / 1000.0;
			m.desenho_ms = desenho_us / 60.0 / 1000.0;
			gpu_us = 0.0;
			desenho_us = 0.0;
			m.desenhos = perf->get_monitor(Performance::RENDER_TOTAL_DRAW_CALLS_IN_FRAME);
			m.objetos = perf->get_monitor(Performance::RENDER_TOTAL_OBJECTS_IN_FRAME);
			m.video_mib = perf->get_monitor(Performance::RENDER_VIDEO_MEM_USED) / (1024.0 * 1024.0);
			m.quadro_ms = iteracao_us / 60.0 / 1000.0;
			m.troca_ms = DisplayServerSwitch::troca_us / 60.0 / 1000.0;
			m.envio_ms = DisplayServerSwitch::envio_us / 60.0 / 1000.0;
			m.apresentacao_ms = DisplayServerSwitch::apresentacao_us / 60.0 / 1000.0;
			iteracao_us = 0;
			DisplayServerSwitch::troca_us = 0;
			DisplayServerSwitch::envio_us = 0;
			DisplayServerSwitch::apresentacao_us = 0;
			m.prontas = true;
			medidas = m;
		}
	}
	// fechado pelo sistema (HOME → fechar o software): o pedido de fechar chega ao jogo antes de sair
	if (DisplayServerSwitch::get_singleton()) {
		static_cast<DisplayServerSwitch *>(DisplayServer::get_singleton())->avisar_janela(DisplayServerEnums::WINDOW_EVENT_CLOSE_REQUEST);
	}
	appletUnhook(&gancho);
	// o modo de foco volta ao padrão da libnx (o processo segue vivo no carregador, com o próximo .nro)
	appletSetFocusHandlingMode(AppletFocusHandlingMode_SuspendHomeSleep);
	if (com_sentinela) {
		sentinela_ligada = false;
		threadWaitForExit(&sentinela);
		threadClose(&sentinela);
	}
	main_loop->finalize();
	acelerar(false);
}
