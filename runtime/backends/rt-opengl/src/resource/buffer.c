#include "buffer.h"

#include "context.h"
#include "error.h"
#include "execution.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

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

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTGL_DEFINE_RESOURCE_PRIVATE(buffer)

bool rtgl_buffer_uses_host_storage(enum rt_buffer_mode mode, enum rt_buffer_usage usage) {
	return mode == RT_BUFFER_DYNAMIC || (usage & RT_BUFFER_USAGE_STAGING) != 0;
}

void rtgl_buffer_init(struct rtgl_context* ctx, struct rtgl_buffer* buffer) {
	rtgl_init_resource_base(ctx, RTGL_RESOURCE_BASE(buffer), RTGL_RESOURCE_BUFFER);
	buffer->mode = RT_BUFFER_DYNAMIC;
	buffer->usage = (enum rt_buffer_usage)(RT_BUFFER_USAGE_VERTEX | RT_BUFFER_USAGE_INDEX | RT_BUFFER_USAGE_UNIFORM | RT_BUFFER_USAGE_STORAGE | RT_BUFFER_USAGE_TRANSFER_SRC | RT_BUFFER_USAGE_TRANSFER_DST);
	rtgl_execution_buffer_create(ctx, buffer);
}

void rtgl_buffer_finish(struct rtgl_buffer* buffer) {
	rtgl_execution_buffer_delete(buffer->base.ctx, buffer);
	rtgl_finish_resource_base(RTGL_RESOURCE_BASE(buffer));
}

struct rtgl_timepoint rtgl_buffer_data(struct rtgl_context* ctx, struct rtgl_buffer* buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data) {
	struct rtgl_timepoint timepoint = { NULL, 0 };

	buffer->mode = mode;
	buffer->usage = usage;
	buffer->size = size;
	rtgl_execution_buffer_data(ctx, buffer, mode, usage, size, (const u08*)data);
	return timepoint;
}

struct rtgl_timepoint rtgl_buffer_subdata(struct rtgl_context* ctx, struct rtgl_buffer* buffer, u64 offset, u64 size, const void* data) {
	struct rtgl_timepoint timepoint = { NULL, 0 };

	if (!size) {
		return timepoint;
	}
	rtgl_execution_buffer_subdata(ctx, buffer, offset, size, (const u08*)data);
	return timepoint;
}

void rtgl_buffer_read(struct rtgl_context* ctx, struct rtgl_buffer* buffer, u64 offset, u64 size, void* data) {
	if (!size) {
		return;
	}
	rtgl_execution_buffer_read(ctx, buffer, offset, size, (u08*)data);
}
