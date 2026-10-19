/**************************************************************************/
/*  platform_gl.h                                                         */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/**************************************************************************/

#pragma once

// O mesa do devkitPro (switch-mesa) dá OpenGL ES 3 pelo EGL, como no Android.
#ifndef GLES_API_ENABLED
#define GLES_API_ENABLED
#endif

// IWYU pragma: begin_exports.
#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <GLES3/gl3ext.h>
// IWYU pragma: end_exports.

// O driver GLES3 do Godot chama duas funções que o mesa do Switch não declara:
// - a multiview (OVR_multiview) só existe para XR, que o Switch não tem; nunca é chamada com uma vista só;
// - glTexImage3DMultisample é do OpenGL de PC; no GLES 3.2 a mesma textura sai de glTexStorage3DMultisample.
static inline void glFramebufferTextureMultiviewOVR(GLenum, GLenum, GLuint, GLint, GLint, GLsizei) {}
static inline void glTexImage3DMultisample(GLenum p_target, GLsizei p_samples, GLenum p_format, GLsizei p_width, GLsizei p_height, GLsizei p_depth, GLboolean p_fixed) {
	glTexStorage3DMultisample(p_target, p_samples, p_format, p_width, p_height, p_depth, p_fixed);
}
