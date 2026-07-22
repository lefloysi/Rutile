#ifndef RTGL_GRAPHICS_PROGRAM_H
#define RTGL_GRAPHICS_PROGRAM_H

#include "glad/gl.h"
#include "resource.h"

#define RTGL_MAX_VERTEX_ATTRIBUTES 16

RTGL_EXTERN_C_ENTER

RTGL_API rt_graphics_program rtGraphicsProgramCreate(void);
RTGL_API void rtGraphicsProgramDestroy(rt_graphics_program program);
RTGL_API void rtGraphicsProgramLayout(rt_graphics_program program, const rt_vertex_layout* layout);
RTGL_API void rtGraphicsProgramSource(rt_graphics_program program, u64 size, const void* data);
RTGL_API void rtGraphicsProgramRasterState(rt_graphics_program program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode);
RTGL_API void rtGraphicsProgramBlendState(rt_graphics_program program, bool enabled, enum rt_blend_factor src_color, enum rt_blend_factor dst_color, enum rt_blend_op color_op, enum rt_blend_factor src_alpha, enum rt_blend_factor dst_alpha, enum rt_blend_op alpha_op);
RTGL_API void rtGraphicsProgramFinalize(rt_graphics_program program);
RTGL_API void rtGraphicsProgramReset(rt_graphics_program program);
RTGL_API rt_uniform_location rtGraphicsProgramUniformLocation(rt_graphics_program program, const char* name);

typedef enum rtgl_uniform_location_kind {
	RTGL_UNIFORM_LOCATION_UNIFORM_BUFFER,
	RTGL_UNIFORM_LOCATION_STORAGE_BUFFER,
	RTGL_UNIFORM_LOCATION_TEXTURE,
} rtgl_uniform_location_kind;

typedef struct rtgl_uniform_location {
	struct rtgl_graphics_program* program;
	char name[64];
	u32 binding;
	GLint gl_location;
	rtgl_uniform_location_kind kind;
} rtgl_uniform_location;

struct rtgl_graphics_program {
	struct rtgl_resource_base base;
	GLuint gl_program;
	u08* source_bytes;
	u64 source_size;
	rtgl_uniform_location uniform_locations[16];
	u32 uniform_location_count;
	rt_vertex_layout vertex_layout;
	rt_vertex_attribute vertex_attributes[RTGL_MAX_VERTEX_ATTRIBUTES];
	enum rt_cull_mode cull_mode;
	enum rt_front_face front_face;
	enum rt_fill_mode fill_mode;
	bool blend_enabled;
	enum rt_blend_factor src_color_blend;
	enum rt_blend_factor dst_color_blend;
	enum rt_blend_op color_blend_op;
	enum rt_blend_factor src_alpha_blend;
	enum rt_blend_factor dst_alpha_blend;
	enum rt_blend_op alpha_blend_op;
	bool finalized;
};
RTGL_DECLARE_NEW_RESOURCE(graphics_program)

void rtgl_graphics_program_prepare(struct rtgl_context* ctx, struct rtgl_graphics_program* program);
void rtgl_graphics_program_destroy_program(struct rtgl_context* ctx, struct rtgl_graphics_program* program);

RTGL_EXTERN_C_EXIT
#endif /* RTGL_GRAPHICS_PROGRAM_H */
