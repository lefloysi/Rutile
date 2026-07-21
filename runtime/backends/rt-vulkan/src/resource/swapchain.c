#include "resource/swapchain.h"
#include "context.h"
#include "error.h"
#include "resource/queue.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

const VkSurfaceFormatKHR rtvk_swapchain_format_preferences[] = {
	{ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
	{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
	{ VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
	{ VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
};
const u32 rtvk_swapchain_format_preferences_count = (u32)(sizeof(rtvk_swapchain_format_preferences) / sizeof(rtvk_swapchain_format_preferences[0]));
const VkPresentModeKHR rtvk_swapchain_present_mode_preferences[] = {
	VK_PRESENT_MODE_MAILBOX_KHR,
	VK_PRESENT_MODE_IMMEDIATE_KHR,
	VK_PRESENT_MODE_FIFO_RELAXED_KHR,
};
const u32 rtvk_swapchain_present_mode_preferences_count = (u32)(sizeof(rtvk_swapchain_present_mode_preferences) / sizeof(rtvk_swapchain_present_mode_preferences[0]));

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


static void rtvk_swapchain_destroy_present_command(struct rtvk_context* ctx, struct rtvk_swapchain_frame* frame) {
	vkDestroyCommandPool(ctx->vk_device, frame->present_command_pool, VK_ALLOCATOR);
	frame->present_command_pool = VK_NULL_HANDLE;
	frame->present_command_buffer = VK_NULL_HANDLE;
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

// Wait until all in-flight work touching any frame has settled. Called from
// the swapchain destroy path before we tear down frame semaphores/pools.
static void rtvk_swapchain_wait_all_frames(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain) {
	if (!swapchain->frames) {
		return;
	}
	for (u32 i = 0; i < swapchain->image_count; i++) {
		if (swapchain->frames[i]) {
			rtvk_swapchain_wait_frame(ctx, swapchain->frames[i]);
		}
	}
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

void rtvk_swapchain_frame_init(struct rtvk_context* ctx, struct rtvk_swapchain_frame* frame) {
	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(frame), RT_RESOURCE_SWAPCHAIN_FRAME);
	frame->base.vk_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	frame->base.vk_format = VK_FORMAT_UNDEFINED;
	frame->base.type = RT_TEXTURE_2D;
	frame->base.depth = 1;
	frame->base.mip_levels = 1;
}

// VkImage itself belongs to VkSwapchainKHR and is destroyed by that API.
void rtvk_swapchain_frame_finish(struct rtvk_swapchain_frame* frame) {
	struct rtvk_context* ctx = frame->base.base.ctx;
	vkDestroySemaphore(ctx->vk_device, frame->image_available, VK_ALLOCATOR);
	vkDestroySemaphore(ctx->vk_device, frame->present_ready, VK_ALLOCATOR);
	vkDestroyCommandPool(ctx->vk_device, frame->present_command_pool, VK_ALLOCATOR);
	frame->image_available = VK_NULL_HANDLE;
	frame->present_ready = VK_NULL_HANDLE;
	frame->present_command_pool = VK_NULL_HANDLE;
	frame->present_command_buffer = VK_NULL_HANDLE;
	frame->base.vk_image = VK_NULL_HANDLE;
	rtvk_finish_resource_base(RTVK_RESOURCE_BASE(frame));
}

static void rtvk_swapchain_destroy_frames(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain) {
	(void)ctx;
	if (!swapchain->frames) {
		return;
	}
	for (u32 i = 0; i < swapchain->image_count; i++) {
		if (swapchain->frames[i]) {
			rtvk_resource_retire(RTVK_RESOURCE_BASE(swapchain->frames[i]));
			swapchain->frames[i] = NULL;
		}
	}
	free(swapchain->frames);
	swapchain->frames = NULL;
}

void rtvk_swapchain_finish(struct rtvk_swapchain* swapchain) {
	struct rtvk_context* ctx = swapchain->base.ctx;
	rtvk_swapchain_force_unacquired(swapchain);
	vkDeviceWaitIdle(ctx->vk_device);
	rtvk_swapchain_wait_all_frames(ctx, swapchain);
	rtvk_swapchain_destroy_framebuffers(ctx, swapchain);
	rtvk_swapchain_destroy_color_views(ctx, swapchain);
	rtvk_swapchain_destroy_frames(ctx, swapchain);

	vkDestroySwapchainKHR(ctx->vk_device, swapchain->vk_swapchain, VK_ALLOCATOR);
	vkDestroySurfaceKHR(ctx->vk_instance, swapchain->vk_surface, VK_ALLOCATOR);
	rtvk_release_resource(swapchain->present_queue);
	swapchain->vk_swapchain = VK_NULL_HANDLE;
	swapchain->vk_surface = VK_NULL_HANDLE;
	swapchain->present_queue = NULL;
	swapchain->image_count = 0;
	rtvk_swapchain_unlock(swapchain);
	rtvk_swapchain_finish_sync(swapchain);
	rtvk_finish_resource_base(RTVK_RESOURCE_BASE(swapchain));
}

void rtvk_swapchain_create_framebuffers(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain) {
	VkImage* images = NULL;
	u32 image_count = 0;

	VkResult result = vkGetSwapchainImagesKHR(ctx->vk_device, swapchain->vk_swapchain, &image_count, NULL);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "vkGetSwapchainImagesKHR failed while counting swapchain images");
		return;
	}

	images = calloc(image_count, sizeof(*images));
	RTVK_CHECK_ALLOC(images, (usize)image_count * sizeof(*images), "swapchain image list");
	if (rtvk_error() != RT_SUCCESS) {
		goto cleanup;
	}

	swapchain->frames = calloc(image_count, sizeof(*swapchain->frames));
	RTVK_CHECK_ALLOC(swapchain->frames, (usize)image_count * sizeof(*swapchain->frames), "swapchain frame list");
	if (rtvk_error() != RT_SUCCESS) {
		goto cleanup;
	}

	swapchain->framebuffers = calloc(image_count, sizeof(*swapchain->framebuffers));
	RTVK_CHECK_ALLOC(swapchain->framebuffers, (usize)image_count * sizeof(*swapchain->framebuffers), "swapchain framebuffer list");
	if (rtvk_error() != RT_SUCCESS) {
		goto cleanup;
	}

	result = vkGetSwapchainImagesKHR(ctx->vk_device, swapchain->vk_swapchain, &image_count, images);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "vkGetSwapchainImagesKHR failed while fetching swapchain images");
		goto cleanup;
	}

	swapchain->image_count = image_count;
	swapchain->color_views = calloc(image_count, sizeof(*swapchain->color_views));
	RTVK_CHECK_ALLOC(swapchain->color_views, (usize)image_count * sizeof(*swapchain->color_views), "swapchain color view list");
	if (rtvk_error() != RT_SUCCESS) {
		goto cleanup;
	}
	for (u32 i = 0; i < image_count; i++) {
		struct rtvk_swapchain_frame* frame = calloc(1, sizeof(*frame));
		if (!frame) {
			rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate swapchain frame");
			goto cleanup;
		}
		rtvk_swapchain_frame_init(ctx, frame);
		frame->base.vk_image = images[i];
		frame->base.vk_format = swapchain->vk_format;
		frame->base.width = swapchain->extent.width;
		frame->base.height = swapchain->extent.height;
		swapchain->frames[i] = frame;

		swapchain->framebuffers[i] = rtvk_framebuffer_create(ctx);
		if (!swapchain->framebuffers[i]) {
			goto cleanup;
		}

		swapchain->color_views[i] = rtvk_texture_view_create(ctx);
		if (!swapchain->color_views[i]) {
			goto cleanup;
		}
		rtvk_texture_view_bind_image(ctx, swapchain->color_views[i], &frame->base);
		rtvk_framebuffer_set_color_view(ctx, swapchain->framebuffers[i], 0, swapchain->color_views[i]);
		if (rtvk_error() != RT_SUCCESS) {
			goto cleanup;
		}
	}

cleanup:
	free(images);
	if (rtvk_error() != RT_SUCCESS) {
		rtvk_swapchain_destroy_framebuffers(ctx, swapchain);
		rtvk_swapchain_destroy_color_views(ctx, swapchain);
		rtvk_swapchain_destroy_frames(ctx, swapchain);
	}
}

void rtvk_swapchain_create_frame_sync(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain) {
	// Frames must already be allocated by rtvk_swapchain_create_framebuffers.
	assert(swapchain->frames);

	VkSemaphoreCreateInfo semaphore_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	semaphore_info.pNext = NULL;
	semaphore_info.flags = 0;

	for (u32 i = 0; i < swapchain->image_count; i++) {
		struct rtvk_swapchain_frame* frame = swapchain->frames[i];
		frame->present_command_family_index = (u32)-1;

		VkResult result = vkCreateSemaphore(ctx->vk_device, &semaphore_info, VK_ALLOCATOR, &frame->image_available);
		if (result != VK_SUCCESS) {
			rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
			return;
		}

		result = vkCreateSemaphore(ctx->vk_device, &semaphore_info, VK_ALLOCATOR, &frame->present_ready);
		if (result != VK_SUCCESS) {
			rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
			return;
		}
	}
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

void rtvk_swapchain_init_from_surface(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, VkSurfaceKHR surface, u32 width, u32 height) {
	swapchain->vk_surface = surface;
	swapchain->present_queue = rtvk_queue_query_present(ctx, surface);
	if (!swapchain->present_queue) {
		if (rtvk_error() == RT_SUCCESS) {
			rtvk_throwf(RT_UNSUPPORTED_PLATFORM, NULL);
		}
		return;
	}
	rtvk_retain_resource(swapchain->present_queue);

	VkSurfaceCapabilitiesKHR capabilities;
	VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx->vk_physical_device, surface, &capabilities);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}

	u32 format_count = 0;
	result = vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->vk_physical_device, surface, &format_count, NULL);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}

	VkSurfaceFormatKHR* formats = calloc(format_count, sizeof(*formats));
	if (!formats) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %u Vulkan surface formats", format_count);
		return;
	}
	result = vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->vk_physical_device, surface, &format_count, formats);
	if (result != VK_SUCCESS) {
		free(formats);
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}

	u32 present_mode_count = 0;
	result = vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->vk_physical_device, surface, &present_mode_count, NULL);
	if (result != VK_SUCCESS) {
		free(formats);
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}
	if (present_mode_count == 0) {
		free(formats);
		rtvk_throwf(RT_INCOMPATIBLE_DRIVER, NULL);
		return;
	}

	VkPresentModeKHR* present_modes = calloc(present_mode_count, sizeof(*present_modes));
	if (!present_modes) {
		free(formats);
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %u Vulkan present modes", present_mode_count);
		return;
	}
	result = vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->vk_physical_device, surface, &present_mode_count, present_modes);
	if (result != VK_SUCCESS) {
		free(formats);
		free(present_modes);
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
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
		return;
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
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}

	swapchain->vk_format = format.format;
	swapchain->extent = swapchain_info.imageExtent;

	rtvk_swapchain_create_framebuffers(ctx, swapchain);
	if (rtvk_error() != RT_SUCCESS) {
		return;
	}
	rtvk_swapchain_create_frame_sync(ctx, swapchain);
}

void rtvk_swapchain_prepare_present_command(
	struct rtvk_context* ctx,
	struct rtvk_swapchain_frame* frame,
	u32 family_index
) {
	if (frame->present_command_pool && frame->present_command_family_index != family_index) {
		rtvk_swapchain_destroy_present_command(ctx, frame);
	}
	if (frame->present_command_pool) {
		return;
	}

	VkCommandPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	pool_info.pNext = NULL;
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = family_index;

	VkResult result = vkCreateCommandPool(ctx->vk_device, &pool_info, VK_ALLOCATOR, &frame->present_command_pool);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}

	VkCommandBufferAllocateInfo allocate_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	allocate_info.pNext = NULL;
	allocate_info.commandPool = frame->present_command_pool;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = 1;

	result = vkAllocateCommandBuffers(ctx->vk_device, &allocate_info, &frame->present_command_buffer);
	if (result != VK_SUCCESS) {
		rtvk_swapchain_destroy_present_command(ctx, frame);
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}

	frame->present_command_family_index = family_index;
}

void rtvk_swapchain_submit_present_transition(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, struct rtvk_swapchain_frame* frame, struct rtvk_timepoint rendered) {
	struct rtvk_swapchain_frame* current = swapchain->frames[swapchain->current_image_index];
	if (!current || !rendered.queue) {
		return;
	}
	rtvk_swapchain_prepare_present_command(ctx, frame, rendered.queue->family_index);
	if (rtvk_error() != RT_SUCCESS) {
		return;
	}

	VkResult result = vkResetCommandPool(ctx->vk_device, frame->present_command_pool, 0);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}

	VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	result = vkBeginCommandBuffer(frame->present_command_buffer, &begin_info);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}

	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.pNext = NULL;
	barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.dstAccessMask = 0;
	barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = current->base.vk_image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	vkCmdPipelineBarrier(frame->present_command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

	result = vkEndCommandBuffer(frame->present_command_buffer);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
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
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}

	struct rtvk_framebuffer* framebuffer = swapchain->framebuffers ? swapchain->framebuffers[swapchain->current_image_index] : NULL;
	struct rtvk_texture_view* color_view = framebuffer ? framebuffer->color_views[0] : NULL;
	if (color_view && color_view->image) {
		// Post-present, the swapchain image is in PRESENT_SRC_KHR. Sync the
		// backing frame's layout so the next transition uses the correct
		// oldLayout.
		color_view->image->vk_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
	rendered.queue->timeline_value = signal_value;
	rendered.queue->submitted_value = signal_value;
	frame->present_done = (struct rtvk_timepoint){ rendered.queue, signal_value };
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
	rtvk_retain_resource(present_queue);

	rtvk_swapchain_wait_all_frames(ctx, swapchain);
	rtvk_swapchain_destroy_framebuffers(ctx, swapchain);
	rtvk_swapchain_destroy_color_views(ctx, swapchain);
	rtvk_swapchain_destroy_frames(ctx, swapchain);
	vkDestroySwapchainKHR(ctx->vk_device, swapchain->vk_swapchain, VK_ALLOCATOR);
	rtvk_release_resource(swapchain->present_queue);
	swapchain->vk_swapchain = VK_NULL_HANDLE;
	swapchain->present_queue = NULL;

	rtvk_swapchain_init_from_surface(ctx, swapchain, surface, width, height);
	ok = rtvk_error() == RT_SUCCESS;
	rtvk_release_resource(present_queue);
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
	struct rtvk_swapchain_frame* acquire_frame = swapchain->frames[swapchain->current_frame_index];
	rtvk_swapchain_wait_frame(ctx, acquire_frame);

	/* Comment ** finite timeout: the spec forbids UINT64_MAX when forward progress
	 * cannot be guaranteed (VUID-vkAcquireNextImageKHR-surface-07783). One second is
	 * long enough that a healthy frame pipeline never trips it, short enough that a
	 * deadlock surfaces as VK_TIMEOUT instead of hanging the app. */
	VkResult result = vkAcquireNextImageKHR(ctx->vk_device, swapchain->vk_swapchain, 1000000000ull, acquire_frame->image_available, VK_NULL_HANDLE, &swapchain->current_image_index);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		/* Window resizing can invalidate the surface between the GLFW resize
		 * callback and this acquire. This is a transient no-frame condition,
		 * not an application error; the pending resize will rebuild the
		 * swapchain and the render loop can retry on its next iteration. */
		rtvk_swapchain_unlock(swapchain);
		return acquire;
	}
	if (result == VK_TIMEOUT || result == VK_NOT_READY) {
		/* Occlusion and interactive resizing may temporarily make forward
		 * progress unavailable. Match the out-of-date path and skip a frame. */
		rtvk_swapchain_unlock(swapchain);
		return acquire;
	}
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		rtvk_throwf(rtvk_error_from_vk(result), "swapchain acquire failed: vkAcquireNextImageKHR returned %s", rtvk_vk_result_name(result));
		rtvk_swapchain_unlock(swapchain);
		return acquire;
	}

	swapchain->frame_acquired = true;

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
	if (!acquire.timepoint.queue || acquire.timepoint.value == 0) {
		rtvk_throwf(RT_PLATFORM_FAILURE, "swapchain acquire failed: null timepoint");
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
	struct rtvk_swapchain_frame* frame = swapchain->frames[swapchain->current_frame_index];
	if (!rendered.queue || rendered.value == 0) {
		rtvk_throwf(RT_IMPROPER_USAGE, "swapchain present requires a render timepoint");
		rtvk_swapchain_mark_unacquired(swapchain);
		return;
	}
	rtvk_swapchain_submit_present_transition(ctx, swapchain, frame, rendered);
	if (rtvk_error() != RT_SUCCESS) {
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
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		rtvk_swapchain_mark_unacquired(swapchain);
		return;
	}

	rtvk_swapchain_mark_unacquired(swapchain);
}
