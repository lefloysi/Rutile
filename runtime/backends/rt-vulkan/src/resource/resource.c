#include "resource.h"
#include "buffer.h"
#include "command_buffer.h"
#include "error.h"
#include "framebuffer.h"
#include "graphics_program.h"
#include "resource/swapchain.h"
#include "texture.h"

#include <assert.h>
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


void rtvk_init_resource_base(struct rtvk_context* ctx, struct rtvk_resource_base* base, rtvk_resource_type type) {
	assert(base);
	base->type = type;
	base->ctx = ctx;
	base->ref_count = 1;
	base->job_count = 0;
	base->zombie = false;
	base->finalizing = false;
}

void rtvk_finish_resource_base(struct rtvk_resource_base* base) {
	assert(base);
	base->ctx = NULL;
	rtvk_atomic_bool_store(&base->zombie, true);
}

void rtvk_resource_try_free(struct rtvk_resource_base* base) {
	if (rtvk_resource_ready_to_destroy(base) && !rtvk_atomic_bool_exchange(&base->finalizing, true)) {
		rtvk_resource_finalize(base);
		free(base);
	}
}

void rtvk_resource_retain(struct rtvk_resource_base* base) {
	assert(base);
	rtvk_atomic_inc(&base->ref_count);
}

void rtvk_resource_release(struct rtvk_resource_base* base) {
	assert(base);
	assert(rtvk_atomic_load(&base->ref_count) > 0);
	rtvk_atomic_dec(&base->ref_count);
	rtvk_resource_try_free(base);
}

void rtvk_resource_job_begin(struct rtvk_resource_base* base) {
	assert(base);
	rtvk_atomic_inc(&base->job_count);
}

void rtvk_resource_job_end(struct rtvk_resource_base* base) {
	assert(base);
	assert(rtvk_atomic_load(&base->job_count) > 0);
	rtvk_atomic_dec(&base->job_count);
	rtvk_resource_try_free(base);
}

void rtvk_resource_retire(struct rtvk_resource_base* base) {
	assert(base);
	rtvk_atomic_bool_store(&base->zombie, true);
	rtvk_resource_release(base);
}

void rtvk_resource_finalize(struct rtvk_resource_base* base) {
	assert(base);
	if (base->type == RT_RESOURCE_UNKNOWN) {
		rtvk_throwf(RT_IMPROPER_USAGE, "resource finalization reached RT_RESOURCE_UNKNOWN");
		return;
	}

	switch (base->type) {
	case RT_RESOURCE_BUFFER:
		rtvk_buffer_finish((struct rtvk_buffer*)base);
		break;
	case RT_RESOURCE_COMMAND_BUFFER:
		rtvk_command_buffer_finish((struct rtvk_command_buffer*)base);
		break;
	case RT_RESOURCE_FRAMEBUFFER:
		rtvk_framebuffer_finish((struct rtvk_framebuffer*)base);
		break;
	case RT_RESOURCE_GRAPHICS_PROGRAM:
		rtvk_graphics_program_finish((struct rtvk_graphics_program*)base);
		break;
	case RT_RESOURCE_SWAPCHAIN:
		rtvk_swapchain_finish((struct rtvk_swapchain*)base);
		break;
	case RT_RESOURCE_SWAPCHAIN_FRAME:
		rtvk_swapchain_frame_finish((struct rtvk_swapchain_frame*)base);
		break;
	case RT_RESOURCE_TEXTURE:
		rtvk_texture_finish((struct rtvk_texture*)base);
		break;
	case RT_RESOURCE_TEXTURE_VIEW:
		rtvk_texture_view_finish((struct rtvk_texture_view*)base);
		break;
	case RT_RESOURCE_QUEUE:
	case RT_RESOURCE_UNKNOWN:
		break;
	}
}

bool rtvk_resource_ready_to_destroy(struct rtvk_resource_base* base) {
	assert(base);
	return rtvk_atomic_bool_load(&base->zombie) &&
		   rtvk_atomic_load(&base->ref_count) == 0 &&
		   rtvk_atomic_load(&base->job_count) == 0;
}

rt_timepoint rtvk_timepoint_to_public(struct rtvk_timepoint timepoint) {
	rt_timepoint result = { (rt_queue)timepoint.queue, timepoint.value };
	return result;
}
