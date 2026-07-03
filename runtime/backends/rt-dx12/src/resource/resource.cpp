#include "resource.hpp"
#include "buffer.hpp"
#include "command_buffer.hpp"
#include "error.hpp"
#include "framebuffer.hpp"
#include "graphics_program.hpp"
#include "resource/swapchain.hpp"
#include "texture.hpp"

void* rtdx_alloc_resource(usize size) {
	rtdx_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for graphics resource", size);
	return nullptr;
}

void rtdx_free_resource(void* /*resource*/) {
}

void rtdx_init_resource_base(rtdx_context* ctx, rtdx_resource_base* base, rtdx_resource_type type) {
	base->type = type;
	base->ctx = ctx;
	rtdx_atomic_u32_init(&base->ref_count, 1);
	rtdx_atomic_u32_init(&base->job_count, 0);
	rtdx_atomic_bool_init(&base->zombie, false);
}

void rtdx_finish_resource_base(rtdx_context* /*ctx*/, rtdx_resource_base* base) {
	base->type = rtdx_resource_type::unknown;
	base->ctx = nullptr;
	rtdx_atomic_bool_store(&base->zombie, true);
}

void rtdx_resource_finalize(rtdx_resource_base* base) {
	if (!base || !base->ctx || base->type == rtdx_resource_type::unknown) {
		return;
	}

	rtdx_context* ctx = base->ctx;
	switch (base->type) {
	case rtdx_resource_type::buffer:
		rtdx_buffer_finish(ctx, static_cast<rtdx_buffer*>(static_cast<void*>(base)));
		break;
	case rtdx_resource_type::command_buffer:
		rtdx_command_buffer_finish(ctx, static_cast<rtdx_command_buffer*>(static_cast<void*>(base)));
		break;
	case rtdx_resource_type::framebuffer:
		rtdx_framebuffer_finish(ctx, static_cast<rtdx_framebuffer*>(static_cast<void*>(base)));
		break;
	case rtdx_resource_type::graphics_program:
		rtdx_graphics_program_finish(ctx, static_cast<rtdx_graphics_program*>(static_cast<void*>(base)));
		break;
	case rtdx_resource_type::swapchain:
		rtdx_swapchain_finish(ctx, static_cast<rtdx_swapchain*>(static_cast<void*>(base)));
		break;
	case rtdx_resource_type::texture:
		rtdx_texture_finish(ctx, static_cast<rtdx_texture*>(static_cast<void*>(base)));
		break;
	case rtdx_resource_type::texture_view:
		rtdx_texture_view_finish(ctx, static_cast<rtdx_texture_view*>(static_cast<void*>(base)));
		break;
	case rtdx_resource_type::queue:
	case rtdx_resource_type::unknown:
		break;
	}
}

static void rtdx_resource_delete(rtdx_resource_base* base, rtdx_resource_type type) {
	switch (type) {
	case rtdx_resource_type::buffer:
		delete static_cast<rtdx_buffer*>(static_cast<void*>(base));
		break;
	case rtdx_resource_type::command_buffer:
		delete static_cast<rtdx_command_buffer*>(static_cast<void*>(base));
		break;
	case rtdx_resource_type::framebuffer:
		delete static_cast<rtdx_framebuffer*>(static_cast<void*>(base));
		break;
	case rtdx_resource_type::graphics_program:
		delete static_cast<rtdx_graphics_program*>(static_cast<void*>(base));
		break;
	case rtdx_resource_type::swapchain:
		delete static_cast<rtdx_swapchain*>(static_cast<void*>(base));
		break;
	case rtdx_resource_type::texture:
		delete static_cast<rtdx_texture*>(static_cast<void*>(base));
		break;
	case rtdx_resource_type::texture_view:
		delete static_cast<rtdx_texture_view*>(static_cast<void*>(base));
		break;
	case rtdx_resource_type::queue:
		delete static_cast<rtdx_queue*>(static_cast<void*>(base));
		break;
	case rtdx_resource_type::unknown:
		break;
	}
}

static void rtdx_resource_try_free(rtdx_resource_base* base) {
	if (rtdx_resource_ready_to_destroy(base)) {
		// Snapshot the type before finalize wipes it.
		rtdx_resource_type type = base->type;
		rtdx_resource_finalize(base);
		rtdx_atomic_bool_finish(&base->zombie);
		rtdx_atomic_u32_finish(&base->job_count);
		rtdx_atomic_u32_finish(&base->ref_count);
		rtdx_resource_delete(base, type);
	}
}

void rtdx_resource_retain(rtdx_resource_base* base) {
	rtdx_atomic_inc(&base->ref_count);
}

void rtdx_resource_release(rtdx_resource_base* base) {
	rtdx_atomic_dec(&base->ref_count);
	rtdx_resource_try_free(base);
}

void rtdx_retain_resource_impl(void* resource) {
	if (resource) {
		rtdx_resource_retain(static_cast<rtdx_resource_base*>(resource));
	}
}

void rtdx_release_resource_impl(void* resource) {
	if (resource) {
		rtdx_resource_release(static_cast<rtdx_resource_base*>(resource));
	}
}

void rtdx_resource_retire(rtdx_resource_base* base) {
	rtdx_atomic_bool_store(&base->zombie, true);
	rtdx_resource_release(base);
}

bool rtdx_resource_ready_to_destroy(rtdx_resource_base* base) {
	return rtdx_atomic_bool_load(&base->zombie) &&
		   rtdx_atomic_load(&base->ref_count) == 0 &&
		   rtdx_atomic_load(&base->job_count) == 0;
}

rt_timepoint rtdx_timepoint_to_public(rtdx_timepoint timepoint) {
	return rt_timepoint{ reinterpret_cast<rt_queue>(timepoint.queue), timepoint.value };
}
