/**************************************************************************/
/*  rendering_context_driver_vulkan_switch.cpp                            */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/**************************************************************************/

#include "rendering_context_driver_vulkan_switch.h"

#ifdef VULKAN_ENABLED

#include "core/variant/variant.h"

#include <drivers/vulkan/godot_vulkan.h>

const char *RenderingContextDriverVulkanSwitch::_get_platform_surface_extension() const {
	return VK_NN_VI_SURFACE_EXTENSION_NAME;
}

RenderingContextDriver::SurfaceID RenderingContextDriverVulkanSwitch::surface_create(const void *p_platform_data) {
	const WindowPlatformData *wpd = (const WindowPlatformData *)(p_platform_data);

	VkViSurfaceCreateInfoNN create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN;
	create_info.window = wpd->janela;

	VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
	VkResult err = vkCreateViSurfaceNN(instance_get(), &create_info, get_allocation_callbacks(VK_OBJECT_TYPE_SURFACE_KHR), &vk_surface);
	ERR_FAIL_COND_V_MSG(err != VK_SUCCESS, SurfaceID(), vformat("Couldn't create Switch VI surface (VkResult error %d).", err));

	Surface *surface = memnew(Surface);
	surface->vk_surface = vk_surface;
	return SurfaceID(surface);
}

#endif // VULKAN_ENABLED
