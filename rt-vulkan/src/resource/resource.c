#include "resource.h"
#include "buffer.h"
#include "command_buffer.h"
#include "compute_program.h"
#include "error.h"
#include "framebuffer.h"
#include "graphics_program.h"
#include "texture.h"
#include "extension/swapchain/swapchain.h"

#include <stdlib.h>

void* rtvk_alloc_resource(usize size) {
	void* resource = calloc(1, size);
	if (!resource) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for graphics resource", size);
	}
	return resource;
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtvk_free_resource(void* resource) {
	free(resource);
}

void rtvk_init_resource_base(struct rtvk_context* ctx, struct rtvk_resource_base* base, rtvk_resource_type type) {
	base->type = type;
	base->ctx = ctx;
	rtvk_atomic_u32_init(&base->ref_count, 1);
	rtvk_atomic_u32_init(&base->job_count, 0);
	rtvk_atomic_bool_init(&base->zombie, false);
}

void rtvk_finish_resource_base(struct rtvk_context* ctx, struct rtvk_resource_base* base) {
	base->type = RT_RESOURCE_UNKNOWN;
	base->ctx = NULL;
	rtvk_atomic_bool_store(&base->zombie, true);
}
static void rtvk_resource_try_free(struct rtvk_resource_base* base) {
	if (rtvk_resource_ready_to_destroy(base)) {
		rtvk_resource_finalize(base);
		rtvk_atomic_bool_finish(&base->zombie);
		rtvk_atomic_u32_finish(&base->job_count);
		rtvk_atomic_u32_finish(&base->ref_count);
		rtvk_free_resource(base);
	}
}

void rtvk_resource_retain(struct rtvk_resource_base* base) {
	rtvk_atomic_inc(&base->ref_count);
}

void rtvk_resource_release(struct rtvk_resource_base* base) {
	rtvk_atomic_dec(&base->ref_count);
	rtvk_resource_try_free(base);
}

void rtvk_retain_resource_impl(void* resource) {
	if (resource) { rtvk_resource_retain((struct rtvk_resource_base*)resource); }
}

void rtvk_release_resource_impl(void* resource) {
	if (resource) { rtvk_resource_release((struct rtvk_resource_base*)resource); }
}

void rtvk_resource_job_begin(struct rtvk_resource_base* base) {
	rtvk_atomic_inc(&base->job_count);
}

void rtvk_resource_job_end(struct rtvk_resource_base* base) {
	rtvk_atomic_dec(&base->job_count);
	rtvk_resource_try_free(base);
}

void rtvk_resource_retire(struct rtvk_resource_base* base) {
	rtvk_atomic_bool_store(&base->zombie, true);
	rtvk_resource_release(base);
}

void rtvk_resource_finalize(struct rtvk_resource_base* base) {
	if (!base || !base->ctx || base->type == RT_RESOURCE_UNKNOWN) {
		return;
	}

	struct rtvk_context* ctx = base->ctx;
	switch (base->type) {
	case RT_RESOURCE_BUFFER:
		rtvk_buffer_finish(ctx, (struct rtvk_buffer*)base);
		break;
	case RT_RESOURCE_COMMAND_BUFFER:
		rtvk_command_buffer_finish(ctx, (struct rtvk_command_buffer*)base);
		break;
	case RT_RESOURCE_FRAMEBUFFER:
		rtvk_framebuffer_finish(ctx, (struct rtvk_framebuffer*)base);
		break;
	case RT_RESOURCE_GRAPHICS_PROGRAM:
		rtvk_graphics_program_finish(ctx, (struct rtvk_graphics_program*)base);
		break;
	case RT_RESOURCE_COMPUTE_PROGRAM:
		rtvk_compute_program_finish(ctx, (struct rtvk_compute_program*)base);
		break;
	case RT_RESOURCE_SWAPCHAIN:
		rtvk_swapchain_finish(ctx, (struct rtvk_swapchain*)base);
		break;
	case RT_RESOURCE_TEXTURE:
		rtvk_texture_finish(ctx, (struct rtvk_texture*)base);
		break;
	case RT_RESOURCE_TEXTURE_VIEW:
		rtvk_texture_view_finish(ctx, (struct rtvk_texture_view*)base);
		break;
	case RT_RESOURCE_QUEUE:
	case RT_RESOURCE_UNKNOWN:
		break;
	}
}

bool rtvk_resource_ready_to_destroy(struct rtvk_resource_base* base) {
	return rtvk_atomic_bool_load(&base->zombie) &&
		rtvk_atomic_load(&base->ref_count) == 0 &&
		rtvk_atomic_load(&base->job_count) == 0;
}

rt_timepoint rtvk_timepoint_to_public(struct rtvk_timepoint timepoint) {
	rt_timepoint result = { (rt_queue)timepoint.queue, timepoint.value };
	return result;
}


