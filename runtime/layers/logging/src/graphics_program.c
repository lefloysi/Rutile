#include "procs.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT rt_graphics_program rtGraphicsProgramCreate(void) {
	return rtlog_rtGraphicsProgramCreate();
}
RT_EXPORT void rtGraphicsProgramDestroy(rt_graphics_program program) {
	rtlog_rtGraphicsProgramDestroy(program);
}
RT_EXPORT void rtGraphicsProgramVertexLayout(rt_graphics_program program, const rt_vertex_layout *layout) {
	rtlog_rtGraphicsProgramVertexLayout(program, layout);
}
RT_EXPORT void rtGraphicsProgramVertexShader(rt_graphics_program program, u64 size, const void *data) {
	rtlog_rtGraphicsProgramVertexShader(program, size, data);
}
RT_EXPORT void rtGraphicsProgramFragmentShader(rt_graphics_program program, u64 size, const void *data) {
	rtlog_rtGraphicsProgramFragmentShader(program, size, data);
}
RT_EXPORT void rtGraphicsProgramRasterState(rt_graphics_program program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode) {
	rtlog_rtGraphicsProgramRasterState(program, cull_mode, front_face, fill_mode);
}
RT_EXPORT void rtGraphicsProgramBlendState(rt_graphics_program program, bool enabled, enum rt_blend_factor src_color, enum rt_blend_factor dst_color, enum rt_blend_op color_op, enum rt_blend_factor src_alpha, enum rt_blend_factor dst_alpha, enum rt_blend_op alpha_op) {
	rtlog_rtGraphicsProgramBlendState(program, enabled, src_color, dst_color, color_op, src_alpha, dst_alpha, alpha_op);
}
RT_EXPORT void rtGraphicsProgramLink(rt_graphics_program program) {
	rtlog_rtGraphicsProgramLink(program);
}
RT_EXPORT rt_uniform_location rtGraphicsProgramUniformLocation(rt_graphics_program program, const char *name) {
	return rtlog_rtGraphicsProgramUniformLocation(program, name);
}
RT_EXPORT rt_compute_program rtComputeProgramCreate(void) {
	return rtlog_rtComputeProgramCreate();
}
RT_EXPORT void rtComputeProgramDestroy(rt_compute_program program) {
	rtlog_rtComputeProgramDestroy(program);
}
RT_EXPORT void rtComputeProgramShader(rt_compute_program program, u64 size, const void *data) {
	rtlog_rtComputeProgramShader(program, size, data);
}
RT_EXPORT void rtComputeProgramLink(rt_compute_program program) {
	rtlog_rtComputeProgramLink(program);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_graphics_program rtlog_rtGraphicsProgramCreate(void) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtGraphicsProgramCreate()\n");
	rt_graphics_program result = next_rtGraphicsProgramCreate();
	rtlog_printf("rtGraphicsProgramCreate -> %s [%s]\n", rtlog_pointer(result), rtlog_elapsed(start_ns));
	rtlog_error("rtGraphicsProgramCreate");
	return result;
}

void rtlog_rtGraphicsProgramDestroy(rt_graphics_program program) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtGraphicsProgramDestroy(program=%s)\n", rtlog_pointer(program));
	next_rtGraphicsProgramDestroy(program);
	rtlog_printf("rtGraphicsProgramDestroy completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtGraphicsProgramDestroy");
}

void rtlog_rtGraphicsProgramVertexLayout(rt_graphics_program program, const rt_vertex_layout *layout) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtGraphicsProgramVertexLayout(program=%s, layout=%s)\n", rtlog_pointer(program), rtlog_pointer(layout));
	next_rtGraphicsProgramVertexLayout(program, layout);
	rtlog_printf("rtGraphicsProgramVertexLayout completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtGraphicsProgramVertexLayout");
}

void rtlog_rtGraphicsProgramVertexShader(rt_graphics_program program, u64 size, const void *data) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtGraphicsProgramVertexShader(program=%s, size=%llu, data=%s)\n", rtlog_pointer(program), (u64)size, rtlog_pointer(data));
	next_rtGraphicsProgramVertexShader(program, size, data);
	rtlog_printf("rtGraphicsProgramVertexShader completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtGraphicsProgramVertexShader");
}

void rtlog_rtGraphicsProgramFragmentShader(rt_graphics_program program, u64 size, const void *data) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtGraphicsProgramFragmentShader(program=%s, size=%llu, data=%s)\n", rtlog_pointer(program), (u64)size, rtlog_pointer(data));
	next_rtGraphicsProgramFragmentShader(program, size, data);
	rtlog_printf("rtGraphicsProgramFragmentShader completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtGraphicsProgramFragmentShader");
}

void rtlog_rtGraphicsProgramRasterState(rt_graphics_program program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtGraphicsProgramRasterState(program=%s, cull=%d, front=%d, fill=%d)\n", rtlog_pointer(program), (int)cull_mode, (int)front_face, (int)fill_mode);
	next_rtGraphicsProgramRasterState(program, cull_mode, front_face, fill_mode);
	rtlog_printf("rtGraphicsProgramRasterState completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtGraphicsProgramRasterState");
}

void rtlog_rtGraphicsProgramBlendState(
	rt_graphics_program program,
	bool enabled,
	enum rt_blend_factor src_color,
	enum rt_blend_factor dst_color,
	enum rt_blend_op color_op,
	enum rt_blend_factor src_alpha,
	enum rt_blend_factor dst_alpha,
	enum rt_blend_op alpha_op
) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtGraphicsProgramBlendState(program=%s, enabled=%d)\n", rtlog_pointer(program), enabled ? 1 : 0);
	next_rtGraphicsProgramBlendState(program, enabled, src_color, dst_color, color_op, src_alpha, dst_alpha, alpha_op);
	rtlog_printf("rtGraphicsProgramBlendState completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtGraphicsProgramBlendState");
}

void rtlog_rtGraphicsProgramLink(rt_graphics_program program) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtGraphicsProgramLink(program=%s)\n", rtlog_pointer(program));
	next_rtGraphicsProgramLink(program);
	rtlog_printf("rtGraphicsProgramLink completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtGraphicsProgramLink");
}

rt_uniform_location rtlog_rtGraphicsProgramUniformLocation(rt_graphics_program program, const char *name) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtGraphicsProgramUniformLocation(program=%s, name=\"%s\")\n", rtlog_pointer(program), name ? name : "<null>");
	rt_uniform_location result = next_rtGraphicsProgramUniformLocation(program, name);
	rtlog_printf("rtGraphicsProgramUniformLocation -> %s [%s]\n", rtlog_pointer(result), rtlog_elapsed(start_ns));
	rtlog_error("rtGraphicsProgramUniformLocation");
	return result;
}

rt_compute_program rtlog_rtComputeProgramCreate(void) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtComputeProgramCreate()\n");
	rt_compute_program result = next_rtComputeProgramCreate();
	rtlog_printf("rtComputeProgramCreate -> %s [%s]\n", rtlog_pointer(result), rtlog_elapsed(start_ns));
	rtlog_error("rtComputeProgramCreate");
	return result;
}

void rtlog_rtComputeProgramDestroy(rt_compute_program program) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtComputeProgramDestroy(program=%s)\n", rtlog_pointer(program));
	next_rtComputeProgramDestroy(program);
	rtlog_printf("rtComputeProgramDestroy completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtComputeProgramDestroy");
}

void rtlog_rtComputeProgramShader(rt_compute_program program, u64 size, const void *data) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtComputeProgramShader(program=%s, size=%llu, data=%s)\n", rtlog_pointer(program), (u64)size, rtlog_pointer(data));
	next_rtComputeProgramShader(program, size, data);
	rtlog_printf("rtComputeProgramShader completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtComputeProgramShader");
}

void rtlog_rtComputeProgramLink(rt_compute_program program) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtComputeProgramLink(program=%s)\n", rtlog_pointer(program));
	next_rtComputeProgramLink(program);
	rtlog_printf("rtComputeProgramLink completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtComputeProgramLink");
}
