#ifndef RTVK_RESOURCE_H
#define RTVK_RESOURCE_H

#include "atomic.h"
#include "types.h"

#include <stdbool.h>
#include <stddef.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtvk_context;
struct rtvk_resource_base;

typedef enum rtvk_resource_type {
	RT_RESOURCE_UNKNOWN,
	RT_RESOURCE_BUFFER,
	RT_RESOURCE_COMMAND_BUFFER,
	RT_RESOURCE_FRAMEBUFFER,
	RT_RESOURCE_GRAPHICS_PROGRAM,
	RT_RESOURCE_QUEUE,
	RT_RESOURCE_SWAPCHAIN,
	RT_RESOURCE_SWAPCHAIN_FRAME,
	RT_RESOURCE_TEXTURE,
	RT_RESOURCE_TEXTURE_VIEW,
} rtvk_resource_type;

struct rtvk_resource_base {
	rtvk_resource_type type;
	struct rtvk_context* ctx;
	u32 ref_count;
	u32 job_count;
	bool zombie;
	bool finalizing;
};

struct rtvk_timepoint {
	struct rtvk_queue* queue;
	u64 value;
};

void* rtvk_alloc_resource(usize size);
void rtvk_init_resource_base(struct rtvk_context* ctx, struct rtvk_resource_base* base, rtvk_resource_type type);
void rtvk_finish_resource_base(struct rtvk_resource_base* base);
void rtvk_resource_retain(struct rtvk_resource_base* base);
void rtvk_resource_release(struct rtvk_resource_base* base);
void rtvk_retain_resource_impl(void* resource);
void rtvk_release_resource_impl(void* resource);
void rtvk_resource_job_begin(struct rtvk_resource_base* base);
void rtvk_resource_job_end(struct rtvk_resource_base* base);
void rtvk_resource_retire(struct rtvk_resource_base* base);
void rtvk_resource_finalize(struct rtvk_resource_base* base);
bool rtvk_resource_ready_to_destroy(struct rtvk_resource_base* base);
rt_timepoint rtvk_timepoint_to_public(struct rtvk_timepoint timepoint);

#define RTVK_ALLOC_RESOURCE(type) (type*)rtvk_alloc_resource(sizeof(type))
#define RTVK_RESOURCE_BASE(resource) ((struct rtvk_resource_base*)&(resource)->base)
#define rtvk_retain_resource(resource) rtvk_resource_retain((RTVK_RESOURCE_BASE(resource)))
#define rtvk_release_resource(resource)                            \
	do {                                                           \
		if (resource) {                                            \
			rtvk_resource_release((RTVK_RESOURCE_BASE(resource))); \
			(resource) = NULL;                                     \
		}                                                          \
	} while (0)

#define RTVK_DECLARE_NEW_RESOURCE(type)                                                                               \
	static inline struct rtvk_##type* rtvk_##type##_from_handle(rt_##type type) { return (struct rtvk_##type*)type; } \
	static inline rt_##type rtvk_##type##_to_handle(struct rtvk_##type* type) { return (rt_##type)type; }             \
	struct rtvk_##type* rtvk_##type##_create(struct rtvk_context* ctx);                                               \
	void rtvk_##type##_destroy(struct rtvk_context* ctx, struct rtvk_##type* type);                                   \
	void rtvk_##type##_init(struct rtvk_context* ctx, struct rtvk_##type* type);                                      \
	void rtvk_##type##_finish(struct rtvk_##type* type);

#define RTVK_DEFINE_RESOURCE_PRIVATE(type)                                           \
	struct rtvk_##type* rtvk_##type##_create(struct rtvk_context* ctx) {             \
		struct rtvk_##type* type = RTVK_ALLOC_RESOURCE(struct rtvk_##type);          \
		if (type) {                                                                  \
			rtvk_##type##_init(ctx, type);                                           \
		}                                                                            \
		return type;                                                                 \
	}                                                                                \
	void rtvk_##type##_destroy(struct rtvk_context* ctx, struct rtvk_##type* type) { \
		(void)ctx;                                                                   \
		if (!type) {                                                                 \
			return;                                                                  \
		}                                                                            \
		rtvk_resource_retire(RTVK_RESOURCE_BASE(type));                              \
	}

#endif
