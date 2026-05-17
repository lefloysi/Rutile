#include "logger.h"
#include "procs.h"

#include <string.h>

#define RTVAL_MAX_COMMAND_BUFFERS 1024
#define RTVAL_DROP(message) rtval_printf("[validation] %s, dropping call\n", message)

typedef struct rtval_command_buffer_state {
	rt_command_buffer command_buffer;
	rt_queue queue;
	bool recording;
	bool rendering;
} rtval_command_buffer_state;

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

static rtval_command_buffer_state rtval_command_buffers[RTVAL_MAX_COMMAND_BUFFERS];

static rtval_command_buffer_state* rtval_find_command_buffer(rt_command_buffer command_buffer) {
	for (u32 i = 0; i < RTVAL_MAX_COMMAND_BUFFERS; i++) {
		if (rtval_command_buffers[i].command_buffer == command_buffer) {
			return &rtval_command_buffers[i];
		}
	}
	return NULL;
}

static rtval_command_buffer_state* rtval_track_command_buffer(rt_command_buffer command_buffer) {
	rtval_command_buffer_state* state = rtval_find_command_buffer(command_buffer);
	if (state) {
		return state;
	}

	for (u32 i = 0; i < RTVAL_MAX_COMMAND_BUFFERS; i++) {
		if (!rtval_command_buffers[i].command_buffer) {
			rtval_command_buffers[i].command_buffer = command_buffer;
			return &rtval_command_buffers[i];
		}
	}

	return NULL;
}

static void rtval_forget_command_buffer(rt_command_buffer command_buffer) {
	rtval_command_buffer_state* state = rtval_find_command_buffer(command_buffer);
	if (state) {
		*state = (rtval_command_buffer_state){ 0 };
	}
}

void rtval_command_buffer_reset_tracking(void) {
	memset(rtval_command_buffers, 0, sizeof(rtval_command_buffers));
}

static bool rtval_command_buffer_recording(rt_command_buffer command_buffer, const char* call_name) {
	if (!command_buffer) {
		RTVAL_DROP("command buffer call: NULL command buffer");
		return false;
	}

	rtval_command_buffer_state* state = rtval_find_command_buffer(command_buffer);
	if (!state || !state->queue) {
		rtval_printf("[validation] %s: command buffer has no queue, dropping call\n", call_name);
		return false;
	}
	if (!state->recording) {
		rtval_printf("[validation] %s: command buffer is not recording, dropping call\n", call_name);
		return false;
	}

	return true;
}

static bool rtval_command_buffer_rendering(rt_command_buffer command_buffer, const char* call_name) {
	if (!rtval_command_buffer_recording(command_buffer, call_name)) {
		return false;
	}

	rtval_command_buffer_state* state = rtval_find_command_buffer(command_buffer);
	if (!state->rendering) {
		rtval_printf("[validation] %s: command buffer is not rendering, dropping call\n", call_name);
		return false;
	}

	return true;
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT rt_command_buffer rtCmdCreate(void) {
	return rtval_rtCmdCreate();
}
RT_EXPORT void rtCmdDestroy(rt_command_buffer command_buffer) {
	rtval_rtCmdDestroy(command_buffer);
}
RT_EXPORT void rtCmdBegin(rt_command_buffer command_buffer, rt_queue queue) {
	rtval_rtCmdBegin(command_buffer, queue);
}
RT_EXPORT void rtCmdBeginRendering(rt_command_buffer command_buffer, rt_framebuffer framebuffer) {
	rtval_rtCmdBeginRendering(command_buffer, framebuffer);
}
RT_EXPORT void rtCmdClearColor(rt_command_buffer command_buffer, u32 color_index, f32 r, f32 g, f32 b, f32 a) {
	rtval_rtCmdClearColor(command_buffer, color_index, r, g, b, a);
}
RT_EXPORT void rtCmdClearDepth(rt_command_buffer command_buffer, f32 depth) {
	rtval_rtCmdClearDepth(command_buffer, depth);
}
RT_EXPORT void rtCmdClearStencil(rt_command_buffer command_buffer, u32 stencil) {
	rtval_rtCmdClearStencil(command_buffer, stencil);
}
RT_EXPORT void rtCmdUseGraphicsProgram(rt_command_buffer command_buffer, rt_graphics_program program) {
	rtval_rtCmdUseGraphicsProgram(command_buffer, program);
}
RT_EXPORT void rtCmdSetScissor(rt_command_buffer command_buffer, u32 x, u32 y, u32 width, u32 height) {
	rtval_rtCmdSetScissor(command_buffer, x, y, width, height);
}
RT_EXPORT void rtCmdUseComputeProgram(rt_command_buffer command_buffer, rt_compute_program program) {
	rtval_rtCmdUseComputeProgram(command_buffer, program);
}
RT_EXPORT void rtCmdUniformBuffer(rt_command_buffer command_buffer, rt_uniform_location location, rt_buffer buffer, u64 offset, u64 size) {
	rtval_rtCmdUniformBuffer(command_buffer, location, buffer, offset, size);
}
RT_EXPORT void rtCmdUniformTexture(rt_command_buffer command_buffer, rt_uniform_location location, rt_texture_view texture_view) {
	rtval_rtCmdUniformTexture(command_buffer, location, texture_view);
}
RT_EXPORT void rtCmdStorageBuffer(rt_command_buffer command_buffer, u32 binding, rt_buffer buffer, u64 offset, u64 size) {
	rtval_rtCmdStorageBuffer(command_buffer, binding, buffer, offset, size);
}
RT_EXPORT void rtCmdStorageTexture(rt_command_buffer command_buffer, u32 binding, rt_texture_view texture_view) {
	rtval_rtCmdStorageTexture(command_buffer, binding, texture_view);
}
RT_EXPORT void rtCmdComputeBarrier(rt_command_buffer command_buffer) {
	rtval_rtCmdComputeBarrier(command_buffer);
}
RT_EXPORT void rtCmdBindVertexBuffer(rt_command_buffer command_buffer, rt_buffer buffer, u64 offset) {
	rtval_rtCmdBindVertexBuffer(command_buffer, buffer, offset);
}
RT_EXPORT void rtCmdDraw(rt_command_buffer command_buffer, u32 vertex_count, u32 first_vertex) {
	rtval_rtCmdDraw(command_buffer, vertex_count, first_vertex);
}
RT_EXPORT void rtCmdDispatch(rt_command_buffer command_buffer, u32 group_count_x, u32 group_count_y, u32 group_count_z) {
	rtval_rtCmdDispatch(command_buffer, group_count_x, group_count_y, group_count_z);
}
RT_EXPORT void rtCmdEndRendering(rt_command_buffer command_buffer) {
	rtval_rtCmdEndRendering(command_buffer);
}
RT_EXPORT void rtCmdEnd(rt_command_buffer command_buffer) {
	rtval_rtCmdEnd(command_buffer);
}

rt_command_buffer rtval_rtCmdCreate(void) {
	rt_command_buffer command_buffer = rtval_next_rtCmdCreate();
	rtval_report_error("rtCmdCreate");

	if (command_buffer) {
		rtval_track_command_buffer(command_buffer);
	}

	return command_buffer;
}

void rtval_rtCmdDestroy(rt_command_buffer command_buffer) {
	rtval_forget_command_buffer(command_buffer);
	rtval_next_rtCmdDestroy(command_buffer);
	rtval_report_error("rtCmdDestroy");
}

void rtval_rtCmdBegin(rt_command_buffer command_buffer, rt_queue queue) {
	if (!command_buffer) {
		RTVAL_DROP("command_buffer_begin: NULL command buffer");
		return;
	}
	if (!queue) {
		RTVAL_DROP("command_buffer_begin: NULL queue");
		return;
	}

	rtval_command_buffer_state* state = rtval_track_command_buffer(command_buffer);
	if (!state) {
		RTVAL_DROP("command_buffer_begin: command buffer tracking table is full");
		return;
	}
	if (state->recording) {
		RTVAL_DROP("command_buffer_begin: command buffer is already recording");
		return;
	}

	state->queue = queue;
	state->recording = true;
	state->rendering = false;
	rtval_next_rtCmdBegin(command_buffer, queue);
	rtval_report_error("rtCmdBegin");
}

void rtval_rtCmdBeginRendering(rt_command_buffer command_buffer, rt_framebuffer framebuffer) {
	if (!rtval_command_buffer_recording(command_buffer, "cmd_begin_rendering")) {
		return;
	}
	if (!framebuffer) {
		RTVAL_DROP("cmd_begin_rendering: NULL framebuffer");
		return;
	}

	rtval_command_buffer_state* state = rtval_find_command_buffer(command_buffer);
	if (state->rendering) {
		RTVAL_DROP("cmd_begin_rendering: command buffer is already rendering");
		return;
	}

	state->rendering = true;
	rtval_next_rtCmdBeginRendering(command_buffer, framebuffer);
	rtval_report_error("rtCmdBeginRendering");
}

void rtval_rtCmdClearColor(rt_command_buffer command_buffer, u32 color_index, f32 r, f32 g, f32 b, f32 a) {
	if (!rtval_command_buffer_rendering(command_buffer, "cmd_clear_color")) {
		return;
	}

	rtval_next_rtCmdClearColor(command_buffer, color_index, r, g, b, a);
	rtval_report_error("rtCmdClearColor");
}

void rtval_rtCmdClearDepth(rt_command_buffer command_buffer, f32 depth) {
	if (!rtval_command_buffer_rendering(command_buffer, "cmd_clear_depth")) {
		return;
	}

	rtval_next_rtCmdClearDepth(command_buffer, depth);
	rtval_report_error("rtCmdClearDepth");
}

void rtval_rtCmdClearStencil(rt_command_buffer command_buffer, u32 stencil) {
	if (!rtval_command_buffer_rendering(command_buffer, "cmd_clear_stencil")) {
		return;
	}

	rtval_next_rtCmdClearStencil(command_buffer, stencil);
	rtval_report_error("rtCmdClearStencil");
}

void rtval_rtCmdUseGraphicsProgram(rt_command_buffer command_buffer, rt_graphics_program program) {
	if (!rtval_command_buffer_rendering(command_buffer, "cmd_use_graphics_program")) {
		return;
	}
	if (!program) {
		RTVAL_DROP("cmd_use_graphics_program: NULL program");
		return;
	}

	rtval_next_rtCmdUseGraphicsProgram(command_buffer, program);
	rtval_report_error("rtCmdUseGraphicsProgram");
}

void rtval_rtCmdSetScissor(rt_command_buffer command_buffer, u32 x, u32 y, u32 width, u32 height) {
	if (!rtval_command_buffer_rendering(command_buffer, "cmd_set_scissor")) {
		return;
	}
	(void)x;
	(void)y;

	rtval_next_rtCmdSetScissor(command_buffer, x, y, width, height);
	rtval_report_error("rtCmdSetScissor");
}

void rtval_rtCmdUseComputeProgram(rt_command_buffer command_buffer, rt_compute_program program) {
	if (!rtval_command_buffer_recording(command_buffer, "cmd_use_compute_program")) {
		return;
	}
	rtval_command_buffer_state* state = rtval_find_command_buffer(command_buffer);
	if (state->rendering) {
		RTVAL_DROP("cmd_use_compute_program: command buffer is rendering");
		return;
	}
	if (!program) {
		RTVAL_DROP("cmd_use_compute_program: NULL program");
		return;
	}
	rtval_next_rtCmdUseComputeProgram(command_buffer, program);
	rtval_report_error("rtCmdUseComputeProgram");
}

void rtval_rtCmdUniformBuffer(rt_command_buffer command_buffer, rt_uniform_location location, rt_buffer buffer, u64 offset, u64 size) {
	if (!rtval_command_buffer_rendering(command_buffer, "cmd_uniform_buffer")) {
		return;
	}
	if (!location) {
		RTVAL_DROP("cmd_uniform_buffer: NULL location");
		return;
	}
	if (!buffer) {
		RTVAL_DROP("cmd_uniform_buffer: NULL buffer");
		return;
	}
	if (size == 0) {
		RTVAL_DROP("cmd_uniform_buffer: zero size");
		return;
	}

	rtval_next_rtCmdUniformBuffer(command_buffer, location, buffer, offset, size);
	rtval_report_error("rtCmdUniformBuffer");
}

void rtval_rtCmdUniformTexture(rt_command_buffer command_buffer, rt_uniform_location location, rt_texture_view texture_view) {
	if (!rtval_command_buffer_rendering(command_buffer, "cmd_uniform_texture")) {
		return;
	}
	if (!location) {
		RTVAL_DROP("cmd_uniform_texture: NULL location");
		return;
	}
	if (!texture_view) {
		RTVAL_DROP("cmd_uniform_texture: NULL texture view");
		return;
	}

	rtval_next_rtCmdUniformTexture(command_buffer, location, texture_view);
	rtval_report_error("rtCmdUniformTexture");
}

void rtval_rtCmdStorageBuffer(rt_command_buffer command_buffer, u32 binding, rt_buffer buffer, u64 offset, u64 size) {
	if (!rtval_command_buffer_recording(command_buffer, "cmd_storage_buffer")) {
		return;
	}
	if (!buffer) {
		RTVAL_DROP("cmd_storage_buffer: NULL buffer");
		return;
	}
	if (size == 0) {
		RTVAL_DROP("cmd_storage_buffer: zero size");
		return;
	}
	rtval_next_rtCmdStorageBuffer(command_buffer, binding, buffer, offset, size);
	rtval_report_error("rtCmdStorageBuffer");
}

void rtval_rtCmdStorageTexture(rt_command_buffer command_buffer, u32 binding, rt_texture_view texture_view) {
	if (!rtval_command_buffer_recording(command_buffer, "cmd_storage_texture")) {
		return;
	}
	if (!texture_view) {
		RTVAL_DROP("cmd_storage_texture: NULL texture view");
		return;
	}
	rtval_next_rtCmdStorageTexture(command_buffer, binding, texture_view);
	rtval_report_error("rtCmdStorageTexture");
}

void rtval_rtCmdComputeBarrier(rt_command_buffer command_buffer) {
	if (!rtval_command_buffer_recording(command_buffer, "cmd_compute_barrier")) {
		return;
	}
	rtval_command_buffer_state* state = rtval_find_command_buffer(command_buffer);
	if (state->rendering) {
		RTVAL_DROP("cmd_compute_barrier: command buffer is rendering");
		return;
	}
	rtval_next_rtCmdComputeBarrier(command_buffer);
	rtval_report_error("rtCmdComputeBarrier");
}

void rtval_rtCmdBindVertexBuffer(rt_command_buffer command_buffer, rt_buffer buffer, u64 offset) {
	if (!rtval_command_buffer_rendering(command_buffer, "cmd_bind_vertex_buffer")) {
		return;
	}
	if (!buffer) {
		RTVAL_DROP("cmd_bind_vertex_buffer: NULL buffer");
		return;
	}

	rtval_next_rtCmdBindVertexBuffer(command_buffer, buffer, offset);
	rtval_report_error("rtCmdBindVertexBuffer");
}

void rtval_rtCmdDraw(rt_command_buffer command_buffer, u32 vertex_count, u32 first_vertex) {
	if (!rtval_command_buffer_rendering(command_buffer, "cmd_draw")) {
		return;
	}
	if (vertex_count == 0) {
		RTVAL_DROP("cmd_draw: zero vertex count");
		return;
	}

	rtval_next_rtCmdDraw(command_buffer, vertex_count, first_vertex);
	rtval_report_error("rtCmdDraw");
}

void rtval_rtCmdDispatch(rt_command_buffer command_buffer, u32 group_count_x, u32 group_count_y, u32 group_count_z) {
	if (!rtval_command_buffer_recording(command_buffer, "cmd_dispatch")) {
		return;
	}
	rtval_command_buffer_state* state = rtval_find_command_buffer(command_buffer);
	if (state->rendering) {
		RTVAL_DROP("cmd_dispatch: command buffer is rendering");
		return;
	}
	if (!group_count_x || !group_count_y || !group_count_z) {
		RTVAL_DROP("cmd_dispatch: zero group count");
		return;
	}
	rtval_next_rtCmdDispatch(command_buffer, group_count_x, group_count_y, group_count_z);
	rtval_report_error("rtCmdDispatch");
}

void rtval_rtCmdEndRendering(rt_command_buffer command_buffer) {
	if (!rtval_command_buffer_rendering(command_buffer, "cmd_end_rendering")) {
		return;
	}

	rtval_find_command_buffer(command_buffer)->rendering = false;
	rtval_next_rtCmdEndRendering(command_buffer);
	rtval_report_error("rtCmdEndRendering");
}

void rtval_rtCmdEnd(rt_command_buffer command_buffer) {
	if (!command_buffer) {
		RTVAL_DROP("command_buffer_end: NULL command buffer");
		return;
	}

	rtval_command_buffer_state* state = rtval_find_command_buffer(command_buffer);
	if (!state || !state->queue) {
		RTVAL_DROP("command_buffer_end: command buffer has no queue");
		return;
	}
	if (!state->recording) {
		RTVAL_DROP("command_buffer_end: command buffer is not recording");
		return;
	}
	if (state->rendering) {
		RTVAL_DROP("command_buffer_end: command buffer is still rendering");
		return;
	}

	state->recording = false;
	rtval_next_rtCmdEnd(command_buffer);
	rtval_report_error("rtCmdEnd");
}

bool rtval_command_buffer_ready_for_submit(rt_command_buffer command_buffer, rt_queue queue) {
	if (!command_buffer) {
		RTVAL_DROP("queue_submit: NULL command buffer");
		return false;
	}
	if (!queue) {
		RTVAL_DROP("queue_submit: NULL queue");
		return false;
	}

	rtval_command_buffer_state* state = rtval_find_command_buffer(command_buffer);
	if (!state || !state->queue) {
		RTVAL_DROP("queue_submit: command buffer has no queue");
		return false;
	}
	if (state->queue != queue) {
		RTVAL_DROP("queue_submit: queue does not match command buffer queue");
		return false;
	}
	if (state->recording) {
		RTVAL_DROP("queue_submit: command buffer is still recording");
		return false;
	}

	return true;
}

#undef RTVAL_DROP
#undef RTVAL_MAX_COMMAND_BUFFERS
