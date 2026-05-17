#include "procs.h"
#include "logger.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT rt_queue rtQueueQuery(enum rt_queue_capability capability) { return rtval_rtQueueQuery(capability); }
RT_EXPORT void rtQueueWait(rt_queue queue, rt_timepoint timepoint) { rtval_rtQueueWait(queue, timepoint); }
RT_EXPORT rt_timepoint rtQueueSubmit(rt_queue queue, rt_command_buffer command_buffer) { return rtval_rtQueueSubmit(queue, command_buffer); }
RT_EXPORT rt_timepoint rtQueueFlush(rt_queue queue) { return rtval_rtQueueFlush(queue); }
RT_EXPORT void rtTimepointWait(rt_timepoint timepoint) { rtval_rtTimepointWait(timepoint); }
RT_EXPORT bool rtTimepointReached(rt_timepoint timepoint) { return rtval_rtTimepointReached(timepoint); }

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_queue rtval_rtQueueQuery(enum rt_queue_capability capability) {
	rt_queue queue = rtval_next_rtQueueQuery(capability);
	rtval_report_error("rtQueueQuery");
	return queue;
}

void rtval_rtQueueWait(rt_queue queue, rt_timepoint timepoint) {
	if (!queue) {
		rtval_printf("[validation] queue_wait: NULL queue, dropping call\n");
		return;
	}
	rtval_next_rtQueueWait(queue, timepoint);
	rtval_report_error("rtQueueWait");
}

rt_timepoint rtval_rtQueueSubmit(rt_queue queue, rt_command_buffer command_buffer) {
	rt_timepoint timepoint;

	if (!rtval_command_buffer_ready_for_submit(command_buffer, queue)) {
		timepoint.queue = queue;
		timepoint.value = 0;
		return timepoint;
	}

	timepoint = rtval_next_rtQueueSubmit(queue, command_buffer);
	rtval_report_error("rtQueueSubmit");
	return timepoint;
}

rt_timepoint rtval_rtQueueFlush(rt_queue queue) {
	rt_timepoint timepoint = { queue, 0 };
	if (!queue) {
		rtval_printf("[validation] queue_flush: NULL queue, dropping call\n");
		return timepoint;
	}
	timepoint = rtval_next_rtQueueFlush(queue);
	rtval_report_error("rtQueueFlush");
	return timepoint;
}

void rtval_rtTimepointWait(rt_timepoint timepoint) {
	rtval_next_rtTimepointWait(timepoint);
	rtval_report_error("rtTimepointWait");
}

bool rtval_rtTimepointReached(rt_timepoint timepoint) {
	bool reached = rtval_next_rtTimepointReached(timepoint);
	rtval_report_error("rtTimepointReached");
	return reached;
}


