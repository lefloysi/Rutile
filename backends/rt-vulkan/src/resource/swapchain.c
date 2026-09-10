#include "resource/swapchain.h"
#include "context.h"
#include "error.h"
#include "glfw.h"
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
	rtvk_begin_errorable_operation();
	return rtvk_swapchain_to_handle(rtvk_swapchain_create(rtvk_get_current_context()));
}

void rtSwapchainDestroy(rt_swapchain swapchain) {
	rtvk_swapchain_destroy(rtvk_get_current_context(), rtvk_swapchain_from_handle(swapchain));
}

void rtSwapchainResize(rt_swapchain swapchain, u32 width, u32 height) {
	rtvk_begin_errorable_operation();
	rtvk_swapchain_resize(
		rtvk_get_current_context(),
		rtvk_swapchain_from_handle(swapchain),
		width,
		height
	);
}

rt_swapchain_acquire_result rtSwapchainAcquire(rt_swapchain swapchain) {
	rtvk_begin_errorable_operation();
	return rtvk_swapchain_acquire(
		rtvk_get_current_context(),
		rtvk_swapchain_from_handle(swapchain)
	);
}

void rtSwapchainPresent(rt_swapchain swapchain, rt_timepoint rendered) {
	rtvk_begin_errorable_operation();
	rtvk_swapchain_present(
		rtvk_get_current_context(),
		rtvk_swapchain_from_handle(swapchain),
		rendered
	);
}

void rtSwapchainBindGLFW(rt_swapchain swapchain, struct GLFWwindow* window) {
	rtvk_begin_errorable_operation();
	rtvk_swapchain_bind_glfw(
		rtvk_get_current_context(),
		rtvk_swapchain_from_handle(swapchain),
		window
	);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_DEFINE_RESOURCE_PRIVATE(swapchain)
RTVK_DEFINE_RESOURCE_FINALIZER(swapchain_generation)
RTVK_DEFINE_RESOURCE_FINALIZER(swapchain_image)

void rtvk_swapchain_bind_glfw(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, struct GLFWwindow* window) {
	VkSurfaceKHR surface;
	int width = 0;
	int height = 0;

	if (!ctx || !swapchain || !window) {
		rtvk_throwf(RT_IMPROPER_USAGE, "swapchain, context, and GLFW window must be valid");
		return;
	}

	rtvk_init_glfw_platform();
	if (rtvk_error() != RT_SUCCESS) {
		return;
	}

	surface = rtvk_create_glfw_surface(ctx, window);
	if (rtvk_error() != RT_SUCCESS) {
		return;
	}

	rtvk_glfw_get_framebuffer_size(window, &width, &height);
	if (width <= 0 || height <= 0) {
		rtvk_throwf(
			RT_IMPROPER_USAGE,
			"glfwGetFramebufferSize returned non-positive extent (%dx%d); window may be minimized or not yet shown",
			width,
			height
		);
		return;
	}
	rtvk_swapchain_init_from_surface(ctx, swapchain, surface, (u32)width, (u32)height);
}

void rtvk_swapchain_init(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain) {
	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(swapchain), swapchain, rtvk_swapchain_finalize_resource);
	if (!rtvk_mutex_init(&swapchain->frame_lock) || !rtvk_condition_init(&swapchain->frame_condition)) {
		rtvk_throwf(RT_PLATFORM_FAILURE, "failed to create Vulkan swapchain synchronization");
	}
}

static void rtvk_swapchain_wait_frame(struct rtvk_context* ctx, struct rtvk_swapchain_generation* generation, u32 frame_index);

static struct rtvk_swapchain_generation* rtvk_swapchain_generation_create(struct rtvk_context* ctx) {
	struct rtvk_swapchain_generation* generation = RTVK_ALLOC_RESOURCE(struct rtvk_swapchain_generation);
	if (!generation) {
		return NULL;
	}
	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(generation), generation, rtvk_swapchain_generation_finalize_resource);
	return generation;
}

static void rtvk_swapchain_generation_retire(struct rtvk_context* ctx, struct rtvk_swapchain_generation* generation) {
	if (!generation) {
		return;
	}
	if (generation->present_queue) {
		rtvk_mutex_lock(&generation->present_queue->lock);
		VkResult result = vkQueueWaitIdle(generation->present_queue->vk_queue);
		rtvk_mutex_unlock(&generation->present_queue->lock);
		if (result != VK_SUCCESS) {
			rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
			return;
		}
	}
	for (u32 index = 0; index < RTVK_MAX_FRAMES_IN_FLIGHT; index++) {
		rtvk_swapchain_wait_frame(ctx, generation, index);
	}

	for (u32 index = 0; index < generation->image_count; index++) {
		struct rtvk_swapchain_image* image = generation->images ? generation->images[index] : NULL;
		if (!image) {
			continue;
		}
		if (image->framebuffer) {
			rtvk_framebuffer_destroy(ctx, image->framebuffer);
			image->framebuffer = NULL;
		}
		if (image->color_view) {
			rtvk_texture_view_destroy(ctx, image->color_view);
			image->color_view = NULL;
		}
		rtvk_resource_retire(RTVK_RESOURCE_BASE(image));
		generation->images[index] = NULL;
	}
	free(generation->images);
	generation->images = NULL;
	generation->image_count = 0;
	rtvk_resource_retire(RTVK_RESOURCE_BASE(generation));
}

void rtvk_swapchain_generation_finish(struct rtvk_swapchain_generation* generation) {
	struct rtvk_context* ctx = generation->base.ctx;
	assert(!generation->images);
	for (u32 index = 0; index < RTVK_MAX_FRAMES_IN_FLIGHT; index++) {
		if (generation->image_available[index]) {
			vkDestroySemaphore(ctx->vk_device, generation->image_available[index], VK_ALLOCATOR);
		}
		if (generation->present_ready[index]) {
			vkDestroySemaphore(ctx->vk_device, generation->present_ready[index], VK_ALLOCATOR);
		}
	}
	if (generation->vk_swapchain) {
		vkDestroySwapchainKHR(ctx->vk_device, generation->vk_swapchain, VK_ALLOCATOR);
		generation->vk_swapchain = VK_NULL_HANDLE;
	}
	if (generation->present_queue) {
		rtvk_release_resource(generation->present_queue);
	}
}

static void rtvk_swapchain_lock(struct rtvk_swapchain* swapchain) {
	rtvk_mutex_lock(&swapchain->frame_lock);
}

static void rtvk_swapchain_unlock(struct rtvk_swapchain* swapchain) {
	rtvk_mutex_unlock(&swapchain->frame_lock);
}

static void rtvk_swapchain_lock_unacquired(struct rtvk_swapchain* swapchain) {
	rtvk_swapchain_lock(swapchain);
	while (swapchain->frame_acquired) {
		rtvk_condition_wait(&swapchain->frame_condition, &swapchain->frame_lock);
	}
}

static void rtvk_swapchain_mark_unacquired_locked(struct rtvk_swapchain* swapchain) {
	swapchain->frame_acquired = false;
	if (swapchain->generation && swapchain->generation->image_count) {
		swapchain->generation->frame_index = (swapchain->generation->frame_index + 1) % RTVK_MAX_FRAMES_IN_FLIGHT;
	}
	rtvk_condition_broadcast(&swapchain->frame_condition);
}

static void rtvk_swapchain_mark_unacquired(struct rtvk_swapchain* swapchain) {
	rtvk_swapchain_lock(swapchain);
	rtvk_swapchain_mark_unacquired_locked(swapchain);
	rtvk_swapchain_unlock(swapchain);
}

static void rtvk_swapchain_release_acquired_image_locked(struct rtvk_swapchain* swapchain, struct rtvk_swapchain_generation* generation) {
	u32 frame_index = generation->frame_index;
	VkSemaphore present_wait = generation->image_available[frame_index];
	if (generation->acquire_wait[frame_index].value) {
		rt_timepoint release = rtvk_queue_signal_binary_after_timepoint(
			generation->present_queue,
			rtvk_timepoint_value(generation->acquire_wait[frame_index]),
			generation->present_ready[frame_index]
		);
		if (release.value) {
			rtvk_mutex_lock(&generation->present_queue->lock);
			rtvk_queue_flush_locked(swapchain->base.ctx, generation->present_queue);
			rtvk_mutex_unlock(&generation->present_queue->lock);
			if (rtvk_error() == RT_SUCCESS) {
				generation->present_done[frame_index] = release;
				present_wait = generation->present_ready[frame_index];
			}
		}
	}
	VkPresentInfoKHR present_info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &present_wait;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &generation->vk_swapchain;
	present_info.pImageIndices = &generation->acquired_image_index;
	present_info.pResults = NULL;
	rtvk_mutex_lock(&generation->present_queue->lock);
	(void)vkQueuePresentKHR(generation->present_queue->vk_queue, &present_info);
	rtvk_mutex_unlock(&generation->present_queue->lock);
	rtvk_swapchain_mark_unacquired_locked(swapchain);
}

static void rtvk_swapchain_force_unacquired(struct rtvk_swapchain* swapchain) {
	rtvk_swapchain_lock(swapchain);
	swapchain->frame_acquired = false;
	rtvk_swapchain_unlock(swapchain);
}

static void rtvk_swapchain_finish_sync(struct rtvk_swapchain* swapchain) {
	rtvk_condition_finish(&swapchain->frame_condition);
	rtvk_mutex_finish(&swapchain->frame_lock);
}

static void rtvk_swapchain_wait_frame(struct rtvk_context* ctx, struct rtvk_swapchain_generation* generation, u32 frame_index) {
	bool reused = false;
	if (generation->acquire_wait[frame_index].value) {
		rtvk_timepoint_wait(ctx, generation->acquire_wait[frame_index]);
		generation->acquire_wait[frame_index] = (rt_timepoint){ 0 };
		reused = true;
	}

	if (generation->present_done[frame_index].value) {
		rtvk_timepoint_wait(ctx, generation->present_done[frame_index]);
		generation->present_done[frame_index] = (rt_timepoint){ 0 };
		reused = true;
	}

	/* A timeline signal from the render submission does not prove that
	 * vkQueuePresentKHR has consumed its binary wait semaphore. Before reusing
	 * this frame slot's acquire semaphore, wait for the presentation queue so
	 * Vulkan cannot observe an unfinished wait on that semaphore. */
	if (reused && generation->present_queue) {
		rtvk_mutex_lock(&generation->present_queue->lock);
		VkResult result = vkQueueWaitIdle(generation->present_queue->vk_queue);
		rtvk_mutex_unlock(&generation->present_queue->lock);
		if (result != VK_SUCCESS) {
			rtvk_throwf(rtvk_error_from_vk(result), "vkQueueWaitIdle before swapchain acquire returned %s", rtvk_vk_result_name(result));
		}
	}
}

void rtvk_swapchain_image_init(struct rtvk_context* ctx, struct rtvk_swapchain_image* image) {
	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(image), image, rtvk_swapchain_image_finalize_resource);
	image->base.vk_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	image->base.vk_format = VK_FORMAT_UNDEFINED;
	image->base.type = RT_TEXTURE_2D;
	image->base.depth = 1;
	image->base.mip_levels = 1;
	image->base.presentable = true;
}

// VkImage itself belongs to VkSwapchainKHR and is destroyed by that API.
void rtvk_swapchain_image_finish(struct rtvk_swapchain_image* image) {
	struct rtvk_context* ctx = image->base.base.ctx;
	if (image->framebuffer) {
		rtvk_framebuffer_destroy(ctx, image->framebuffer);
	}
	if (image->color_view) {
		rtvk_texture_view_destroy(ctx, image->color_view);
	}
	image->color_view = NULL;
	image->framebuffer = NULL;
	image->base.vk_image = VK_NULL_HANDLE;
	if (image->generation) {
		rtvk_release_resource(image->generation);
	}
}

void rtvk_swapchain_finish(struct rtvk_swapchain* swapchain) {
	struct rtvk_context* ctx = swapchain->base.ctx;
	rtvk_swapchain_force_unacquired(swapchain);
	if (swapchain->generation) {
		rtvk_swapchain_generation_retire(ctx, swapchain->generation);
		swapchain->generation = NULL;
	}
	if (swapchain->surface) {
		vkDestroySurfaceKHR(ctx->vk_instance, swapchain->surface, VK_ALLOCATOR);
		swapchain->surface = VK_NULL_HANDLE;
	}
	rtvk_swapchain_finish_sync(swapchain);
}

static void rtvk_swapchain_create_images(struct rtvk_context* ctx, struct rtvk_swapchain_generation* generation) {
	VkImage* images = NULL;
	u32 image_count = 0;

	VkResult result = vkGetSwapchainImagesKHR(ctx->vk_device, generation->vk_swapchain, &image_count, NULL);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "vkGetSwapchainImagesKHR failed while counting swapchain images");
		return;
	}

	images = calloc(image_count, sizeof(*images));
	RTVK_CHECK_ALLOC(images, (usize)image_count * sizeof(*images), "swapchain image list");
	if (rtvk_error() != RT_SUCCESS) {
		goto cleanup;
	}

	generation->images = calloc(image_count, sizeof(*generation->images));
	RTVK_CHECK_ALLOC(generation->images, (usize)image_count * sizeof(*generation->images), "swapchain image list");
	if (rtvk_error() != RT_SUCCESS) {
		goto cleanup;
	}

	result = vkGetSwapchainImagesKHR(ctx->vk_device, generation->vk_swapchain, &image_count, images);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "vkGetSwapchainImagesKHR failed while fetching swapchain images");
		goto cleanup;
	}

	generation->image_count = image_count;
	for (u32 i = 0; i < image_count; i++) {
		struct rtvk_swapchain_image* image = calloc(1, sizeof(*image));
		if (!image) {
			rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate swapchain image");
			goto cleanup;
		}
		rtvk_swapchain_image_init(ctx, image);
		image->base.vk_image = images[i];
		image->base.vk_format = generation->vk_format;
		image->base.width = generation->extent.width;
		image->base.height = generation->extent.height;
		image->generation = generation;
		rtvk_retain_resource(generation);
		generation->images[i] = image;

		image->color_view = rtvk_texture_view_create(ctx);
		if (!image->color_view) {
			goto cleanup;
		}
		rtvk_texture_view_bind_image(ctx, image->color_view, &image->base);
		if (rtvk_error() != RT_SUCCESS) {
			goto cleanup;
		}
		image->framebuffer = rtvk_framebuffer_create(ctx);
		if (!image->framebuffer) {
			goto cleanup;
		}
		rtvk_framebuffer_set_color_view(ctx, image->framebuffer, 0, image->color_view);
		if (rtvk_error() != RT_SUCCESS) {
			goto cleanup;
		}
	}

cleanup:
	free(images);
}

static void rtvk_swapchain_create_frame_sync(struct rtvk_context* ctx, struct rtvk_swapchain_generation* generation) {
	VkSemaphoreCreateInfo semaphore_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	semaphore_info.pNext = NULL;
	semaphore_info.flags = 0;

	for (u32 i = 0; i < RTVK_MAX_FRAMES_IN_FLIGHT; i++) {
		VkResult result = vkCreateSemaphore(ctx->vk_device, &semaphore_info, VK_ALLOCATOR, &generation->image_available[i]);
		if (result != VK_SUCCESS) {
			rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
			return;
		}

		result = vkCreateSemaphore(ctx->vk_device, &semaphore_info, VK_ALLOCATOR, &generation->present_ready[i]);
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

static struct rtvk_swapchain_generation* rtvk_swapchain_generation_build(
	struct rtvk_context* ctx,
	VkSurfaceKHR surface,
	u32 width,
	u32 height,
	VkSwapchainKHR old_swapchain
) {
	struct rtvk_swapchain_generation* generation;
	generation = rtvk_swapchain_generation_create(ctx);
	if (!generation) {
		return NULL;
	}
	generation->present_queue = rtvk_queue_query_present(ctx, surface);
	if (!generation->present_queue) {
		if (rtvk_error() == RT_SUCCESS) {
			rtvk_throwf(RT_UNSUPPORTED_PLATFORM, NULL);
		}
		goto cleanup;
	}
	rtvk_retain_resource(generation->present_queue);

	VkSurfaceCapabilitiesKHR capabilities;
	VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx->vk_physical_device, surface, &capabilities);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		goto cleanup;
	}

	u32 format_count = 0;
	result = vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->vk_physical_device, surface, &format_count, NULL);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		goto cleanup;
	}

	VkSurfaceFormatKHR* formats = calloc(format_count, sizeof(*formats));
	if (!formats) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %u Vulkan surface formats", format_count);
		goto cleanup;
	}
	result = vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->vk_physical_device, surface, &format_count, formats);
	if (result != VK_SUCCESS) {
		free(formats);
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		goto cleanup;
	}

	u32 present_mode_count = 0;
	result = vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->vk_physical_device, surface, &present_mode_count, NULL);
	if (result != VK_SUCCESS) {
		free(formats);
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		goto cleanup;
	}
	if (present_mode_count == 0) {
		free(formats);
		rtvk_throwf(RT_INCOMPATIBLE_DRIVER, NULL);
		goto cleanup;
	}

	VkPresentModeKHR* present_modes = calloc(present_mode_count, sizeof(*present_modes));
	if (!present_modes) {
		free(formats);
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %u Vulkan present modes", present_mode_count);
		goto cleanup;
	}
	result = vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->vk_physical_device, surface, &present_mode_count, present_modes);
	if (result != VK_SUCCESS) {
		free(formats);
		free(present_modes);
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		goto cleanup;
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

	/* Presentation itself only needs color-attachment usage. Transfer-source is
	 * optional for a Vulkan surface and no current Rutile presentation operation
	 * reads a swapchain image back, so requiring it rejects otherwise valid
	 * windows before their first acquire. */
	VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if ((capabilities.supportedUsageFlags & image_usage) != image_usage) {
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "swapchain images do not support color attachment usage");
		goto cleanup;
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
	if (graphics_queue && graphics_queue->family_index != generation->present_queue->family_index) {
		queue_family_indices[0] = graphics_queue->family_index;
		queue_family_indices[1] = generation->present_queue->family_index;
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
	swapchain_info.oldSwapchain = old_swapchain;

	result = vkCreateSwapchainKHR(ctx->vk_device, &swapchain_info, VK_ALLOCATOR, &generation->vk_swapchain);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		goto cleanup;
	}

	generation->vk_format = format.format;
	generation->extent = swapchain_info.imageExtent;
	rtvk_swapchain_create_images(ctx, generation);
	if (rtvk_error() != RT_SUCCESS) {
		goto cleanup;
	}
	rtvk_swapchain_create_frame_sync(ctx, generation);
	if (rtvk_error() != RT_SUCCESS) {
		goto cleanup;
	}
	return generation;

cleanup:
	rtvk_swapchain_generation_retire(ctx, generation);
	return NULL;
}

void rtvk_swapchain_init_from_surface(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, VkSurfaceKHR surface, u32 width, u32 height) {
	swapchain->generation = rtvk_swapchain_generation_build(ctx, surface, width, height, VK_NULL_HANDLE);
	if (!swapchain->generation) {
		vkDestroySurfaceKHR(ctx->vk_instance, surface, VK_ALLOCATOR);
		return;
	}
	swapchain->surface = surface;
}

bool rtvk_swapchain_resize(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, u32 width, u32 height) {
	if (!swapchain || !swapchain->surface || !swapchain->generation) {
		rtvk_throwf(RT_IMPROPER_USAGE, "swapchain resize requires a valid swapchain");
		return false;
	}
	if (width == 0 || height == 0) {
		return false;
	}

	rtvk_swapchain_lock_unacquired(swapchain);
	struct rtvk_swapchain_generation* old_generation = swapchain->generation;
	if (width == old_generation->extent.width && height == old_generation->extent.height) {
		rtvk_swapchain_unlock(swapchain);
		return true;
	}

	struct rtvk_swapchain_generation* new_generation = rtvk_swapchain_generation_build(
		ctx,
		swapchain->surface,
		width,
		height,
		old_generation->vk_swapchain
	);
	if (new_generation) {
		swapchain->generation = new_generation;
		rtvk_swapchain_generation_retire(ctx, old_generation);
	}
	rtvk_swapchain_unlock(swapchain);
	return new_generation != NULL;
}

rt_swapchain_acquire_result rtvk_swapchain_acquire(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain) {
	rt_swapchain_acquire_result acquire = { 0 };
	if (!swapchain) {
		rtvk_throwf(RT_IMPROPER_USAGE, "swapchain acquire requires a valid swapchain");
		return acquire;
	}
	rtvk_swapchain_lock_unacquired(swapchain);
	struct rtvk_swapchain_generation* generation = swapchain->generation;
	if (!generation || !generation->images || generation->image_count == 0) {
		rtvk_swapchain_unlock(swapchain);
		rtvk_throwf(RT_IMPROPER_USAGE, "swapchain has no images");
		return acquire;
	}
	u32 frame_index = generation->frame_index;
	rtvk_swapchain_wait_frame(ctx, generation, frame_index);

	/* Comment ** finite timeout: the spec forbids UINT64_MAX when forward progress
	 * cannot be guaranteed (VUID-vkAcquireNextImageKHR-surface-07783). One second is
	 * long enough that a healthy frame pipeline never trips it, short enough that a
	 * deadlock surfaces as VK_TIMEOUT instead of hanging the app. */
	VkResult result = vkAcquireNextImageKHR(
		ctx->vk_device,
		generation->vk_swapchain,
		1000000000ull,
		generation->image_available[frame_index],
		VK_NULL_HANDLE,
		&generation->acquired_image_index
	);
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
	generation->acquire_wait[frame_index] = rtvk_queue_wait_binary(
		ctx,
		generation->present_queue,
		generation->image_available[frame_index]
	);
	if (rtvk_error() != RT_SUCCESS) {
		generation->acquire_wait[frame_index] = (rt_timepoint){ 0 };
		rtvk_swapchain_release_acquired_image_locked(swapchain, generation);
		rtvk_swapchain_unlock(swapchain);
		return acquire;
	}
	rtvk_mutex_lock(&generation->present_queue->lock);
	rtvk_queue_flush_locked(ctx, generation->present_queue);
	rtvk_mutex_unlock(&generation->present_queue->lock);
	if (rtvk_error() != RT_SUCCESS) {
		generation->acquire_wait[frame_index] = (rt_timepoint){ 0 };
		rtvk_swapchain_release_acquired_image_locked(swapchain, generation);
		rtvk_swapchain_unlock(swapchain);
		return acquire;
	}
	if (generation->acquire_wait[frame_index].value == 0) {
		rtvk_throwf(RT_PLATFORM_FAILURE, "swapchain acquire failed: acquire wait is invalid");
		generation->acquire_wait[frame_index] = (rt_timepoint){ 0 };
		rtvk_swapchain_release_acquired_image_locked(swapchain, generation);
		rtvk_swapchain_unlock(swapchain);
		return acquire;
	}

	acquire.timepoint = generation->acquire_wait[frame_index];
	acquire.framebuffer = rtvk_framebuffer_to_handle(generation->images[generation->acquired_image_index]->framebuffer);
	if (!acquire.timepoint.value || !acquire.framebuffer) {
		rtvk_throwf(RT_PLATFORM_FAILURE, "swapchain acquire failed: null timepoint");
		rtvk_swapchain_release_acquired_image_locked(swapchain, generation);
		rtvk_swapchain_unlock(swapchain);
		return (rt_swapchain_acquire_result){ 0 };
	}
	rtvk_swapchain_unlock(swapchain);
	return acquire;
}

void rtvk_swapchain_present(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, rt_timepoint rendered) {
	if (!swapchain || !swapchain->generation || !swapchain->generation->images || swapchain->generation->image_count == 0 || !swapchain->frame_acquired) {
		rtvk_throwf(RT_IMPROPER_USAGE, "swapchain present requires an acquired frame");
		return;
	}
	struct rtvk_swapchain_generation* generation = swapchain->generation;
	u32 frame_index = generation->frame_index;
	if (!rtvk_timepoint_queue(ctx, rendered) || rendered.value == 0) {
		rtvk_throwf(RT_IMPROPER_USAGE, "swapchain present requires a render timepoint");
		rtvk_swapchain_mark_unacquired(swapchain);
		return;
	}
	struct rtvk_queue* rendered_queue = rtvk_timepoint_queue(ctx, rendered);
	generation->present_done[frame_index] = rtvk_queue_signal_binary_after_timepoint(
		rendered_queue,
		rtvk_timepoint_value(rendered),
		generation->present_ready[frame_index]
	);
	if (!generation->present_done[frame_index].value) {
		rtvk_swapchain_mark_unacquired(swapchain);
		return;
	}
	rtvk_mutex_lock(&rendered_queue->lock);
	rtvk_queue_flush_locked(ctx, rendered_queue);
	rtvk_mutex_unlock(&rendered_queue->lock);
	if (rtvk_error() != RT_SUCCESS) {
		rtvk_swapchain_mark_unacquired(swapchain);
		return;
	}

	VkPresentInfoKHR present_info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	present_info.pNext = NULL;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &generation->present_ready[frame_index];
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &generation->vk_swapchain;
	present_info.pImageIndices = &generation->acquired_image_index;
	present_info.pResults = NULL;

	rtvk_mutex_lock(&generation->present_queue->lock);
	VkResult result = vkQueuePresentKHR(generation->present_queue->vk_queue, &present_info);
	rtvk_mutex_unlock(&generation->present_queue->lock);
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
