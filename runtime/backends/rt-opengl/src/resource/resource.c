#include "resource.h"

#include "buffer.h"
#include "error.h"

#include <assert.h>
#include <stdlib.h>

void* rtgl_alloc_resource(usize size) {
	void* resource = calloc(1, size);
	if (!resource) {
		rtgl_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for graphics resource", size);
	}
	return resource;
}

void rtgl_init_resource_base(struct rtgl_context* ctx, struct rtgl_resource_base* base, rtgl_resource_type type) {
	assert(base);
	base->type = type;
	base->ctx = ctx;
	base->ref_count = 1;
	base->job_count = 0;
	base->zombie = false;
}

void rtgl_finish_resource_base(struct rtgl_resource_base* base) {
	assert(base);
	base->ctx = NULL;
	base->zombie = true;
}

static void rtgl_resource_try_free(struct rtgl_resource_base* base) {
	if (rtgl_resource_ready_to_destroy(base)) {
		rtgl_resource_finalize(base);
		free(base);
	}
}

void rtgl_resource_retain(struct rtgl_resource_base* base) {
	assert(base);
	base->ref_count++;
}

void rtgl_resource_release(struct rtgl_resource_base* base) {
	assert(base);
	assert(base->ref_count > 0);
	base->ref_count--;
	rtgl_resource_try_free(base);
}

void rtgl_resource_retire(struct rtgl_resource_base* base) {
	assert(base);
	base->zombie = true;
	rtgl_resource_release(base);
}

void rtgl_resource_finalize(struct rtgl_resource_base* base) {
	assert(base);
	switch (base->type) {
	case RTGL_RESOURCE_BUFFER:
		rtgl_buffer_finish((struct rtgl_buffer*)base);
		break;
	case RTGL_RESOURCE_COMMAND_BUFFER:
	case RTGL_RESOURCE_FRAMEBUFFER:
	case RTGL_RESOURCE_GRAPHICS_PROGRAM:
	case RTGL_RESOURCE_QUEUE:
	case RTGL_RESOURCE_SWAPCHAIN:
	case RTGL_RESOURCE_TEXTURE:
	case RTGL_RESOURCE_TEXTURE_VIEW:
	case RTGL_RESOURCE_UNKNOWN:
		break;
	}
}

bool rtgl_resource_ready_to_destroy(struct rtgl_resource_base* base) {
	assert(base);
	return base->zombie && base->ref_count == 0 && base->job_count == 0;
}

rt_timepoint rtgl_timepoint_to_public(struct rtgl_timepoint timepoint) {
	rt_timepoint result = { (rt_queue)timepoint.queue, timepoint.value };
	return result;
}
