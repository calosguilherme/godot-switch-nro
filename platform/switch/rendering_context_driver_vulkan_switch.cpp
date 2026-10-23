/**************************************************************************/
/*  rendering_context_driver_vulkan_switch.cpp                            */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/**************************************************************************/

#include "rendering_context_driver_vulkan_switch.h"

#ifdef VULKAN_ENABLED

#include "display_server_switch.h"

#include "core/variant/variant.h"

#include <drivers/vulkan/godot_vulkan.h>

#include "switch_wrapper.h"

// Onde a CPU espera a placa no Vulkan (docs/decisoes.md, D-15): o Godot chama estas duas direto (sem o volk), e
// o ligador as embrulha (-Wl,--wrap, detect.py). A espera pelas cercas é a "espera da GPU" do pulso (no OpenGL,
// a troca do EGL); o tempo do envio separa um envio que volta logo de um que espera a placa terminar (a placa
// e a CPU em série, e cada ms da placa vira ms de quadro).
extern "C" {
VkResult __real_vkWaitForFences(VkDevice p_device, uint32_t p_count, const VkFence *p_fences, VkBool32 p_all, uint64_t p_timeout);
VkResult __real_vkQueueSubmit(VkQueue p_queue, uint32_t p_count, const VkSubmitInfo *p_submits, VkFence p_fence);

VkResult __wrap_vkWaitForFences(VkDevice p_device, uint32_t p_count, const VkFence *p_fences, VkBool32 p_all, uint64_t p_timeout) {
	const u64 antes = armGetSystemTick();
	const VkResult r = __real_vkWaitForFences(p_device, p_count, p_fences, p_all, p_timeout);
	DisplayServerSwitch::troca_us += armTicksToNs(armGetSystemTick() - antes) / 1000;
	return r;
}

VkResult __wrap_vkQueueSubmit(VkQueue p_queue, uint32_t p_count, const VkSubmitInfo *p_submits, VkFence p_fence) {
	const u64 antes = armGetSystemTick();
	const VkResult r = __real_vkQueueSubmit(p_queue, p_count, p_submits, p_fence);
	DisplayServerSwitch::envio_us += armTicksToNs(armGetSystemTick() - antes) / 1000;
	return r;
}
}

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
