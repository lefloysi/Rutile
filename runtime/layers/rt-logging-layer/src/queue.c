#include "procs.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT rt_queue rtQueueQuery(enum rt_queue_capability capability) { return rtlog_rtQueueQuery(capability); }
RT_EXPORT void rtQueueWait(rt_queue queue, rt_timepoint timepoint) { rtlog_rtQueueWait(queue, timepoint); }
RT_EXPORT rt_timepoint rtQueueSubmit(rt_queue queue, rt_command_buffer command_buffer) { return rtlog_rtQueueSubmit(queue, command_buffer); }
RT_EXPORT rt_timepoint rtQueueFlush(rt_queue queue) { return rtlog_rtQueueFlush(queue); }
RT_EXPORT void rtTimepointWait(rt_timepoint timepoint) { rtlog_rtTimepointWait(timepoint); }
RT_EXPORT bool rtTimepointReached(rt_timepoint timepoint) { return rtlog_rtTimepointReached(timepoint); }

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_queue rtlog_rtQueueQuery(enum rt_queue_capability capability) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtQueueQuery(capability=%d)\n", (i32)capability);
	rt_queue result = next_rtQueueQuery(capability);
	rtlog_printf("rtQueueQuery -> %s [%s]\n", rtlog_pointer(result), rtlog_elapsed(start_ns));
	rtlog_error("rtQueueQuery");
	return result;
}

void rtlog_rtQueueWait(rt_queue queue, rt_timepoint timepoint) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtQueueWait(queue=%s, timepoint=%s)\n", rtlog_pointer(queue), rtlog_timepoint(timepoint));
	next_rtQueueWait(queue, timepoint);
	rtlog_printf("rtQueueWait completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtQueueWait");
}

rt_timepoint rtlog_rtQueueSubmit(rt_queue queue, rt_command_buffer command_buffer) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtQueueSubmit(queue=%s, command_buffer=%s)\n", rtlog_pointer(queue), rtlog_pointer(command_buffer));
	rt_timepoint result = next_rtQueueSubmit(queue, command_buffer);
	rtlog_printf("rtQueueSubmit -> %s [%s]\n", rtlog_timepoint(result), rtlog_elapsed(start_ns));
	rtlog_error("rtQueueSubmit");
	return result;
}

rt_timepoint rtlog_rtQueueFlush(rt_queue queue) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtQueueFlush(queue=%s)\n", rtlog_pointer(queue));
	rt_timepoint result = next_rtQueueFlush(queue);
	rtlog_printf("rtQueueFlush -> %s [%s]\n", rtlog_timepoint(result), rtlog_elapsed(start_ns));
	rtlog_error("rtQueueFlush");
	return result;
}

void rtlog_rtTimepointWait(rt_timepoint timepoint) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtTimepointWait(timepoint={queue=%s, value=%llu})\n", rtlog_pointer(timepoint.queue), (u64)timepoint.value);
	next_rtTimepointWait(timepoint);
	rtlog_printf("rtTimepointWait completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtTimepointWait");
}

bool rtlog_rtTimepointReached(rt_timepoint timepoint) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtTimepointReached(timepoint={queue=%s, value=%llu})\n", rtlog_pointer(timepoint.queue), (u64)timepoint.value);
	bool result = next_rtTimepointReached(timepoint);
	rtlog_printf("rtTimepointReached -> %s [%s]\n", result ? "true" : "false", rtlog_elapsed(start_ns));
	rtlog_error("rtTimepointReached");
	return result;
}
