#ifndef RTVAL_COMPUTE_PROGRAM_H
#define RTVAL_COMPUTE_PROGRAM_H

#include "handles.h"

struct rtval_compute_program {
	rt_compute_program backend;
};

struct rtval_compute_program* rtval_compute_program_create(void);
void rtval_compute_program_destroy(struct rtval_compute_program* program);
void rtval_compute_program_shader(struct rtval_compute_program* program, u64 size, const void* data);
void rtval_compute_program_link(struct rtval_compute_program* program);

#endif
