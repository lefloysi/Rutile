#ifndef RTVAL_SWAPCHAIN_H
#define RTVAL_SWAPCHAIN_H

#include "handles.h"

struct rtval_swapchain {
	rt_swapchain backend;
	struct rtval_framebuffer* current_framebuffer;
	bool has_current_framebuffer;
};

struct rtval_swapchain* rtval_swapchain_create(void);
void rtval_swapchain_destroy(struct rtval_swapchain* swapchain);
void rtval_swapchain_resize(struct rtval_swapchain* swapchain, u32 width, u32 height);
rt_swapchain_acquire_result rtval_swapchain_acquire(struct rtval_swapchain* swapchain);
void rtval_swapchain_present(struct rtval_swapchain* swapchain, rt_timepoint rendered);
void rtval_swapchain_bind_window_glfw(struct rtval_swapchain* swapchain, GLFWwindow* window);

#endif
