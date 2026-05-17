#include "procs.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT rt_buffer rtBufferCreate(void) { return rtlog_rtBufferCreate(); }
RT_EXPORT void rtBufferDestroy(rt_buffer buffer) { rtlog_rtBufferDestroy(buffer); }
RT_EXPORT rt_timepoint rtBufferData(rt_buffer buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data) { return rtlog_rtBufferData(buffer, mode, usage, size, data); }
RT_EXPORT rt_timepoint rtBufferSubdata(rt_buffer buffer, u64 offset, u64 size, const void* data) { return rtlog_rtBufferSubdata(buffer, offset, size, data); }
RT_EXPORT void rtBufferRead(rt_buffer buffer, u64 offset, u64 size, void* data) { rtlog_rtBufferRead(buffer, offset, size, data); }
RT_EXPORT void* rtBufferMap(rt_buffer buffer, u64 offset, u64 size) { return rtlog_rtBufferMap(buffer, offset, size); }
RT_EXPORT void rtBufferUnmap(rt_buffer buffer) { rtlog_rtBufferUnmap(buffer); }

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_buffer rtlog_rtBufferCreate(void) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtBufferCreate()\n");
	rt_buffer result = next_rtBufferCreate();
	rtlog_printf("rtBufferCreate -> %s [%s]\n", rtlog_pointer(result), rtlog_elapsed(start_ns));
	rtlog_error("rtBufferCreate");
	return result;
}

void rtlog_rtBufferDestroy(rt_buffer buffer) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtBufferDestroy(buffer=%s)\n", rtlog_pointer(buffer));
	next_rtBufferDestroy(buffer);
	rtlog_printf("rtBufferDestroy completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtBufferDestroy");
}

rt_timepoint rtlog_rtBufferData(rt_buffer buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtBufferData(buffer=%s, mode=%d, usage=%d, size=%llu, data=%s)\n", rtlog_pointer(buffer), (i32)mode, (i32)usage, (u64)size, rtlog_pointer(data));
	rt_timepoint result = next_rtBufferData(buffer, mode, usage, size, data);
	rtlog_printf("rtBufferData -> %s [%s]\n", rtlog_timepoint(result), rtlog_elapsed(start_ns));
	rtlog_error("rtBufferData");
	return result;
}

rt_timepoint rtlog_rtBufferSubdata(rt_buffer buffer, u64 offset, u64 size, const void* data) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtBufferSubdata(buffer=%s, offset=%llu, size=%llu, data=%s)\n", rtlog_pointer(buffer), (u64)offset, (u64)size, rtlog_pointer(data));
	rt_timepoint result = next_rtBufferSubdata(buffer, offset, size, data);
	rtlog_printf("rtBufferSubdata -> %s [%s]\n", rtlog_timepoint(result), rtlog_elapsed(start_ns));
	rtlog_error("rtBufferSubdata");
	return result;
}

void rtlog_rtBufferRead(rt_buffer buffer, u64 offset, u64 size, void* data) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtBufferRead(buffer=%s, offset=%llu, size=%llu, data=%s)\n", rtlog_pointer(buffer), (u64)offset, (u64)size, rtlog_pointer(data));
	next_rtBufferRead(buffer, offset, size, data);
	rtlog_printf("rtBufferRead completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtBufferRead");
}

void* rtlog_rtBufferMap(rt_buffer buffer, u64 offset, u64 size) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtBufferMap(buffer=%s, offset=%llu, size=%llu)\n", rtlog_pointer(buffer), (u64)offset, (u64)size);
	void* result = next_rtBufferMap(buffer, offset, size);
	rtlog_printf("rtBufferMap -> %s [%s]\n", rtlog_pointer(result), rtlog_elapsed(start_ns));
	rtlog_error("rtBufferMap");
	return result;
}

void rtlog_rtBufferUnmap(rt_buffer buffer) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtBufferUnmap(buffer=%s)\n", rtlog_pointer(buffer));
	next_rtBufferUnmap(buffer);
	rtlog_printf("rtBufferUnmap completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtBufferUnmap");
}


