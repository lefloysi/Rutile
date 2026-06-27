#ifndef RTVAL_COMMAND_BUFFER_H
#define RTVAL_COMMAND_BUFFER_H

#include "handles.h"

struct rtval_command_buffer {
	rt_command_buffer backend;
	struct rtval_queue *queue;
	bool recording;
	bool rendering;
};

struct rtval_command_buffer *rtval_command_buffer_create(void);
void rtval_command_buffer_destroy(struct rtval_command_buffer *cb);

void rtval_command_buffer_begin(struct rtval_command_buffer *cb, struct rtval_queue *queue);
void rtval_command_buffer_begin_rendering(struct rtval_command_buffer *cb, struct rtval_framebuffer *framebuffer);
void rtval_command_buffer_clear_color(struct rtval_command_buffer *cb, u32 color_index, f32 r, f32 g, f32 b, f32 a);
void rtval_command_buffer_clear_depth(struct rtval_command_buffer *cb, f32 depth);
void rtval_command_buffer_clear_stencil(struct rtval_command_buffer *cb, u32 stencil);
void rtval_command_buffer_end_rendering(struct rtval_command_buffer *cb);
void rtval_command_buffer_end(struct rtval_command_buffer *cb);

void rtval_command_buffer_use_graphics_program(struct rtval_command_buffer *cb, struct rtval_graphics_program *program);
void rtval_command_buffer_set_scissor(struct rtval_command_buffer *cb, u32 x, u32 y, u32 width, u32 height);
void rtval_command_buffer_use_compute_program(struct rtval_command_buffer *cb, struct rtval_compute_program *program);
void rtval_command_buffer_uniform_buffer(struct rtval_command_buffer *cb, rt_uniform_location location, struct rtval_buffer *buffer, u64 offset, u64 size);
void rtval_command_buffer_uniform_texture(struct rtval_command_buffer *cb, rt_uniform_location location, struct rtval_texture_view *view);
void rtval_command_buffer_storage_buffer(struct rtval_command_buffer *cb, u32 binding, struct rtval_buffer *buffer, u64 offset, u64 size);
void rtval_command_buffer_storage_texture(struct rtval_command_buffer *cb, u32 binding, struct rtval_texture_view *view);
void rtval_command_buffer_compute_barrier(struct rtval_command_buffer *cb);

void rtval_command_buffer_bind_vertex_buffer(struct rtval_command_buffer *cb, struct rtval_buffer *buffer, u64 offset);

void rtval_command_buffer_draw(struct rtval_command_buffer *cb, u32 vertex_count, u32 first_vertex);
void rtval_command_buffer_dispatch(struct rtval_command_buffer *cb, u32 group_count_x, u32 group_count_y, u32 group_count_z);

bool rtval_command_buffer_ready_for_submit(struct rtval_command_buffer *cb, struct rtval_queue *queue);

#endif
