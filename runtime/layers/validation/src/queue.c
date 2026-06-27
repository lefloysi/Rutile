#include "queue.h"
#include "command_buffer.h"
#include "logger.h"

#define RTVAL_DROP(message) rtval_printf("[validation] %s, dropping call\n", message)

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT rt_queue rtQueueQuery(enum rt_queue_capability capability) {
	return rtval_queue_to_handle(rtval_queue_query(capability));
}

RT_EXPORT void rtQueueWait(rt_queue queue, rt_timepoint timepoint) {
	rtval_queue_wait(rtval_queue_from_handle(queue), timepoint);
}

RT_EXPORT rt_timepoint rtQueueSubmit(rt_queue queue, rt_command_buffer command_buffer) {
	return rtval_queue_submit(rtval_queue_from_handle(queue), rtval_command_buffer_from_handle(command_buffer));
}

RT_EXPORT rt_timepoint rtQueueFlush(rt_queue queue) {
	return rtval_queue_flush(rtval_queue_from_handle(queue));
}

RT_EXPORT void rtTimepointWait(rt_timepoint timepoint) {
	rtval_timepoint_wait_public(timepoint);
}

RT_EXPORT bool rtTimepointReached(rt_timepoint timepoint) {
	return rtval_timepoint_reached_public(timepoint);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtval_queue* rtval_queue_query(enum rt_queue_capability capability) {
	rt_queue backend = rtval_next_rtQueueQuery(capability);
	rtval_report_error("rtQueueQuery");
	return rtval_queue_wrap(backend);
}

void rtval_queue_wait(struct rtval_queue* queue, rt_timepoint timepoint) {
	struct rtval_queue* state = RTVAL_PAYLOAD(queue, struct rtval_queue);
	if (!state) {
		RTVAL_DROP("rtQueueWait: invalid queue");
		return;
	}
	rtval_next_rtQueueWait(state->backend, rtval_timepoint_unwrap(timepoint));
	rtval_report_error("rtQueueWait");
}

rt_timepoint rtval_queue_submit(struct rtval_queue* queue, struct rtval_command_buffer* cb) {
	rt_timepoint timepoint = {rtval_queue_to_handle(queue), 0};
	if (!rtval_command_buffer_ready_for_submit(cb, queue)) {
		return timepoint;
	}

	struct rtval_queue* q = RTVAL_PAYLOAD(queue, struct rtval_queue);
	struct rtval_command_buffer* c = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	timepoint = rtval_next_rtQueueSubmit(q->backend, c->backend);
	rtval_report_error("rtQueueSubmit");
	return rtval_timepoint_wrap(timepoint);
}

rt_timepoint rtval_queue_flush(struct rtval_queue* queue) {
	rt_timepoint timepoint = {rtval_queue_to_handle(queue), 0};
	struct rtval_queue* state = RTVAL_PAYLOAD(queue, struct rtval_queue);
	if (!state) {
		RTVAL_DROP("rtQueueFlush: invalid queue");
		return timepoint;
	}

	timepoint = rtval_next_rtQueueFlush(state->backend);
	rtval_report_error("rtQueueFlush");
	return rtval_timepoint_wrap(timepoint);
}

void rtval_timepoint_wait_public(rt_timepoint timepoint) {
	rtval_next_rtTimepointWait(rtval_timepoint_unwrap(timepoint));
	rtval_report_error("rtTimepointWait");
}

bool rtval_timepoint_reached_public(rt_timepoint timepoint) {
	bool reached = rtval_next_rtTimepointReached(rtval_timepoint_unwrap(timepoint));
	rtval_report_error("rtTimepointReached");
	return reached;
}

#undef RTVAL_DROP
