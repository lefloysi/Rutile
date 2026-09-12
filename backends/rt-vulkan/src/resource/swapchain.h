#ifndef RTVK_SWAPCHAIN_H
#define RTVK_SWAPCHAIN_H

#include "config.h"
#include "resource/framebuffer.h"
#include "resource/resource.h"
#include "resource/texture.h"
#include "sync.h"
#include <volk.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_API rt_swapchain rtSwapchainCreate(void);
RTVK_API void rtSwapchainDestroy(rt_swapchain swapchain);
RTVK_API void rtSwapchainResize(rt_swapchain swapchain, u32 width, u32 height);
RTVK_API rt_swapchain_acquire_result rtSwapchainAcquire(rt_swapchain swapchain);
RTVK_API void rtSwapchainPresent(rt_swapchain swapchain, rt_timepoint rendered);
RTVK_API void rtSwapchainBindGLFW(rt_swapchain swapchain, struct GLFWwindow* window);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

extern const VkSurfaceFormatKHR rtvk_swapchain_format_preferences[];
extern const u32 rtvk_swapchain_format_preferences_count;
extern const VkPresentModeKHR rtvk_swapchain_present_mode_preferences[];
extern const u32 rtvk_swapchain_present_mode_preferences_count;

struct rtvk_swapchain {
	struct rtvk_resource_base base;

	VkSurfaceKHR surface;
	struct rtvk_swapchain_generation* generation;
	bool frame_acquired;

	struct rtvk_mutex frame_lock;
	struct rtvk_condition frame_condition;
};
RTVK_DECLARE_NEW_RESOURCE(swapchain)

struct rtvk_swapchain_generation {
	struct rtvk_resource_base base;

	struct rtvk_queue* present_queue;
	VkSwapchainKHR vk_swapchain;
	struct rtvk_swapchain_image** images;
	VkSemaphore image_available[RTVK_MAX_FRAMES_IN_FLIGHT];
	rt_timepoint acquire_wait[RTVK_MAX_FRAMES_IN_FLIGHT];
	rt_timepoint present_done[RTVK_MAX_FRAMES_IN_FLIGHT];

	VkExtent2D extent;
	VkFormat vk_format;
	u32 image_count;
	u32 acquired_image_index;
	u32 frame_index;
};

struct rtvk_swapchain_image {
	struct rtvk_image_base base;
	VkSemaphore present_ready;

	struct rtvk_swapchain_generation* generation;
	struct rtvk_texture_view* color_view;
	struct rtvk_framebuffer* framebuffer;
};

void rtvk_swapchain_image_init(struct rtvk_context* ctx, struct rtvk_swapchain_image* image);
void rtvk_swapchain_image_finish(struct rtvk_swapchain_image* image);
void rtvk_swapchain_generation_finish(struct rtvk_swapchain_generation* generation);

void rtvk_swapchain_init_from_surface(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, VkSurfaceKHR surface, u32 width, u32 height);
bool rtvk_swapchain_resize(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, u32 width, u32 height);

rt_swapchain_acquire_result rtvk_swapchain_acquire(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain);
void rtvk_swapchain_present(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, rt_timepoint rendered);
void rtvk_swapchain_bind_glfw(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, struct GLFWwindow* window);

#endif
