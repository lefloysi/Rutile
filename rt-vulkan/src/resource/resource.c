#include "resource.h"
#include "buffer.h"
#include "command_buffer.h"
#include "compute_program.h"
#include "error.h"
#include "framebuffer.h"
#include "graphics_program.h"
#include "texture.h"
#include "resource/swapchain.h"
#include "error.h"

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

void rtvk_free_resource(void* resource) {
	free(resource);
}

void rtvk_init_resource_base(struct rtvk_context* ctx, struct rtvk_resource_base* base, rtvk_resource_type type) {
	assert(base);
	base->type = type;
	base->ctx = ctx;
	base->ref_count = 1;
	base->job_count = 0;
	base->zombie = false;
}

void rtvk_finish_resource_base(struct rtvk_context* ctx, struct rtvk_resource_base* base) {
	assert(base);
	base->ctx = NULL;
	base->zombie = true;
}
static void rtvk_resource_try_free(struct rtvk_resource_base* base) {
	assert(base);
	if (rtvk_resource_ready_to_destroy(base)) {
		rtvk_resource_finalize(base);
		rtvk_free_resource(base);
	}
}

void rtvk_resource_retain(struct rtvk_resource_base* base) {
	assert(base);
	base->ref_count++;
}

void rtvk_resource_release(struct rtvk_resource_base* base) {
	assert(base);
	assert(base->ref_count > 0);
	base->ref_count--;
	rtvk_resource_try_free(base);
}

void rtvk_resource_job_begin(struct rtvk_resource_base* base) {
	assert(base);
	base->job_count++;
}

void rtvk_resource_job_end(struct rtvk_resource_base* base) {
	assert(base);
	assert(base->job_count > 0);
	base->job_count--;
	rtvk_resource_try_free(base);
}

void rtvk_resource_retire(struct rtvk_resource_base* base) {
	assert(base);
	base->zombie = true;
	rtvk_resource_release(base);
}

void rtvk_resource_finalize(struct rtvk_resource_base* base) {
	assert(base);
	if (base->type == RT_RESOURCE_UNKNOWN) {
		rtvk_throwf(RT_IMPROPER_USAGE, "resource finalization reached RT_RESOURCE_UNKNOWN");
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
	assert(base);
	return base->zombie && base->ref_count == 0 && base->job_count == 0;
}

rt_timepoint rtvk_timepoint_to_public(struct rtvk_timepoint timepoint) {
	rt_timepoint result = { (rt_queue)timepoint.queue, timepoint.value };
	return result;
}


