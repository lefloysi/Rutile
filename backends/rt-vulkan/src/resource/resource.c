#include "resource.h"
#include "context.h"
#include "error.h"
#include "queue.h"

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

void rtvk_init_resource_base(struct rtvk_context* ctx, struct rtvk_resource_base* base, void* resource, rtvk_resource_destroy_proc destroy) {
	assert(base);
	assert(resource);
	assert(destroy);
	base->ctx = ctx;
	base->resource = resource;
	base->destroy = destroy;
	base->ref_count = 1;
	base->job_count = 0;
	base->zombie = false;
	base->finalizing = false;
}

void rtvk_resource_try_free(struct rtvk_resource_base* base) {
	if (rtvk_resource_ready_to_destroy(base) && !rtvk_atomic_bool_exchange(&base->finalizing, true)) {
		rtvk_resource_destroy_proc destroy = base->destroy;
		void* resource = base->resource;
		assert(destroy);
		assert(resource);
		destroy(resource);
	}
}

void rtvk_resource_retain(struct rtvk_resource_base* base) {
	assert(base);
	rtvk_atomic_inc(&base->ref_count);
}

void rtvk_resource_release(struct rtvk_resource_base* base) {
	assert(base);
	assert(rtvk_atomic_load(&base->ref_count) > 0);
	if (rtvk_atomic_dec(&base->ref_count) == 0) {
		rtvk_resource_try_free(base);
	}
}

void rtvk_resource_job_begin(struct rtvk_resource_base* base) {
	assert(base);
	rtvk_resource_retain(base);
	rtvk_atomic_inc(&base->job_count);
}

void rtvk_resource_job_end(struct rtvk_resource_base* base) {
	assert(base);
	assert(rtvk_atomic_load(&base->job_count) > 0);
	rtvk_atomic_dec(&base->job_count);
	rtvk_resource_release(base);
}

void rtvk_resource_retire(struct rtvk_resource_base* base) {
	assert(base);
	rtvk_atomic_bool_store(&base->zombie, true);
	rtvk_resource_release(base);
}

bool rtvk_resource_ready_to_destroy(struct rtvk_resource_base* base) {
	assert(base);
	return rtvk_atomic_bool_load(&base->zombie) &&
		   rtvk_atomic_load(&base->ref_count) == 0 &&
		   rtvk_atomic_load(&base->job_count) == 0;
}

rt_timepoint rtvk_timepoint_make(struct rtvk_queue* queue, u64 value) {
	if (!queue || value == 0) {
		return (rt_timepoint){ 0 };
	}
	return (rt_timepoint){ ((u64)queue->timepoint_id << 56) | value };
}

struct rtvk_queue* rtvk_timepoint_queue(struct rtvk_context* ctx, rt_timepoint timepoint) {
	u32 id = (u32)(timepoint.value >> 56);
	u64 value = timepoint.value & UINT64_C(0x00FFFFFFFFFFFFFF);
	if (!id || !value || id > ctx->queue_count) {
		return NULL;
	}
	return ctx->queues[id - 1];
}

u64 rtvk_timepoint_value(rt_timepoint timepoint) {
	return timepoint.value & UINT64_C(0x00FFFFFFFFFFFFFF);
}
