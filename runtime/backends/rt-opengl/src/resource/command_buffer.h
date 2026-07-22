#ifndef RTGL_COMMAND_BUFFER_H
#define RTGL_COMMAND_BUFFER_H

#include "buffer.h"
#include "framebuffer.h"
#include "graphics_program.h"
#include "queue.h"

RTGL_EXTERN_C_ENTER

RTGL_API rt_command_buffer rtCommandBufferCreate(void);
RTGL_API void rtCommandBufferDestroy(rt_command_buffer command_buffer);
RTGL_API void rtCmdBegin(rt_command_buffer command_buffer, rt_queue queue);
RTGL_API void rtCmdBeginRendering(rt_command_buffer command_buffer, rt_framebuffer framebuffer);
RTGL_API void rtCmdClearColor(rt_command_buffer command_buffer, u32 color_index, f32 r, f32 g, f32 b, f32 a);
RTGL_API void rtCmdClearDepth(rt_command_buffer command_buffer, f32 depth);
RTGL_API void rtCmdClearStencil(rt_command_buffer command_buffer, u32 stencil);
RTGL_API void rtCmdUseGraphicsProgram(rt_command_buffer command_buffer, rt_graphics_program program);
RTGL_API void rtCmdSetScissor(rt_command_buffer command_buffer, u32 x, u32 y, u32 width, u32 height);
RTGL_API void rtCmdUniformBuffer(rt_command_buffer command_buffer, rt_uniform_location location, rt_buffer buffer, u64 offset, u64 size);
RTGL_API void rtCmdUniformTexture(rt_command_buffer command_buffer, rt_uniform_location location, rt_texture_view texture_view);
RTGL_API void rtCmdStorageBuffer(rt_command_buffer command_buffer, u32 binding, rt_buffer buffer, u64 offset, u64 size);
RTGL_API void rtCmdStorageTexture(rt_command_buffer command_buffer, u32 binding, rt_texture_view texture_view);
RTGL_API void rtCmdBindVertexBuffer(rt_command_buffer command_buffer, rt_buffer buffer, u64 offset);
RTGL_API void rtCmdDraw(rt_command_buffer command_buffer, u32 vertex_count, u32 first_vertex);
RTGL_API void rtCmdEndRendering(rt_command_buffer command_buffer);
RTGL_API void rtCmdEnd(rt_command_buffer command_buffer);

typedef enum rtgl_recorded_command_kind {
	RTGL_RECORDED_COMMAND_BEGIN_RENDERING,
	RTGL_RECORDED_COMMAND_CLEAR_COLOR,
	RTGL_RECORDED_COMMAND_CLEAR_DEPTH,
	RTGL_RECORDED_COMMAND_USE_GRAPHICS_PROGRAM,
	RTGL_RECORDED_COMMAND_SET_SCISSOR,
	RTGL_RECORDED_COMMAND_UNIFORM_BUFFER,
	RTGL_RECORDED_COMMAND_UNIFORM_TEXTURE,
	RTGL_RECORDED_COMMAND_STORAGE_BUFFER,
	RTGL_RECORDED_COMMAND_BIND_VERTEX_BUFFER,
	RTGL_RECORDED_COMMAND_DRAW,
	RTGL_RECORDED_COMMAND_END_RENDERING,
} rtgl_recorded_command_kind;

typedef struct rtgl_recorded_command {
	rtgl_recorded_command_kind kind;
	union {
		struct {
			struct rtgl_framebuffer* framebuffer;
			struct rtgl_texture_view* color_view;
			struct rtgl_texture_view* depth_view;
		} begin_rendering;
		struct {
			u32 color_index;
			f32 values[4];
		} clear_color;
		struct {
			f32 depth;
		} clear_depth;
		struct {
			struct rtgl_graphics_program* program;
		} use_graphics_program;
		struct {
			u32 x;
			u32 y;
			u32 width;
			u32 height;
		} set_scissor;
		struct {
			rtgl_uniform_location* location;
			struct rtgl_buffer* buffer;
			u64 offset;
			u64 size;
		} uniform_buffer;
		struct {
			rtgl_uniform_location* location;
			struct rtgl_texture_view* texture_view;
		} uniform_texture;
		struct {
			u32 binding;
			struct rtgl_buffer* buffer;
			u64 offset;
			u64 size;
		} storage_buffer;
		struct {
			struct rtgl_buffer* buffer;
			u64 offset;
		} bind_vertex_buffer;
		struct {
			u32 vertex_count;
			u32 first_vertex;
		} draw;
	};
} rtgl_recorded_command;

struct rtgl_command_buffer {
	struct rtgl_resource_base base;
	struct rtgl_queue* queue;
	rtgl_recorded_command* commands;
	u32 command_count;
	u32 command_capacity;
	bool recording;
	bool rendering;
};
RTGL_DECLARE_NEW_RESOURCE(command_buffer)

struct rtgl_timepoint rtgl_command_buffer_submit(struct rtgl_context* ctx, struct rtgl_queue* queue, struct rtgl_command_buffer* command_buffer);
void rtgl_command_buffer_execute(struct rtgl_context* ctx, struct rtgl_command_buffer* command_buffer, struct rtgl_queue* queue, u64 complete_value);
void rtgl_command_buffer_clear_recorded_resources(struct rtgl_command_buffer* command_buffer);

RTGL_EXTERN_C_EXIT
#endif /* RTGL_COMMAND_BUFFER_H */
