#ifndef RTVK_SWAPCHAIN_H
#define RTVK_SWAPCHAIN_H

#include "config.h"
#include "resource/framebuffer.h"
#include "resource/resource.h"
#include "resource/texture.h"
#include <volk.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
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

extern const VkSurfaceFormatKHR rtvk_swapchain_format_preferences[];
extern const u32 rtvk_swapchain_format_preferences_count;
extern const VkPresentModeKHR rtvk_swapchain_present_mode_preferences[];
extern const u32 rtvk_swapchain_present_mode_preferences_count;

struct rtvk_swapchain {
	struct rtvk_resource_base base;

	VkSurfaceKHR vk_surface;
	VkSwapchainKHR vk_swapchain;

	struct rtvk_queue* present_queue;
	struct rtvk_framebuffer** framebuffers;
	struct rtvk_texture_view** color_views;
	struct rtvk_swapchain_frame** frames;

	VkExtent2D extent;
	VkFormat vk_format;
	u32 image_count;
	u32 current_image_index;
	u32 current_frame_index;
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
	struct rtvk_resource_base base;

	VkImage vk_image;
	VkFormat vk_format;
	VkImageLayout vk_layout;
	enum rt_texture_type type;
	u32 width;
	u32 height;
	u32 depth;
	u32 mip_levels;

	VkSemaphore image_available;
	VkSemaphore present_ready;
	VkCommandPool present_command_pool;
	VkCommandBuffer present_command_buffer;
	struct rtvk_timepoint acquire_wait;
	struct rtvk_timepoint present_done;
	u32 present_command_family_index;
};

void rtvk_swapchain_frame_init(struct rtvk_context* ctx, struct rtvk_swapchain_frame* frame);
void rtvk_swapchain_frame_finish(struct rtvk_swapchain_frame* frame);

RTVK_DECLARE_NEW_RESOURCE(swapchain)

void rtvk_swapchain_init_from_surface(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, VkSurfaceKHR surface, u32 width, u32 height);
bool rtvk_swapchain_resize(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, u32 width, u32 height);

rt_swapchain_acquire_result rtvk_swapchain_acquire(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain);
void rtvk_swapchain_present(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, struct rtvk_timepoint rendered);

#endif
