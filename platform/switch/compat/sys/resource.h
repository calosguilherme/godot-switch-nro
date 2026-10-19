/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.
 * A newlib tem sys/resource.h sem o rlimit: completa aqui (sem limites conhecidos). */
#pragma once

#include_next <sys/resource.h>

#ifndef RLIMIT_STACK
#define RLIMIT_STACK 3
typedef unsigned long rlim_t;
struct rlimit {
	rlim_t rlim_cur;
	rlim_t rlim_max;
};
#ifdef __cplusplus
extern "C" {
#endif
static inline int getrlimit(int p_resource, struct rlimit *r_limit) {
	(void)p_resource;
	r_limit->rlim_cur = 0;
	r_limit->rlim_max = 0;
	return 0;
}
#ifdef __cplusplus
}
#endif
#endif
