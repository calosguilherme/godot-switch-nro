/**************************************************************************/
/*  display_server_switch.cpp                                             */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/* Referência: o port do Godot 3.5 de Stary2001 e cpasjuste (MIT).        */
/**************************************************************************/

#include "display_server_switch.h"

#include "core/input/input.h"
#include "core/input/input_event.h"

#if defined(GLES3_ENABLED)
#include "platform_gl.h"
#include "drivers/gles3/rasterizer_gles3.h"
#endif
#if defined(RD_ENABLED)
#include "rendering_context_driver_vulkan_switch.h"
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/rendering_device.h"
#endif

#include <cstdio>

Vector<String> DisplayServerSwitch::get_rendering_drivers_func() {
	Vector<String> drivers;
#if defined(VULKAN_ENABLED)
	drivers.push_back("vulkan");
#endif
#if defined(GLES3_ENABLED)
	drivers.push_back("opengl3");
#endif
	return drivers;
}

DisplayServer *DisplayServerSwitch::create_func(const String &p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error &r_error) {
	DisplayServerSwitch *ds = memnew(DisplayServerSwitch(p_rendering_driver, p_vsync_mode, r_error));
	if (r_error != OK) {
		memdelete(ds);
		return nullptr;
	}
	return ds;
}

void DisplayServerSwitch::register_switch_driver() {
	register_create_function("switch", create_func, get_rendering_drivers_func);
}

bool DisplayServerSwitch::has_feature(DisplayServerEnums::Feature p_feature) const {
	switch (p_feature) {
		case DisplayServerEnums::FEATURE_TOUCHSCREEN:
			return true;
		case DisplayServerEnums::FEATURE_SWAP_BUFFERS:
			return rendering_driver == "opengl3";
		default:
			return false;
	}
}

Vector<DisplayServerEnums::WindowID> DisplayServerSwitch::get_window_list() const {
	Vector<DisplayServerEnums::WindowID> lista;
	lista.push_back(DisplayServerEnums::MAIN_WINDOW_ID);
	return lista;
}

#if defined(GLES3_ENABLED)
// A janela padrão do homebrew (nwindowGetDefault) é 1280×720; encaixado na TV o sistema amplia.
Error DisplayServerSwitch::_iniciar_egl() {
	egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (egl_display == EGL_NO_DISPLAY) {
		printf("EGL: sem display (%d)\n", eglGetError());
		return ERR_UNAVAILABLE;
	}
	eglInitialize(egl_display, nullptr, nullptr);
	if (eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE) {
		printf("EGL: sem OpenGL ES (%d)\n", eglGetError());
		return ERR_UNAVAILABLE;
	}

	static const EGLint atributos[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_STENCIL_SIZE, 8,
		EGL_NONE
	};
	EGLConfig config;
	EGLint quantas = 0;
	eglChooseConfig(egl_display, atributos, &config, 1, &quantas);
	if (quantas == 0) {
		printf("EGL: nenhuma configuração (%d)\n", eglGetError());
		return ERR_UNAVAILABLE;
	}

	NWindow *janela = nwindowGetDefault();
	nwindowSetDimensions(janela, tamanho.width, tamanho.height);
	egl_surface = eglCreateWindowSurface(egl_display, config, janela, nullptr);
	if (egl_surface == EGL_NO_SURFACE) {
		printf("EGL: superfície falhou (%d)\n", eglGetError());
		return ERR_UNAVAILABLE;
	}

	static const EGLint contexto[] = {
		EGL_CONTEXT_MAJOR_VERSION, 3,
		EGL_CONTEXT_MINOR_VERSION, 2,
		EGL_NONE
	};
	egl_context = eglCreateContext(egl_display, config, EGL_NO_CONTEXT, contexto);
	if (egl_context == EGL_NO_CONTEXT) {
		printf("EGL: contexto GLES 3.2 falhou (%d)\n", eglGetError());
		return ERR_UNAVAILABLE;
	}
	eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);
	return OK;
}

void DisplayServerSwitch::_encerrar_egl() {
	if (egl_display == EGL_NO_DISPLAY) {
		return;
	}
	eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	if (egl_context != EGL_NO_CONTEXT) {
		eglDestroyContext(egl_display, egl_context);
		egl_context = EGL_NO_CONTEXT;
	}
	if (egl_surface != EGL_NO_SURFACE) {
		eglDestroySurface(egl_display, egl_surface);
		egl_surface = EGL_NO_SURFACE;
	}
	eglTerminate(egl_display);
	egl_display = EGL_NO_DISPLAY;
}
#endif

#if defined(RD_ENABLED)
// Vulkan pelo NVK (docs/decisoes.md): o contexto, a superfície sobre a janela do homebrew e o RenderingDevice, como
// no display server do Linux. Qualquer falha devolve ERR_UNAVAILABLE (o godot_switch.cpp volta ao OpenGL).
Error DisplayServerSwitch::_iniciar_vulkan(DisplayServerEnums::VSyncMode p_vsync_mode) {
	rendering_context = memnew(RenderingContextDriverVulkanSwitch);
	if (rendering_context->initialize() != OK) {
		printf("Vulkan: o contexto não iniciou\n");
		memdelete(rendering_context);
		rendering_context = nullptr;
		return ERR_UNAVAILABLE;
	}
	NWindow *janela = nwindowGetDefault();
	nwindowSetDimensions(janela, tamanho.width, tamanho.height);
	RenderingContextDriverVulkanSwitch::WindowPlatformData wpd;
	wpd.janela = janela;
	if (rendering_context->window_create(DisplayServerEnums::MAIN_WINDOW_ID, &wpd) != OK) {
		printf("Vulkan: a superfície não foi criada\n");
		memdelete(rendering_context);
		rendering_context = nullptr;
		return ERR_UNAVAILABLE;
	}
	rendering_context->window_set_size(DisplayServerEnums::MAIN_WINDOW_ID, tamanho.width, tamanho.height);
	rendering_context->window_set_vsync_mode(DisplayServerEnums::MAIN_WINDOW_ID, p_vsync_mode);
	rendering_device = memnew(RenderingDevice);
	if (rendering_device->initialize(rendering_context, DisplayServerEnums::MAIN_WINDOW_ID) != OK) {
		printf("Vulkan: o RenderingDevice não iniciou\n");
		memdelete(rendering_device);
		rendering_device = nullptr;
		rendering_context->window_destroy(DisplayServerEnums::MAIN_WINDOW_ID);
		memdelete(rendering_context);
		rendering_context = nullptr;
		return ERR_UNAVAILABLE;
	}
	rendering_device->screen_create(DisplayServerEnums::MAIN_WINDOW_ID);
	RendererCompositorRD::make_current();
	return OK;
}

void DisplayServerSwitch::_encerrar_vulkan() {
	if (rendering_device) {
		rendering_device->screen_free(DisplayServerEnums::MAIN_WINDOW_ID);
		memdelete(rendering_device);
		rendering_device = nullptr;
	}
	if (rendering_context) {
		rendering_context->window_destroy(DisplayServerEnums::MAIN_WINDOW_ID);
		memdelete(rendering_context);
		rendering_context = nullptr;
	}
}
#endif

void DisplayServerSwitch::window_set_vsync_mode(DisplayServerEnums::VSyncMode p_vsync_mode, DisplayServerEnums::WindowID p_window) {
	vsync = p_vsync_mode;
#if defined(GLES3_ENABLED)
	if (egl_display != EGL_NO_DISPLAY) {
		eglSwapInterval(egl_display, p_vsync_mode == DisplayServerEnums::VSYNC_DISABLED ? 0 : 1);
	}
#endif
#if defined(RD_ENABLED)
	if (rendering_context) {
		rendering_context->window_set_vsync_mode(DisplayServerEnums::MAIN_WINDOW_ID, p_vsync_mode);
	}
#endif
}

// Toque: cada dedo vira InputEventScreenTouch (encostou/soltou) e InputEventScreenDrag (arrastou),
// em coordenadas da janela (a tela de toque tem as mesmas 1280×720 da janela).
void DisplayServerSwitch::_processar_toque() {
	HidTouchScreenState estado = {};
	if (hidGetTouchScreenStates(&estado, 1) == 0) {
		return;
	}
	bool agora[MAX_TOQUES] = {};
	for (int i = 0; i < estado.count && i < (int)(sizeof(estado.touches) / sizeof(estado.touches[0])); i++) {
		const HidTouchState &t = estado.touches[i];
		int id = (int)(t.finger_id % MAX_TOQUES);
		Vector2 pos(t.x * tamanho.width / 1280.0f, t.y * tamanho.height / 720.0f);
		agora[id] = true;
		if (!tocando[id]) {
			Ref<InputEventScreenTouch> ev;
			ev.instantiate();
			ev->set_index(id);
			ev->set_position(pos);
			ev->set_pressed(true);
			Input::get_singleton()->parse_input_event(ev);
		} else if (pos != ultimo_toque[id]) {
			Ref<InputEventScreenDrag> ev;
			ev.instantiate();
			ev->set_index(id);
			ev->set_position(pos);
			ev->set_relative(pos - ultimo_toque[id]);
			ev->set_relative_screen_position(pos - ultimo_toque[id]);
			Input::get_singleton()->parse_input_event(ev);
		}
		ultimo_toque[id] = pos;
	}
	for (int id = 0; id < MAX_TOQUES; id++) {
		if (tocando[id] && !agora[id]) {
			Ref<InputEventScreenTouch> ev;
			ev.instantiate();
			ev->set_index(id);
			ev->set_position(ultimo_toque[id]);
			ev->set_pressed(false);
			Input::get_singleton()->parse_input_event(ev);
		}
		tocando[id] = agora[id];
	}
}

void DisplayServerSwitch::process_events() {
	_processar_toque();
	Input::get_singleton()->flush_buffered_events();
}

uint64_t DisplayServerSwitch::troca_us = 0;
uint64_t DisplayServerSwitch::envio_us = 0;

void DisplayServerSwitch::avisar_janela(DisplayServerEnums::WindowEvent p_evento) {
	if (window_event_callback.is_valid()) {
		window_event_callback.call((int)p_evento);
	}
}

#if defined(GLES3_ENABLED)
void DisplayServerSwitch::swap_buffers() {
	const u64 antes = armGetSystemTick();
	eglSwapBuffers(egl_display, egl_surface);
	troca_us += armTicksToNs(armGetSystemTick() - antes) / 1000;
}

void DisplayServerSwitch::release_rendering_thread() {
	eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void DisplayServerSwitch::gl_window_make_current(DisplayServerEnums::WindowID p_window_id) {
	if (eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context) != EGL_TRUE) {
		printf("[switch] eglMakeCurrent falhou na thread de desenho (0x%x)\n", eglGetError());
	}
}

// A tela de abertura: os pixels crus (RGBA, 1280×720, de cima para baixo) em romfs:/abertura.rgba, que o
// gsn.py gera da imagem de abertura do projeto e põe no romfs do .nro. Sem o arquivo, a tela segue preta
// até o Godot mostrar a abertura dele (docs/decisoes.md, D-11).
static const int ABERTURA_LARGURA = 1280;
static const int ABERTURA_ALTURA = 720;

static uint8_t *_ler_abertura() {
	FILE *f = fopen("romfs:/abertura.rgba", "rb");
	if (!f) {
		return nullptr;
	}
	const size_t tamanho = (size_t)ABERTURA_LARGURA * ABERTURA_ALTURA * 4;
	uint8_t *pixels = (uint8_t *)malloc(tamanho);
	if (pixels && fread(pixels, 1, tamanho, f) != tamanho) {
		free(pixels);
		pixels = nullptr;
	}
	fclose(f);
	return pixels;
}

// Desenha a abertura assim que o vídeo existe: o Godot ainda leva uns 7 s compilando os shaders dele
// antes da própria imagem de abertura, e antes era tela preta. Devolve se havia imagem.
static bool _mostrar_abertura(EGLDisplay p_display, EGLSurface p_surface, Size2i p_tamanho) {
	uint8_t *abertura_rgba = _ler_abertura();
	if (!abertura_rgba) {
		return false;
	}
	GLuint textura = 0;
	GLuint quadro = 0;
	glGenTextures(1, &textura);
	glBindTexture(GL_TEXTURE_2D, textura);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ABERTURA_LARGURA, ABERTURA_ALTURA, 0, GL_RGBA, GL_UNSIGNED_BYTE, abertura_rgba);
	glGenFramebuffers(1, &quadro);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, quadro);
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textura, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	// a imagem vem de cima para baixo; no OpenGL a linha 0 é a de baixo: inverte na cópia
	glBlitFramebuffer(0, 0, ABERTURA_LARGURA, ABERTURA_ALTURA, 0, p_tamanho.height, p_tamanho.width, 0, GL_COLOR_BUFFER_BIT, GL_LINEAR);
	eglSwapBuffers(p_display, p_surface);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glDeleteFramebuffers(1, &quadro);
	glDeleteTextures(1, &textura);
	free(abertura_rgba);
	return true;
}

#endif

DisplayServerSwitch::DisplayServerSwitch(const String &p_rendering_driver, DisplayServerEnums::VSyncMode p_vsync_mode, Error &r_error) {
	rendering_driver = p_rendering_driver;
	r_error = ERR_UNAVAILABLE;
#if defined(RD_ENABLED)
	if (rendering_driver == "vulkan") {
		r_error = _iniciar_vulkan(p_vsync_mode);
		if (r_error != OK) {
			return;
		}
		vsync = p_vsync_mode;
		hidInitializeTouchScreen();
		printf("Tela: %dx%d, Vulkan (NVK)\n", tamanho.width, tamanho.height);
		return;
	}
#endif
#if defined(GLES3_ENABLED)
	if (rendering_driver == "opengl3") {
		r_error = _iniciar_egl();
		if (r_error != OK) {
			return;
		}
		if (_mostrar_abertura(egl_display, egl_surface, tamanho)) {
			printf("[switch] abertura na tela\n");
		}
		window_set_vsync_mode(p_vsync_mode);
		hidInitializeTouchScreen();
		RasterizerGLES3::make_current(false);
		printf("Tela: %dx%d, OpenGL ES %s\n", tamanho.width, tamanho.height, (const char *)glGetString(GL_VERSION));
		// o cache de shaders do Godot só funciona se o driver entregar o shader compilado em binário (docs/decisoes.md)
		GLint formatos = 0;
		glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &formatos);
		printf("[switch] formatos binários de shader do driver: %d\n", formatos);
		return;
	}
#endif
	printf("[switch] driver de vídeo sem suporte neste .nro: %s\n", rendering_driver.utf8().get_data());
}

DisplayServerSwitch::~DisplayServerSwitch() {
#if defined(RD_ENABLED)
	_encerrar_vulkan();
#endif
#if defined(GLES3_ENABLED)
	_encerrar_egl();
#endif
}
