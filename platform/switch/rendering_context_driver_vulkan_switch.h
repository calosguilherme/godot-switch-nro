/**************************************************************************/
/*  rendering_context_driver_vulkan_switch.h                              */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/**************************************************************************/

#pragma once

#ifdef VULKAN_ENABLED

#include "drivers/vulkan/rendering_context_driver_vulkan.h"

// Vulkan no Switch homebrew (docs/decisoes.md): o NVK do Mesa 26 (danfromtico/mesa-switch), ligado direto no .nro
// (sem carregador). A superfície é a da extensão VK_NN_vi_surface, sobre a janela do homebrew (NWindow).
class RenderingContextDriverVulkanSwitch : public RenderingContextDriverVulkan {
private:
	virtual const char *_get_platform_surface_extension() const override final;

protected:
	SurfaceID surface_create(const void *p_platform_data) override final;
	bool _use_validation_layers() const override final { return false; }

public:
	struct WindowPlatformData {
		void *janela; // NWindow *
	};

	RenderingContextDriverVulkanSwitch() = default;
	~RenderingContextDriverVulkanSwitch() override = default;
};

#endif // VULKAN_ENABLED
