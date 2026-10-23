/**************************************************************************/
/*  display_server_switch.h                                               */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/* Referência: o port do Godot 3.5 de Stary2001 e cpasjuste (MIT).        */
/**************************************************************************/

#pragma once

#include "servers/display/display_server_headless.h"

#if defined(GLES3_ENABLED)
#include <EGL/egl.h>
#endif
#include "switch_wrapper.h"

#if defined(RD_ENABLED)
class RenderingContextDriver;
class RenderingDevice;
#endif

// Uma janela só, em tela cheia: herda os "não faz nada" do headless e preenche o que importa no
// Switch (vídeo, tamanho, troca de quadros, toque). O vídeo é OpenGL ES pelo EGL (o <jogo>.nro, Mesa
// 20.1) ou Vulkan pelo NVK (o <jogo>.vulkan, Mesa 26; docs/decisoes.md): cada .nro é compilado com um dos dois.
class DisplayServerSwitch : public DisplayServerHeadless {
	GDSOFTCLASS(DisplayServerSwitch, DisplayServerHeadless);

#if defined(GLES3_ENABLED)
	EGLDisplay egl_display = EGL_NO_DISPLAY;
	EGLSurface egl_surface = EGL_NO_SURFACE;
	EGLContext egl_context = EGL_NO_CONTEXT;
#endif
#if defined(RD_ENABLED)
	RenderingContextDriver *rendering_context = nullptr;
	RenderingDevice *rendering_device = nullptr;
#endif
	String rendering_driver;
	Size2i tamanho = Size2i(1280, 720);
	DisplayServerEnums::VSyncMode vsync = DisplayServerEnums::VSYNC_ENABLED;
	Callable rect_changed_callback;
	Callable window_event_callback;

	static const int MAX_TOQUES = 16;
	bool tocando[MAX_TOQUES] = {};
	Vector2 ultimo_toque[MAX_TOQUES];

	static Vector<String> get_rendering_drivers_func();
	static DisplayServer *create_func(const String &p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error &r_error);

#if defined(GLES3_ENABLED)
	Error _iniciar_egl();
	void _encerrar_egl();
#endif
#if defined(RD_ENABLED)
	Error _iniciar_vulkan(DisplayServerEnums::VSyncMode p_vsync_mode);
	void _encerrar_vulkan();
#endif
	void _processar_toque();

public:
	static void register_switch_driver();

	bool has_feature(DisplayServerEnums::Feature p_feature) const override;
	String get_name() const override { return "Switch"; }

	int get_screen_count() const override { return 1; }
	int get_primary_screen() const override { return 0; }
	Size2i screen_get_size(int p_screen = DisplayServerEnums::SCREEN_OF_MAIN_WINDOW) const override { return tamanho; }
	Rect2i screen_get_usable_rect(int p_screen = DisplayServerEnums::SCREEN_OF_MAIN_WINDOW) const override { return Rect2i(Point2i(), tamanho); }
	int screen_get_dpi(int p_screen = DisplayServerEnums::SCREEN_OF_MAIN_WINDOW) const override { return 237; }
	float screen_get_refresh_rate(int p_screen = DisplayServerEnums::SCREEN_OF_MAIN_WINDOW) const override { return 60.0; }

	Vector<DisplayServerEnums::WindowID> get_window_list() const override;
	void window_set_rect_changed_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override { rect_changed_callback = p_callable; }
	void window_set_window_event_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override { window_event_callback = p_callable; }
	int window_get_current_screen(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return 0; }
	Size2i window_get_size(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return tamanho; }
	Size2i window_get_size_with_decorations(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return tamanho; }
	DisplayServerEnums::WindowMode window_get_mode(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return DisplayServerEnums::WINDOW_MODE_EXCLUSIVE_FULLSCREEN; }
	void window_set_vsync_mode(DisplayServerEnums::VSyncMode p_vsync_mode, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	DisplayServerEnums::VSyncMode window_get_vsync_mode(DisplayServerEnums::WindowID p_window) const override { return vsync; }
	bool window_can_draw(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return true; }
	bool can_any_window_draw() const override { return true; }
	bool is_touchscreen_available() const override { return true; }

	void process_events() override;
	// manda um evento à janela (o Godot o espalha aos nós: NOTIFICATION_WM_CLOSE_REQUEST etc.)
	void avisar_janela(DisplayServerEnums::WindowEvent p_evento);
#if defined(GLES3_ENABLED)
	void swap_buffers() override;
	// desenho numa thread separada (rendering/driver/threads/thread_model; no Switch só com o arquivo
	// render_separado ao lado do .nro): o contexto do OpenGL sai da thread principal e vai para a dela
	void release_rendering_thread() override;
	void gl_window_make_current(DisplayServerEnums::WindowID p_window_id) override;
#endif

	// tempo esperando a placa de vídeo terminar, somado em µs (OpenGL: a troca do EGL; Vulkan: as cercas,
	// vkWaitForFences): o OS_Switch lê e zera
	static uint64_t troca_us;
	// Vulkan: o tempo dentro do vkQueueSubmit, somado em µs
	static uint64_t envio_us;

	DisplayServerSwitch(const String &p_rendering_driver, DisplayServerEnums::VSyncMode p_vsync_mode, Error &r_error);
	~DisplayServerSwitch();
};
