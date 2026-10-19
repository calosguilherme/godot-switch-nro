/**************************************************************************/
/*  posix_switch.cpp                                                      */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/**************************************************************************/

// Funções POSIX que o Godot chama (drivers/unix) e a newlib do Switch não tem. O homebrew não cria
// processos, pipes nem sinais: elas devolvem "não suportado" (ENOSYS). A memória alinhada funciona.

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>

extern "C" {

// fraca: o NVK do <jogo>.vulkan traz a própria (docs/decisoes.md), e a dele vale quando as duas existem
__attribute__((weak)) int posix_memalign(void **r_ptr, size_t p_alignment, size_t p_size) {
	void *p = memalign(p_alignment, p_size);
	if (!p) {
		return ENOMEM;
	}
	*r_ptr = p;
	return 0;
}

pid_t waitpid(pid_t, int *, int) {
	errno = ENOSYS;
	return -1;
}

int pipe(int[2]) {
	errno = ENOSYS;
	return -1;
}

int execvp(const char *, char *const[]) {
	errno = ENOSYS;
	return -1;
}

pid_t setsid(void) {
	errno = ENOSYS;
	return -1;
}

int sigaction(int, const struct sigaction *, struct sigaction *) {
	errno = ENOSYS;
	return -1;
}

FILE *popen(const char *, const char *) {
	errno = ENOSYS;
	return nullptr;
}

int pclose(FILE *) {
	errno = ENOSYS;
	return -1;
}

int mkfifo(const char *, mode_t) {
	errno = ENOSYS;
	return -1;
}
}
