#include "buffer.h"
#include "logger.h"

#define RTVAL_DROP(message) rtval_printf("[validation] %s, dropping call\n", message)
#define RTVAL_RESOLVE(handle, call_name, ret)                                  \
	struct rtval_buffer *state = RTVAL_PAYLOAD((handle), struct rtval_buffer); \
	if (!state) {                                                              \
		RTVAL_DROP(call_name ": invalid handle");                              \
		return ret;                                                            \
	}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT rt_buffer rtBufferCreate(void) {
	return rtval_buffer_to_handle(rtval_buffer_create());
}

RT_EXPORT void rtBufferDestroy(rt_buffer buffer) {
	rtval_buffer_destroy(rtval_buffer_from_handle(buffer));
}

RT_EXPORT rt_timepoint rtBufferData(rt_buffer buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void *data) {
	return rtval_buffer_data(rtval_buffer_from_handle(buffer), mode, usage, size, data);
}

RT_EXPORT rt_timepoint rtBufferSubdata(rt_buffer buffer, u64 offset, u64 size, const void *data) {
	return rtval_buffer_subdata(rtval_buffer_from_handle(buffer), offset, size, data);
}

RT_EXPORT void rtBufferRead(rt_buffer buffer, u64 offset, u64 size, void *data) {
	rtval_buffer_read(rtval_buffer_from_handle(buffer), offset, size, data);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtval_buffer *rtval_buffer_create(void) {
	rt_buffer backend = rtval_next_rtBufferCreate();
	if (!backend) {
		rtval_report_error("rtBufferCreate");
		return NULL;
	}
	struct rtval_buffer *handle = rtval_handle_create(RTVAL_HANDLE_TYPE_BUFFER);
	if (!handle) {
		rtval_next_rtBufferDestroy(backend);
		return NULL;
	}
	struct rtval_buffer *state = RTVAL_PAYLOAD(handle, struct rtval_buffer);
	state->backend = backend;
	rtval_report_error("rtBufferCreate");
	return handle;
}

void rtval_buffer_destroy(struct rtval_buffer *buffer) {
	if (!buffer) {
		return;
	}
	struct rtval_buffer *state = RTVAL_PAYLOAD(buffer, struct rtval_buffer);
	if (!state) {
		RTVAL_DROP("rtBufferDestroy: invalid handle");
		return;
	}
	rtval_next_rtBufferDestroy(state->backend);
	rtval_handle_destroy(buffer);
}

rt_timepoint rtval_buffer_data(struct rtval_buffer *buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void *data) {
	rt_timepoint timepoint = {RT_NULL_HANDLE, 0};
	RTVAL_RESOLVE(buffer, "rtBufferData", timepoint);
	if (mode != RT_BUFFER_STATIC && mode != RT_BUFFER_DYNAMIC) {
		RTVAL_DROP("rtBufferData: unsupported buffer mode");
		return timepoint;
	}
	if (usage == RT_BUFFER_USAGE_NONE) {
		RTVAL_DROP("rtBufferData: empty usage");
		return timepoint;
	}

	timepoint = rtval_next_rtBufferData(state->backend, mode, usage, size, data);
	rtval_report_error("rtBufferData");
	return rtval_timepoint_wrap(timepoint);
}

rt_timepoint rtval_buffer_subdata(struct rtval_buffer *buffer, u64 offset, u64 size, const void *data) {
	rt_timepoint timepoint = {RT_NULL_HANDLE, 0};
	RTVAL_RESOLVE(buffer, "rtBufferSubdata", timepoint);
	if (size && !data) {
		RTVAL_DROP("rtBufferSubdata: NULL data");
		return timepoint;
	}

	timepoint = rtval_next_rtBufferSubdata(state->backend, offset, size, data);
	rtval_report_error("rtBufferSubdata");
	return rtval_timepoint_wrap(timepoint);
}

void rtval_buffer_read(struct rtval_buffer *buffer, u64 offset, u64 size, void *data) {
	RTVAL_RESOLVE(buffer, "rtBufferRead", );
	if (size && !data) {
		RTVAL_DROP("rtBufferRead: NULL destination");
		return;
	}

	rtval_next_rtBufferRead(state->backend, offset, size, data);
	rtval_report_error("rtBufferRead");
}

#undef RTVAL_RESOLVE
#undef RTVAL_DROP
