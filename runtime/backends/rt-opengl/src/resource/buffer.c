#include "buffer.h"

#include "context.h"
#include "error.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

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

static GLenum rtgl_buffer_gl_usage(enum rt_buffer_mode mode) {
	return mode == RT_BUFFER_STATIC ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW;
}

static bool rtgl_buffer_uses_host_storage(enum rt_buffer_mode mode, enum rt_buffer_usage usage) {
	return mode == RT_BUFFER_DYNAMIC || (usage & RT_BUFFER_USAGE_STAGING) != 0;
}

void rtgl_buffer_init(struct rtgl_context* ctx, struct rtgl_buffer* buffer) {
	assert(ctx);
	assert(buffer);
	rtgl_init_resource_base(ctx, RTGL_RESOURCE_BASE(buffer), RTGL_RESOURCE_BUFFER);
	buffer->mode = RT_BUFFER_DYNAMIC;
	buffer->usage = (enum rt_buffer_usage)(RT_BUFFER_USAGE_VERTEX | RT_BUFFER_USAGE_INDEX | RT_BUFFER_USAGE_UNIFORM | RT_BUFFER_USAGE_STORAGE | RT_BUFFER_USAGE_TRANSFER_SRC | RT_BUFFER_USAGE_TRANSFER_DST);
}

static void rtgl_buffer_exec_create(struct rtgl_context* ctx, void* data) {
	struct rtgl_buffer* buffer = (struct rtgl_buffer*)data;
	(void)ctx;
	glCreateBuffers(1, &buffer->gl_buffer);
}

struct rtgl_buffer* rtgl_buffer_create(struct rtgl_context* ctx) {
	struct rtgl_buffer* buffer = RTGL_ALLOC_RESOURCE(struct rtgl_buffer);
	if (!buffer) {
		return NULL;
	}
	rtgl_buffer_init(ctx, buffer);
	rtgl_context_execute(ctx, rtgl_buffer_exec_create, buffer);
	return buffer;
}

typedef struct rtgl_buffer_delete_job {
	struct rtgl_buffer* buffer;
	GLuint gl_buffer;
} rtgl_buffer_delete_job;

static void rtgl_buffer_exec_delete(struct rtgl_context* ctx, void* data) {
	rtgl_buffer_delete_job* job = (rtgl_buffer_delete_job*)data;
	(void)ctx;
	if (job->gl_buffer) {
		glDeleteBuffers(1, &job->gl_buffer);
	}
	rtgl_finish_resource_base(RTGL_RESOURCE_BASE(job->buffer));
	free(job->buffer);
	free(job);
}

void rtgl_buffer_destroy(struct rtgl_context* ctx, struct rtgl_buffer* buffer) {
	rtgl_buffer_delete_job* job;

	(void)ctx;
	if (!buffer) {
		return;
	}
	buffer->base.zombie = true;
	job = calloc(1, sizeof(*job));
	RTGL_CHECK_ALLOC(job, sizeof(*job), "OpenGL buffer delete job");
	if (!job) {
		return;
	}
	job->buffer = buffer;
	job->gl_buffer = buffer->gl_buffer;
	buffer->gl_buffer = 0;
	rtgl_context_enqueue(buffer->base.ctx, rtgl_buffer_exec_delete, job);
}

void rtgl_buffer_finish(struct rtgl_buffer* buffer) {
	rtgl_finish_resource_base(RTGL_RESOURCE_BASE(buffer));
}

static bool rtgl_buffer_validate_data(struct rtgl_buffer* buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage) {
	if (!buffer || buffer->base.zombie) {
		rtgl_throwf(RT_IMPROPER_USAGE, "buffer is NULL");
		return false;
	}
	if (mode != RT_BUFFER_STATIC && mode != RT_BUFFER_DYNAMIC) {
		rtgl_throwf(RT_IMPROPER_USAGE, "unsupported buffer mode");
		return false;
	}
	if (usage == RT_BUFFER_USAGE_NONE) {
		rtgl_throwf(RT_IMPROPER_USAGE, "buffer usage must not be empty");
		return false;
	}
	return true;
}

typedef struct rtgl_buffer_data_job {
	struct rtgl_buffer* buffer;
	enum rt_buffer_mode mode;
	enum rt_buffer_usage usage;
	u64 size;
	void* data;
} rtgl_buffer_data_job;

static void rtgl_buffer_exec_data(struct rtgl_context* ctx, void* data) {
	rtgl_buffer_data_job* job = (rtgl_buffer_data_job*)data;
	(void)ctx;
	glNamedBufferData(job->buffer->gl_buffer, (GLsizeiptr)job->size, job->data, rtgl_buffer_gl_usage(job->mode));
}

struct rtgl_timepoint rtgl_buffer_data(struct rtgl_context* ctx, struct rtgl_buffer* buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data) {
	struct rtgl_timepoint timepoint = { NULL, 0 };
	rtgl_buffer_data_job job;

	if (!rtgl_buffer_validate_data(buffer, mode, usage)) {
		return timepoint;
	}
	if (size && !data) {
		rtgl_throwf(RT_IMPROPER_USAGE, "buffer data source is NULL");
		return timepoint;
	}
	if (size > (u64)SIZE_MAX) {
		rtgl_throwf(RT_IMPROPER_USAGE, "buffer data size is too large");
		return timepoint;
	}
	buffer->mode = mode;
	buffer->usage = usage;
	buffer->size = size;
	job.buffer = buffer;
	job.mode = mode;
	job.usage = usage;
	job.size = size;
	job.data = NULL;
	if (size) {
		job.data = malloc((size_t)size);
		RTGL_CHECK_ALLOC(job.data, (usize)size, "OpenGL buffer data upload");
		if (!job.data) {
			return timepoint;
		}
		memcpy(job.data, data, (size_t)size);
	}
	rtgl_context_execute(ctx, rtgl_buffer_exec_data, &job);
	free(job.data);
	return timepoint;
}

typedef struct rtgl_buffer_subdata_job {
	struct rtgl_buffer* buffer;
	u64 offset;
	u64 size;
	void* data;
} rtgl_buffer_subdata_job;

static void rtgl_buffer_exec_subdata(struct rtgl_context* ctx, void* data) {
	rtgl_buffer_subdata_job* job = (rtgl_buffer_subdata_job*)data;
	(void)ctx;
	glNamedBufferSubData(job->buffer->gl_buffer, (GLintptr)job->offset, (GLsizeiptr)job->size, job->data);
}

struct rtgl_timepoint rtgl_buffer_subdata(struct rtgl_context* ctx, struct rtgl_buffer* buffer, u64 offset, u64 size, const void* data) {
	struct rtgl_timepoint timepoint = { NULL, 0 };
	rtgl_buffer_subdata_job job;

	if (!buffer || buffer->base.zombie) {
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
	if (size > (u64)SIZE_MAX) {
		rtgl_throwf(RT_IMPROPER_USAGE, "buffer subdata size is too large");
		return timepoint;
	}
	job.buffer = buffer;
	job.offset = offset;
	job.size = size;
	job.data = malloc((size_t)size);
	RTGL_CHECK_ALLOC(job.data, (usize)size, "OpenGL buffer subdata upload");
	if (!job.data) {
		return timepoint;
	}
	memcpy(job.data, data, (size_t)size);
	rtgl_context_execute(ctx, rtgl_buffer_exec_subdata, &job);
	free(job.data);
	return timepoint;
}

typedef struct rtgl_buffer_read_job {
	struct rtgl_buffer* buffer;
	u64 offset;
	u64 size;
	void* data;
} rtgl_buffer_read_job;

static void rtgl_buffer_exec_read(struct rtgl_context* ctx, void* data) {
	rtgl_buffer_read_job* job = (rtgl_buffer_read_job*)data;
	(void)ctx;
	glGetNamedBufferSubData(job->buffer->gl_buffer, (GLintptr)job->offset, (GLsizeiptr)job->size, job->data);
}

void rtgl_buffer_read(struct rtgl_context* ctx, struct rtgl_buffer* buffer, u64 offset, u64 size, void* data) {
	rtgl_buffer_read_job job;

	if (!buffer || buffer->base.zombie) {
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
	job.buffer = buffer;
	job.offset = offset;
	job.size = size;
	job.data = data;
	rtgl_context_execute(ctx, rtgl_buffer_exec_read, &job);
}
