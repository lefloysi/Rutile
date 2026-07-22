#include "resource/swapchain.h"

#include "context.h"
#include "error.h"
#include "execution.h"
#include "resource/queue.h"

#include <stdlib.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_swapchain rtSwapchainCreate(void) {
	return rtgl_swapchain_to_handle(rtgl_swapchain_create(rtgl_get_current_context()));
}

void rtSwapchainDestroy(rt_swapchain swapchain) {
	rtgl_swapchain_destroy(rtgl_get_current_context(), rtgl_swapchain_from_handle(swapchain));
}

void rtSwapchainResize(rt_swapchain swapchain, u32 width, u32 height) {
	rtgl_swapchain_resize(rtgl_get_current_context(), rtgl_swapchain_from_handle(swapchain), width, height);
}

rt_swapchain_acquire_result rtSwapchainAcquire(rt_swapchain swapchain) {
	return rtgl_swapchain_acquire(rtgl_get_current_context(), rtgl_swapchain_from_handle(swapchain));
}

void rtSwapchainPresent(rt_swapchain swapchain, rt_timepoint rendered) {
	struct rtgl_timepoint timepoint = { (struct rtgl_queue*)rendered.queue, rendered.value };
	rtgl_swapchain_present(rtgl_get_current_context(), rtgl_swapchain_from_handle(swapchain), timepoint);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTGL_DEFINE_RESOURCE_PRIVATE(swapchain)

const u32 rtgl_swapchain_default_image_count = 2;

void rtgl_swapchain_init(struct rtgl_context* ctx, struct rtgl_swapchain* swapchain) {
	rtgl_init_resource_base(ctx, RTGL_RESOURCE_BASE(swapchain), RTGL_RESOURCE_SWAPCHAIN);
	rtgl_printf("rt-opengl: swapchain created\n");
}

void rtgl_swapchain_frame_init(struct rtgl_context* ctx, struct rtgl_swapchain_frame* frame) {
	rtgl_init_resource_base(ctx, RTGL_RESOURCE_BASE(frame), RTGL_RESOURCE_SWAPCHAIN_FRAME);
	frame->base.type = RT_TEXTURE_2D;
	frame->base.format = RT_RGBA8_UNORM;
	frame->base.depth = 1;
	frame->base.mip_levels = 1;
	frame->base.gl_target = GL_TEXTURE_2D;
	frame->base.gl_internal_format = GL_SRGB8_ALPHA8;
}

void rtgl_swapchain_frame_finish(struct rtgl_swapchain_frame* frame) {
	struct rtgl_context* ctx = frame->base.base.ctx;
	rtgl_framebuffer_destroy(ctx, frame->framebuffer);
	rtgl_texture_view_destroy(ctx, frame->color_view);
	rtgl_execution_texture_delete(ctx, &frame->base);
	frame->framebuffer = NULL;
	frame->color_view = NULL;
	rtgl_finish_resource_base(RTGL_RESOURCE_BASE(frame));
}

void rtgl_swapchain_destroy_images(struct rtgl_context* ctx, struct rtgl_swapchain* swapchain) {
	if (swapchain->frames) {
		for (u32 i = 0; i < swapchain->image_count; i++) {
			if (swapchain->frames[i]) {
				rtgl_resource_retire(RTGL_RESOURCE_BASE(swapchain->frames[i]));
				swapchain->frames[i] = NULL;
			}
		}
		free(swapchain->frames);
		swapchain->frames = NULL;
	}
	swapchain->image_count = 0;
	swapchain->current_frame_index = 0;
}

void rtgl_swapchain_finish(struct rtgl_swapchain* swapchain) {
	rtgl_swapchain_destroy_images(swapchain->base.ctx, swapchain);
	if (swapchain->surface) {
		rtgl_execution_surface_destroy(swapchain->base.ctx, swapchain->surface);
		swapchain->surface = NULL;
	}
	rtgl_printf("rt-opengl: swapchain destroyed\n");
	rtgl_finish_resource_base(RTGL_RESOURCE_BASE(swapchain));
}

void rtgl_swapchain_create_images(struct rtgl_context* ctx, struct rtgl_swapchain* swapchain, u32 width, u32 height) {
	swapchain->image_count = rtgl_swapchain_default_image_count;
	swapchain->frames = calloc(swapchain->image_count, sizeof(*swapchain->frames));
	RTGL_CHECK_ALLOC(swapchain->frames, (usize)swapchain->image_count * sizeof(*swapchain->frames), "OpenGL swapchain frames");
	if (!swapchain->frames) {
		return;
	}
	for (u32 i = 0; i < swapchain->image_count; i++) {
		struct rtgl_swapchain_frame* frame = calloc(1, sizeof(*frame));
		RTGL_CHECK_ALLOC(frame, sizeof(*frame), "OpenGL swapchain frame");
		if (!frame) {
			rtgl_swapchain_destroy_images(ctx, swapchain);
			return;
		}
		rtgl_swapchain_frame_init(ctx, frame);
		frame->base.width = width;
		frame->base.height = height;
		swapchain->frames[i] = frame;
		rtgl_execution_texture_create(ctx, &frame->base);
		rtgl_execution_texture_data(ctx, &frame->base, NULL);

		frame->framebuffer = rtgl_framebuffer_create(ctx);
		frame->color_view = rtgl_texture_view_create(ctx);
		if (!frame->framebuffer || !frame->color_view) {
			rtgl_swapchain_destroy_images(ctx, swapchain);
			return;
		}
		rtgl_texture_view_bind_image(ctx, frame->color_view, &frame->base);
		rtgl_framebuffer_set_color_view(ctx, frame->framebuffer, 0, frame->color_view);
		if (rtgl_error() != RT_SUCCESS) {
			rtgl_swapchain_destroy_images(ctx, swapchain);
			return;
		}
	}
}

void rtgl_swapchain_resize(struct rtgl_context* ctx, struct rtgl_swapchain* swapchain, u32 width, u32 height) {
	for (u32 i = 0; i < swapchain->image_count; i++) {
		struct rtgl_swapchain_frame* frame = swapchain->frames[i];
		if (!frame) {
			continue;
		}
		rtgl_execution_texture_delete(ctx, &frame->base);
		rtgl_execution_texture_create(ctx, &frame->base);
		frame->base.width = width;
		frame->base.height = height;
		rtgl_execution_texture_data(ctx, &frame->base, NULL);
		if (rtgl_error() != RT_SUCCESS) {
			return;
		}
		rtgl_texture_view_bind_image(ctx, frame->color_view, &frame->base);
		rtgl_framebuffer_set_color_view(ctx, frame->framebuffer, 0, frame->color_view);
		if (rtgl_error() != RT_SUCCESS) {
			return;
		}
	}
}

rt_swapchain_acquire_result rtgl_swapchain_acquire(struct rtgl_context* ctx, struct rtgl_swapchain* swapchain) {
	struct rtgl_framebuffer* framebuffer = swapchain->frames[swapchain->current_frame_index]->framebuffer;
	return (rt_swapchain_acquire_result){ rtgl_framebuffer_to_handle(framebuffer), { RT_NULL_HANDLE, 0 } };
}

void rtgl_swapchain_present(struct rtgl_context* ctx, struct rtgl_swapchain* swapchain, struct rtgl_timepoint rendered) {
	struct rtgl_queue* queue = rendered.queue ? rendered.queue : rtgl_context_graphics_queue(ctx);
	rtgl_timepoint_wait(ctx, rendered);
	rtgl_queue_present(queue, swapchain, swapchain->frames[swapchain->current_frame_index]->framebuffer);
	swapchain->current_frame_index = (swapchain->current_frame_index + 1) % swapchain->image_count;
}
