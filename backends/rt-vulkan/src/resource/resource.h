#ifndef RTVK_RESOURCE_H
#define RTVK_RESOURCE_H

#include "atomic.h"
#define RT_TYPES_ONLY
#include "rutile.h"
#undef RT_TYPES_ONLY

#include <stddef.h>
#include <stdlib.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtvk_context;
struct rtvk_resource_base;
struct rtvk_queue;

typedef void (*rtvk_resource_destroy_proc)(void* resource);

struct rtvk_resource_base {
	struct rtvk_context* ctx;
	void* resource;
	rtvk_resource_destroy_proc destroy;
	u32 ref_count;
	u32 job_count;
	bool zombie;
	bool finalizing;
};

struct rtvk_resource_job {
	struct rtvk_resource_base* resource;
};

void* rtvk_alloc_resource(usize size);
usize rtvk_bit_ceil(usize value);
void rtvk_init_resource_base(struct rtvk_context* ctx, struct rtvk_resource_base* base, void* resource, rtvk_resource_destroy_proc destroy);
void rtvk_resource_retain(struct rtvk_resource_base* base);
void rtvk_resource_release(struct rtvk_resource_base* base);
void rtvk_retain_resource_impl(void* resource);
void rtvk_release_resource_impl(void* resource);
void rtvk_resource_job_begin(struct rtvk_resource_base* base);
void rtvk_resource_job_end(struct rtvk_resource_base* base);
void rtvk_resource_retire(struct rtvk_resource_base* base);
bool rtvk_resource_ready_to_destroy(struct rtvk_resource_base* base);
rt_timepoint rtvk_timepoint_make(struct rtvk_queue* queue, u64 value);
struct rtvk_queue* rtvk_timepoint_queue(struct rtvk_context* ctx, rt_timepoint timepoint);
u64 rtvk_timepoint_value(rt_timepoint timepoint);

#define RTVK_ALLOC_RESOURCE(type) (type*)rtvk_alloc_resource(sizeof(type))
#define RTVK_RESOURCE_BASE(resource) ((struct rtvk_resource_base*)&(resource)->base)
#define rtvk_retain_resource(resource)                            \
	do {                                                          \
		if (resource) {                                           \
			rtvk_resource_retain((RTVK_RESOURCE_BASE(resource))); \
		}                                                         \
	} while (0)
#define rtvk_release_resource(resource)                            \
	do {                                                           \
		if (resource) {                                            \
			rtvk_resource_release((RTVK_RESOURCE_BASE(resource))); \
			(resource) = NULL;                                     \
		}                                                          \
	} while (0)

#define RTVK_DECLARE_HANDLE(handle, type)                         \
	struct type* rtvk_##handle##_from_handle(rt_##handle handle); \
	rt_##handle rtvk_##handle##_to_handle(struct type* handle);

#define RTVK_DEFINE_HANDLE(handle, type)                           \
	struct type* rtvk_##handle##_from_handle(rt_##handle handle) { \
		return (struct type*)handle;                               \
	}                                                              \
	rt_##handle rtvk_##handle##_to_handle(struct type* handle) {   \
		return (rt_##handle)handle;                                \
	}

#define RTVK_DECLARE_NEW_RESOURCE(type)                                             \
	RTVK_DECLARE_HANDLE(type, rtvk_##type)                                          \
	struct rtvk_##type* rtvk_##type##_create(struct rtvk_context* ctx);             \
	void rtvk_##type##_destroy(struct rtvk_context* ctx, struct rtvk_##type* type); \
	void rtvk_##type##_init(struct rtvk_context* ctx, struct rtvk_##type* type);    \
	void rtvk_##type##_finish(struct rtvk_##type* type);

#define RTVK_DEFINE_RESOURCE_FINALIZER(type)                   \
	static void rtvk_##type##_finalize_resource(void* value) { \
		struct rtvk_##type* resource = value;                  \
		rtvk_##type##_finish(resource);                        \
		free(resource);                                        \
	}

#define RTVK_DEFINE_RESOURCE_PRIVATE(type)                                               \
	RTVK_DEFINE_RESOURCE_FINALIZER(type)                                                 \
	RTVK_DEFINE_HANDLE(type, rtvk_##type)                                                \
	struct rtvk_##type* rtvk_##type##_create(struct rtvk_context* ctx) {                 \
		struct rtvk_##type* resource = RTVK_ALLOC_RESOURCE(struct rtvk_##type);          \
		if (resource) {                                                                  \
			rtvk_##type##_init(ctx, resource);                                           \
		}                                                                                \
		return resource;                                                                 \
	}                                                                                    \
	void rtvk_##type##_destroy(struct rtvk_context* ctx, struct rtvk_##type* resource) { \
		(void)ctx;                                                                       \
		if (!resource) {                                                                 \
			return;                                                                      \
		}                                                                                \
		rtvk_resource_retire(RTVK_RESOURCE_BASE(resource));                              \
	}

#endif
