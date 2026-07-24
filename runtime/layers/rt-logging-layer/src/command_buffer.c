#include "procs.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT rt_command_buffer rtCommandBufferCreate(void) {
	return rtlog_rtCommandBufferCreate();
}
RT_EXPORT void rtCommandBufferDestroy(rt_command_buffer command_buffer) {
	rtlog_rtCommandBufferDestroy(command_buffer);
}
RT_EXPORT void rtCmdBegin(rt_command_buffer command_buffer, rt_queue queue) {
	rtlog_rtCmdBegin(command_buffer, queue);
}
RT_EXPORT void rtCmdBeginRendering(rt_command_buffer command_buffer, rt_framebuffer framebuffer) {
	rtlog_rtCmdBeginRendering(command_buffer, framebuffer);
}
RT_EXPORT void rtCmdClearColor(rt_command_buffer command_buffer, u32 color_index, f32 r, f32 g, f32 b, f32 a) {
	rtlog_rtCmdClearColor(command_buffer, color_index, r, g, b, a);
}
RT_EXPORT void rtCmdClearDepth(rt_command_buffer command_buffer, f32 depth) {
	rtlog_rtCmdClearDepth(command_buffer, depth);
}
RT_EXPORT void rtCmdClearStencil(rt_command_buffer command_buffer, u32 stencil) {
	rtlog_rtCmdClearStencil(command_buffer, stencil);
}
RT_EXPORT void rtCmdUseGraphicsProgram(rt_command_buffer command_buffer, rt_graphics_program program) {
	rtlog_rtCmdUseGraphicsProgram(command_buffer, program);
}
RT_EXPORT void rtCmdSetScissor(rt_command_buffer command_buffer, u32 x, u32 y, u32 width, u32 height) {
	rtlog_rtCmdSetScissor(command_buffer, x, y, width, height);
}
RT_EXPORT void rtCmdUseComputeProgram(rt_command_buffer command_buffer, rt_compute_program program) {
	rtlog_rtCmdUseComputeProgram(command_buffer, program);
}
RT_EXPORT void rtCmdUniformBuffer(rt_command_buffer command_buffer, rt_uniform_location location, rt_buffer buffer, u64 offset, u64 size) {
	rtlog_rtCmdUniformBuffer(command_buffer, location, buffer, offset, size);
}
RT_EXPORT void rtCmdUniformTexture(rt_command_buffer command_buffer, rt_uniform_location location, rt_texture_view texture_view) {
	rtlog_rtCmdUniformTexture(command_buffer, location, texture_view);
}
RT_EXPORT void rtCmdStorageBuffer(rt_command_buffer command_buffer, rt_uniform_location location, rt_buffer buffer, u64 offset, u64 size) {
	rtlog_rtCmdStorageBuffer(command_buffer, location, buffer, offset, size);
}
RT_EXPORT void rtCmdStorageTexture(rt_command_buffer command_buffer, u32 binding, rt_texture_view texture_view) {
	rtlog_rtCmdStorageTexture(command_buffer, binding, texture_view);
}
RT_EXPORT void rtCmdComputeBarrier(rt_command_buffer command_buffer) {
	rtlog_rtCmdComputeBarrier(command_buffer);
}
RT_EXPORT void rtCmdBindVertexBuffer(rt_command_buffer command_buffer, rt_buffer buffer, u64 offset) {
	rtlog_rtCmdBindVertexBuffer(command_buffer, buffer, offset);
}
RT_EXPORT void rtCmdDraw(rt_command_buffer command_buffer, u32 vertex_count, u32 first_vertex) {
	rtlog_rtCmdDraw(command_buffer, vertex_count, first_vertex);
}
RT_EXPORT void rtCmdDispatch(rt_command_buffer command_buffer, u32 group_count_x, u32 group_count_y, u32 group_count_z) {
	rtlog_rtCmdDispatch(command_buffer, group_count_x, group_count_y, group_count_z);
}
RT_EXPORT void rtCmdEndRendering(rt_command_buffer command_buffer) {
	rtlog_rtCmdEndRendering(command_buffer);
}
RT_EXPORT void rtCmdEnd(rt_command_buffer command_buffer) {
	rtlog_rtCmdEnd(command_buffer);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_command_buffer rtlog_rtCommandBufferCreate(void) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtCommandBufferCreate()\n");

	rt_command_buffer result = next_rtCommandBufferCreate();
	rtlog_printf("rtCommandBufferCreate -> %s [%s]\n", rtlog_pointer(result), rtlog_elapsed(start_ns));
	rtlog_error("rtCommandBufferCreate");
	return result;
}

void rtlog_rtCommandBufferDestroy(rt_command_buffer command_buffer) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtCommandBufferDestroy(command_buffer=%s)\n", rtlog_pointer(command_buffer));
	next_rtCommandBufferDestroy(command_buffer);
	rtlog_printf("rtCommandBufferDestroy completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtCommandBufferDestroy");
}

void rtlog_rtCmdBegin(rt_command_buffer command_buffer, rt_queue queue) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtCmdBegin(command_buffer=%s, queue=%s)\n", rtlog_pointer(command_buffer), rtlog_pointer(queue));
	next_rtCmdBegin(command_buffer, queue);
	rtlog_printf("rtCmdBegin completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtCmdBegin");
}

void rtlog_rtCmdBeginRendering(rt_command_buffer command_buffer, rt_framebuffer framebuffer) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtCmdBeginRendering(command_buffer=%s, framebuffer=%s)\n", rtlog_pointer(command_buffer), rtlog_pointer(framebuffer));
	next_rtCmdBeginRendering(command_buffer, framebuffer);
	rtlog_printf("rtCmdBeginRendering completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtCmdBeginRendering");
}

void rtlog_rtCmdClearColor(rt_command_buffer command_buffer, u32 color_index, f32 r, f32 g, f32 b, f32 a) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtCmdClearColor(command_buffer=%s, color_index=%u, rgba=(%f,%f,%f,%f))\n", rtlog_pointer(command_buffer), color_index, r, g, b, a);
	next_rtCmdClearColor(command_buffer, color_index, r, g, b, a);
	rtlog_printf("rtCmdClearColor completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtCmdClearColor");
}

void rtlog_rtCmdClearDepth(rt_command_buffer command_buffer, f32 depth) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtCmdClearDepth(command_buffer=%s, depth=%f)\n", rtlog_pointer(command_buffer), depth);
	next_rtCmdClearDepth(command_buffer, depth);
	rtlog_printf("rtCmdClearDepth completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtCmdClearDepth");
}

void rtlog_rtCmdClearStencil(rt_command_buffer command_buffer, u32 stencil) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtCmdClearStencil(command_buffer=%s, stencil=%u)\n", rtlog_pointer(command_buffer), stencil);
	next_rtCmdClearStencil(command_buffer, stencil);
	rtlog_printf("rtCmdClearStencil completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtCmdClearStencil");
}

void rtlog_rtCmdUseGraphicsProgram(rt_command_buffer command_buffer, rt_graphics_program program) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtCmdUseGraphicsProgram(command_buffer=%s, program=%s)\n", rtlog_pointer(command_buffer), rtlog_pointer(program));
	next_rtCmdUseGraphicsProgram(command_buffer, program);
	rtlog_printf("rtCmdUseGraphicsProgram completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtCmdUseGraphicsProgram");
}

void rtlog_rtCmdSetScissor(rt_command_buffer command_buffer, u32 x, u32 y, u32 width, u32 height) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtCmdSetScissor(command_buffer=%s, x=%u, y=%u, width=%u, height=%u)\n", rtlog_pointer(command_buffer), x, y, width, height);
	next_rtCmdSetScissor(command_buffer, x, y, width, height);
	rtlog_printf("rtCmdSetScissor completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtCmdSetScissor");
}

void rtlog_rtCmdUseComputeProgram(rt_command_buffer command_buffer, rt_compute_program program) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtCmdUseComputeProgram(command_buffer=%s, program=%s)\n", rtlog_pointer(command_buffer), rtlog_pointer(program));
	next_rtCmdUseComputeProgram(command_buffer, program);
	rtlog_printf("rtCmdUseComputeProgram completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtCmdUseComputeProgram");
}

void rtlog_rtCmdUniformBuffer(rt_command_buffer command_buffer, rt_uniform_location location, rt_buffer buffer, u64 offset, u64 size) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf(
		"rtCmdUniformBuffer(command_buffer=%s, location=%s, buffer=%s, offset=%llu, size=%llu)\n",
		rtlog_pointer(command_buffer),
		rtlog_pointer(location),
		rtlog_pointer(buffer),
		(u64)offset,
		(u64)size
	);
	next_rtCmdUniformBuffer(command_buffer, location, buffer, offset, size);
	rtlog_printf("rtCmdUniformBuffer completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtCmdUniformBuffer");
}

void rtlog_rtCmdUniformTexture(rt_command_buffer command_buffer, rt_uniform_location location, rt_texture_view texture_view) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf(
		"rtCmdUniformTexture(command_buffer=%s, location=%s, texture_view=%s)\n",
		rtlog_pointer(command_buffer),
		rtlog_pointer(location),
		rtlog_pointer(texture_view)
	);
	next_rtCmdUniformTexture(command_buffer, location, texture_view);
	rtlog_printf("rtCmdUniformTexture completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtCmdUniformTexture");
}

void rtlog_rtCmdStorageBuffer(rt_command_buffer command_buffer, rt_uniform_location location, rt_buffer buffer, u64 offset, u64 size) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtCmdStorageBuffer(command_buffer=%s, location=%s, buffer=%s, offset=%llu, size=%llu)\n", rtlog_pointer(command_buffer), rtlog_pointer(location), rtlog_pointer(buffer), (u64)offset, (u64)size);
	next_rtCmdStorageBuffer(command_buffer, location, buffer, offset, size);
	rtlog_printf("rtCmdStorageBuffer completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtCmdStorageBuffer");
}

void rtlog_rtCmdStorageTexture(rt_command_buffer command_buffer, u32 binding, rt_texture_view texture_view) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtCmdStorageTexture(command_buffer=%s, binding=%u, texture_view=%s)\n", rtlog_pointer(command_buffer), binding, rtlog_pointer(texture_view));
	next_rtCmdStorageTexture(command_buffer, binding, texture_view);
	rtlog_printf("rtCmdStorageTexture completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtCmdStorageTexture");
}

void rtlog_rtCmdComputeBarrier(rt_command_buffer command_buffer) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtCmdComputeBarrier(command_buffer=%s)\n", rtlog_pointer(command_buffer));
	next_rtCmdComputeBarrier(command_buffer);
	rtlog_printf("rtCmdComputeBarrier completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtCmdComputeBarrier");
}

void rtlog_rtCmdBindVertexBuffer(rt_command_buffer command_buffer, rt_buffer buffer, u64 offset) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtCmdBindVertexBuffer(command_buffer=%s, buffer=%s, offset=%llu)\n", rtlog_pointer(command_buffer), rtlog_pointer(buffer), (u64)offset);
	next_rtCmdBindVertexBuffer(command_buffer, buffer, offset);
	rtlog_printf("rtCmdBindVertexBuffer completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtCmdBindVertexBuffer");
}

void rtlog_rtCmdDraw(rt_command_buffer command_buffer, u32 vertex_count, u32 first_vertex) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtCmdDraw(command_buffer=%s, vertex_count=%u, first_vertex=%u)\n", rtlog_pointer(command_buffer), vertex_count, first_vertex);
	next_rtCmdDraw(command_buffer, vertex_count, first_vertex);
	rtlog_printf("rtCmdDraw completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtCmdDraw");
}

void rtlog_rtCmdDispatch(rt_command_buffer command_buffer, u32 group_count_x, u32 group_count_y, u32 group_count_z) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtCmdDispatch(command_buffer=%s, groups=(%u, %u, %u))\n", rtlog_pointer(command_buffer), group_count_x, group_count_y, group_count_z);
	next_rtCmdDispatch(command_buffer, group_count_x, group_count_y, group_count_z);
	rtlog_printf("rtCmdDispatch completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtCmdDispatch");
}

void rtlog_rtCmdEndRendering(rt_command_buffer command_buffer) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtCmdEndRendering(command_buffer=%s)\n", rtlog_pointer(command_buffer));
	next_rtCmdEndRendering(command_buffer);
	rtlog_printf("rtCmdEndRendering completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtCmdEndRendering");
}

void rtlog_rtCmdEnd(rt_command_buffer command_buffer) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtCmdEnd(command_buffer=%s)\n", rtlog_pointer(command_buffer));
	next_rtCmdEnd(command_buffer);
	rtlog_printf("rtCmdEnd completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtCmdEnd");
}
