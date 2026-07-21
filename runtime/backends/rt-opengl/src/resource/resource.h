#ifndef RTGL_RESOURCE_H
#define RTGL_RESOURCE_H

#include "types.h"

#include <stdbool.h>
#include <stddef.h>

struct rtgl_context;
struct rtgl_resource_base;

typedef enum rtgl_resource_type {
	RTGL_RESOURCE_UNKNOWN,
	RTGL_RESOURCE_BUFFER,
	RTGL_RESOURCE_COMMAND_BUFFER,
	RTGL_RESOURCE_FRAMEBUFFER,
	RTGL_RESOURCE_GRAPHICS_PROGRAM,
	RTGL_RESOURCE_QUEUE,
	RTGL_RESOURCE_SWAPCHAIN,
	RTGL_RESOURCE_TEXTURE,
	RTGL_RESOURCE_TEXTURE_VIEW,
} rtgl_resource_type;

struct rtgl_resource_base {
	rtgl_resource_type type;
	struct rtgl_context* ctx;
	u32 ref_count;
	u32 job_count;
	bool zombie;
};

struct rtgl_timepoint {
	struct rtgl_queue* queue;
	u64 value;
};

void* rtgl_alloc_resource(usize size);
void rtgl_init_resource_base(struct rtgl_context* ctx, struct rtgl_resource_base* base, rtgl_resource_type type);
void rtgl_finish_resource_base(struct rtgl_resource_base* base);
void rtgl_resource_retain(struct rtgl_resource_base* base);
void rtgl_resource_release(struct rtgl_resource_base* base);
void rtgl_resource_retire(struct rtgl_resource_base* base);
void rtgl_resource_finalize(struct rtgl_resource_base* base);
bool rtgl_resource_ready_to_destroy(struct rtgl_resource_base* base);
rt_timepoint rtgl_timepoint_to_public(struct rtgl_timepoint timepoint);

#define RTGL_ALLOC_RESOURCE(type) (type*)rtgl_alloc_resource(sizeof(type))
#define RTGL_RESOURCE_BASE(resource) (&(resource)->base)
#define rtgl_retain_resource(resource) rtgl_resource_retain((RTGL_RESOURCE_BASE(resource)))
#define rtgl_release_resource(resource)                            \
	do {                                                           \
		if (resource) {                                            \
			rtgl_resource_release((RTGL_RESOURCE_BASE(resource))); \
			(resource) = NULL;                                     \
		}                                                          \
	} while (0)

#define RTGL_DECLARE_NEW_RESOURCE(type)                                                                               \
	static inline struct rtgl_##type* rtgl_##type##_from_handle(rt_##type type) { return (struct rtgl_##type*)type; } \
	static inline rt_##type rtgl_##type##_to_handle(struct rtgl_##type* type) { return (rt_##type)type; }             \
	struct rtgl_##type* rtgl_##type##_create(struct rtgl_context* ctx);                                               \
	void rtgl_##type##_destroy(struct rtgl_context* ctx, struct rtgl_##type* type);                                   \
	void rtgl_##type##_init(struct rtgl_context* ctx, struct rtgl_##type* type);                                      \
	void rtgl_##type##_finish(struct rtgl_##type* type);

#define RTGL_DEFINE_RESOURCE_PRIVATE(type)                                           \
	struct rtgl_##type* rtgl_##type##_create(struct rtgl_context* ctx) {             \
		struct rtgl_##type* type = RTGL_ALLOC_RESOURCE(struct rtgl_##type);          \
		if (type) {                                                                  \
			rtgl_##type##_init(ctx, type);                                           \
		}                                                                            \
		return type;                                                                 \
	}                                                                                \
	void rtgl_##type##_destroy(struct rtgl_context* ctx, struct rtgl_##type* type) { \
		(void)ctx;                                                                   \
		if (!type) {                                                                 \
			return;                                                                  \
		}                                                                            \
		rtgl_resource_retire(RTGL_RESOURCE_BASE(type));                              \
	}

#endif /* RTGL_RESOURCE_H */
