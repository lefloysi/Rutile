#ifndef RTDX_COMMAND_BUFFER_H
#define RTDX_COMMAND_BUFFER_H

#include "config.h"
#include "resource.h"
#include "resource/buffer.h"
#include "resource/framebuffer.h"
#include "resource/graphics_program.h"
#include "resource/queue.h"

#include <d3d12.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTDX_EXTERN_C_ENTER
RTDX_API rt_command_buffer rtCmdCreate(void);
RTDX_API void rtCmdDestroy(rt_command_buffer command_buffer);
RTDX_API void rtCmdBegin(rt_command_buffer command_buffer, rt_queue queue);
RTDX_API void rtCmdBeginRendering(rt_command_buffer command_buffer, rt_framebuffer framebuffer);
RTDX_API void rtCmdClearColor(rt_command_buffer command_buffer, u32 color_index, f32 r, f32 g, f32 b, f32 a);
RTDX_API void rtCmdClearDepth(rt_command_buffer command_buffer, f32 depth);
RTDX_API void rtCmdClearStencil(rt_command_buffer command_buffer, u32 stencil);
RTDX_API void rtCmdUseGraphicsProgram(rt_command_buffer command_buffer, rt_graphics_program program);
RTDX_API void rtCmdSetScissor(rt_command_buffer command_buffer, u32 x, u32 y, u32 width, u32 height);
RTDX_API void rtCmdUniformBuffer(rt_command_buffer command_buffer, rt_uniform_location location, rt_buffer buffer, u64 offset, u64 size);
RTDX_API void rtCmdUniformTexture(rt_command_buffer command_buffer, rt_uniform_location location, rt_texture_view texture_view);
RTDX_API void rtCmdStorageBuffer(rt_command_buffer command_buffer, u32 binding, rt_buffer buffer, u64 offset, u64 size);
RTDX_API void rtCmdBindVertexBuffer(rt_command_buffer command_buffer, rt_buffer buffer, u64 offset);
RTDX_API void rtCmdDraw(rt_command_buffer command_buffer, u32 vertex_count, u32 first_vertex);
RTDX_API void rtCmdEndRendering(rt_command_buffer command_buffer);
RTDX_API void rtCmdEnd(rt_command_buffer command_buffer);
RTDX_EXTERN_C_EXIT

typedef enum rtdx_uniform_slot_kind {
	RTDX_UNIFORM_SLOT_EMPTY,
	RTDX_UNIFORM_SLOT_BUFFER,
	RTDX_UNIFORM_SLOT_STORAGE_BUFFER,
	RTDX_UNIFORM_SLOT_TEXTURE,
} rtdx_uniform_slot_kind;

typedef struct rtdx_uniform_slot {
	rtdx_uniform_slot_kind kind;
	union {
		struct {
			struct rtdx_buffer_storage* storage;
			u64 offset;
			u64 size;
		} buffer;
		struct {
			struct rtdx_texture_view* view;
		} texture;
	};
} rtdx_uniform_slot;

struct rtdx_command_buffer {
	struct rtdx_resource_base base;
	struct rtdx_command_buffer* active;
	struct rtdx_command_buffer* next;

	ID3D12DescriptorHeap* d3d_srv_heap;
	ID3D12DescriptorHeap* d3d_sampler_heap;
	ID3D12CommandAllocator* d3d_allocator;
	ID3D12GraphicsCommandList* d3d_command_list;

	struct rtdx_queue* queue;
	struct rtdx_framebuffer* framebuffer;
	struct rtdx_graphics_program* graphics_program;
	struct rtdx_texture_view* color_texture_view;
	struct rtdx_texture_view* depth_texture_view;
	struct rtdx_buffer* vertex_buffer;
	struct rtdx_buffer_storage* vertex_buffer_storage;
	rtdx_uniform_slot* uniform_slots;
	struct rtdx_texture_view** recorded_texture_views;

	u32 uniform_slot_count;
	u32 recorded_texture_view_count;
	u32 recorded_texture_view_capacity;
	u32 descriptor_cursor;
	bool recording;
};
RTDX_DECLARE_NEW_RESOURCE(command_buffer)

void rtdx_command_buffer_begin(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, struct rtdx_queue* queue);
void rtdx_command_buffer_begin_rendering(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, struct rtdx_framebuffer* framebuffer);
void rtdx_command_buffer_clear_color(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, u32 color_index, f32 r, f32 g, f32 b, f32 a);
void rtdx_command_buffer_clear_depth(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, f32 depth);
void rtdx_command_buffer_clear_stencil(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, u32 stencil);
void rtdx_command_buffer_use_graphics_program(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, struct rtdx_graphics_program* program);
void rtdx_command_buffer_set_scissor(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, u32 x, u32 y, u32 width, u32 height);
void rtdx_command_buffer_uniform_buffer(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, rt_uniform_location location, struct rtdx_buffer* buffer, u64 offset, u64 size);
void rtdx_command_buffer_uniform_texture(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, rt_uniform_location location, struct rtdx_texture_view* texture_view);
void rtdx_command_buffer_storage_buffer(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, u32 binding, struct rtdx_buffer* buffer, u64 offset, u64 size);
void rtdx_command_buffer_bind_vertex_buffer(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, struct rtdx_buffer* buffer, u64 offset);
void rtdx_command_buffer_draw(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, u32 vertex_count, u32 first_vertex);
void rtdx_command_buffer_end_rendering(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer);
void rtdx_command_buffer_end(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer);
void rtdx_command_buffer_node_retain(struct rtdx_command_buffer* command_buffer);
void rtdx_command_buffer_node_release(struct rtdx_command_buffer* command_buffer);
struct rtdx_command_buffer* rtdx_command_buffer_active_node(struct rtdx_command_buffer* command_buffer);

#endif /* RTDX_COMMAND_BUFFER_H */


