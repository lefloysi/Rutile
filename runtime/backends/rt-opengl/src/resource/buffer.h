#ifndef RTGL_BUFFER_H
#define RTGL_BUFFER_H

#include "config.h"
#include "glad/gl.h"
#include "resource.h"

RTGL_API rt_buffer rtBufferCreate(void);
RTGL_API void rtBufferDestroy(rt_buffer buffer);
RTGL_API rt_timepoint rtBufferData(rt_buffer buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data);
RTGL_API rt_timepoint rtBufferSubdata(rt_buffer buffer, u64 offset, u64 size, const void* data);
RTGL_API void rtBufferRead(rt_buffer buffer, u64 offset, u64 size, void* data);

struct rtgl_buffer {
	struct rtgl_resource_base base;
	GLuint gl_buffer;
	u64 size;
	enum rt_buffer_mode mode;
	enum rt_buffer_usage usage;
};
RTGL_DECLARE_NEW_RESOURCE(buffer)

struct rtgl_timepoint rtgl_buffer_data(struct rtgl_context* ctx, struct rtgl_buffer* buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data);
struct rtgl_timepoint rtgl_buffer_subdata(struct rtgl_context* ctx, struct rtgl_buffer* buffer, u64 offset, u64 size, const void* data);
void rtgl_buffer_read(struct rtgl_context* ctx, struct rtgl_buffer* buffer, u64 offset, u64 size, void* data);

#endif /* RTGL_BUFFER_H */
