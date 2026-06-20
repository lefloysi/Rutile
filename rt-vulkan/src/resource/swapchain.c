#include "resource/swapchain.h"
#include "context.h"
#include "error.h"
#include "resource/queue.h"

#include <stdlib.h>

const VkSurfaceFormatKHR rtvk_swapchain_format_preferences[] = {
	{ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
	{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
	{ VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
	{ VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
};
const u32 rtvk_swapchain_format_preferences_count =
	(u32)(sizeof(rtvk_swapchain_format_preferences) / sizeof(rtvk_swapchain_format_preferences[0]));
const VkPresentModeKHR rtvk_swapchain_present_mode_preferences[] = {
	VK_PRESENT_MODE_MAILBOX_KHR,
	VK_PRESENT_MODE_IMMEDIATE_KHR,
	VK_PRESENT_MODE_FIFO_RELAXED_KHR,
};
const u32 rtvk_swapchain_present_mode_preferences_count =
	(u32)(sizeof(rtvk_swapchain_present_mode_preferences) / sizeof(rtvk_swapchain_present_mode_preferences[0]));

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_swapchain rtSwapchainCreate(void) {
	return rtvk_swapchain_to_handle(rtvk_swapchain_create(rtvk_get_current_context()));
}

void rtSwapchainDestroy(rt_swapchain swapchain) {
	rtvk_swapchain_destroy(rtvk_get_current_context(), rtvk_swapchain_from_handle(swapchain));
}

void rtSwapchainResize(rt_swapchain swapchain, u32 width, u32 height) {
	rtvk_swapchain_resize(
		rtvk_get_current_context(),
		rtvk_swapchain_from_handle(swapchain),
		width,
		height
	);
}


rt_swapchain_acquire_result rtSwapchainAcquire(rt_swapchain swapchain) {
	return rtvk_swapchain_acquire(
		rtvk_get_current_context(),
		rtvk_swapchain_from_handle(swapchain)
	);
}

void rtSwapchainPresent(rt_swapchain swapchain, rt_timepoint rendered) {
	struct rtvk_timepoint timepoint = { rtvk_queue_from_handle(rendered.queue), rendered.value };
	rtvk_swapchain_present(
		rtvk_get_current_context(),
		rtvk_swapchain_from_handle(swapchain),
		timepoint
	);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_DEFINE_RESOURCE_PRIVATE(swapchain)

void rtvk_swapchain_init(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain) {
	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(swapchain), RT_RESOURCE_SWAPCHAIN);
#if defined(_WIN32)
	InitializeCriticalSection(&swapchain->frame_lock);
	InitializeConditionVariable(&swapchain->frame_condition);
#else
	pthread_mutex_init(&swapchain->frame_lock, NULL);
	pthread_cond_init(&swapchain->frame_condition, NULL);
#endif
}

static void rtvk_swapchain_lock(struct rtvk_swapchain* swapchain) {
#if defined(_WIN32)
	EnterCriticalSection(&swapchain->frame_lock);
#else
	pthread_mutex_lock(&swapchain->frame_lock);
#endif
}

static void rtvk_swapchain_unlock(struct rtvk_swapchain* swapchain) {
#if defined(_WIN32)
	LeaveCriticalSection(&swapchain->frame_lock);
#else
	pthread_mutex_unlock(&swapchain->frame_lock);
#endif
}

static void rtvk_swapchain_lock_unacquired(struct rtvk_swapchain* swapchain) {
	rtvk_swapchain_lock(swapchain);
	while (swapchain->frame_acquired) {
#if defined(_WIN32)
		SleepConditionVariableCS(&swapchain->frame_condition, &swapchain->frame_lock, INFINITE);
#else
		pthread_cond_wait(&swapchain->frame_condition, &swapchain->frame_lock);
#endif
	}
}

static void rtvk_swapchain_mark_unacquired(struct rtvk_swapchain* swapchain) {
	rtvk_swapchain_lock(swapchain);
	swapchain->frame_acquired = false;
	if (swapchain->image_count) {
		swapchain->current_frame_index = (swapchain->current_frame_index + 1) % swapchain->image_count;
	}
#if defined(_WIN32)
	WakeAllConditionVariable(&swapchain->frame_condition);
#else
	pthread_cond_broadcast(&swapchain->frame_condition);
#endif
	rtvk_swapchain_unlock(swapchain);
}

static void rtvk_swapchain_force_unacquired(struct rtvk_swapchain* swapchain) {
	rtvk_swapchain_lock(swapchain);
	swapchain->frame_acquired = false;
	rtvk_swapchain_unlock(swapchain);
}

static void rtvk_swapchain_finish_sync(struct rtvk_swapchain* swapchain) {
#if defined(_WIN32)
	DeleteCriticalSection(&swapchain->frame_lock);
#else
	pthread_cond_destroy(&swapchain->frame_condition);
	pthread_mutex_destroy(&swapchain->frame_lock);
#endif
}

static const char* rtvk_vk_result_name(VkResult result) {
	switch (result) {
	case VK_SUCCESS: return "VK_SUCCESS";
	case VK_NOT_READY: return "VK_NOT_READY";
	case VK_TIMEOUT: return "VK_TIMEOUT";
	case VK_EVENT_SET: return "VK_EVENT_SET";
	case VK_EVENT_RESET: return "VK_EVENT_RESET";
	case VK_INCOMPLETE: return "VK_INCOMPLETE";
	case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
	case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
	case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
	case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
	case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
	case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
	case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
	case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
	case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
	case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
	case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
	case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
	case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
	case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
	case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
	case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
	default: return "VK_UNKNOWN_RESULT";
	}
}

static void rtvk_swapchain_destroy_present_command(struct rtvk_context* ctx, struct rtvk_swapchain_frame* frame) {
	if (frame->present_command_pool) {
		vkDestroyCommandPool(ctx->vk_device, frame->present_command_pool, VK_ALLOCATOR);
		frame->present_command_pool = VK_NULL_HANDLE;
		frame->present_command_buffer = VK_NULL_HANDLE;
	}
	frame->present_command_family_index = (u32)-1;
}

static void rtvk_swapchain_wait_frame(struct rtvk_context* ctx, struct rtvk_swapchain_frame* frame) {
	if (frame->acquire_wait.queue) {
		rtvk_timepoint_wait(ctx, frame->acquire_wait);
		frame->acquire_wait.queue = NULL;
		frame->acquire_wait.value = 0;
	}

	if (frame->present_done.queue) {
		rtvk_timepoint_wait(ctx, frame->present_done);
		frame->present_done.queue = NULL;
		frame->present_done.value = 0;
	}
}

static void rtvk_swapchain_destroy_frame_sync(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain) {
	if (!swapchain->frames) {
		return;
	}

	for (u32 i = 0; i < swapchain->image_count; i++) {
		rtvk_swapchain_wait_frame(ctx, &swapchain->frames[i]);
		rtvk_swapchain_destroy_present_command(ctx, &swapchain->frames[i]);
		if (swapchain->frames[i].image_available) {
			vkDestroySemaphore(ctx->vk_device, swapchain->frames[i].image_available, VK_ALLOCATOR);
		}
		if (swapchain->frames[i].present_ready) {
			vkDestroySemaphore(ctx->vk_device, swapchain->frames[i].present_ready, VK_ALLOCATOR);
		}
	}

	free(swapchain->frames);
	swapchain->frames = NULL;
}

static void rtvk_swapchain_destroy_framebuffers(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain) {
	if (swapchain->framebuffers) {
		for (u32 i = 0; i < swapchain->image_count; i++) {
			rtvk_framebuffer_destroy(ctx, swapchain->framebuffers[i]);
		}
		free(swapchain->framebuffers);
		swapchain->framebuffers = NULL;
	}

}

static void rtvk_swapchain_destroy_color_views(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain) {
	(void)ctx;
	if (!swapchain->color_views) {
		return;
	}
	for (u32 i = 0; i < swapchain->image_count; i++) {
		if (swapchain->color_views[i]) {
			rtvk_texture_view_destroy(ctx, swapchain->color_views[i]);
		}
	}
	free(swapchain->color_views);
	swapchain->color_views = NULL;
}

void rtvk_swapchain_finish(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain) {
	rtvk_swapchain_force_unacquired(swapchain);
	if (ctx && ctx->vk_device) {
		vkDeviceWaitIdle(ctx->vk_device);
	}
	rtvk_swapchain_destroy_frame_sync(ctx, swapchain);
	rtvk_swapchain_destroy_framebuffers(ctx, swapchain);
	rtvk_swapchain_destroy_color_views(ctx, swapchain);

	if (swapchain->vk_swapchain) {
		vkDestroySwapchainKHR(ctx->vk_device, swapchain->vk_swapchain, VK_ALLOCATOR);
		swapchain->vk_swapchain = VK_NULL_HANDLE;
	}
	if (swapchain->vk_surface) {
		vkDestroySurfaceKHR(ctx->vk_instance, swapchain->vk_surface, VK_ALLOCATOR);
		swapchain->vk_surface = VK_NULL_HANDLE;
	}
	if (swapchain->present_queue) {
		rtvk_release_resource(swapchain->present_queue);
		swapchain->present_queue = NULL;
	}
	swapchain->image_count = 0;
	rtvk_swapchain_unlock(swapchain);
	rtvk_swapchain_finish_sync(swapchain);
	rtvk_finish_resource_base(ctx, RTVK_RESOURCE_BASE(swapchain));
}

static bool rtvk_swapchain_create_framebuffers(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain) {
	bool ok = false;
	VkImage* images = NULL;
	u32 image_count = 0;

	VkResult result = vkGetSwapchainImagesKHR(ctx->vk_device, swapchain->vk_swapchain, &image_count, NULL);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "vkGetSwapchainImagesKHR failed while counting swapchain images");
		return false;
	}

	images = calloc(image_count, sizeof(*images));
	RTVK_CHECK_ALLOC(images, (usize)image_count * sizeof(*images), "swapchain image list");
	if (rtError() != RT_SUCCESS) { goto cleanup; }

	swapchain->frames = calloc(image_count, sizeof(*swapchain->frames));
	RTVK_CHECK_ALLOC(swapchain->frames, (usize)image_count * sizeof(*swapchain->frames), "swapchain frame sync list");
	if (rtError() != RT_SUCCESS) { goto cleanup; }

	swapchain->framebuffers = calloc(image_count, sizeof(*swapchain->framebuffers));
	RTVK_CHECK_ALLOC(swapchain->framebuffers, (usize)image_count * sizeof(*swapchain->framebuffers), "swapchain framebuffer list");
	if (rtError() != RT_SUCCESS) { goto cleanup; }

	result = vkGetSwapchainImagesKHR(ctx->vk_device, swapchain->vk_swapchain, &image_count, images);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "vkGetSwapchainImagesKHR failed while fetching swapchain images");
		goto cleanup;
	}

	swapchain->image_count = image_count;
	swapchain->color_views = calloc(image_count, sizeof(*swapchain->color_views));
	RTVK_CHECK_ALLOC(swapchain->color_views, (usize)image_count * sizeof(*swapchain->color_views), "swapchain color view list");
	if (rtError() != RT_SUCCESS) { goto cleanup; }
	for (u32 i = 0; i < image_count; i++) {
		struct rtvk_swapchain_frame* frame = &swapchain->frames[i];
		frame->vk_image = images[i];
		frame->vk_format = swapchain->vk_format;
		frame->width = swapchain->extent.width;
		frame->height = swapchain->extent.height;

		swapchain->framebuffers[i] = rtvk_framebuffer_create(ctx);
		if (!swapchain->framebuffers[i]) {
			goto cleanup;
		}

		swapchain->color_views[i] = rtvk_texture_view_create_for_swapchain_image(ctx, images[i], swapchain->vk_format, swapchain->extent.width, swapchain->extent.height);
		rtvk_framebuffer_set_color_view(ctx, swapchain->framebuffers[i], 0, swapchain->color_views[i]);
		if (rtError() != RT_SUCCESS) {
			goto cleanup;
		}
	}

	ok = true;

cleanup:
	free(images);
	if (!ok) {
		rtvk_swapchain_destroy_frame_sync(ctx, swapchain);
		rtvk_swapchain_destroy_framebuffers(ctx, swapchain);
		rtvk_swapchain_destroy_color_views(ctx, swapchain);
	}
	return ok;
}

static bool rtvk_swapchain_create_frame_sync(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain) {
	VkSemaphoreCreateInfo semaphore_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	if (!swapchain->frames) {
		swapchain->frames = calloc(swapchain->image_count, sizeof(*swapchain->frames));
		if (!swapchain->frames) {
			rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %u swapchain sync frames", swapchain->image_count);
			return false;
		}
	}

	semaphore_info.pNext = NULL;
	semaphore_info.flags = 0;

	for (u32 i = 0; i < swapchain->image_count; i++) {
		swapchain->frames[i].present_command_family_index = (u32)-1;

		VkResult result = vkCreateSemaphore(ctx->vk_device, &semaphore_info, VK_ALLOCATOR, &swapchain->frames[i].image_available);
		if (result != VK_SUCCESS) {
			rtvk_swapchain_destroy_frame_sync(ctx, swapchain);
			rtvk_throwf(rtvk_error_from_vk(result), NULL);
			return false;
		}

		result = vkCreateSemaphore(ctx->vk_device, &semaphore_info, VK_ALLOCATOR, &swapchain->frames[i].present_ready);
		if (result != VK_SUCCESS) {
			rtvk_swapchain_destroy_frame_sync(ctx, swapchain);
			rtvk_throwf(rtvk_error_from_vk(result), NULL);
			return false;
		}
	}

	return true;
}

static VkSurfaceFormatKHR rtvk_choose_swapchain_format(VkSurfaceFormatKHR* formats, u32 format_count) {
	for (u32 p = 0; p < rtvk_swapchain_format_preferences_count; p++) {
		for (u32 i = 0; i < format_count; i++) {
			if (formats[i].format == rtvk_swapchain_format_preferences[p].format && formats[i].colorSpace == rtvk_swapchain_format_preferences[p].colorSpace) {
				return formats[i];
			}
		}
	}

	return format_count ? formats[0] : (VkSurfaceFormatKHR){ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
}

static VkPresentModeKHR rtvk_choose_swapchain_present_mode(VkPresentModeKHR* present_modes, u32 present_mode_count) {
	for (u32 p = 0; p < rtvk_swapchain_present_mode_preferences_count; p++) {
		for (u32 i = 0; i < present_mode_count; i++) {
			if (present_modes[i] == rtvk_swapchain_present_mode_preferences[p]) {
				return present_modes[i];
			}
		}
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D rtvk_swapchain_choose_extent(VkSurfaceCapabilitiesKHR capabilities, u32 width, u32 height) {
	VkExtent2D extent;
	if (capabilities.currentExtent.width != 0xffffffffu) {
		return capabilities.currentExtent;
	}

	extent.width = width;
	extent.height = height;
	if (extent.width < capabilities.minImageExtent.width) {
		extent.width = capabilities.minImageExtent.width;
	}
	if (extent.width > capabilities.maxImageExtent.width) {
		extent.width = capabilities.maxImageExtent.width;
	}
	if (extent.height < capabilities.minImageExtent.height) {
		extent.height = capabilities.minImageExtent.height;
	}
	if (extent.height > capabilities.maxImageExtent.height) {
		extent.height = capabilities.maxImageExtent.height;
	}
	return extent;
}

bool rtvk_swapchain_create_for_surface(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, VkSurfaceKHR surface, u32 width, u32 height) {
	swapchain->vk_surface = surface;
	swapchain->present_queue = rtvk_queue_query_present(ctx, surface);
	if (!swapchain->present_queue) {
		if (rtError() == RT_SUCCESS) {
			rtvk_throwf(RT_UNSUPPORTED_PLATFORM, NULL);
		}
		return false;
	}
	rtvk_retain_resource(swapchain->present_queue);

	VkSurfaceCapabilitiesKHR capabilities;
	VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx->vk_physical_device, surface, &capabilities);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}

	u32 format_count = 0;
	result = vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->vk_physical_device, surface, &format_count, NULL);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}

	VkSurfaceFormatKHR* formats = calloc(format_count, sizeof(*formats));
	if (!formats) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %u Vulkan surface formats", format_count);
		return false;
	}
	result = vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->vk_physical_device, surface, &format_count, formats);
	if (result != VK_SUCCESS) {
		free(formats);
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}

	u32 present_mode_count = 0;
	result = vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->vk_physical_device, surface, &present_mode_count, NULL);
	if (result != VK_SUCCESS) {
		free(formats);
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}
	if (present_mode_count == 0) {
		free(formats);
		rtvk_throwf(RT_INCOMPATIBLE_DRIVER, NULL);
		return false;
	}

	VkPresentModeKHR* present_modes = calloc(present_mode_count, sizeof(*present_modes));
	if (!present_modes) {
		free(formats);
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %u Vulkan present modes", present_mode_count);
		return false;
	}
	result = vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->vk_physical_device, surface, &present_mode_count, present_modes);
	if (result != VK_SUCCESS) {
		free(formats);
		free(present_modes);
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}

	VkSurfaceFormatKHR format = rtvk_choose_swapchain_format(formats, format_count);
	VkPresentModeKHR present_mode = rtvk_choose_swapchain_present_mode(present_modes, present_mode_count);
	free(formats);
	free(present_modes);

	u32 image_count = RTVK_MAX_FRAMES_IN_FLIGHT;
	if (image_count < capabilities.minImageCount) {
		image_count = capabilities.minImageCount;
	}
	if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
		image_count = capabilities.maxImageCount;
	}

	VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if ((capabilities.supportedUsageFlags & image_usage) != image_usage) {
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "swapchain images do not support transfer source usage");
		return false;
	}

	VkSwapchainCreateInfoKHR swapchain_info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	swapchain_info.pNext = NULL;
	swapchain_info.flags = 0;
	swapchain_info.surface = surface;
	swapchain_info.minImageCount = image_count;
	swapchain_info.imageFormat = format.format;
	swapchain_info.imageColorSpace = format.colorSpace;
	swapchain_info.imageExtent = rtvk_swapchain_choose_extent(capabilities, width, height);
	swapchain_info.imageArrayLayers = 1;
	swapchain_info.imageUsage = image_usage;

	struct rtvk_queue* graphics_queue = rtvk_queue_query(ctx, RT_QUEUE_GRAPHICS);
	u32 queue_family_indices[2];
	if (graphics_queue && graphics_queue->family_index != swapchain->present_queue->family_index) {
		queue_family_indices[0] = graphics_queue->family_index;
		queue_family_indices[1] = swapchain->present_queue->family_index;
		swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchain_info.queueFamilyIndexCount = 2;
		swapchain_info.pQueueFamilyIndices = queue_family_indices;
	} else {
		swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchain_info.queueFamilyIndexCount = 0;
		swapchain_info.pQueueFamilyIndices = NULL;
	}
	swapchain_info.preTransform = capabilities.currentTransform;
	swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_info.presentMode = present_mode;
	swapchain_info.clipped = VK_TRUE;
	swapchain_info.oldSwapchain = VK_NULL_HANDLE;

	result = vkCreateSwapchainKHR(ctx->vk_device, &swapchain_info, VK_ALLOCATOR, &swapchain->vk_swapchain);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}

	swapchain->vk_format = format.format;
	swapchain->extent = swapchain_info.imageExtent;

	if (!rtvk_swapchain_create_framebuffers(ctx, swapchain)) {
		return false;
	}
	if (!rtvk_swapchain_create_frame_sync(ctx, swapchain)) {
		return false;
	}

	return true;
}


static bool rtvk_swapchain_prepare_present_command(
	struct rtvk_context* ctx,
	struct rtvk_swapchain_frame* frame,
	u32 family_index) {
	if (frame->present_command_pool && frame->present_command_family_index != family_index) {
		rtvk_swapchain_destroy_present_command(ctx, frame);
	}
	if (frame->present_command_pool) {
		return true;
	}

	VkCommandPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	pool_info.pNext = NULL;
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = family_index;

	VkResult result = vkCreateCommandPool(ctx->vk_device, &pool_info, VK_ALLOCATOR, &frame->present_command_pool);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}

	VkCommandBufferAllocateInfo allocate_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	allocate_info.pNext = NULL;
	allocate_info.commandPool = frame->present_command_pool;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = 1;

	result = vkAllocateCommandBuffers(ctx->vk_device, &allocate_info, &frame->present_command_buffer);
	if (result != VK_SUCCESS) {
		rtvk_swapchain_destroy_present_command(ctx, frame);
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}

	frame->present_command_family_index = family_index;
	return true;
}

static bool rtvk_swapchain_submit_present_transition(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, struct rtvk_swapchain_frame* frame, struct rtvk_timepoint rendered) {
	struct rtvk_swapchain_frame* current = &swapchain->frames[swapchain->current_image_index];
	if (!current || !rendered.queue) {
		return false;
	}
	if (!rtvk_swapchain_prepare_present_command(ctx, frame, rendered.queue->family_index)) {
		return false;
	}

	VkResult result = vkResetCommandPool(ctx->vk_device, frame->present_command_pool, 0);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}

	VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	result = vkBeginCommandBuffer(frame->present_command_buffer, &begin_info);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}

	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.pNext = NULL;
	barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.dstAccessMask = 0;
	barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = current->vk_image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	vkCmdPipelineBarrier(frame->present_command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
						 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

	result = vkEndCommandBuffer(frame->present_command_buffer);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}

	if (rendered.value > rendered.queue->submitted_value) {
		rtvk_queue_flush(ctx, rendered.queue);
	}

	u64 signal_value = rendered.queue->timeline_value + 1;
	u64 signal_values[2] = { 0, signal_value };
	VkSemaphore signal_semaphores[2] = { frame->present_ready, rendered.queue->vk_timeline };

	VkTimelineSemaphoreSubmitInfo timeline_info = { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
	timeline_info.pNext = NULL;
	timeline_info.waitSemaphoreValueCount = 1;
	timeline_info.pWaitSemaphoreValues = &rendered.value;
	timeline_info.signalSemaphoreValueCount = 2;
	timeline_info.pSignalSemaphoreValues = signal_values;

	VkPipelineStageFlags wait_stage = rtvk_queue_wait_stage(rendered.queue);
	VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.pNext = &timeline_info;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &rendered.queue->vk_timeline;
	submit_info.pWaitDstStageMask = &wait_stage;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &frame->present_command_buffer;
	submit_info.signalSemaphoreCount = 2;
	submit_info.pSignalSemaphores = signal_semaphores;

	result = vkQueueSubmit(rendered.queue->vk_queue, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}

	struct rtvk_framebuffer* framebuffer = swapchain->framebuffers ? swapchain->framebuffers[swapchain->current_image_index] : NULL;
	struct rtvk_texture_view* color_view = framebuffer ? framebuffer->color_views[0] : NULL;
	if (color_view) {
		color_view->vk_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		if (color_view->texture) {
			color_view->texture->vk_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		}
	}
	rendered.queue->timeline_value = signal_value;
	rendered.queue->submitted_value = signal_value;
	frame->present_done = (struct rtvk_timepoint){ rendered.queue, signal_value };
	return true;
}

u32 rtSwapchainFramebufferCount(rt_swapchain swapchain) {
	struct rtvk_swapchain* state = rtvk_swapchain_from_handle(swapchain);
	return state ? state->image_count : 0;
}

rt_framebuffer rtSwapchainFramebufferAt(rt_swapchain swapchain, u32 index) {
	struct rtvk_swapchain* state = rtvk_swapchain_from_handle(swapchain);
	if (!state || index >= state->image_count || !state->framebuffers) {
		return RT_NULL_HANDLE;
	}
	return rtvk_framebuffer_to_handle(state->framebuffers[index]);
}

bool rtvk_swapchain_resize(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, u32 width, u32 height) {
	bool ok = false;
	if (!swapchain || !swapchain->vk_surface || !swapchain->vk_swapchain) {
		rtvk_throwf(RT_IMPROPER_USAGE, "swapchain resize requires a valid swapchain");
		return false;
	}
	if (width == 0 || height == 0) {
		return false;
	}

	rtvk_swapchain_lock_unacquired(swapchain);
	if (width == swapchain->extent.width && height == swapchain->extent.height) {
		rtvk_swapchain_unlock(swapchain);
		return true;
	}

	vkDeviceWaitIdle(ctx->vk_device);
	VkSurfaceKHR surface = swapchain->vk_surface;
	struct rtvk_queue* present_queue = swapchain->present_queue;
	if (present_queue) {
		rtvk_retain_resource(present_queue);
	}

	rtvk_swapchain_destroy_frame_sync(ctx, swapchain);
	rtvk_swapchain_destroy_framebuffers(ctx, swapchain);
	if (swapchain->vk_swapchain) {
		vkDestroySwapchainKHR(ctx->vk_device, swapchain->vk_swapchain, VK_ALLOCATOR);
		swapchain->vk_swapchain = VK_NULL_HANDLE;
	}
	if (swapchain->present_queue) {
		rtvk_release_resource(swapchain->present_queue);
		swapchain->present_queue = NULL;
	}

	ok = rtvk_swapchain_create_for_surface(ctx, swapchain, surface, width, height);
	if (present_queue) {
		rtvk_release_resource(present_queue);
	}
	rtvk_swapchain_unlock(swapchain);
	return ok;
}

rt_swapchain_acquire_result rtvk_swapchain_acquire(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain) {
	rt_swapchain_acquire_result acquire = { RT_NULL_HANDLE, { RT_NULL_HANDLE, 0 } };
	if (!swapchain) {
		rtvk_throwf(RT_IMPROPER_USAGE, "swapchain acquire requires a valid swapchain");
		return acquire;
	}
	rtvk_swapchain_lock_unacquired(swapchain);
	if (!swapchain->frames || swapchain->image_count == 0) {
		rtvk_swapchain_unlock(swapchain);
		rtvk_throwf(RT_IMPROPER_USAGE, "swapchain has no sync frames");
		return acquire;
	}
	struct rtvk_swapchain_frame* acquire_frame = &swapchain->frames[swapchain->current_frame_index];
	rtvk_swapchain_wait_frame(ctx, acquire_frame);

	VkResult result = vkAcquireNextImageKHR(ctx->vk_device, swapchain->vk_swapchain, UINT64_MAX, acquire_frame->image_available, VK_NULL_HANDLE, &swapchain->current_image_index);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		rtvk_throwf(RT_PLATFORM_FAILURE, "swapchain acquire failed: swapchain is out of date");
		rtvk_swapchain_unlock(swapchain);
		return acquire;
	}
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		rtvk_throwf(rtvk_error_from_vk(result), "swapchain acquire failed: vkAcquireNextImageKHR returned %s", rtvk_vk_result_name(result));
		rtvk_swapchain_unlock(swapchain);
		return acquire;
	}

	acquire.framebuffer = rtvk_framebuffer_to_handle(swapchain->framebuffers[swapchain->current_image_index]);
	if (!acquire.framebuffer) {
		rtvk_throwf(RT_PLATFORM_FAILURE, "swapchain acquire failed: framebuffer is null");
		rtvk_swapchain_unlock(swapchain);
		return acquire;
	}
	acquire_frame->acquire_wait = rtvk_queue_wait_binary(ctx, swapchain->present_queue, acquire_frame->image_available);
	if (!acquire_frame->acquire_wait.queue || acquire_frame->acquire_wait.value == 0) {
		rtvk_throwf(RT_PLATFORM_FAILURE, "swapchain acquire failed: acquire wait is invalid");
		rtvk_swapchain_unlock(swapchain);
		return acquire;
	}
	acquire.timepoint = rtvk_timepoint_to_public(acquire_frame->acquire_wait);
	swapchain->frame_acquired = true;
	if (!acquire.timepoint.queue || acquire.timepoint.value == 0) {
		rtvk_throwf(RT_PLATFORM_FAILURE, "swapchain acquire failed: null timepoint");
		swapchain->frame_acquired = false;
		rtvk_swapchain_unlock(swapchain);
		return (rt_swapchain_acquire_result){ RT_NULL_HANDLE, { RT_NULL_HANDLE, 0 } };
	}
	rtvk_swapchain_unlock(swapchain);
	return acquire;
}

void rtvk_swapchain_present(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, struct rtvk_timepoint rendered) {
	if (!swapchain || !swapchain->frames || swapchain->image_count == 0 || !swapchain->frame_acquired) {
		rtvk_throwf(RT_IMPROPER_USAGE, "swapchain present requires an acquired frame");
		return;
	}
	struct rtvk_swapchain_frame* frame = &swapchain->frames[swapchain->current_frame_index];
	if (!rendered.queue || rendered.value == 0) {
		rtvk_throwf(RT_IMPROPER_USAGE, "swapchain present requires a render timepoint");
		rtvk_swapchain_mark_unacquired(swapchain);
		return;
	}
	if (!rtvk_swapchain_submit_present_transition(ctx, swapchain, frame, rendered)) {
		rtvk_swapchain_mark_unacquired(swapchain);
		return;
	}

	VkPresentInfoKHR present_info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	present_info.pNext = NULL;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &frame->present_ready;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &swapchain->vk_swapchain;
	present_info.pImageIndices = &swapchain->current_image_index;
	present_info.pResults = NULL;

	VkResult result = vkQueuePresentKHR(swapchain->present_queue->vk_queue, &present_info);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		rtvk_swapchain_mark_unacquired(swapchain);
		return;
	}
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		rtvk_swapchain_mark_unacquired(swapchain);
		return;
	}

	rtvk_swapchain_mark_unacquired(swapchain);
}
