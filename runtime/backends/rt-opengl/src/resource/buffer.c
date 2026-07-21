#include "buffer.h"

#include "context.h"
#include "error.h"

#include <assert.h>

rt_buffer rtBufferCreate(void) {
	return rtgl_buffer_to_handle(rtgl_buffer_create(rtgl_get_current_context()));
}

void rtBufferDestroy(rt_buffer buffer) {
	rtgl_buffer_destroy(rtgl_get_current_context(), rtgl_buffer_from_handle(buffer));
}

rt_timepoint rtBufferData(rt_buffer buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data) {
	return rtgl_timepoint_to_public(rtgl_buffer_data(rtgl_get_current_context(), rtgl_buffer_from_handle(buffer), mode, usage, size, data));
}

rt_timepoint rtBufferSubdata(rt_buffer buffer, u64 offset, u64 size, const void* data) {
	return rtgl_timepoint_to_public(rtgl_buffer_subdata(rtgl_get_current_context(), rtgl_buffer_from_handle(buffer), offset, size, data));
}

void rtBufferRead(rt_buffer buffer, u64 offset, u64 size, void* data) {
	rtgl_buffer_read(rtgl_get_current_context(), rtgl_buffer_from_handle(buffer), offset, size, data);
}

RTGL_DEFINE_RESOURCE_PRIVATE(buffer)

static GLenum rtgl_buffer_gl_usage(enum rt_buffer_mode mode) {
	return mode == RT_BUFFER_STATIC ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW;
}

static bool rtgl_buffer_uses_host_storage(enum rt_buffer_mode mode, enum rt_buffer_usage usage) {
	return mode == RT_BUFFER_DYNAMIC || (usage & RT_BUFFER_USAGE_STAGING) != 0;
}

static bool rtgl_buffer_make_current(struct rtgl_context* ctx) {
	if (!ctx || !ctx->gl_context) {
		rtgl_throwf(RT_IMPROPER_USAGE, "OpenGL buffer operation requires an initialized context");
		return false;
	}
	rtgl_make_glcontext_current(ctx->gl_context, NULL);
	return true;
}

void rtgl_buffer_init(struct rtgl_context* ctx, struct rtgl_buffer* buffer) {
	assert(ctx);
	assert(buffer);
	rtgl_init_resource_base(ctx, RTGL_RESOURCE_BASE(buffer), RTGL_RESOURCE_BUFFER);
	buffer->mode = RT_BUFFER_DYNAMIC;
	buffer->usage = (enum rt_buffer_usage)(RT_BUFFER_USAGE_VERTEX | RT_BUFFER_USAGE_INDEX | RT_BUFFER_USAGE_UNIFORM | RT_BUFFER_USAGE_STORAGE | RT_BUFFER_USAGE_TRANSFER_SRC | RT_BUFFER_USAGE_TRANSFER_DST);
	if (!rtgl_buffer_make_current(ctx)) {
		return;
	}
	glCreateBuffers(1, &buffer->gl_buffer);
	rtgl_release_current_context();
}

void rtgl_buffer_finish(struct rtgl_buffer* buffer) {
	struct rtgl_context* ctx = buffer->base.ctx;
	if (ctx && ctx->gl_context && buffer->gl_buffer) {
		rtgl_make_glcontext_current(ctx->gl_context, NULL);
		glDeleteBuffers(1, &buffer->gl_buffer);
		rtgl_release_current_context();
		buffer->gl_buffer = 0;
	}
	rtgl_finish_resource_base(RTGL_RESOURCE_BASE(buffer));
}

struct rtgl_timepoint rtgl_buffer_data(struct rtgl_context* ctx, struct rtgl_buffer* buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data) {
	struct rtgl_timepoint timepoint = { NULL, 0 };
	if (!buffer || !buffer->gl_buffer) {
		rtgl_throwf(RT_IMPROPER_USAGE, "buffer is NULL");
		return timepoint;
	}
	if (mode != RT_BUFFER_STATIC && mode != RT_BUFFER_DYNAMIC) {
		rtgl_throwf(RT_IMPROPER_USAGE, "unsupported buffer mode");
		return timepoint;
	}
	if (usage == RT_BUFFER_USAGE_NONE) {
		rtgl_throwf(RT_IMPROPER_USAGE, "buffer usage must not be empty");
		return timepoint;
	}
	if (!rtgl_buffer_make_current(ctx)) {
		return timepoint;
	}
	buffer->mode = mode;
	buffer->usage = usage;
	buffer->size = size;
	glNamedBufferData(buffer->gl_buffer, (GLsizeiptr)size, data, rtgl_buffer_gl_usage(mode));
	rtgl_release_current_context();
	return timepoint;
}

struct rtgl_timepoint rtgl_buffer_subdata(struct rtgl_context* ctx, struct rtgl_buffer* buffer, u64 offset, u64 size, const void* data) {
	struct rtgl_timepoint timepoint = { NULL, 0 };
	if (!buffer || !buffer->gl_buffer) {
		rtgl_throwf(RT_IMPROPER_USAGE, "buffer has no storage");
		return timepoint;
	}
	if (!data && size) {
		rtgl_throwf(RT_IMPROPER_USAGE, "buffer subdata source is NULL");
		return timepoint;
	}
	if (offset > buffer->size || size > buffer->size - offset) {
		rtgl_throwf(RT_IMPROPER_USAGE, "buffer subdata range is out of bounds");
		return timepoint;
	}
	if (!size) {
		return timepoint;
	}
	if (!rtgl_buffer_make_current(ctx)) {
		return timepoint;
	}
	glNamedBufferSubData(buffer->gl_buffer, (GLintptr)offset, (GLsizeiptr)size, data);
	rtgl_release_current_context();
	return timepoint;
}

void rtgl_buffer_read(struct rtgl_context* ctx, struct rtgl_buffer* buffer, u64 offset, u64 size, void* data) {
	if (!buffer || !buffer->gl_buffer) {
		rtgl_throwf(RT_IMPROPER_USAGE, "buffer has no storage");
		return;
	}
	if (!data && size) {
		rtgl_throwf(RT_IMPROPER_USAGE, "buffer read destination is NULL");
		return;
	}
	if (offset > buffer->size || size > buffer->size - offset) {
		rtgl_throwf(RT_IMPROPER_USAGE, "buffer read range is out of bounds");
		return;
	}
	if (!rtgl_buffer_uses_host_storage(buffer->mode, buffer->usage)) {
		rtgl_throwf(RT_IMPROPER_USAGE, "buffer has no CPU-readable storage");
		return;
	}
	if (!size) {
		return;
	}
	if (!rtgl_buffer_make_current(ctx)) {
		return;
	}
	glGetNamedBufferSubData(buffer->gl_buffer, (GLintptr)offset, (GLsizeiptr)size, data);
	rtgl_release_current_context();
}
