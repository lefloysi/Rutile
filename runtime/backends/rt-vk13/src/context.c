#include "context.h"
#include "error.h"
#include "resource/buffer.h"
#include "resource/queue.h"
#include "resource/texture.h"
#include "shader_compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

/*
** SPEC.html §2.6 Backend instances, §5.1 Backend state, §5.2 Init
** Owns the process-wide instance, device, allocator, and queue table.
** Context teardown follows queues -> allocator -> device -> messenger -> instance.
*/

struct rtvk_context* current_context = NULL;

struct rtvk_context* rtvk_get_current_context(void) { return current_context; }

static u64 rtvk_now_ns(void) {
#if defined(_WIN32)
	LARGE_INTEGER counter;
	LARGE_INTEGER frequency;
	QueryPerformanceCounter(&counter);
	QueryPerformanceFrequency(&frequency);
	return (u64)((counter.QuadPart * 1000000000ull) / frequency.QuadPart);
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (u64)ts.tv_sec * 1000000000ull + (u64)ts.tv_nsec;
#endif
}

static void rtvk_log_startup_time(u64 start_ns) {
	u64 elapsed_ns = rtvk_now_ns() - start_ns;
	rtvk_printf("rt-vk13: initialized in %.3f ms\n", (double)elapsed_ns / 1000000.0);
}

#if defined(RTVK_ENABLE_VULKAN_VALIDATION)
static const char* rtvk_validation_layers[] = {
	"VK_LAYER_KHRONOS_validation",
};

static bool rtvk_debug_message_ignored(const char* message) {
	if (!message) {
		return false;
	}

	return strstr(message, "loader_get_json: Failed to open JSON file") != NULL ||
		   strstr(message, "VK_LAYER_OW_OVERLAY uses API version 1.2") != NULL ||
		   strstr(message, "VK_LAYER_OW_OBS_HOOK uses API version 1.2") != NULL ||
		   strstr(message, "VK_LAYER_OBS_HOOK uses API version 1.3") != NULL;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL rtvk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {
	if (rtvk_debug_message_ignored(callback_data ? callback_data->pMessage : NULL)) {
		return VK_FALSE;
	}

	if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		rtvk_printf("vulkan validation: %s\n", callback_data->pMessage);
	}

	return VK_FALSE;
}
static bool rtvk_instance_layer_available(const char* name) {
	u32 layer_count = 0;
	VkResult result = vkEnumerateInstanceLayerProperties(&layer_count, NULL);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return false;
	}

	VkLayerProperties* layers = calloc(layer_count, sizeof(*layers));
	if (!layers) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %u Vulkan layer entries", layer_count);
		return false;
	}

	result = vkEnumerateInstanceLayerProperties(&layer_count, layers);
	if (result != VK_SUCCESS) {
		free(layers);
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return false;
	}

	bool found = false;
	for (u32 i = 0; i < layer_count; i++) {
		if (strcmp(layers[i].layerName, name) == 0) {
			found = true;
			break;
		}
	}

	free(layers);
	return found;
}
static void rtvk_context_create_debug_messenger(struct rtvk_context* ctx) {
	PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT_proc = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ctx->vk_instance, "vkCreateDebugUtilsMessengerEXT");
	if (!vkCreateDebugUtilsMessengerEXT_proc) {
		rtvk_printf("rt-vk13: vkCreateDebugUtilsMessengerEXT not available; debug messenger disabled\n");
		return;
	}

	VkDebugUtilsMessengerCreateInfoEXT debug_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debug_info.pfnUserCallback = rtvk_debug_callback;

	VkResult result = vkCreateDebugUtilsMessengerEXT_proc(ctx->vk_instance, &debug_info, VK_ALLOCATOR, &ctx->vk_debug_messenger);
	if (result != VK_SUCCESS) {
		ctx->vk_debug_messenger = VK_NULL_HANDLE;
	}
}
static void rtvk_context_destroy_debug_messenger(struct rtvk_context* ctx) {
	if (!ctx->vk_debug_messenger) {
		return;
	}

	PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT_proc = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ctx->vk_instance, "vkDestroyDebugUtilsMessengerEXT");
	if (vkDestroyDebugUtilsMessengerEXT_proc) {
		vkDestroyDebugUtilsMessengerEXT_proc(ctx->vk_instance, ctx->vk_debug_messenger, VK_ALLOCATOR);
	}
	ctx->vk_debug_messenger = VK_NULL_HANDLE;
}
#endif

static void rtvk_enumerate_instance_extensions(VkExtensionProperties** out_extensions, u32* out_count) {
	*out_extensions = NULL;
	*out_count = 0;

	VkResult result = vkEnumerateInstanceExtensionProperties(NULL, out_count, NULL);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}
	if (*out_count == 0) {
		return;
	}

	VkExtensionProperties* extensions = calloc(*out_count, sizeof(*extensions));
	if (!extensions) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %u Vulkan instance extension entries", *out_count);
		return;
	}

	result = vkEnumerateInstanceExtensionProperties(NULL, out_count, extensions);
	if (result != VK_SUCCESS) {
		free(extensions);
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}

	*out_extensions = extensions;
}

static bool rtvk_instance_extension_available(const VkExtensionProperties* available_extensions, u32 available_extension_count, const char* name) {
	for (u32 i = 0; i < available_extension_count; i++) {
		if (strcmp(available_extensions[i].extensionName, name) == 0) {
			return true;
		}
	}
	return false;
}

static bool rtvk_instance_extension_enabled(const char* const* extensions, u32 extension_count, const char* name) {
	for (u32 i = 0; i < extension_count; i++) {
		if (strcmp(extensions[i], name) == 0) {
			return true;
		}
	}
	return false;
}

static void rtvk_add_instance_extension(const char* name, const VkExtensionProperties* available_extensions, u32 available_extension_count, const char** instance_extensions, u32* instance_extension_count, u32 instance_extension_capacity) {
	if (!rtvk_instance_extension_available(available_extensions, available_extension_count, name)) {
		rtvk_throwf(RT_UNSUPPORTED_PLATFORM, "required Vulkan instance extension is not available: %s", name);
		return;
	}
	if (rtvk_instance_extension_enabled(instance_extensions, *instance_extension_count, name)) {
		return;
	}
	if (*instance_extension_count >= instance_extension_capacity) {
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "too many Vulkan instance extensions requested");
		return;
	}
	instance_extensions[*instance_extension_count] = name;
	(*instance_extension_count)++;
}

static bool rtvk_try_add_instance_extension(const char* name, const VkExtensionProperties* available_extensions, u32 available_extension_count, const char** instance_extensions, u32* instance_extension_count, u32 instance_extension_capacity) {
	if (!rtvk_instance_extension_available(available_extensions, available_extension_count, name)) {
		return false;
	}
	rtvk_add_instance_extension(
		name,
		available_extensions,
		available_extension_count,
		instance_extensions,
		instance_extension_count,
		instance_extension_capacity
	);
	return rtvk_error() == RT_SUCCESS;
}

static void rtvk_add_presentation_instance_extensions(const VkExtensionProperties* available_extensions, u32 available_extension_count, const char** instance_extensions, u32* instance_extension_count, u32 instance_extension_capacity) {
	rtvk_add_instance_extension(
		VK_KHR_SURFACE_EXTENSION_NAME,
		available_extensions,
		available_extension_count,
		instance_extensions,
		instance_extension_count,
		instance_extension_capacity
	);
	if (rtvk_error() != RT_SUCCESS) {
		return;
	}

#if defined(_WIN32)
	rtvk_add_instance_extension(
		"VK_KHR_win32_surface",
		available_extensions,
		available_extension_count,
		instance_extensions,
		instance_extension_count,
		instance_extension_capacity
	);
#elif defined(__APPLE__)
	rtvk_add_instance_extension(
		"VK_EXT_metal_surface",
		available_extensions,
		available_extension_count,
		instance_extensions,
		instance_extension_count,
		instance_extension_capacity
	);
#elif defined(__ANDROID__)
	rtvk_add_instance_extension(
		"VK_KHR_android_surface",
		available_extensions,
		available_extension_count,
		instance_extensions,
		instance_extension_count,
		instance_extension_capacity
	);
#elif defined(__linux__)
	bool found_surface_backend = false;
	found_surface_backend |= rtvk_try_add_instance_extension(
		"VK_KHR_xcb_surface",
		available_extensions,
		available_extension_count,
		instance_extensions,
		instance_extension_count,
		instance_extension_capacity
	);
	found_surface_backend |= rtvk_try_add_instance_extension(
		"VK_KHR_xlib_surface",
		available_extensions,
		available_extension_count,
		instance_extensions,
		instance_extension_count,
		instance_extension_capacity
	);
	found_surface_backend |= rtvk_try_add_instance_extension(
		"VK_KHR_wayland_surface",
		available_extensions,
		available_extension_count,
		instance_extensions,
		instance_extension_count,
		instance_extension_capacity
	);
	if (rtvk_error() != RT_SUCCESS) {
		return;
	}
	if (!found_surface_backend) {
		rtvk_throwf(RT_UNSUPPORTED_PLATFORM, "no supported Vulkan platform surface extension was found");
	}
#else
	rtvk_throwf(RT_UNSUPPORTED_PLATFORM, "presentation is not supported on this platform");
#endif
}

static enum rt_queue_capability rtvk_queue_capability_from_vk(VkQueueFlags flags) {
	if (flags & VK_QUEUE_GRAPHICS_BIT) {
		return RT_QUEUE_GRAPHICS;
	}
	if (flags & VK_QUEUE_COMPUTE_BIT) {
		return RT_QUEUE_COMPUTE;
	}
	return RT_QUEUE_TRANSFER;
}

static void rtvk_context_destroy_queues(struct rtvk_context* ctx) {
	for (u32 i = 0; i < ctx->queue_count; i++) {
		rtvk_queue_destroy(ctx, ctx->queues[i]);
	}
	free(ctx->queues);
	ctx->queues = NULL;
	ctx->queue_count = 0;
}

static void rtvk_context_pick_physical_device(struct rtvk_context* ctx) {
	u32 physical_device_count = 0;
	VkResult result = vkEnumeratePhysicalDevices(ctx->vk_instance, &physical_device_count, NULL);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}
	if (physical_device_count == 0) {
		rtvk_throwf(RT_INCOMPATIBLE_DRIVER, NULL);
		return;
	}

	VkPhysicalDevice* physical_devices = calloc(physical_device_count, sizeof(*physical_devices));
	if (!physical_devices) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %u Vulkan physical device entries", physical_device_count);
		return;
	}

	result = vkEnumeratePhysicalDevices(ctx->vk_instance, &physical_device_count, physical_devices);
	if (result != VK_SUCCESS) {
		free(physical_devices);
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}

	ctx->vk_physical_device = physical_devices[0];
	for (u32 i = 0; i < physical_device_count; i++) {
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(physical_devices[i], &properties);
		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			ctx->vk_physical_device = physical_devices[i];
			break;
		}
	}
	free(physical_devices);
}

static void rtvk_context_create_device(struct rtvk_context* ctx) {
	u32 queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(ctx->vk_physical_device, &queue_family_count, NULL);
	if (queue_family_count == 0) {
		rtvk_throwf(RT_INCOMPATIBLE_DRIVER, NULL);
		return;
	}

	VkQueueFamilyProperties* queue_families = calloc(queue_family_count, sizeof(*queue_families));
	VkDeviceQueueCreateInfo* queue_infos = calloc(queue_family_count, sizeof(*queue_infos));
	f32* priorities = calloc(queue_family_count, sizeof(*priorities));
	if (!queue_families || !queue_infos || !priorities) {
		free(queue_families);
		free(queue_infos);
		free(priorities);
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate Vulkan queue family metadata for %u families", queue_family_count);
		return;
	}

	vkGetPhysicalDeviceQueueFamilyProperties(ctx->vk_physical_device, &queue_family_count, queue_families);

	for (u32 i = 0; i < queue_family_count; i++) {
		priorities[i] = 1.0f;

		queue_infos[i] = (VkDeviceQueueCreateInfo){ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		queue_infos[i].pNext = NULL;
		queue_infos[i].flags = 0;
		queue_infos[i].queueFamilyIndex = i;
		queue_infos[i].queueCount = 1;
		queue_infos[i].pQueuePriorities = &priorities[i];
	}

	VkPhysicalDeviceVulkan13Features features13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	features13.pNext = NULL;
	features13.robustImageAccess = VK_FALSE;
	features13.inlineUniformBlock = VK_FALSE;
	features13.descriptorBindingInlineUniformBlockUpdateAfterBind = VK_FALSE;
	features13.pipelineCreationCacheControl = VK_FALSE;
	features13.privateData = VK_FALSE;
	features13.shaderDemoteToHelperInvocation = VK_FALSE;
	features13.shaderTerminateInvocation = VK_FALSE;
	features13.subgroupSizeControl = VK_FALSE;
	features13.computeFullSubgroups = VK_FALSE;
	features13.synchronization2 = VK_FALSE;
	features13.textureCompressionASTC_HDR = VK_FALSE;
	features13.shaderZeroInitializeWorkgroupMemory = VK_FALSE;
	features13.dynamicRendering = VK_TRUE;
	features13.shaderIntegerDotProduct = VK_FALSE;
	features13.maintenance4 = VK_FALSE;

	VkPhysicalDeviceVulkan12Features features12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	features12.pNext = &features13;
	features12.samplerMirrorClampToEdge = VK_FALSE;
	features12.drawIndirectCount = VK_FALSE;
	features12.storageBuffer8BitAccess = VK_FALSE;
	features12.uniformAndStorageBuffer8BitAccess = VK_FALSE;
	features12.storagePushConstant8 = VK_FALSE;
	features12.shaderBufferInt64Atomics = VK_FALSE;
	features12.shaderSharedInt64Atomics = VK_FALSE;
	features12.shaderFloat16 = VK_FALSE;
	features12.shaderInt8 = VK_FALSE;
	features12.descriptorIndexing = VK_FALSE;
	features12.shaderInputAttachmentArrayDynamicIndexing = VK_FALSE;
	features12.shaderUniformTexelBufferArrayDynamicIndexing = VK_FALSE;
	features12.shaderStorageTexelBufferArrayDynamicIndexing = VK_FALSE;
	features12.shaderUniformBufferArrayNonUniformIndexing = VK_FALSE;
	features12.shaderSampledImageArrayNonUniformIndexing = VK_FALSE;
	features12.shaderStorageBufferArrayNonUniformIndexing = VK_FALSE;
	features12.shaderStorageImageArrayNonUniformIndexing = VK_FALSE;
	features12.shaderInputAttachmentArrayNonUniformIndexing = VK_FALSE;
	features12.shaderUniformTexelBufferArrayNonUniformIndexing = VK_FALSE;
	features12.shaderStorageTexelBufferArrayNonUniformIndexing = VK_FALSE;
	features12.descriptorBindingUniformBufferUpdateAfterBind = VK_FALSE;
	features12.descriptorBindingSampledImageUpdateAfterBind = VK_FALSE;
	features12.descriptorBindingStorageImageUpdateAfterBind = VK_FALSE;
	features12.descriptorBindingStorageBufferUpdateAfterBind = VK_FALSE;
	features12.descriptorBindingUniformTexelBufferUpdateAfterBind = VK_FALSE;
	features12.descriptorBindingStorageTexelBufferUpdateAfterBind = VK_FALSE;
	features12.descriptorBindingUpdateUnusedWhilePending = VK_FALSE;
	features12.descriptorBindingPartiallyBound = VK_FALSE;
	features12.descriptorBindingVariableDescriptorCount = VK_FALSE;
	features12.runtimeDescriptorArray = VK_FALSE;
	features12.samplerFilterMinmax = VK_FALSE;
	features12.scalarBlockLayout = VK_FALSE;
	features12.imagelessFramebuffer = VK_FALSE;
	features12.uniformBufferStandardLayout = VK_FALSE;
	features12.shaderSubgroupExtendedTypes = VK_FALSE;
	features12.separateDepthStencilLayouts = VK_FALSE;
	features12.hostQueryReset = VK_FALSE;
	features12.timelineSemaphore = VK_TRUE;
	features12.bufferDeviceAddress = VK_FALSE;
	features12.bufferDeviceAddressCaptureReplay = VK_FALSE;
	features12.bufferDeviceAddressMultiDevice = VK_FALSE;
	features12.vulkanMemoryModel = VK_FALSE;
	features12.vulkanMemoryModelDeviceScope = VK_FALSE;
	features12.vulkanMemoryModelAvailabilityVisibilityChains = VK_FALSE;
	features12.shaderOutputViewportIndex = VK_FALSE;
	features12.shaderOutputLayer = VK_FALSE;
	features12.subgroupBroadcastDynamicId = VK_FALSE;

	VkPhysicalDeviceFeatures supported_features = { 0 };
	vkGetPhysicalDeviceFeatures(ctx->vk_physical_device, &supported_features);
	VkPhysicalDeviceFeatures features = { 0 };
	features.shaderInt64 = supported_features.shaderInt64;

	VkDeviceCreateInfo device_info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	device_info.pNext = &features12;
	device_info.flags = 0;
	device_info.queueCreateInfoCount = queue_family_count;
	device_info.pQueueCreateInfos = queue_infos;
	device_info.enabledLayerCount = 0;
	device_info.ppEnabledLayerNames = NULL;
	if (ctx->flags.presentation) {
		static const char* device_extensions[] = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		};
		device_info.enabledExtensionCount = (u32)(sizeof(device_extensions) / sizeof(device_extensions[0]));
		device_info.ppEnabledExtensionNames = device_extensions;
	} else {
		device_info.enabledExtensionCount = 0;
		device_info.ppEnabledExtensionNames = NULL;
	}
	device_info.pEnabledFeatures = &features;

	VkResult result = vkCreateDevice(ctx->vk_physical_device, &device_info, VK_ALLOCATOR, &ctx->vk_device);
	free(priorities);
	free(queue_infos);

	if (result != VK_SUCCESS) {
		free(queue_families);
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}

	ctx->queues = calloc(queue_family_count, sizeof(*ctx->queues));
	if (!ctx->queues) {
		free(queue_families);
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %u Vulkan queue handles", queue_family_count);
		return;
	}

	for (u32 i = 0; i < queue_family_count; i++) {
		VkQueue vk_queue;
		vkGetDeviceQueue(ctx->vk_device, i, 0, &vk_queue);

		struct rtvk_queue* queue = rtvk_queue_create(ctx, vk_queue, rtvk_queue_capability_from_vk(queue_families[i].queueFlags), i, 0);
		if (!queue) {
			free(queue_families);
			return;
		}
		ctx->queues[ctx->queue_count] = queue;
		ctx->queue_count++;
	}

	free(queue_families);
}

static void rtvk_context_create_allocator(struct rtvk_context* ctx) {
	VmaAllocatorCreateInfo allocator_info = { 0 };
	allocator_info.flags = 0;
	allocator_info.physicalDevice = ctx->vk_physical_device;
	allocator_info.device = ctx->vk_device;
	allocator_info.instance = ctx->vk_instance;
	allocator_info.vulkanApiVersion = RTVK_VULKAN_API_VERSION;

	VkResult result = vmaCreateAllocator(&allocator_info, &ctx->vma_allocator);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
	}
}

struct rtvk_context* rtvk_create_context(rtvk_context_flags flags) {
	struct rtvk_context* result = calloc(1, sizeof(struct rtvk_context));
	if (!result) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for Vulkan context", sizeof(struct rtvk_context));
		return NULL;
	}

	result->flags = flags;
	rtvk_context_init(result);
	if (rtvk_error() != RT_SUCCESS) {
		rtvk_context_destroy(result);
		return NULL;
	}

	return result;
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtvk_context_init(struct rtvk_context* ctx) {
	u64 start_ns = rtvk_now_ns();
	const char* instance_extensions[16];
	u32 instance_extension_count = 0;
	VkExtensionProperties* available_instance_extensions = NULL;
	u32 available_instance_extension_count = 0;
#if defined(RTVK_ENABLE_VULKAN_VALIDATION)
	bool validation_enabled = true;
	for (u32 i = 0; i < (u32)(sizeof(rtvk_validation_layers) / sizeof(rtvk_validation_layers[0])); i++) {
		if (!rtvk_instance_layer_available(rtvk_validation_layers[i])) {
			validation_enabled = false;
			break;
		}
	}
	VkDebugUtilsMessengerCreateInfoEXT debug_info;
#endif

	rtvk_enumerate_instance_extensions(&available_instance_extensions, &available_instance_extension_count);
	if (rtvk_error() != RT_SUCCESS) {
		return;
	}

	if (ctx->flags.presentation) {
		rtvk_add_presentation_instance_extensions(
			available_instance_extensions,
			available_instance_extension_count,
			instance_extensions,
			&instance_extension_count,
			(u32)(sizeof(instance_extensions) / sizeof(instance_extensions[0]))
		);
		if (rtvk_error() != RT_SUCCESS) {
			free(available_instance_extensions);
			return;
		}
	}

#if defined(RTVK_ENABLE_VULKAN_VALIDATION)
	if (validation_enabled) {
		rtvk_add_instance_extension(
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
			available_instance_extensions,
			available_instance_extension_count,
			instance_extensions,
			&instance_extension_count,
			(u32)(sizeof(instance_extensions) / sizeof(instance_extensions[0]))
		);
		if (rtvk_error() != RT_SUCCESS) {
			free(available_instance_extensions);
			return;
		}

		debug_info = (VkDebugUtilsMessengerCreateInfoEXT){ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debug_info.pfnUserCallback = rtvk_debug_callback;
	}
#endif

	free(available_instance_extensions);

	VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app_info.pNext = NULL;
	app_info.pApplicationName = "Rutile Application";
	app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
	app_info.pEngineName = "Rutile";
	app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
	app_info.apiVersion = RTVK_VULKAN_API_VERSION;

	VkInstanceCreateInfo instance_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
#if defined(RTVK_ENABLE_VULKAN_VALIDATION)
	instance_info.pNext = validation_enabled ? &debug_info : NULL;
#else
	instance_info.pNext = NULL;
#endif
	instance_info.flags = 0;
	instance_info.pApplicationInfo = &app_info;
#if defined(RTVK_ENABLE_VULKAN_VALIDATION)
	instance_info.enabledLayerCount = validation_enabled ? (u32)(sizeof(rtvk_validation_layers) / sizeof(rtvk_validation_layers[0])) : 0;
	instance_info.ppEnabledLayerNames = validation_enabled ? rtvk_validation_layers : NULL;
#else
	instance_info.enabledLayerCount = 0;
	instance_info.ppEnabledLayerNames = NULL;
#endif
	instance_info.enabledExtensionCount = instance_extension_count;
	instance_info.ppEnabledExtensionNames = instance_extensions;

	VkResult result = vkCreateInstance(&instance_info, VK_ALLOCATOR, &ctx->vk_instance);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}
#if defined(RTVK_ENABLE_VULKAN_VALIDATION)
	if (validation_enabled) {
		rtvk_context_create_debug_messenger(ctx);
	}
#endif

	rtvk_context_pick_physical_device(ctx);
	if (rtvk_error() != RT_SUCCESS) {
		return;
	}

	rtvk_context_create_device(ctx);
	if (rtvk_error() != RT_SUCCESS) {
		return;
	}

	rtvk_context_create_allocator(ctx);
	if (rtvk_error() != RT_SUCCESS) {
		return;
	}

	rtvk_log_startup_time(start_ns);
}
void rtvk_context_finish(struct rtvk_context* ctx) {
	if (!ctx) {
		return;
	}

	rtvk_context_destroy_queues(ctx);
	if (ctx->vma_allocator) {
#if defined(RTVK_ENABLE_VULKAN_VALIDATION)
		VmaTotalStatistics stats = { 0 };
		vmaCalculateStatistics(ctx->vma_allocator, &stats);
		if (stats.total.statistics.allocationCount) {
			char* leak_dump = NULL;
			vmaBuildStatsString(ctx->vma_allocator, &leak_dump, VK_TRUE);
			fprintf(stderr, "[rt-vk13] vmaDestroyAllocator: %u dedicated/pool allocations still live (%llu bytes)\n", stats.total.statistics.allocationCount, (unsigned long long)stats.total.statistics.allocationBytes);
			if (leak_dump) {
				fprintf(stderr, "%s\n", leak_dump);
				vmaFreeStatsString(ctx->vma_allocator, leak_dump);
			}
		}
#endif
		vmaDestroyAllocator(ctx->vma_allocator);
		ctx->vma_allocator = VK_NULL_HANDLE;
	}
	if (ctx->vk_device) {
		vkDestroyDevice(ctx->vk_device, VK_ALLOCATOR);
		ctx->vk_device = VK_NULL_HANDLE;
	}
#if defined(RTVK_ENABLE_VULKAN_VALIDATION)
	rtvk_context_destroy_debug_messenger(ctx);
#endif
	if (ctx->vk_instance) {
		vkDestroyInstance(ctx->vk_instance, VK_ALLOCATOR);
		ctx->vk_instance = VK_NULL_HANDLE;
	}
}
void rtvk_context_destroy(struct rtvk_context* ctx) {
	if (!ctx) {
		return;
	}

	rtvk_context_finish(ctx);
	free(ctx);
}
