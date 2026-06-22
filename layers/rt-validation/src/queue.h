#ifndef RTVAL_QUEUE_H
#define RTVAL_QUEUE_H

#include "handles.h"

struct rtval_queue {
	rt_queue backend;
};

struct rtval_queue* rtval_queue_query(enum rt_queue_capability capability);
void                rtval_queue_wait(struct rtval_queue* queue, rt_timepoint timepoint);
rt_timepoint        rtval_queue_submit(struct rtval_queue* queue, struct rtval_command_buffer* cb);
rt_timepoint        rtval_queue_flush(struct rtval_queue* queue);

void rtval_timepoint_wait_public(rt_timepoint timepoint);
bool rtval_timepoint_reached_public(rt_timepoint timepoint);

#endif
