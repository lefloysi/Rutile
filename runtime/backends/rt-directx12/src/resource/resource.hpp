#pragma once

#include "atomic.hpp"
#include "types.hpp"

#include <cstddef>
#include <new>

struct rtdx_context;
struct rtdx_queue;

enum class rtdx_resource_type {
	unknown,
	buffer,
	command_buffer,
	framebuffer,
	graphics_program,
	queue,
	swapchain,
	texture,
	texture_view,
};

struct rtdx_resource_base {
	rtdx_resource_type type;
	rtdx_context* ctx;
	atomic_u32 ref_count;
	atomic_u32 job_count;
	atomic_bool zombie;
};

struct rtdx_timepoint {
	rtdx_queue* queue;
	u64 value;
};

void* rtdx_alloc_resource(usize size);
void rtdx_free_resource(void* resource);
void rtdx_init_resource_base(rtdx_context* ctx, rtdx_resource_base* base, rtdx_resource_type type);
void rtdx_finish_resource_base(rtdx_context* ctx, rtdx_resource_base* base);
void rtdx_resource_retain(rtdx_resource_base* base);
void rtdx_resource_release(rtdx_resource_base* base);
void rtdx_retain_resource_impl(void* resource);
void rtdx_release_resource_impl(void* resource);
void rtdx_resource_retire(rtdx_resource_base* base);
void rtdx_resource_finalize(rtdx_resource_base* base);
bool rtdx_resource_ready_to_destroy(rtdx_resource_base* base);
rt_timepoint rtdx_timepoint_to_public(rtdx_timepoint timepoint);

template <typename T>
inline T* rtdx_new_resource() {
	T* result = new (std::nothrow) T{};
	if (!result) {
		rtdx_alloc_resource(sizeof(T)); // reports OOM through error machinery
	}
	return result;
}

template <typename T>
inline void rtdx_delete_resource(T* resource) {
	delete resource;
}

#define RTDX_ALLOC_RESOURCE(type) (new (std::nothrow) type{})
#define RTDX_FREE_RESOURCE(resource) (delete (resource))
#define RTDX_RESOURCE_BASE(resource) (&(resource)->base)
#define rtdx_retain_resource(resource) rtdx_retain_resource_impl(resource)
#define rtdx_release_resource(resource) rtdx_release_resource_impl(resource)

#define RTDX_DECLARE_NEW_RESOURCE(type)                                                                            \
	inline rtdx_##type* rtdx_##type##_from_handle(rt_##type type) { return reinterpret_cast<rtdx_##type*>(type); } \
	inline rt_##type rtdx_##type##_to_handle(rtdx_##type* type) { return reinterpret_cast<rt_##type>(type); }      \
	rtdx_##type* rtdx_##type##_create(rtdx_context* ctx);                                                          \
	void rtdx_##type##_destroy(rtdx_context* ctx, rtdx_##type* type);                                              \
	void rtdx_##type##_init(rtdx_context* ctx, rtdx_##type* type);                                                 \
	void rtdx_##type##_finish(rtdx_context* ctx, rtdx_##type* type);

#define RTDX_DEFINE_RESOURCE_PRIVATE(type)                              \
	rtdx_##type* rtdx_##type##_create(rtdx_context* ctx) {              \
		rtdx_##type* type = rtdx_new_resource<rtdx_##type>();           \
		if (type) {                                                     \
			rtdx_##type##_init(ctx, type);                              \
		}                                                               \
		return type;                                                    \
	}                                                                   \
	void rtdx_##type##_destroy(rtdx_context*, rtdx_##type* type) {      \
		if (!type) {                                                    \
			return;                                                     \
		}                                                               \
		rtdx_resource_retire(RTDX_RESOURCE_BASE(type));                 \
	}
