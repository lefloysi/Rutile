#include "command_buffer.h"
#include "buffer.h"
#include "compute_program.h"
#include "framebuffer.h"
#include "graphics_program.h"
#include "logger.h"
#include "queue.h"
#include "texture_view.h"

#define RTVAL_DROP(message) rtval_printf("[validation] %s, dropping call\n", message)

static bool rtval_cb_recording(struct rtval_command_buffer* state, const char* call_name) {
	if (!state->queue)     { rtval_printf("[validation] %s: command buffer has no queue, dropping call\n",   call_name); return false; }
	if (!state->recording) { rtval_printf("[validation] %s: command buffer is not recording, dropping call\n", call_name); return false; }
	return true;
}

static bool rtval_cb_rendering(struct rtval_command_buffer* state, const char* call_name) {
	if (!rtval_cb_recording(state, call_name)) { return false; }
	if (!state->rendering) { rtval_printf("[validation] %s: command buffer is not rendering, dropping call\n", call_name); return false; }
	return true;
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT rt_command_buffer rtCmdCreate(void) {
	return rtval_command_buffer_to_handle(rtval_command_buffer_create());
}

RT_EXPORT void rtCmdDestroy(rt_command_buffer command_buffer) {
	rtval_command_buffer_destroy(rtval_command_buffer_from_handle(command_buffer));
}

RT_EXPORT void rtCmdBegin(rt_command_buffer command_buffer, rt_queue queue) {
	rtval_command_buffer_begin(
		rtval_command_buffer_from_handle(command_buffer),
		rtval_queue_from_handle(queue)
	);
}

RT_EXPORT void rtCmdBeginRendering(rt_command_buffer command_buffer, rt_framebuffer framebuffer) {
	rtval_command_buffer_begin_rendering(
		rtval_command_buffer_from_handle(command_buffer),
		rtval_framebuffer_from_handle(framebuffer)
	);
}

RT_EXPORT void rtCmdClearColor(rt_command_buffer command_buffer, u32 color_index, f32 r, f32 g, f32 b, f32 a) {
	rtval_command_buffer_clear_color(rtval_command_buffer_from_handle(command_buffer), color_index, r, g, b, a);
}

RT_EXPORT void rtCmdClearDepth(rt_command_buffer command_buffer, f32 depth) {
	rtval_command_buffer_clear_depth(rtval_command_buffer_from_handle(command_buffer), depth);
}

RT_EXPORT void rtCmdClearStencil(rt_command_buffer command_buffer, u32 stencil) {
	rtval_command_buffer_clear_stencil(rtval_command_buffer_from_handle(command_buffer), stencil);
}

RT_EXPORT void rtCmdUseGraphicsProgram(rt_command_buffer command_buffer, rt_graphics_program program) {
	rtval_command_buffer_use_graphics_program(
		rtval_command_buffer_from_handle(command_buffer),
		rtval_graphics_program_from_handle(program)
	);
}

RT_EXPORT void rtCmdSetScissor(rt_command_buffer command_buffer, u32 x, u32 y, u32 width, u32 height) {
	rtval_command_buffer_set_scissor(rtval_command_buffer_from_handle(command_buffer), x, y, width, height);
}

RT_EXPORT void rtCmdUseComputeProgram(rt_command_buffer command_buffer, rt_compute_program program) {
	rtval_command_buffer_use_compute_program(
		rtval_command_buffer_from_handle(command_buffer),
		rtval_compute_program_from_handle(program)
	);
}

RT_EXPORT void rtCmdUniformBuffer(rt_command_buffer command_buffer, rt_uniform_location location, rt_buffer buffer, u64 offset, u64 size) {
	rtval_command_buffer_uniform_buffer(
		rtval_command_buffer_from_handle(command_buffer),
		location,
		rtval_buffer_from_handle(buffer),
		offset,
		size
	);
}

RT_EXPORT void rtCmdUniformTexture(rt_command_buffer command_buffer, rt_uniform_location location, rt_texture_view view) {
	rtval_command_buffer_uniform_texture(
		rtval_command_buffer_from_handle(command_buffer),
		location,
		rtval_texture_view_from_handle(view)
	);
}

RT_EXPORT void rtCmdStorageBuffer(rt_command_buffer command_buffer, u32 binding, rt_buffer buffer, u64 offset, u64 size) {
	rtval_command_buffer_storage_buffer(
		rtval_command_buffer_from_handle(command_buffer),
		binding,
		rtval_buffer_from_handle(buffer),
		offset,
		size
	);
}

RT_EXPORT void rtCmdStorageTexture(rt_command_buffer command_buffer, u32 binding, rt_texture_view view) {
	rtval_command_buffer_storage_texture(
		rtval_command_buffer_from_handle(command_buffer),
		binding,
		rtval_texture_view_from_handle(view)
	);
}

RT_EXPORT void rtCmdComputeBarrier(rt_command_buffer command_buffer) {
	rtval_command_buffer_compute_barrier(rtval_command_buffer_from_handle(command_buffer));
}

RT_EXPORT void rtCmdBindVertexBuffer(rt_command_buffer command_buffer, rt_buffer buffer, u64 offset) {
	rtval_command_buffer_bind_vertex_buffer(
		rtval_command_buffer_from_handle(command_buffer),
		rtval_buffer_from_handle(buffer),
		offset
	);
}

RT_EXPORT void rtCmdDraw(rt_command_buffer command_buffer, u32 vertex_count, u32 first_vertex) {
	rtval_command_buffer_draw(rtval_command_buffer_from_handle(command_buffer), vertex_count, first_vertex);
}

RT_EXPORT void rtCmdDispatch(rt_command_buffer command_buffer, u32 group_count_x, u32 group_count_y, u32 group_count_z) {
	rtval_command_buffer_dispatch(rtval_command_buffer_from_handle(command_buffer), group_count_x, group_count_y, group_count_z);
}

RT_EXPORT void rtCmdEndRendering(rt_command_buffer command_buffer) {
	rtval_command_buffer_end_rendering(rtval_command_buffer_from_handle(command_buffer));
}

RT_EXPORT void rtCmdEnd(rt_command_buffer command_buffer) {
	rtval_command_buffer_end(rtval_command_buffer_from_handle(command_buffer));
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtval_command_buffer* rtval_command_buffer_create(void) {
	rt_command_buffer backend = rtval_next_rtCmdCreate();
	if (!backend) {
		rtval_report_error("rtCmdCreate");
		return NULL;
	}
	struct rtval_command_buffer* handle = rtval_handle_create(RTVAL_HANDLE_TYPE_COMMAND_BUFFER);
	if (!handle) {
		rtval_next_rtCmdDestroy(backend);
		return NULL;
	}
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(handle, struct rtval_command_buffer);
	state->backend = backend;
	rtval_report_error("rtCmdCreate");
	return handle;
}

void rtval_command_buffer_destroy(struct rtval_command_buffer* cb) {
	if (!cb) { return; }
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	if (!state) {
		RTVAL_DROP("rtCmdDestroy: invalid handle");
		return;
	}
	rtval_next_rtCmdDestroy(state->backend);
	rtval_handle_destroy(cb);
}

void rtval_command_buffer_begin(struct rtval_command_buffer* cb, struct rtval_queue* queue) {
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	if (!state) { RTVAL_DROP("rtCmdBegin: invalid command buffer"); return; }
	struct rtval_queue* queue_state = RTVAL_PAYLOAD(queue, struct rtval_queue);
	if (!queue_state) { RTVAL_DROP("rtCmdBegin: invalid queue"); return; }
	if (state->recording) { RTVAL_DROP("rtCmdBegin: command buffer is already recording"); return; }

	state->queue = queue;
	state->recording = true;
	state->rendering = false;
	rtval_next_rtCmdBegin(state->backend, queue_state->backend);
	rtval_report_error("rtCmdBegin");
}

void rtval_command_buffer_begin_rendering(struct rtval_command_buffer* cb, struct rtval_framebuffer* framebuffer) {
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	if (!state) { RTVAL_DROP("rtCmdBeginRendering: invalid command buffer"); return; }
	if (!rtval_cb_recording(state, "rtCmdBeginRendering")) { return; }
	struct rtval_framebuffer* fb_state = RTVAL_PAYLOAD(framebuffer, struct rtval_framebuffer);
	if (!fb_state) { RTVAL_DROP("rtCmdBeginRendering: invalid framebuffer"); return; }
	if (state->rendering) { RTVAL_DROP("rtCmdBeginRendering: command buffer is already rendering"); return; }

	state->rendering = true;
	rtval_next_rtCmdBeginRendering(state->backend, fb_state->backend);
	rtval_report_error("rtCmdBeginRendering");
}

void rtval_command_buffer_clear_color(struct rtval_command_buffer* cb, u32 color_index, f32 r, f32 g, f32 b, f32 a) {
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	if (!state) { RTVAL_DROP("rtCmdClearColor: invalid handle"); return; }
	if (!rtval_cb_rendering(state, "rtCmdClearColor")) { return; }
	rtval_next_rtCmdClearColor(state->backend, color_index, r, g, b, a);
	rtval_report_error("rtCmdClearColor");
}

void rtval_command_buffer_clear_depth(struct rtval_command_buffer* cb, f32 depth) {
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	if (!state) { RTVAL_DROP("rtCmdClearDepth: invalid handle"); return; }
	if (!rtval_cb_rendering(state, "rtCmdClearDepth")) { return; }
	rtval_next_rtCmdClearDepth(state->backend, depth);
	rtval_report_error("rtCmdClearDepth");
}

void rtval_command_buffer_clear_stencil(struct rtval_command_buffer* cb, u32 stencil) {
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	if (!state) { RTVAL_DROP("rtCmdClearStencil: invalid handle"); return; }
	if (!rtval_cb_rendering(state, "rtCmdClearStencil")) { return; }
	rtval_next_rtCmdClearStencil(state->backend, stencil);
	rtval_report_error("rtCmdClearStencil");
}

void rtval_command_buffer_use_graphics_program(struct rtval_command_buffer* cb, struct rtval_graphics_program* program) {
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	if (!state) { RTVAL_DROP("rtCmdUseGraphicsProgram: invalid command buffer"); return; }
	if (!rtval_cb_rendering(state, "rtCmdUseGraphicsProgram")) { return; }
	struct rtval_graphics_program* prog_state = RTVAL_PAYLOAD(program, struct rtval_graphics_program);
	if (!prog_state) { RTVAL_DROP("rtCmdUseGraphicsProgram: invalid program"); return; }
	rtval_next_rtCmdUseGraphicsProgram(state->backend, prog_state->backend);
	rtval_report_error("rtCmdUseGraphicsProgram");
}

void rtval_command_buffer_set_scissor(struct rtval_command_buffer* cb, u32 x, u32 y, u32 width, u32 height) {
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	if (!state) { RTVAL_DROP("rtCmdSetScissor: invalid handle"); return; }
	if (!rtval_cb_rendering(state, "rtCmdSetScissor")) { return; }
	rtval_next_rtCmdSetScissor(state->backend, x, y, width, height);
	rtval_report_error("rtCmdSetScissor");
}

void rtval_command_buffer_use_compute_program(struct rtval_command_buffer* cb, struct rtval_compute_program* program) {
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	if (!state) { RTVAL_DROP("rtCmdUseComputeProgram: invalid command buffer"); return; }
	if (!rtval_cb_recording(state, "rtCmdUseComputeProgram")) { return; }
	if (state->rendering) { RTVAL_DROP("rtCmdUseComputeProgram: command buffer is rendering"); return; }
	struct rtval_compute_program* prog_state = RTVAL_PAYLOAD(program, struct rtval_compute_program);
	if (!prog_state) { RTVAL_DROP("rtCmdUseComputeProgram: invalid program"); return; }
	rtval_next_rtCmdUseComputeProgram(state->backend, prog_state->backend);
	rtval_report_error("rtCmdUseComputeProgram");
}

void rtval_command_buffer_uniform_buffer(struct rtval_command_buffer* cb, rt_uniform_location location, struct rtval_buffer* buffer, u64 offset, u64 size) {
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	if (!state) { RTVAL_DROP("rtCmdUniformBuffer: invalid command buffer"); return; }
	if (!rtval_cb_rendering(state, "rtCmdUniformBuffer")) { return; }
	if (!location) { RTVAL_DROP("rtCmdUniformBuffer: NULL location"); return; }
	struct rtval_buffer* buf_state = RTVAL_PAYLOAD(buffer, struct rtval_buffer);
	if (!buf_state) { RTVAL_DROP("rtCmdUniformBuffer: invalid buffer"); return; }
	if (size == 0) { RTVAL_DROP("rtCmdUniformBuffer: zero size"); return; }
	rtval_next_rtCmdUniformBuffer(state->backend, location, buf_state->backend, offset, size);
	rtval_report_error("rtCmdUniformBuffer");
}

void rtval_command_buffer_uniform_texture(struct rtval_command_buffer* cb, rt_uniform_location location, struct rtval_texture_view* view) {
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	if (!state) { RTVAL_DROP("rtCmdUniformTexture: invalid command buffer"); return; }
	if (!rtval_cb_rendering(state, "rtCmdUniformTexture")) { return; }
	if (!location) { RTVAL_DROP("rtCmdUniformTexture: NULL location"); return; }
	struct rtval_texture_view* view_state = RTVAL_PAYLOAD(view, struct rtval_texture_view);
	if (!view_state) { RTVAL_DROP("rtCmdUniformTexture: invalid view"); return; }
	rtval_next_rtCmdUniformTexture(state->backend, location, view_state->backend);
	rtval_report_error("rtCmdUniformTexture");
}

void rtval_command_buffer_storage_buffer(struct rtval_command_buffer* cb, u32 binding, struct rtval_buffer* buffer, u64 offset, u64 size) {
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	if (!state) { RTVAL_DROP("rtCmdStorageBuffer: invalid command buffer"); return; }
	if (!rtval_cb_recording(state, "rtCmdStorageBuffer")) { return; }
	struct rtval_buffer* buf_state = RTVAL_PAYLOAD(buffer, struct rtval_buffer);
	if (!buf_state) { RTVAL_DROP("rtCmdStorageBuffer: invalid buffer"); return; }
	if (size == 0) { RTVAL_DROP("rtCmdStorageBuffer: zero size"); return; }
	rtval_next_rtCmdStorageBuffer(state->backend, binding, buf_state->backend, offset, size);
	rtval_report_error("rtCmdStorageBuffer");
}

void rtval_command_buffer_storage_texture(struct rtval_command_buffer* cb, u32 binding, struct rtval_texture_view* view) {
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	if (!state) { RTVAL_DROP("rtCmdStorageTexture: invalid command buffer"); return; }
	if (!rtval_cb_recording(state, "rtCmdStorageTexture")) { return; }
	struct rtval_texture_view* view_state = RTVAL_PAYLOAD(view, struct rtval_texture_view);
	if (!view_state) { RTVAL_DROP("rtCmdStorageTexture: invalid view"); return; }
	rtval_next_rtCmdStorageTexture(state->backend, binding, view_state->backend);
	rtval_report_error("rtCmdStorageTexture");
}

void rtval_command_buffer_compute_barrier(struct rtval_command_buffer* cb) {
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	if (!state) { RTVAL_DROP("rtCmdComputeBarrier: invalid handle"); return; }
	if (!rtval_cb_recording(state, "rtCmdComputeBarrier")) { return; }
	if (state->rendering) { RTVAL_DROP("rtCmdComputeBarrier: command buffer is rendering"); return; }
	rtval_next_rtCmdComputeBarrier(state->backend);
	rtval_report_error("rtCmdComputeBarrier");
}

void rtval_command_buffer_bind_vertex_buffer(struct rtval_command_buffer* cb, struct rtval_buffer* buffer, u64 offset) {
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	if (!state) { RTVAL_DROP("rtCmdBindVertexBuffer: invalid command buffer"); return; }
	if (!rtval_cb_rendering(state, "rtCmdBindVertexBuffer")) { return; }
	struct rtval_buffer* buf_state = RTVAL_PAYLOAD(buffer, struct rtval_buffer);
	if (!buf_state) { RTVAL_DROP("rtCmdBindVertexBuffer: invalid buffer"); return; }
	rtval_next_rtCmdBindVertexBuffer(state->backend, buf_state->backend, offset);
	rtval_report_error("rtCmdBindVertexBuffer");
}

void rtval_command_buffer_draw(struct rtval_command_buffer* cb, u32 vertex_count, u32 first_vertex) {
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	if (!state) { RTVAL_DROP("rtCmdDraw: invalid handle"); return; }
	if (!rtval_cb_rendering(state, "rtCmdDraw")) { return; }
	if (vertex_count == 0) { RTVAL_DROP("rtCmdDraw: zero vertex count"); return; }
	rtval_next_rtCmdDraw(state->backend, vertex_count, first_vertex);
	rtval_report_error("rtCmdDraw");
}

void rtval_command_buffer_dispatch(struct rtval_command_buffer* cb, u32 group_count_x, u32 group_count_y, u32 group_count_z) {
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	if (!state) { RTVAL_DROP("rtCmdDispatch: invalid handle"); return; }
	if (!rtval_cb_recording(state, "rtCmdDispatch")) { return; }
	if (state->rendering) { RTVAL_DROP("rtCmdDispatch: command buffer is rendering"); return; }
	if (!group_count_x || !group_count_y || !group_count_z) { RTVAL_DROP("rtCmdDispatch: zero group count"); return; }
	rtval_next_rtCmdDispatch(state->backend, group_count_x, group_count_y, group_count_z);
	rtval_report_error("rtCmdDispatch");
}

void rtval_command_buffer_end_rendering(struct rtval_command_buffer* cb) {
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	if (!state) { RTVAL_DROP("rtCmdEndRendering: invalid handle"); return; }
	if (!rtval_cb_rendering(state, "rtCmdEndRendering")) { return; }
	state->rendering = false;
	rtval_next_rtCmdEndRendering(state->backend);
	rtval_report_error("rtCmdEndRendering");
}

void rtval_command_buffer_end(struct rtval_command_buffer* cb) {
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	if (!state) { RTVAL_DROP("rtCmdEnd: invalid handle"); return; }
	if (!state->queue)     { RTVAL_DROP("rtCmdEnd: command buffer has no queue");   return; }
	if (!state->recording) { RTVAL_DROP("rtCmdEnd: command buffer is not recording"); return; }
	if (state->rendering)  { RTVAL_DROP("rtCmdEnd: command buffer is still rendering"); return; }

	state->recording = false;
	rtval_next_rtCmdEnd(state->backend);
	rtval_report_error("rtCmdEnd");
}

bool rtval_command_buffer_ready_for_submit(struct rtval_command_buffer* cb, struct rtval_queue* queue) {
	struct rtval_command_buffer* state = RTVAL_PAYLOAD(cb, struct rtval_command_buffer);
	if (!state)                           { RTVAL_DROP("rtQueueSubmit: invalid command buffer"); return false; }
	struct rtval_queue* queue_state = RTVAL_PAYLOAD(queue, struct rtval_queue);
	if (!queue_state)                     { RTVAL_DROP("rtQueueSubmit: invalid queue");          return false; }
	if (!state->queue)                    { RTVAL_DROP("rtQueueSubmit: command buffer has no queue"); return false; }
	if (state->queue != queue)            { RTVAL_DROP("rtQueueSubmit: queue does not match command buffer queue"); return false; }
	if (state->recording)                 { RTVAL_DROP("rtQueueSubmit: command buffer is still recording"); return false; }
	return true;
}

#undef RTVAL_DROP
