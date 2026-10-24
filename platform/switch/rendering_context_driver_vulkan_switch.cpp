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

#include <cstdio>
#include <cstring>

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

// O tamanho da troca de quadros (docs/decisoes.md, D-20): o Godot cria a troca com o currentExtent da superfície, e
// o NVK o tira das dimensões da janela da libnx, que só mudam ao criar a troca (um círculo). O Godot pega a função
// pelo vkGetInstanceProcAddr, que o ligador embrulha: a consulta devolve o tamanho do modo (na base, 1920×1080).
static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR _capacidades_reais = nullptr;

static VKAPI_ATTR VkResult VKAPI_CALL _capacidades(VkPhysicalDevice p_dispositivo, VkSurfaceKHR p_superficie, VkSurfaceCapabilitiesKHR *r_capacidades) {
	const VkResult r = _capacidades_reais(p_dispositivo, p_superficie, r_capacidades);
	if (r == VK_SUCCESS) {
		const Size2i t = DisplayServerSwitch::tamanho_da_superficie;
		r_capacidades->currentExtent = { (uint32_t)t.width, (uint32_t)t.height };
	}
	return r;
}

// A apresentação do quadro (D-15): o Godot pega o vkAcquireNextImageKHR e o vkQueuePresentKHR pelo
// vkGetDeviceProcAddr, que ele pega pelo vkGetInstanceProcAddr. O tempo das duas entra no pulso
// ("apresentação"), e uma que passe de APRESENTACAO_LONGA_MS sai no log na hora: um quadro de 305 ms só de espera,
// sem trabalho do jogo, só assim se sabe se foi a troca de quadros (D-15).
static const uint64_t APRESENTACAO_LONGA_MS = 100;
static PFN_vkGetDeviceProcAddr _endereco_real = nullptr;
static PFN_vkAcquireNextImageKHR _pegar_real = nullptr;
static PFN_vkQueuePresentKHR _apresentar_real = nullptr;

static void _contar_apresentacao(const char *p_qual, u64 p_antes) {
	const uint64_t us = armTicksToNs(armGetSystemTick() - p_antes) / 1000;
	DisplayServerSwitch::apresentacao_us += us;
	if (us > APRESENTACAO_LONGA_MS * 1000) {
		printf("[switch] apresentação longa: %s em %llu ms\n", p_qual, (unsigned long long)(us / 1000));
	}
}

static VKAPI_ATTR VkResult VKAPI_CALL _pegar(VkDevice p_dispositivo, VkSwapchainKHR p_troca, uint64_t p_limite, VkSemaphore p_semaforo, VkFence p_cerca, uint32_t *r_indice) {
	const u64 antes = armGetSystemTick();
	const VkResult r = _pegar_real(p_dispositivo, p_troca, p_limite, p_semaforo, p_cerca, r_indice);
	_contar_apresentacao("vkAcquireNextImageKHR", antes);
	return r;
}

static VKAPI_ATTR VkResult VKAPI_CALL _apresentar(VkQueue p_fila, const VkPresentInfoKHR *p_info) {
	const u64 antes = armGetSystemTick();
	const VkResult r = _apresentar_real(p_fila, p_info);
	_contar_apresentacao("vkQueuePresentKHR", antes);
	return r;
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL _endereco(VkDevice p_dispositivo, const char *p_nome) {
	const PFN_vkVoidFunction f = _endereco_real(p_dispositivo, p_nome);
	if (f && p_nome && strcmp(p_nome, "vkAcquireNextImageKHR") == 0) {
		_pegar_real = (PFN_vkAcquireNextImageKHR)f;
		return (PFN_vkVoidFunction)_pegar;
	}
	if (f && p_nome && strcmp(p_nome, "vkQueuePresentKHR") == 0) {
		_apresentar_real = (PFN_vkQueuePresentKHR)f;
		return (PFN_vkVoidFunction)_apresentar;
	}
	return f;
}

extern "C" {
PFN_vkVoidFunction __real_vkGetInstanceProcAddr(VkInstance p_instancia, const char *p_nome);

PFN_vkVoidFunction __wrap_vkGetInstanceProcAddr(VkInstance p_instancia, const char *p_nome) {
	const PFN_vkVoidFunction f = __real_vkGetInstanceProcAddr(p_instancia, p_nome);
	if (f && p_nome && strcmp(p_nome, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR") == 0) {
		_capacidades_reais = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)f;
		return (PFN_vkVoidFunction)_capacidades;
	}
	if (f && p_nome && strcmp(p_nome, "vkGetDeviceProcAddr") == 0) {
		_endereco_real = (PFN_vkGetDeviceProcAddr)f;
		return (PFN_vkVoidFunction)_endereco;
	}
	return f;
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
