#include "resource.h"
#include "buffer.h"
#include "command_buffer.h"
#include "error.h"
#include "framebuffer.h"
#include "graphics_program.h"
#include "texture.h"
#include "extension/swapchain/swapchain.h"

#include <stdlib.h>

void* rtdx_alloc_resource(usize size) {
	void* resource = calloc(1, size);
	if (!resource) {
		rtdx_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for graphics resource", size);
	}
	return resource;
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtdx_free_resource(void* resource) {
	free(resource);
}

void rtdx_init_resource_base(struct rtdx_context* ctx, struct rtdx_resource_base* base, rtdx_resource_type type) {
	base->type = type;
	base->ctx = ctx;
	rtdx_atomic_u32_init(&base->ref_count, 1);
	rtdx_atomic_u32_init(&base->job_count, 0);
	rtdx_atomic_bool_init(&base->zombie, false);
}

void rtdx_finish_resource_base(struct rtdx_context* ctx, struct rtdx_resource_base* base) {
	base->type = RT_RESOURCE_UNKNOWN;
	base->ctx = NULL;
	rtdx_atomic_bool_store(&base->zombie, true);
}

static void rtdx_resource_try_free(struct rtdx_resource_base* base) {
	if (rtdx_resource_ready_to_destroy(base)) {
		rtdx_resource_finalize(base);
		rtdx_atomic_bool_finish(&base->zombie);
		rtdx_atomic_u32_finish(&base->job_count);
		rtdx_atomic_u32_finish(&base->ref_count);
		rtdx_free_resource(base);
	}
}

void rtdx_resource_retain(struct rtdx_resource_base* base) {
	rtdx_atomic_inc(&base->ref_count);
}

void rtdx_resource_release(struct rtdx_resource_base* base) {
	rtdx_atomic_dec(&base->ref_count);
	rtdx_resource_try_free(base);
}

void rtdx_retain_resource_impl(void* resource) {
	if (resource) { rtdx_resource_retain((struct rtdx_resource_base*)resource); }
}

void rtdx_release_resource_impl(void* resource) {
	if (resource) { rtdx_resource_release((struct rtdx_resource_base*)resource); }
}

void rtdx_resource_retire(struct rtdx_resource_base* base) {
	rtdx_atomic_bool_store(&base->zombie, true);
	rtdx_resource_release(base);
}

void rtdx_resource_finalize(struct rtdx_resource_base* base) {
	if (!base || !base->ctx || base->type == RT_RESOURCE_UNKNOWN) {
		return;
	}

	struct rtdx_context* ctx = base->ctx;
	switch (base->type) {
	case RT_RESOURCE_BUFFER:
		rtdx_buffer_finish(ctx, (struct rtdx_buffer*)base);
		break;
	case RT_RESOURCE_COMMAND_BUFFER:
		rtdx_command_buffer_finish(ctx, (struct rtdx_command_buffer*)base);
		break;
	case RT_RESOURCE_FRAMEBUFFER:
		rtdx_framebuffer_finish(ctx, (struct rtdx_framebuffer*)base);
		break;
	case RT_RESOURCE_GRAPHICS_PROGRAM:
		rtdx_graphics_program_finish(ctx, (struct rtdx_graphics_program*)base);
		break;
	case RT_RESOURCE_SWAPCHAIN:
		rtdx_swapchain_finish(ctx, (struct rtdx_swapchain*)base);
		break;
	case RT_RESOURCE_TEXTURE:
		rtdx_texture_finish(ctx, (struct rtdx_texture*)base);
		break;
	case RT_RESOURCE_TEXTURE_VIEW:
		rtdx_texture_view_finish(ctx, (struct rtdx_texture_view*)base);
		break;
	case RT_RESOURCE_QUEUE:
	case RT_RESOURCE_UNKNOWN:
		break;
	}
}

bool rtdx_resource_ready_to_destroy(struct rtdx_resource_base* base) {
	return rtdx_atomic_bool_load(&base->zombie) &&
		rtdx_atomic_load(&base->ref_count) == 0 &&
		rtdx_atomic_load(&base->job_count) == 0;
}

rt_timepoint rtdx_timepoint_to_public(struct rtdx_timepoint timepoint) {
	rt_timepoint result = { (rt_queue)timepoint.queue, timepoint.value };
	return result;
}


