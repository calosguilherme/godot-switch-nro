/**************************************************************************/
/*  godot_switch.cpp                                                      */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/* Referência: o port do Godot 3.5 de Stary2001 e cpasjuste (MIT).        */
/**************************************************************************/

#include "os_switch.h"

#include "core/config/engine.h"
#include "core/os/memory.h"
#include "main/main.h"
#include "main/performance.h"

#include "switch_wrapper.h"
#include "versao.gen.h"

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <malloc.h>
#include <reent.h>
#include <pthread.h>

// Pilha das threads criadas pelo Godot. O std::thread chama pthread_create sem atributos, e a
// pilha padrão da newlib do devkitPro é pequena para o GDScript e o carregamento de cenas: o
// ligador troca pthread_create por esta função (-Wl,--wrap=pthread_create, em detect.py), que dá
// 1 MiB a quem não pediu tamanho.
extern "C" {
int __real_pthread_create(pthread_t *p_thread, const pthread_attr_t *p_attr, void *(*p_func)(void *), void *p_arg);

// Cada thread passa por um trampolim que a anota numa tabela enquanto a função dela roda (docs/decisoes.md): na
// saída, a conferência (_conferir_saida) lista as que sobraram, pelo endereço da função.
static const int VAGAS_THREADS = 64;
static void *volatile _threads_vivas[VAGAS_THREADS] = {};

struct _Partida {
	void *(*func)(void *);
	void *arg;
	int vaga;
};

static void *_trampolim(void *p_partida) {
	const _Partida partida = *(_Partida *)p_partida;
	free(p_partida);
	void *r = partida.func(partida.arg);
	if (partida.vaga >= 0) {
		_threads_vivas[partida.vaga] = nullptr;
	}
	return r;
}

int __wrap_pthread_create(pthread_t *p_thread, const pthread_attr_t *p_attr, void *(*p_func)(void *), void *p_arg) {
	_Partida *partida = (_Partida *)malloc(sizeof(_Partida));
	if (!partida) {
		return EAGAIN;
	}
	partida->func = p_func;
	partida->arg = p_arg;
	partida->vaga = -1;
	for (int i = 0; i < VAGAS_THREADS; i++) {
		if (__sync_bool_compare_and_swap(&_threads_vivas[i], (void *)nullptr, (void *)p_func)) {
			partida->vaga = i;
			break;
		}
	}
	int r;
	if (p_attr != nullptr) {
		r = __real_pthread_create(p_thread, p_attr, _trampolim, partida);
	} else {
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, 1024 * 1024);
		r = __real_pthread_create(p_thread, &attr, _trampolim, partida);
		pthread_attr_destroy(&attr);
	}
	if (r != 0) {
		if (partida->vaga >= 0) {
			_threads_vivas[partida->vaga] = nullptr;
		}
		free(partida);
	}
	return r;
}

// Uma escrita de cada vez no log (docs/decisoes.md): o stdout e o stderr do Godot são dois FILE com travas
// próprias apontando para o mesmo arquivo do cartão, e o deslocamento do arquivo não é protegido. Com o
// pulso em prioridade alta (0.2.24), duas escritas caíam no mesmo ponto: uma linha cobria a outra e
// sobrava um buraco com lixo do cartão. O ligador troca o _write_r da newlib por este
// (-Wl,--wrap=_write_r, em detect.py), que serializa o que vai para o stdout e o stderr.
static RMutex _trava_log = {};

_ssize_t __real__write_r(struct _reent *p_reent, int p_fd, const void *p_dados, size_t p_tamanho);

_ssize_t __wrap__write_r(struct _reent *p_reent, int p_fd, const void *p_dados, size_t p_tamanho) {
	if (p_fd != STDOUT_FILENO && p_fd != STDERR_FILENO) {
		return __real__write_r(p_reent, p_fd, p_dados, p_tamanho);
	}
	rmutexLock(&_trava_log);
	const _ssize_t r = __real__write_r(p_reent, p_fd, p_dados, p_tamanho);
	rmutexUnlock(&_trava_log);
	return r;
}

// a região do heap que a libnx deu a este .nro (o carregador põe o .nro logo antes dela)
extern char *fake_heap_start;
extern char *fake_heap_end;
}

// Conferência da saída (docs/decisoes.md): a libnx não encerra o que sobra, e o processo continua vivo no
// carregador com o próximo .nro. Uma thread que sobra, uma pilha de thread não fechada, memória
// emprestada ao sistema (áudio, rede, placa de vídeo) deixam páginas do heap presas, e um .nro maior não
// cabe por cima delas (2001-0106 na 0.2.21). Escreve as threads que sobraram e cada trecho do heap que
// não está livre (tipo diferente de heap ou com atributo: emprestado, compartilhado com a placa...).
extern "C" void userAppExit(void);

static void _conferir_saida(FILE *p_saida, const char *p_quando) {
	int threads = 0;
	for (int i = 0; i < VAGAS_THREADS; i++) {
		void *func = _threads_vivas[i];
		if (func) {
			threads++;
			fprintf(p_saida, "[switch] saída (%s): thread viva, função userAppExit%+lld\n", p_quando, (long long)((intptr_t)func - (intptr_t)&userAppExit));
		}
	}
	const u64 inicio = (u64)fake_heap_start;
	const u64 fim = (u64)fake_heap_end;
	u64 endereco = inicio;
	u64 presos = 0;
	int trechos = 0;
	while (endereco < fim) {
		MemoryInfo info = {};
		u32 pagina = 0;
		if (R_FAILED(svcQueryMemory(&info, &pagina, endereco)) || info.size == 0) {
			break;
		}
		const u64 a = (u64)info.addr > inicio ? (u64)info.addr : inicio;
		const u64 b = (u64)(info.addr + info.size) < fim ? (u64)(info.addr + info.size) : fim;
		if (info.attr != 0 || (info.type & 0xFF) != MemType_Heap) {
			fprintf(p_saida, "[switch] saída (%s): heap preso em +%.2f MiB, %llu KiB (tipo 0x%x, atributo 0x%x, permissão 0x%x)\n", p_quando,
					(a - inicio) / 1048576.0, (unsigned long long)((b - a) / 1024), info.type, info.attr, info.perm);
			presos += b - a;
			trechos++;
		}
		endereco = info.addr + info.size;
	}
	fprintf(p_saida, "[switch] saída (%s): %d threads vivas, %d trechos do heap presos (%llu KiB)%s\n", p_quando, threads, trechos,
			(unsigned long long)(presos / 1024), threads == 0 && trechos == 0 ? ": tudo solto" : "");
	fflush(p_saida);
}

// o último passo antes de a libnx fechar os serviços e voltar ao carregador (depois dos destrutores e do
// fechamento dos arquivos): a conferência vai para <nome>.saida.txt, ao lado do .nro
static char _caminho_saida[512] = {};

extern "C" void userAppExit(void) {
	if (!_caminho_saida[0]) {
		return;
	}
	FILE *f = fopen(_caminho_saida, "w");
	if (f) {
		_conferir_saida(f, "no fim de tudo");
		fclose(f);
	}
}

// Sem o nxlink no PC, tudo o que o Godot imprime (stdout e stderr) vai para <nome>.log ao lado do
// .nro, sem buffer: se o jogo fechar de repente, o log mostra até onde chegou.
static bool _abrir_log(const char *p_nro) {
	char caminho[512];
	snprintf(caminho, sizeof(caminho), "%s", p_nro);
	char *ponto = strrchr(caminho, '.');
	if (ponto && strlen(ponto) == 4) {
		strcpy(ponto, ".log");
	} else {
		strncat(caminho, ".log", sizeof(caminho) - strlen(caminho) - 1);
	}
	// O log da abertura anterior fica como <nome>.anterior.log: reabrir o jogo (ou a volta do .vulkan ao
	// .nro) apagava o log da sessão que se queria ler (docs/decisoes.md, D-03)
	char anterior[520];
	snprintf(anterior, sizeof(anterior), "%s", caminho);
	char *fim = strrchr(anterior, '.');
	if (fim) {
		strcpy(fim, ".anterior.log");
		remove(anterior);
		rename(caminho, anterior);
	}
	// O cartão do Switch não deixa abrir o mesmo arquivo duas vezes para escrita: abre uma vez e
	// aponta o stdout e o stderr para ele (dup2, como o nxlinkStdio da libnx faz com o socket).
	int fd = open(caminho, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		return false;
	}
	fflush(stdout);
	fflush(stderr);
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);
	close(fd);
	setvbuf(stdout, nullptr, _IONBF, 0);
	setvbuf(stderr, nullptr, _IONBF, 0);
	return true;
}

// Sonda: o .pck ao lado do .nro abre com o C puro e tem a assinatura do Godot ("GDPC")?
static void _sondar_pck(const char *p_nro) {
	char caminho[512];
	snprintf(caminho, sizeof(caminho), "%s", p_nro);
	char *ponto = strrchr(caminho, '.');
	if (ponto) {
		strcpy(ponto, ".pck");
	}
	FILE *f = fopen(caminho, "rb");
	if (!f) {
		printf("[switch] pck: não abriu %s (errno %d)\n", caminho, errno);
		return;
	}
	char magica[5] = {};
	size_t lidos = fread(magica, 1, 4, f);
	fseek(f, 0, SEEK_END);
	long tamanho = ftell(f);
	fclose(f);
	printf("[switch] pck: %s, %ld bytes, assinatura '%s' (%zu lidos)\n", caminho, tamanho, magica, lidos);
}

// Diagnóstico: uma thread à parte escreve no log a cada 5 s há quanto tempo o jogo está aberto. Se o
// pulso continua e o motor não anda, ele está vivo (preso em algo demorado); se o pulso para, travou.
// Ela para antes de o main sair (_parar_pulso): a libnx não encerra as threads que sobram, e a pilha desta
// (1 MiB do heap) ficava presa. Na 0.2.21 o carregador não conseguiu pôr o <jogo>.vulkan (maior que o
// <jogo>.nro) por cima dela e caiu com 2001-0106 (InvalidCurrentMemory).
// É uma thread da libnx com prioridade acima da principal, como a sentinela: a pthread nasce com a
// prioridade 59, abaixo de todas as do Godot, e na 0.2.22 o pulso ficou mudo enquanto o Vulkan compilava
// shaders nos três núcleos. Ela dorme quase o tempo todo e não tira tempo do jogo.
static volatile bool _pulso_ligado = false;
static LibnxThread _pulso_thread;

static void _pulso(void *) {
	const u64 inicio = armGetSystemTick();
	while (_pulso_ligado) {
		for (int i = 0; i < 50 && _pulso_ligado; i++) {
			svcSleepThread(100000000ULL);
		}
		if (!_pulso_ligado) {
			break;
		}
		const u64 ms = armTicksToNs(armGetSystemTick() - inicio) / 1000000ULL;
		// FPS do motor; memória do processo usada/total (do sistema, em MiB); e, com o motor rodando, o
		// tempo de processamento por quadro (código do jogo), chamadas de desenho, objetos e memória de vídeo
		const double fps = Engine::get_singleton() ? Engine::get_singleton()->get_frames_per_second() : 0.0;
		// a memória do malloc em uso e o pico que o heap já alcançou (até a 0.2.22 o pulso mostrava a do
		// processo, sempre 3185/3189 MiB: o carregador reserva o heap inteiro de uma vez). Um vazamento
		// aparece como o "em uso" subindo sem voltar.
		const struct mallinfo mi = mallinfo();
		const u64 usada = (u64)mi.uordblks;
		const u64 total = (u64)mi.arena;
		// os números da renderização vêm do laço principal (OS_Switch::run): consultar a renderização
		// desta thread derruba o processo
		const OS_Switch::Medidas m = OS_Switch::medidas;
		if (m.prontas) {
			printf("[switch] pulso: %llu s%s, %.0f FPS, malloc %llu MiB (pico %llu MiB), quadro %.1f ms (GPU %.1f ms; desenho na CPU %.1f ms; espera da GPU %.1f ms; processo %.1f ms; física %.1f ms), %.0f desenhos, %.0f objetos, vídeo %.0f MiB\n",
					(unsigned long long)(ms / 1000), OS_Switch::acelerado ? " (acelerado)" : "", fps, (unsigned long long)(usada >> 20), (unsigned long long)(total >> 20),
					m.quadro_ms, m.gpu_ms, m.desenho_ms, m.troca_ms, m.processo_ms, m.fisica_ms, m.desenhos, m.objetos, m.video_mib);
		} else {
			printf("[switch] pulso: %llu s, malloc %llu MiB (pico %llu MiB)\n", (unsigned long long)(ms / 1000), (unsigned long long)(usada >> 20), (unsigned long long)(total >> 20));
		}
		// grava no cartão: se o console travar e for desligado à força, o log não se perde
		rmutexLock(&_trava_log);
		fsync(STDOUT_FILENO);
		rmutexUnlock(&_trava_log);
	}
}

static void _iniciar_pulso() {
	_pulso_ligado = true;
	if (R_FAILED(threadCreate(&_pulso_thread, _pulso, nullptr, nullptr, 0x10000, 0x2B, -2))) {
		_pulso_ligado = false;
	} else if (R_FAILED(threadStart(&_pulso_thread))) {
		threadClose(&_pulso_thread);
		_pulso_ligado = false;
	}
}

static void _parar_pulso() {
	if (_pulso_ligado) {
		_pulso_ligado = false;
		threadWaitForExit(&_pulso_thread);
		threadClose(&_pulso_thread);
	}
}

// Quanto custa pedir e devolver memória (docs/decisoes.md): o GDScript pede o tempo todo, e o malloc da newlib
// trava um mutex a cada pedido. Mede com a aceleração da abertura (processador a 1785 MHz); no PC
// (glibc) um par malloc/free leva ~15-20 ns. A conta de inteiros é a régua do processador.
static volatile uintptr_t _sumidouro = 0;

static void _medir_memoria() {
	const int N = 200000;
	u64 t0 = armGetSystemTick();
	for (int i = 0; i < N; i++) {
		void *p = malloc(48);
		_sumidouro = _sumidouro + (uintptr_t)p;
		free(p);
	}
	const double par_ns = armTicksToNs(armGetSystemTick() - t0) / (double)N;
	void *vagas[64] = {};
	t0 = armGetSystemTick();
	for (int i = 0; i < N; i++) {
		const int k = i & 63;
		free(vagas[k]);
		vagas[k] = malloc(16 + (i * 7) % 200);
		_sumidouro = _sumidouro + (uintptr_t)vagas[k];
	}
	const double troca_ns = armTicksToNs(armGetSystemTick() - t0) / (double)N;
	for (int k = 0; k < 64; k++) {
		free(vagas[k]);
	}
	uint32_t x = 1;
	t0 = armGetSystemTick();
	for (int i = 0; i < 20000000; i++) {
		x = x * 1664525u + 1013904223u;
	}
	_sumidouro = _sumidouro + x;
	const double conta_ns = armTicksToNs(armGetSystemTick() - t0) / 20000000.0;
	printf("[switch] memória: malloc+free %.0f ns, troca com 64 vivos %.0f ns; conta de inteiros %.2f ns\n", par_ns, troca_ns, conta_ns);
}

// --- OpenGL ou Vulkan (docs/decisoes.md, D-12 e D-13) -----------------------------------------------
// O jogo grava a escolha em dados/renderizador.txt ("opengl" ou "vulkan"). São dois .nro na mesma pasta:
// o <jogo>.nro (OpenGL, Mesa 20.1) e o <jogo>.vulkan (Vulkan, NVK do Mesa 26; o menu do homebrew não o
// lista). Com Vulkan escolhido (ou sem escolha: o Vulkan é o padrão quando o <jogo>.vulkan existe), o
// <jogo>.nro passa a vez ao outro pelo carregador (envSetNextLoad) antes de iniciar o Godot. O
// <jogo>.vulkan marca dados/vulkan_abrindo e só a apaga depois de rodar (OS_Switch::run); se a marca sobra,
// a abertura com Vulkan falhou ou travou, e a próxima volta ao OpenGL (e grava dados/vulkan_falhou, que o
// jogo pode avisar). O <jogo> é o nome do próprio executável (argv[0]) sem a extensão.

// "sdmc:/switch/jogo/jogo.vulkan" + ".nro" → "sdmc:/switch/jogo/jogo.nro"
static void _irmao(char *r_caminho, size_t p_tamanho, const char *p_exec, const char *p_extensao) {
	snprintf(r_caminho, p_tamanho, "%s", p_exec);
	char *barra = strrchr(r_caminho, '/');
	char *ponto = strrchr(r_caminho, '.');
	if (ponto && (!barra || ponto > barra)) {
		*ponto = 0;
	}
	strncat(r_caminho, p_extensao, p_tamanho - strlen(r_caminho) - 1);
}

static void _na_pasta(char *r_caminho, size_t p_tamanho, const char *p_exec, const char *p_nome) {
	snprintf(r_caminho, p_tamanho, "%s", p_exec);
	char *barra = strrchr(r_caminho, '/');
	if (barra) {
		snprintf(barra + 1, p_tamanho - (barra + 1 - r_caminho), "%s", p_nome);
	} else {
		snprintf(r_caminho, p_tamanho, "%s", p_nome);
	}
}

static bool _existe(const char *p_caminho) {
	FILE *f = fopen(p_caminho, "rb");
	if (f) {
		fclose(f);
		return true;
	}
	return false;
}

static void _gravar_dado(const char *p_exec, const char *p_nome, const char *p_texto) {
	char pasta[512];
	_na_pasta(pasta, sizeof(pasta), p_exec, "dados");
	mkdir(pasta, 0777);
	char caminho[512];
	snprintf(caminho, sizeof(caminho), "%s/%s", pasta, p_nome);
	FILE *f = fopen(caminho, "w");
	if (f) {
		fputs(p_texto, f);
		fclose(f);
	}
}

// passa a vez ao irmão do executável com a extensão `p_extensao` (".nro" ou ".vulkan")
static bool _passar_para(const char *p_exec, const char *p_extensao) {
	char destino[512];
	_irmao(destino, sizeof(destino), p_exec, p_extensao);
	if (!_existe(destino) || !envHasNextLoad()) {
		printf("[switch] não dá para abrir %s (%s)\n", destino, _existe(destino) ? "o carregador não troca de .nro" : "não existe");
		return false;
	}
	char linha[600];
	snprintf(linha, sizeof(linha), "\"%s\"", destino);
	const Result r = envSetNextLoad(destino, linha);
	printf("[switch] passando a vez para %s (0x%x)\n", destino, r);
	return R_SUCCEEDED(r);
}

#if defined(GLES3_ENABLED)
// <jogo>.nro: se o jogo escolheu Vulkan, passa a vez ao <jogo>.vulkan (devolve true: o main sai já)
static bool _abrir_o_vulkan(const char *p_exec) {
	char escolha_arquivo[512];
	_na_pasta(escolha_arquivo, sizeof(escolha_arquivo), p_exec, "dados/renderizador.txt");
	// sem escolha gravada (a primeira abertura), o Vulkan é o padrão: no console ele rodou com o visual
	// inteiro e ~25% mais quadros que o OpenGL (docs/decisoes.md, D-13). Sem o <jogo>.vulkan na pasta, o
	// _passar_para recusa e o jogo segue no OpenGL.
	char escolha[16] = "vulkan";
	FILE *f = fopen(escolha_arquivo, "r");
	if (f) {
		if (!fgets(escolha, sizeof(escolha), f)) {
			escolha[0] = 0;
		}
		fclose(f);
	}
	if (strncmp(escolha, "vulkan", 6) != 0) {
		return false;
	}
	char marca[512];
	_na_pasta(marca, sizeof(marca), p_exec, "dados/vulkan_abrindo");
	if (_existe(marca)) {
		remove(marca);
		_gravar_dado(p_exec, "renderizador.txt", "opengl");
		_gravar_dado(p_exec, "vulkan_falhou", "1");
		printf("[switch] a última abertura com Vulkan não chegou a rodar: volta ao OpenGL\n");
		return false;
	}
	// a marca vai antes de passar a vez (se o <jogo>.vulkan cair antes de gravá-la, o <jogo>.nro passaria a
	// vez de novo a cada abertura); só o <jogo>.vulkan rodando 300 quadros a apaga
	_gravar_dado(p_exec, "vulkan_abrindo", "1");
	if (!_passar_para(p_exec, ".vulkan")) {
		remove(marca);
		return false;
	}
	return true;
}
#else
// <jogo>.vulkan: o Vulkan não abriu; volta ao OpenGL (o <jogo>.nro) e avisa o jogo
static void _voltar_ao_opengl(const char *p_exec) {
	_gravar_dado(p_exec, "renderizador.txt", "opengl");
	_gravar_dado(p_exec, "vulkan_falhou", "1");
	char marca[512];
	_na_pasta(marca, sizeof(marca), p_exec, "dados/vulkan_abrindo");
	remove(marca);
	_passar_para(p_exec, ".nro");
}
#endif

#if !defined(GLES3_ENABLED)
// Marcas de passagem do <jogo>.vulkan (docs/decisoes.md, D-15): ele já caiu antes do main, sem log. A libnx
// monta o cartão em __appInit, chama userAppInit, depois os construtores estáticos (os nossos com
// prioridade 101 vêm antes dos do NVK, do Rust e do Godot) e por fim o main; cada etapa acrescenta uma
// linha em vulkan_etapas.txt, na pasta do executável. A última linha diz até onde chegou. Antes do main
// não há argv: o caminho vem da linha de comando crua que o carregador deixou (envGetArgv).
static char _caminho_etapas[512] = {};

static const char *_etapas() {
	if (_caminho_etapas[0]) {
		return _caminho_etapas;
	}
	const char *linha = (const char *)envGetArgv();
	char exec[512] = {};
	if (linha) {
		const bool aspas = linha[0] == '"';
		const char *inicio = aspas ? linha + 1 : linha;
		size_t n = 0;
		while (inicio[n] && n < sizeof(exec) - 1 && (aspas ? inicio[n] != '"' : inicio[n] != ' ')) {
			exec[n] = inicio[n];
			n++;
		}
	}
	if (!exec[0]) {
		return nullptr;
	}
	_na_pasta(_caminho_etapas, sizeof(_caminho_etapas), exec, "vulkan_etapas.txt");
	return _caminho_etapas;
}

static void _marcar_etapa(const char *p_etapa, const char *p_modo = "a") {
	const char *caminho = _etapas();
	FILE *f = caminho ? fopen(caminho, p_modo) : nullptr;
	if (f) {
		fprintf(f, "%s\n", p_etapa);
		fclose(f);
	}
}

extern "C" void userAppInit(void) {
	_marcar_etapa("1 userAppInit (o cartão montado, os serviços prontos)", "w");
}

__attribute__((constructor(101))) static void _primeiro_construtor() {
	_marcar_etapa("2 primeiro construtor (antes dos do NVK, do Rust e do Godot)");
}
#endif

int main(int argc, char *argv[]) {
#if !defined(GLES3_ENABLED)
	_marcar_etapa("3 main");
#endif
	const char *exec = argc > 0 && argv[0] ? argv[0] : "sdmc:/switch/godot/godot.nro";
	{
		// jogo.nro → jogo.saida.txt; jogo.vulkan → jogo.vulkan.saida.txt (como o log)
		snprintf(_caminho_saida, sizeof(_caminho_saida), "%s", exec);
		char *ponto = strrchr(_caminho_saida, '.');
		if (ponto && strlen(ponto) == 4) {
			*ponto = 0;
		}
		strncat(_caminho_saida, ".saida.txt", sizeof(_caminho_saida) - strlen(_caminho_saida) - 1);
	}
	// Os sockets BSD da libnx: o nxlink e a rede do jogo (docs/decisoes.md, D-18). A configuração padrão basta
	// para o ENet e o UDP: cada socket UDP recebe 42 KiB (0xA500, ~30 pacotes do ENet de 1400 bytes entre dois
	// quadros) e manda 9 KiB; 3 sessões do serviço bsd:u são 3 chamadas ao mesmo tempo, e os sockets do Godot
	// são não bloqueantes (cada chamada volta logo). Fechados no socketExit, depois do Main::cleanup.
	const Result sockets = socketInitializeDefault();
#if !defined(GLES3_ENABLED)
	_marcar_etapa("4 sockets");
#endif
	bool nxlink = false;
#ifdef NXLINK_ENABLED
	// com `nxlink -s jogo.nro` no PC, o que o Godot imprime aparece no terminal de lá
	nxlink = nxlinkStdio() >= 0;
#endif
	if (!nxlink) {
		_abrir_log(exec);
	}
#if !defined(GLES3_ENABLED)
	_marcar_etapa("5 log aberto (daqui em diante, o <jogo>.vulkan.log)");
#endif
	romfsInit();
	OS_Switch::ler_versao_do_jogo();
	_sondar_pck(exec);
	_iniciar_pulso();
	printf("[switch] jogo %s · godot-switch-nro %s (Godot 4.7.2, %s): %s (argc %d, modo %s)\n", OS_Switch::versao_do_jogo, GSN_VERSAO,
#if defined(GLES3_ENABLED)
			"OpenGL",
#else
			"Vulkan",
#endif
			exec, argc, appletGetAppletType() == AppletType_Application ? "título (memória cheia)" : "applet (memória curta)");
	OS_Switch::relatar_rede(sockets);

#if defined(GLES3_ENABLED)
	if (_abrir_o_vulkan(exec)) {
		_parar_pulso();
		romfsExit();
		socketExit();
		_conferir_saida(stdout, "passando a vez");
		fsync(STDOUT_FILENO);
		return 0;
	}
#else
	// a marca sai quando o jogo roda (OS_Switch::run); se sobrar, a próxima abertura volta ao OpenGL
	static char marca_vulkan[512];
	_gravar_dado(exec, "vulkan_abrindo", "1");
	_na_pasta(marca_vulkan, sizeof(marca_vulkan), exec, "dados/vulkan_abrindo");
	OS_Switch::marca_vulkan = marca_vulkan;
#endif

	OS_Switch::caminho_nro = exec;
	// a abertura é toda carregamento e compilação de shaders: acelera desde já (OS_Switch::run desliga)
	OS_Switch::acelerar(true);
	OS_Switch os;
	printf("[switch] OS criado\n");
	_medir_memoria();

	// o .pck pode ir dentro do .nro (romfs:/jogo.pck, `embutir_pck` no gsn.py) ou ao lado dele no cartão
	// (<jogo>.pck, que o Godot acha sozinho pelo nome do executável)
	Vector<char *> args;
	FILE *dentro = fopen("romfs:/jogo.pck", "rb");
	if (dentro) {
		fclose(dentro);
		args.push_back((char *)"--main-pack");
		args.push_back((char *)"romfs:/jogo.pck");
	}
	// modo detalhado só com um arquivo <nome>.verbose ao lado do .nro: o log vai direto para o cartão,
	// sem buffer, e milhares de linhas atrasariam a abertura
	char marca[512];
	snprintf(marca, sizeof(marca), "%s", exec);
	char *ponto_marca = strrchr(marca, '.');
	if (ponto_marca) {
		strcpy(ponto_marca, ".verbose");
	}
	FILE *detalhado = fopen(marca, "rb");
	if (detalhado) {
		fclose(detalhado);
		args.push_back((char *)"--verbose");
		printf("[switch] modo detalhado (%s)\n", marca);
	}
	// Opções do driver de vídeo (docs/decisoes.md): cada linha NOME=valor do mesa_env.txt na pasta do .nro vira uma
	// variável de ambiente antes de o vídeo iniciar (o Mesa 26 lê MESA_SWITCH_GLTHREAD, MESA_SWITCH_GL_DRIVER
	// etc.). Sem o arquivo, o glthread do Mesa 26 fica desligado: com ele (o padrão do driver) o quadro
	// ficou ~13 ms mais caro na 0.2.18; o Godot espera respostas do driver a cada quadro.
	char opcoes[512];
	snprintf(opcoes, sizeof(opcoes), "%s", exec);
	char *barra_opcoes = strrchr(opcoes, '/');
	if (barra_opcoes) {
		strcpy(barra_opcoes + 1, "mesa_env.txt");
		FILE *f = fopen(opcoes, "r");
		if (f) {
			char linha[256];
			while (fgets(linha, sizeof(linha), f)) {
				linha[strcspn(linha, "\r\n")] = 0;
				char *igual = strchr(linha, '=');
				if (linha[0] == '#' || !igual) {
					continue;
				}
				*igual = 0;
				setenv(linha, igual + 1, 1);
				printf("[switch] mesa_env: %s=%s\n", linha, igual + 1);
			}
			fclose(f);
		}
	}
	setenv("MESA_SWITCH_GLTHREAD", "0", 0);
	printf("[switch] glthread do Mesa: %s\n", getenv("MESA_SWITCH_GLTHREAD"));
#if defined(GLES3_ENABLED)
	// o cache em disco do próprio Mesa (docs/decisoes.md): sem casa ($HOME) no Switch ele não tem onde gravar; aqui
	// vai para dados/mesa_cache. Se o driver não suportar o cache no Switch, a pasta fica vazia.
	char dados[512];
	char cache_mesa[512];
	_na_pasta(dados, sizeof(dados), exec, "dados");
	_na_pasta(cache_mesa, sizeof(cache_mesa), exec, "dados/mesa_cache");
	mkdir(dados, 0777);
	mkdir(cache_mesa, 0777);
	setenv("MESA_GLSL_CACHE_DIR", cache_mesa, 0);
	setenv("MESA_SHADER_CACHE_DIR", cache_mesa, 0);
#endif
	OS_Switch::contar_cache("na abertura");
	// Experimento (docs/decisoes.md): o desenho numa thread separada, em outro núcleo, só com um arquivo
	// render_separado na pasta do .nro. O Godot o chama de experimental (no PC, com o OpenGL, ele cai na
	// abertura); sem o arquivo, tudo como antes.
	char separado[512];
	snprintf(separado, sizeof(separado), "%s", exec);
	char *barra = strrchr(separado, '/');
	if (barra) {
		strcpy(barra + 1, "render_separado");
		FILE *f = fopen(separado, "rb");
		if (f) {
			fclose(f);
			args.push_back((char *)"--render-thread");
			args.push_back((char *)"separate");
			printf("[switch] desenho em thread separada (%s)\n", separado);
		}
	}
#if !defined(GLES3_ENABLED)
	// o <jogo>.vulkan desenha com o renderizador Mobile (o mesmo do PC e do celular) pelo Vulkan
	args.push_back((char *)"--rendering-driver");
	args.push_back((char *)"vulkan");
	args.push_back((char *)"--rendering-method");
	args.push_back((char *)"mobile");
#else
	// o <jogo>.nro só tem o OpenGL: o Compatibility, mesmo num projeto sem `rendering_method.switch` (sem
	// isto o Godot procuraria o driver do projeto, "vulkan", e abortaria)
	args.push_back((char *)"--rendering-driver");
	args.push_back((char *)"opengl3");
	args.push_back((char *)"--rendering-method");
	args.push_back((char *)"gl_compatibility");
#endif
	for (int i = 1; i < argc; i++) {
		args.push_back(argv[i]);
	}

	OS::get_singleton()->print("[switch] teste do logger do Godot (stdout)\n");
	OS::get_singleton()->printerr("[switch] teste do logger do Godot (stderr)\n");
	printf("[switch] Main::setup\n");
	Error err = Main::setup(exec, args.size(), args.ptrw());
	printf("[switch] Main::setup = %d\n", (int)err);
	if (err == OK) {
		int inicio = Main::start();
		printf("[switch] Main::start = %d\n", inicio);
		if (inicio == EXIT_SUCCESS) {
			printf("[switch] laço principal\n");
			os.run();
			printf("[switch] fim do laço\n");
		}
	}
#if !defined(GLES3_ENABLED)
	if (err != OK) {
		printf("[switch] o Vulkan não abriu: volta ao OpenGL\n");
		_voltar_ao_opengl(exec);
	}
#endif
	Main::cleanup();
	printf("[switch] saiu (código %d)\n", os.get_exit_code());

	_parar_pulso();
	romfsExit();
	socketExit();
	_conferir_saida(stdout, "fim do main");
	// o log vai ao cartão sem buffer, mas o tamanho do arquivo só se grava no fsync (o do pulso, a cada 5 s):
	// sem este, o fim do log (a saída do jogo, "fim do laço", "saiu", esta conferência) se perdia ao fechar
	fsync(STDOUT_FILENO);
	return os.get_exit_code();
}
