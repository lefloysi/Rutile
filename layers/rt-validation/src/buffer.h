#ifndef RTVAL_BUFFER_H
#define RTVAL_BUFFER_H

#include "handles.h"

struct rtval_buffer {
	rt_buffer backend;
};

struct rtval_buffer* rtval_buffer_create(void);
void                 rtval_buffer_destroy(struct rtval_buffer* buffer);
rt_timepoint         rtval_buffer_data(struct rtval_buffer* buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data);
rt_timepoint         rtval_buffer_subdata(struct rtval_buffer* buffer, u64 offset, u64 size, const void* data);
void                 rtval_buffer_read(struct rtval_buffer* buffer, u64 offset, u64 size, void* data);

#endif
