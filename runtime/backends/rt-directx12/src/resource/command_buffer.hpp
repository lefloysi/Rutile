#pragma once

#include "config.hpp"
#include "resource.hpp"
#include "resource/buffer.hpp"
#include "resource/framebuffer.hpp"
#include "resource/graphics_program.hpp"
#include "resource/queue.hpp"

#include <d3d12.h>

RTDX_API rt_command_buffer rtCommandBufferCreate();
RTDX_API void rtCommandBufferDestroy(rt_command_buffer command_buffer);
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

enum class rtdx_uniform_slot_kind {
	empty,
	buffer,
	storage_buffer,
	texture,
};

struct rtdx_uniform_slot {
	rtdx_uniform_slot_kind kind;
	union {
		struct {
			rtdx_buffer_storage* storage;
			u64 offset;
			u64 size;
		} buffer;
		struct {
			rtdx_texture_view* view;
		} texture;
	};
};

struct rtdx_command_buffer {
	rtdx_resource_base base;
	rtdx_command_buffer* active;
	rtdx_command_buffer* next;

	ID3D12DescriptorHeap* d3d_srv_heap;
	ID3D12DescriptorHeap* d3d_sampler_heap;
	ID3D12CommandAllocator* d3d_allocator;
	ID3D12GraphicsCommandList* d3d_command_list;

	rtdx_queue* queue;
	rtdx_framebuffer* framebuffer;
	rtdx_graphics_program* graphics_program;
	rtdx_texture_view* color_texture_view;
	rtdx_texture_view* depth_texture_view;
	rtdx_buffer* vertex_buffer;
	rtdx_buffer_storage* vertex_buffer_storage;
	rtdx_uniform_slot* uniform_slots;
	rtdx_texture_view** recorded_texture_views;

	u32 uniform_slot_count;
	u32 recorded_texture_view_count;
	u32 recorded_texture_view_capacity;
	u32 descriptor_cursor;
	bool recording;
};
RTDX_DECLARE_NEW_RESOURCE(command_buffer)

void rtdx_command_buffer_begin(rtdx_context* ctx, rtdx_command_buffer* command_buffer, rtdx_queue* queue);
void rtdx_command_buffer_begin_rendering(rtdx_context* ctx, rtdx_command_buffer* command_buffer, rtdx_framebuffer* framebuffer);
void rtdx_command_buffer_clear_color(rtdx_context* ctx, rtdx_command_buffer* command_buffer, u32 color_index, f32 r, f32 g, f32 b, f32 a);
void rtdx_command_buffer_clear_depth(rtdx_context* ctx, rtdx_command_buffer* command_buffer, f32 depth);
void rtdx_command_buffer_clear_stencil(rtdx_context* ctx, rtdx_command_buffer* command_buffer, u32 stencil);
void rtdx_command_buffer_use_graphics_program(rtdx_context* ctx, rtdx_command_buffer* command_buffer, rtdx_graphics_program* program);
void rtdx_command_buffer_set_scissor(rtdx_context* ctx, rtdx_command_buffer* command_buffer, u32 x, u32 y, u32 width, u32 height);
void rtdx_command_buffer_uniform_buffer(rtdx_context* ctx, rtdx_command_buffer* command_buffer, rt_uniform_location location, rtdx_buffer* buffer, u64 offset, u64 size);
void rtdx_command_buffer_uniform_texture(rtdx_context* ctx, rtdx_command_buffer* command_buffer, rt_uniform_location location, rtdx_texture_view* texture_view);
void rtdx_command_buffer_storage_buffer(rtdx_context* ctx, rtdx_command_buffer* command_buffer, u32 binding, rtdx_buffer* buffer, u64 offset, u64 size);
void rtdx_command_buffer_bind_vertex_buffer(rtdx_context* ctx, rtdx_command_buffer* command_buffer, rtdx_buffer* buffer, u64 offset);
void rtdx_command_buffer_draw(rtdx_context* ctx, rtdx_command_buffer* command_buffer, u32 vertex_count, u32 first_vertex);
void rtdx_command_buffer_end_rendering(rtdx_context* ctx, rtdx_command_buffer* command_buffer);
void rtdx_command_buffer_end(rtdx_context* ctx, rtdx_command_buffer* command_buffer);
void rtdx_command_buffer_node_retain(rtdx_command_buffer* command_buffer);
void rtdx_command_buffer_node_release(rtdx_command_buffer* command_buffer);
rtdx_command_buffer* rtdx_command_buffer_active_node(rtdx_command_buffer* command_buffer);
