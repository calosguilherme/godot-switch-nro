/**************************************************************************/
/*  switch_wrapper.h                                                      */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/* Referência: o port do Godot 3.5 de Stary2001 e cpasjuste (MIT).        */
/**************************************************************************/

#pragma once

// A libnx tem tipos com o mesmo nome de classes do Godot (Mutex, Semaphore, Thread, AudioDriver).
// Incluída por aqui, eles viram Libnx*; inclua sempre este arquivo em vez de <switch.h>.
#define Mutex LibnxMutex
#define Semaphore LibnxSemaphore
#define Thread LibnxThread
#define AudioDriver LibnxAudioDriver

#include <switch.h>

#undef AudioDriver
#undef Thread
#undef Semaphore
#undef Mutex
