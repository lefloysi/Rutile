#include "graphics_program.h"
#include "logger.h"

#define RTVAL_DROP(message) rtval_printf("[validation] %s, dropping call\n", message)

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT rt_graphics_program rtGraphicsProgramCreate(void) {
	return rtval_graphics_program_to_handle(rtval_graphics_program_create());
}

RT_EXPORT void rtGraphicsProgramDestroy(rt_graphics_program program) {
	rtval_graphics_program_destroy(rtval_graphics_program_from_handle(program));
}

RT_EXPORT void rtGraphicsProgramLayout(rt_graphics_program program, const rt_vertex_layout* layout) {
	rtval_graphics_program_layout(rtval_graphics_program_from_handle(program), layout);
}
RT_EXPORT void rtGraphicsProgramSource(rt_graphics_program program, u64 size, const void* data) {
	rtval_graphics_program_source(rtval_graphics_program_from_handle(program), size, data);
}

RT_EXPORT void rtGraphicsProgramRasterState(rt_graphics_program program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode) {
	rtval_graphics_program_raster_state(rtval_graphics_program_from_handle(program), cull_mode, front_face, fill_mode);
}

RT_EXPORT void rtGraphicsProgramBlendState(rt_graphics_program program, bool enabled, enum rt_blend_factor src_color, enum rt_blend_factor dst_color, enum rt_blend_op color_op, enum rt_blend_factor src_alpha, enum rt_blend_factor dst_alpha, enum rt_blend_op alpha_op) {
	rtval_graphics_program_blend_state(rtval_graphics_program_from_handle(program), enabled, src_color, dst_color, color_op, src_alpha, dst_alpha, alpha_op);
}

RT_EXPORT void rtGraphicsProgramLink(rt_graphics_program program) {
	rtval_graphics_program_link(rtval_graphics_program_from_handle(program));
}

RT_EXPORT rt_uniform_location rtGraphicsProgramUniformLocation(rt_graphics_program program, const char* name) {
	return rtval_graphics_program_uniform_location(rtval_graphics_program_from_handle(program), name);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtval_graphics_program* rtval_graphics_program_create(void) {
	rt_graphics_program backend = rtval_next_rtGraphicsProgramCreate();
	if (!backend) {
		rtval_report_error("rtGraphicsProgramCreate");
		return NULL;
	}
	struct rtval_graphics_program* handle = rtval_handle_create(RTVAL_HANDLE_TYPE_GRAPHICS_PROGRAM);
	if (!handle) {
		rtval_next_rtGraphicsProgramDestroy(backend);
		return NULL;
	}
	struct rtval_graphics_program* state = RTVAL_PAYLOAD(handle, struct rtval_graphics_program);
	state->backend = backend;
	rtval_report_error("rtGraphicsProgramCreate");
	return handle;
}

void rtval_graphics_program_destroy(struct rtval_graphics_program* program) {
	if (!program) {
		return;
	}
	struct rtval_graphics_program* state = RTVAL_PAYLOAD(program, struct rtval_graphics_program);
	if (!state) {
		RTVAL_DROP("rtGraphicsProgramDestroy: null handle");
		return;
	}
	rtval_next_rtGraphicsProgramDestroy(state->backend);
	rtval_handle_destroy(program);
}

void rtval_graphics_program_layout(struct rtval_graphics_program* program, const rt_vertex_layout* layout) {
	struct rtval_graphics_program* state = RTVAL_PAYLOAD(program, struct rtval_graphics_program);
	if (!state) {
		RTVAL_DROP("rtGraphicsProgramLayout: null handle");
		return;
	}
	if (layout && layout->attribute_count && !layout->attributes) {
		RTVAL_DROP("rtGraphicsProgramLayout: missing attributes");
		return;
	}

	rtval_next_rtGraphicsProgramLayout(state->backend, layout);
	rtval_report_error("rtGraphicsProgramLayout");
}

void rtval_graphics_program_source(struct rtval_graphics_program* program, u64 size, const void* data) {
	struct rtval_graphics_program* state = RTVAL_PAYLOAD(program, struct rtval_graphics_program);
	if (!state) {
		RTVAL_DROP("rtGraphicsProgramSource: null handle");
		return;
	}
	if (!data || size == 0) {
		RTVAL_DROP("rtGraphicsProgramSource: empty source data");
		return;
	}
	rtval_next_rtGraphicsProgramSource(state->backend, size, data);
	rtval_report_error("rtGraphicsProgramSource");
}


void rtval_graphics_program_raster_state(struct rtval_graphics_program* program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode) {
	struct rtval_graphics_program* state = RTVAL_PAYLOAD(program, struct rtval_graphics_program);
	if (!state) {
		RTVAL_DROP("rtGraphicsProgramRasterState: null handle");
		return;
	}

	rtval_next_rtGraphicsProgramRasterState(state->backend, cull_mode, front_face, fill_mode);
	rtval_report_error("rtGraphicsProgramRasterState");
}

void rtval_graphics_program_blend_state(
	struct rtval_graphics_program* program,
	bool enabled,
	enum rt_blend_factor src_color,
	enum rt_blend_factor dst_color,
	enum rt_blend_op color_op,
	enum rt_blend_factor src_alpha,
	enum rt_blend_factor dst_alpha,
	enum rt_blend_op alpha_op
) {
	struct rtval_graphics_program* state = RTVAL_PAYLOAD(program, struct rtval_graphics_program);
	if (!state) {
		RTVAL_DROP("rtGraphicsProgramBlendState: null handle");
		return;
	}

	rtval_next_rtGraphicsProgramBlendState(state->backend, enabled, src_color, dst_color, color_op, src_alpha, dst_alpha, alpha_op);
	rtval_report_error("rtGraphicsProgramBlendState");
}

void rtval_graphics_program_link(struct rtval_graphics_program* program) {
	struct rtval_graphics_program* state = RTVAL_PAYLOAD(program, struct rtval_graphics_program);
	if (!state) {
		RTVAL_DROP("rtGraphicsProgramLink: null handle");
		return;
	}

	rtval_next_rtGraphicsProgramLink(state->backend);
	rtval_report_error("rtGraphicsProgramLink");
}

rt_uniform_location rtval_graphics_program_uniform_location(struct rtval_graphics_program* program, const char* name) {
	struct rtval_graphics_program* state = RTVAL_PAYLOAD(program, struct rtval_graphics_program);
	if (!state) {
		RTVAL_DROP("rtGraphicsProgramUniformLocation: null handle");
		return RT_NULL_HANDLE;
	}
	if (!name) {
		RTVAL_DROP("rtGraphicsProgramUniformLocation: NULL name");
		return RT_NULL_HANDLE;
	}

	rt_uniform_location location = rtval_next_rtGraphicsProgramUniformLocation(state->backend, name);
	rtval_report_error("rtGraphicsProgramUniformLocation");
	return location;
}

#undef RTVAL_DROP
