#ifndef RTVAL_GRAPHICS_PROGRAM_H
#define RTVAL_GRAPHICS_PROGRAM_H

#include "handles.h"

struct rtval_graphics_program {
	rt_graphics_program backend;
};

struct rtval_graphics_program* rtval_graphics_program_create(void);
void rtval_graphics_program_destroy(struct rtval_graphics_program* program);
void rtval_graphics_program_layout(struct rtval_graphics_program* program, const rt_vertex_layout* layout);
void rtval_graphics_program_source(struct rtval_graphics_program* program, u64 size, const void* data);
void rtval_graphics_program_raster_state(struct rtval_graphics_program* program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode);
void rtval_graphics_program_blend_state(struct rtval_graphics_program* program, bool enabled, enum rt_blend_factor src_color, enum rt_blend_factor dst_color, enum rt_blend_op color_op, enum rt_blend_factor src_alpha, enum rt_blend_factor dst_alpha, enum rt_blend_op alpha_op);
void rtval_graphics_program_finalize(struct rtval_graphics_program* program);
void rtval_graphics_program_reset(struct rtval_graphics_program* program);
rt_uniform_location rtval_graphics_program_uniform_location(struct rtval_graphics_program* program, const char* name);

#endif
