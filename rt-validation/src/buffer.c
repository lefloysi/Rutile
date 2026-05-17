#include "procs.h"
#include "logger.h"

#define RTVAL_DROP(message) rtval_printf("[validation] %s, dropping call\n", message)

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT rt_buffer rtBufferCreate(void) { return rtval_rtBufferCreate(); }
RT_EXPORT void rtBufferDestroy(rt_buffer buffer) { rtval_rtBufferDestroy(buffer); }
RT_EXPORT rt_timepoint rtBufferData(rt_buffer buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data) { return rtval_rtBufferData(buffer, mode, usage, size, data); }
RT_EXPORT rt_timepoint rtBufferSubdata(rt_buffer buffer, u64 offset, u64 size, const void* data) { return rtval_rtBufferSubdata(buffer, offset, size, data); }
RT_EXPORT void rtBufferRead(rt_buffer buffer, u64 offset, u64 size, void* data) { rtval_rtBufferRead(buffer, offset, size, data); }
RT_EXPORT void* rtBufferMap(rt_buffer buffer, u64 offset, u64 size) { return rtval_rtBufferMap(buffer, offset, size); }
RT_EXPORT void rtBufferUnmap(rt_buffer buffer) { rtval_rtBufferUnmap(buffer); }

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_buffer rtval_rtBufferCreate(void) {
	rt_buffer buffer = rtval_next_rtBufferCreate();
	rtval_report_error("rtBufferCreate");
	return buffer;
}

void rtval_rtBufferDestroy(rt_buffer buffer) {
	rtval_next_rtBufferDestroy(buffer);
	rtval_report_error("rtBufferDestroy");
}

rt_timepoint rtval_rtBufferData(rt_buffer buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data) {
	rt_timepoint timepoint = { RT_NULL_HANDLE, 0 };
	if (!buffer) {
		RTVAL_DROP("buffer_data: NULL buffer");
		return timepoint;
	}
	if (mode != RT_BUFFER_STATIC && mode != RT_BUFFER_DYNAMIC) {
		RTVAL_DROP("buffer_data: unsupported buffer mode");
		return timepoint;
	}
	if (usage == RT_BUFFER_USAGE_NONE) {
		RTVAL_DROP("buffer_data: empty usage");
		return timepoint;
	}

	timepoint = rtval_next_rtBufferData(buffer, mode, usage, size, data);
	rtval_report_error("rtBufferData");
	return timepoint;
}

rt_timepoint rtval_rtBufferSubdata(rt_buffer buffer, u64 offset, u64 size, const void* data) {
	rt_timepoint timepoint = { RT_NULL_HANDLE, 0 };
	if (!buffer) {
		RTVAL_DROP("buffer_subdata: NULL buffer");
		return timepoint;
	}
	if (size && !data) {
		RTVAL_DROP("buffer_subdata: NULL data");
		return timepoint;
	}

	timepoint = rtval_next_rtBufferSubdata(buffer, offset, size, data);
	rtval_report_error("rtBufferSubdata");
	return timepoint;
}

void rtval_rtBufferRead(rt_buffer buffer, u64 offset, u64 size, void* data) {
	if (!buffer) {
		RTVAL_DROP("buffer_read: NULL buffer");
		return;
	}
	if (size && !data) {
		RTVAL_DROP("buffer_read: NULL destination");
		return;
	}

	rtval_next_rtBufferRead(buffer, offset, size, data);
	rtval_report_error("rtBufferRead");
}

void* rtval_rtBufferMap(rt_buffer buffer, u64 offset, u64 size) {
	if (!buffer) {
		RTVAL_DROP("buffer_map: NULL buffer");
		return NULL;
	}

	void* data = rtval_next_rtBufferMap(buffer, offset, size);
	rtval_report_error("rtBufferMap");
	return data;
}

void rtval_rtBufferUnmap(rt_buffer buffer) {
	if (!buffer) {
		RTVAL_DROP("buffer_unmap: NULL buffer");
		return;
	}

	rtval_next_rtBufferUnmap(buffer);
	rtval_report_error("rtBufferUnmap");
}

#undef RTVAL_DROP

