#include "graphics_program.h"

#include "context.h"
#include "error.h"
#include "execution.h"

#include <stdlib.h>
#include <string.h>

static bool rtgl_graphics_program_location_binding(const char* name, u32* binding, rtgl_uniform_location_kind* kind) {
	if (strcmp(name, "WorldDraw") == 0 || strcmp(name, "OverlayDraw") == 0 || strcmp(name, "UiDraw") == 0) {
		*binding = 0;
		*kind = RTGL_UNIFORM_LOCATION_UNIFORM_BUFFER;
		return true;
	}
	if (strcmp(name, "MaterialAtlas") == 0 || strcmp(name, "EntityAtlas") == 0) {
		*binding = 1;
		*kind = RTGL_UNIFORM_LOCATION_TEXTURE;
		return true;
	}
	if (strcmp(name, "Cells") == 0) {
		*binding = 1;
		*kind = RTGL_UNIFORM_LOCATION_STORAGE_BUFFER;
		return true;
	}
	if (strcmp(name, "Biomes") == 0) {
		*binding = 2;
		*kind = RTGL_UNIFORM_LOCATION_STORAGE_BUFFER;
		return true;
	}
	if (strcmp(name, "Materials") == 0) {
		*binding = 3;
		*kind = RTGL_UNIFORM_LOCATION_STORAGE_BUFFER;
		return true;
	}
	if (strcmp(name, "MaterialFrames") == 0) {
		*binding = 4;
		*kind = RTGL_UNIFORM_LOCATION_STORAGE_BUFFER;
		return true;
	}
	return false;
}

rt_graphics_program rtGraphicsProgramCreate(void) {
	return rtgl_graphics_program_to_handle(rtgl_graphics_program_create(rtgl_get_current_context()));
}

void rtGraphicsProgramDestroy(rt_graphics_program program) {
	rtgl_graphics_program_destroy(rtgl_get_current_context(), rtgl_graphics_program_from_handle(program));
}

void rtGraphicsProgramLayout(rt_graphics_program program, const rt_vertex_layout* layout) {
	struct rtgl_graphics_program* internal = rtgl_graphics_program_from_handle(program);
	if (!layout || !layout->attributes || layout->attribute_count == 0) {
		internal->vertex_layout = (rt_vertex_layout){ 0 };
		return;
	}
	if (layout->attribute_count > RTGL_MAX_VERTEX_ATTRIBUTES) {
		rtgl_throwf(RT_IMPROPER_USAGE, "too many vertex attributes");
		return;
	}
	memcpy(internal->vertex_attributes, layout->attributes, sizeof(layout->attributes[0]) * layout->attribute_count);
	internal->vertex_layout.stride = layout->stride;
	internal->vertex_layout.attributes = internal->vertex_attributes;
	internal->vertex_layout.attribute_count = layout->attribute_count;
}

void rtGraphicsProgramSource(rt_graphics_program program, u64 size, const void* data) {
	struct rtgl_graphics_program* internal = rtgl_graphics_program_from_handle(program);
	free(internal->source_bytes);
	internal->source_bytes = NULL;
	internal->source_size = 0;
	if (!data || size == 0) {
		return;
	}
	internal->source_bytes = (u08*)malloc((usize)size);
	RTGL_CHECK_ALLOC(internal->source_bytes, (usize)size, "OpenGL RTSL program source");
	if (!internal->source_bytes) {
		return;
	}
	memcpy(internal->source_bytes, data, (usize)size);
	internal->source_size = size;
}

void rtGraphicsProgramRasterState(rt_graphics_program program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode) {
	struct rtgl_graphics_program* internal = rtgl_graphics_program_from_handle(program);
	internal->cull_mode = cull_mode;
	internal->front_face = front_face;
	internal->fill_mode = fill_mode;
}

void rtGraphicsProgramBlendState(rt_graphics_program program, bool enabled, enum rt_blend_factor src_color, enum rt_blend_factor dst_color, enum rt_blend_op color_op, enum rt_blend_factor src_alpha, enum rt_blend_factor dst_alpha, enum rt_blend_op alpha_op) {
	struct rtgl_graphics_program* internal = rtgl_graphics_program_from_handle(program);
	internal->blend_enabled = enabled;
	internal->src_color_blend = src_color;
	internal->dst_color_blend = dst_color;
	internal->color_blend_op = color_op;
	internal->src_alpha_blend = src_alpha;
	internal->dst_alpha_blend = dst_alpha;
	internal->alpha_blend_op = alpha_op;
}

void rtGraphicsProgramFinalize(rt_graphics_program program) {
	struct rtgl_graphics_program* internal = rtgl_graphics_program_from_handle(program);
	rtgl_execution_graphics_program_create(rtgl_get_current_context(), internal);
	internal->finalized = internal->gl_program != 0;
}

void rtGraphicsProgramReset(rt_graphics_program program) {
	struct rtgl_graphics_program* internal = rtgl_graphics_program_from_handle(program);
	rtgl_graphics_program_destroy_program(rtgl_get_current_context(), internal);
	internal->finalized = false;
}

rt_uniform_location rtGraphicsProgramUniformLocation(rt_graphics_program program, const char* name) {
	struct rtgl_graphics_program* internal = rtgl_graphics_program_from_handle(program);
	if (!internal || !name || !name[0]) {
		return RT_NULL_HANDLE;
	}
	for (u32 i = 0; i < internal->uniform_location_count; i++) {
		if (strcmp(internal->uniform_locations[i].name, name) == 0) {
			return (rt_uniform_location)&internal->uniform_locations[i];
		}
	}
	if (internal->uniform_location_count >= (u32)(sizeof(internal->uniform_locations) / sizeof(internal->uniform_locations[0]))) {
		rtgl_throwf(RT_OUT_OF_HOST_MEMORY, "too many OpenGL uniform locations");
		return RT_NULL_HANDLE;
	}
	rtgl_uniform_location* location = &internal->uniform_locations[internal->uniform_location_count];
	memset(location, 0, sizeof(*location));
	location->program = internal;
	strncpy(location->name, name, sizeof(location->name) - 1);
	location->gl_location = -1;
	rtgl_uniform_location_kind kind = RTGL_UNIFORM_LOCATION_UNIFORM_BUFFER;
	if (rtgl_graphics_program_location_binding(name, &location->binding, &kind)) {
		location->kind = kind;
	} else {
		location->binding = internal->uniform_location_count;
		location->kind = RTGL_UNIFORM_LOCATION_UNIFORM_BUFFER;
	}
	internal->uniform_location_count++;
	return (rt_uniform_location)location;
}

RTGL_DEFINE_RESOURCE_PRIVATE(graphics_program)

void rtgl_graphics_program_init(struct rtgl_context* ctx, struct rtgl_graphics_program* program) {
	rtgl_init_resource_base(ctx, RTGL_RESOURCE_BASE(program), RTGL_RESOURCE_GRAPHICS_PROGRAM);
	program->cull_mode = RT_CULL_NONE;
	program->front_face = RT_FRONT_FACE_CCW;
	program->fill_mode = RT_FILL_SOLID;
	program->blend_enabled = false;
	program->src_color_blend = RT_BLEND_ONE;
	program->dst_color_blend = RT_BLEND_ZERO;
	program->color_blend_op = RT_BLEND_OP_ADD;
	program->src_alpha_blend = RT_BLEND_ONE;
	program->dst_alpha_blend = RT_BLEND_ZERO;
	program->alpha_blend_op = RT_BLEND_OP_ADD;
}

void rtgl_graphics_program_finish(struct rtgl_graphics_program* program) {
	rtgl_graphics_program_destroy_program(program->base.ctx, program);
	free(program->source_bytes);
	program->source_bytes = NULL;
	program->source_size = 0;
	rtgl_finish_resource_base(RTGL_RESOURCE_BASE(program));
}

void rtgl_graphics_program_prepare(struct rtgl_context* ctx, struct rtgl_graphics_program* program) {
	(void)ctx;
	if (!program || !program->finalized || !program->gl_program) {
		rtgl_throwf(RT_IMPROPER_USAGE, "OpenGL graphics program must be finalized before use");
	}
}

void rtgl_graphics_program_destroy_program(struct rtgl_context* ctx, struct rtgl_graphics_program* program) {
	if (program && program->gl_program) {
		rtgl_execution_graphics_program_delete(ctx, program);
	}
}
