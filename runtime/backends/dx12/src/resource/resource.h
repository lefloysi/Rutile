#ifndef RTDX_RESOURCE_H
#define RTDX_RESOURCE_H

#include "atomic.h"
#include "types.h"

#include <stdbool.h>
#include <stddef.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtdx_context;
struct rtdx_queue;

typedef enum rtdx_resource_type {
	RT_RESOURCE_UNKNOWN,
	RT_RESOURCE_BUFFER,
	RT_RESOURCE_COMMAND_BUFFER,
	RT_RESOURCE_FRAMEBUFFER,
	RT_RESOURCE_GRAPHICS_PROGRAM,
	RT_RESOURCE_QUEUE,
	RT_RESOURCE_SWAPCHAIN,
	RT_RESOURCE_TEXTURE,
	RT_RESOURCE_TEXTURE_VIEW,
} rtdx_resource_type;

struct rtdx_resource_base {
	rtdx_resource_type type;
	struct rtdx_context *ctx;
	atomic_u32 ref_count;
	atomic_u32 job_count;
	atomic_bool zombie;
};

struct rtdx_timepoint {
	struct rtdx_queue *queue;
	u64 value;
};

void *rtdx_alloc_resource(usize size);
void rtdx_free_resource(void *resource);
void rtdx_init_resource_base(struct rtdx_context *ctx, struct rtdx_resource_base *base, rtdx_resource_type type);
void rtdx_finish_resource_base(struct rtdx_context *ctx, struct rtdx_resource_base *base);
void rtdx_resource_retain(struct rtdx_resource_base *base);
void rtdx_resource_release(struct rtdx_resource_base *base);
void rtdx_retain_resource_impl(void *resource);
void rtdx_release_resource_impl(void *resource);
void rtdx_resource_retire(struct rtdx_resource_base *base);
void rtdx_resource_finalize(struct rtdx_resource_base *base);
bool rtdx_resource_ready_to_destroy(struct rtdx_resource_base *base);
rt_timepoint rtdx_timepoint_to_public(struct rtdx_timepoint timepoint);

#define RTDX_ALLOC_RESOURCE(type) (type *)rtdx_alloc_resource(sizeof(type))
#define RTDX_FREE_RESOURCE(resource) rtdx_free_resource(resource)
#define RTDX_RESOURCE_BASE(resource) (&(resource)->base)
#define rtdx_retain_resource(resource) rtdx_retain_resource_impl(resource)
#define rtdx_release_resource(resource) rtdx_release_resource_impl(resource)

#define RTDX_DECLARE_NEW_RESOURCE(type)                                                                                \
	static inline struct rtdx_##type *rtdx_##type##_from_handle(rt_##type type) { return (struct rtdx_##type *)type; } \
	static inline rt_##type rtdx_##type##_to_handle(struct rtdx_##type *type) { return (rt_##type)type; }              \
	struct rtdx_##type *rtdx_##type##_create(struct rtdx_context *ctx);                                                \
	void rtdx_##type##_destroy(struct rtdx_context *ctx, struct rtdx_##type *type);                                    \
	void rtdx_##type##_init(struct rtdx_context *ctx, struct rtdx_##type *type);                                       \
	void rtdx_##type##_finish(struct rtdx_context *ctx, struct rtdx_##type *type);

#define RTDX_DEFINE_RESOURCE_PRIVATE(type)                                           \
	struct rtdx_##type *rtdx_##type##_create(struct rtdx_context *ctx) {             \
		struct rtdx_##type *type = RTDX_ALLOC_RESOURCE(struct rtdx_##type);          \
		if (type) {                                                                  \
			rtdx_##type##_init(ctx, type);                                           \
		}                                                                            \
		return type;                                                                 \
	}                                                                                \
	void rtdx_##type##_destroy(struct rtdx_context *ctx, struct rtdx_##type *type) { \
		(void)ctx;                                                                   \
		if (!type) {                                                                 \
			return;                                                                  \
		}                                                                            \
		rtdx_resource_retire(RTDX_RESOURCE_BASE(type));                              \
	}

#endif /* RTDX_RESOURCE_H */
