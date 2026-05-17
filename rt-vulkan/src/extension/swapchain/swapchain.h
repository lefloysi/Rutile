#ifndef RTVK_SWAPCHAIN_H
#define RTVK_SWAPCHAIN_H

#include "config.h"
#include "resource/resource.h"
#include "resource/framebuffer.h"
#include "resource/texture.h"
#include <vulkan/vulkan.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_API rt_swapchain rtSwapchainCreate(void);
RTVK_API void rtSwapchainDestroy(rt_swapchain swapchain);
RTVK_API void rtSwapchainResize(rt_swapchain swapchain, u32 width, u32 height);

RTVK_API rt_swapchain_acquire_result rtSwapchainAcquire(rt_swapchain swapchain);
RTVK_API void rtSwapchainPresent(rt_swapchain swapchain, rt_timepoint rendered);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtvk_swapchain {
	struct rtvk_resource_base base;

	VkSurfaceKHR vk_surface;
	VkSwapchainKHR vk_swapchain;

	struct rtvk_queue* present_queue;
	struct rtvk_texture** textures;
	struct rtvk_texture_view** texture_views;
	struct rtvk_framebuffer** framebuffers;
	struct rtvk_swapchain_frame* frames;

	VkExtent2D extent;
	VkFormat vk_format;
	u32 image_count;
	u32 current_image_index;
	u32 current_frame_index;
	u32 next_frame_index;
	bool frame_acquired;
#if defined(_WIN32)
	CRITICAL_SECTION frame_lock;
	CONDITION_VARIABLE frame_condition;
#else
	pthread_mutex_t frame_lock;
	pthread_cond_t frame_condition;
#endif
};

struct rtvk_swapchain_frame {
	VkImage vk_image;
	VkSemaphore image_available;
	VkSemaphore present_ready;
	VkCommandPool present_command_pool;
	VkCommandBuffer present_command_buffer;

	struct rtvk_queue* present_queue;

	u64 present_value;
	VkFormat vk_format;
	u32 width;
	u32 height;
	u32 present_command_family_index;
	bool has_present_timepoint;
};

RTVK_DECLARE_NEW_RESOURCE(swapchain)

bool rtvk_swapchain_create_for_surface(
	struct rtvk_context* ctx,
	struct rtvk_swapchain* swapchain,
	VkSurfaceKHR surface,
	u32 width,
	u32 height);
bool rtvk_swapchain_resize(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, u32 width, u32 height);

rt_swapchain_acquire_result rtvk_swapchain_acquire(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain);
void rtvk_swapchain_present(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, struct rtvk_timepoint rendered);

#endif


