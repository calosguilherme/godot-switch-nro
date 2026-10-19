/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.
 * O homebrew não carrega bibliotecas dinâmicas: tudo devolve "não disponível". */
#pragma once

#define RTLD_LAZY 0x1
#define RTLD_NOW 0x2
#define RTLD_GLOBAL 0x100
#define RTLD_LOCAL 0
#define RTLD_NEXT ((void *)-1)
#define RTLD_DEFAULT ((void *)0)

#ifdef __cplusplus
extern "C" {
#endif
static inline void *dlopen(const char *p_file, int p_mode) { (void)p_file; (void)p_mode; return 0; }
static inline void *dlsym(void *p_handle, const char *p_name) { (void)p_handle; (void)p_name; return 0; }
static inline int dlclose(void *p_handle) { (void)p_handle; return 0; }
static inline char *dlerror(void) { return (char *)"sem bibliotecas dinâmicas no Switch"; }
#ifdef __cplusplus
}
#endif
