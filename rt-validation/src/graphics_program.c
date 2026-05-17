#include "logger.h"
#include "procs.h"

#define RTVAL_DROP(message) rtval_printf("[validation] %s, dropping call\n", message)

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT rt_graphics_program rtGraphicsProgramCreate(void) {
	return rtval_rtGraphicsProgramCreate();
}
RT_EXPORT void rtGraphicsProgramDestroy(rt_graphics_program program) {
	rtval_rtGraphicsProgramDestroy(program);
}
RT_EXPORT void rtGraphicsProgramVertexLayout(rt_graphics_program program, const rt_vertex_layout* layout) {
	rtval_rtGraphicsProgramVertexLayout(program, layout);
}
RT_EXPORT void rtGraphicsProgramVertexShader(rt_graphics_program program, u64 size, const void* data) {
	rtval_rtGraphicsProgramVertexShader(program, size, data);
}
RT_EXPORT void rtGraphicsProgramFragmentShader(rt_graphics_program program, u64 size, const void* data) {
	rtval_rtGraphicsProgramFragmentShader(program, size, data);
}
RT_EXPORT void rtGraphicsProgramRasterState(rt_graphics_program program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode) {
	rtval_rtGraphicsProgramRasterState(program, cull_mode, front_face, fill_mode);
}
RT_EXPORT void rtGraphicsProgramBlendState(rt_graphics_program program, bool enabled, enum rt_blend_factor src_color, enum rt_blend_factor dst_color, enum rt_blend_op color_op, enum rt_blend_factor src_alpha, enum rt_blend_factor dst_alpha, enum rt_blend_op alpha_op) {
	rtval_rtGraphicsProgramBlendState(program, enabled, src_color, dst_color, color_op, src_alpha, dst_alpha, alpha_op);
}
RT_EXPORT void rtGraphicsProgramLink(rt_graphics_program program) {
	rtval_rtGraphicsProgramLink(program);
}
RT_EXPORT rt_uniform_location rtGraphicsProgramUniformLocation(rt_graphics_program program, const char* name) {
	return rtval_rtGraphicsProgramUniformLocation(program, name);
}
RT_EXPORT rt_compute_program rtComputeProgramCreate(void) {
	return rtval_rtComputeProgramCreate();
}
RT_EXPORT void rtComputeProgramDestroy(rt_compute_program program) {
	rtval_rtComputeProgramDestroy(program);
}
RT_EXPORT void rtComputeProgramShader(rt_compute_program program, u64 size, const void* data) {
	rtval_rtComputeProgramShader(program, size, data);
}
RT_EXPORT void rtComputeProgramLink(rt_compute_program program) {
	rtval_rtComputeProgramLink(program);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_graphics_program rtval_rtGraphicsProgramCreate(void) {
	rt_graphics_program program = rtval_next_rtGraphicsProgramCreate();
	rtval_report_error("rtGraphicsProgramCreate");
	return program;
}

void rtval_rtGraphicsProgramDestroy(rt_graphics_program program) {
	rtval_next_rtGraphicsProgramDestroy(program);
	rtval_report_error("rtGraphicsProgramDestroy");
}

void rtval_rtGraphicsProgramVertexLayout(rt_graphics_program program, const rt_vertex_layout* layout) {
	if (!program) {
		RTVAL_DROP("graphics_program_vertex_layout: NULL program");
		return;
	}
	if (layout && layout->attribute_count && !layout->attributes) {
		RTVAL_DROP("graphics_program_vertex_layout: missing attributes");
		return;
	}
	if (layout && layout->attribute_count && layout->stride == 0) {
		RTVAL_DROP("graphics_program_vertex_layout: zero stride");
		return;
	}

	rtval_next_rtGraphicsProgramVertexLayout(program, layout);
	rtval_report_error("rtGraphicsProgramVertexLayout");
}

void rtval_rtGraphicsProgramVertexShader(rt_graphics_program program, u64 size, const void* data) {
	if (!program) {
		RTVAL_DROP("graphics_program_vertex_shader: NULL program");
		return;
	}
	if (!data || size == 0) {
		RTVAL_DROP("graphics_program_vertex_shader: empty shader data");
		return;
	}

	rtval_next_rtGraphicsProgramVertexShader(program, size, data);
	rtval_report_error("rtGraphicsProgramVertexShader");
}

void rtval_rtGraphicsProgramFragmentShader(rt_graphics_program program, u64 size, const void* data) {
	if (!program) {
		RTVAL_DROP("graphics_program_fragment_shader: NULL program");
		return;
	}
	if (!data || size == 0) {
		RTVAL_DROP("graphics_program_fragment_shader: empty shader data");
		return;
	}

	rtval_next_rtGraphicsProgramFragmentShader(program, size, data);
	rtval_report_error("rtGraphicsProgramFragmentShader");
}

void rtval_rtGraphicsProgramRasterState(rt_graphics_program program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode) {
	if (!program) {
		RTVAL_DROP("graphics_program_raster_state: NULL program");
		return;
	}

	rtval_next_rtGraphicsProgramRasterState(program, cull_mode, front_face, fill_mode);
	rtval_report_error("rtGraphicsProgramRasterState");
}

void rtval_rtGraphicsProgramBlendState(
	rt_graphics_program program,
	bool enabled,
	enum rt_blend_factor src_color,
	enum rt_blend_factor dst_color,
	enum rt_blend_op color_op,
	enum rt_blend_factor src_alpha,
	enum rt_blend_factor dst_alpha,
	enum rt_blend_op alpha_op) {
	if (!program) {
		RTVAL_DROP("graphics_program_blend_state: NULL program");
		return;
	}

	rtval_next_rtGraphicsProgramBlendState(program, enabled, src_color, dst_color, color_op,
										   src_alpha, dst_alpha, alpha_op);
	rtval_report_error("rtGraphicsProgramBlendState");
}

void rtval_rtGraphicsProgramLink(rt_graphics_program program) {
	if (!program) {
		RTVAL_DROP("graphics_program_link: NULL program");
		return;
	}

	rtval_next_rtGraphicsProgramLink(program);
	rtval_report_error("rtGraphicsProgramLink");
}

rt_uniform_location rtval_rtGraphicsProgramUniformLocation(rt_graphics_program program, const char* name) {
	if (!program) {
		RTVAL_DROP("graphics_program_uniform_location: NULL program");
		return RT_NULL_HANDLE;
	}
	if (!name) {
		RTVAL_DROP("graphics_program_uniform_location: NULL name");
		return RT_NULL_HANDLE;
	}

	rt_uniform_location location = rtval_next_rtGraphicsProgramUniformLocation(program, name);
	rtval_report_error("rtGraphicsProgramUniformLocation");
	return location;
}

rt_compute_program rtval_rtComputeProgramCreate(void) {
	rt_compute_program program = rtval_next_rtComputeProgramCreate();
	rtval_report_error("rtComputeProgramCreate");
	return program;
}

void rtval_rtComputeProgramDestroy(rt_compute_program program) {
	rtval_next_rtComputeProgramDestroy(program);
	rtval_report_error("rtComputeProgramDestroy");
}

void rtval_rtComputeProgramShader(rt_compute_program program, u64 size, const void* data) {
	if (!program) {
		RTVAL_DROP("compute_program_shader: NULL program");
		return;
	}
	if (!data || size == 0) {
		RTVAL_DROP("compute_program_shader: empty shader data");
		return;
	}
	rtval_next_rtComputeProgramShader(program, size, data);
	rtval_report_error("rtComputeProgramShader");
}

void rtval_rtComputeProgramLink(rt_compute_program program) {
	if (!program) {
		RTVAL_DROP("compute_program_link: NULL program");
		return;
	}
	rtval_next_rtComputeProgramLink(program);
	rtval_report_error("rtComputeProgramLink");
}

#undef RTVAL_DROP
