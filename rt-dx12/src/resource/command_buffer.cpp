#include "command_buffer.h"
#include "context.h"
#include "error.h"
#include "resource/texture.h"

#include <stdlib.h>
#include <string.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_command_buffer rtCmdCreate(void) {
	struct rtdx_command_buffer* command_buffer = rtdx_command_buffer_create(rtdx_get_current_context());
	return rtdx_command_buffer_to_handle(command_buffer);
}

void rtCmdDestroy(rt_command_buffer command_buffer) {
	rtdx_command_buffer_destroy(rtdx_get_current_context(), rtdx_command_buffer_from_handle(command_buffer));
}

void rtCmdBegin(rt_command_buffer command_buffer, rt_queue queue) {
	rtdx_command_buffer_begin(
		rtdx_get_current_context(),
		rtdx_command_buffer_from_handle(command_buffer),
		rtdx_queue_from_handle(queue));
}

void rtCmdBeginRendering(rt_command_buffer command_buffer, rt_framebuffer framebuffer) {
	rtdx_command_buffer_begin_rendering(
		rtdx_get_current_context(),
		rtdx_command_buffer_from_handle(command_buffer),
		rtdx_framebuffer_from_handle(framebuffer));
}
void rtCmdClearColor(rt_command_buffer command_buffer, u32 color_index, f32 r, f32 g, f32 b, f32 a) {
	rtdx_command_buffer_clear_color(
		rtdx_get_current_context(),
		rtdx_command_buffer_from_handle(command_buffer),
		color_index,
		r,
		g,
		b,
		a);
}
void rtCmdClearDepth(rt_command_buffer command_buffer, f32 depth) {
	rtdx_command_buffer_clear_depth(
		rtdx_get_current_context(),
		rtdx_command_buffer_from_handle(command_buffer),
		depth);
}
void rtCmdClearStencil(rt_command_buffer command_buffer, u32 stencil) {
	rtdx_command_buffer_clear_stencil(
		rtdx_get_current_context(),
		rtdx_command_buffer_from_handle(command_buffer),
		stencil);
}

void rtCmdUseGraphicsProgram(rt_command_buffer command_buffer, rt_graphics_program program) {
	rtdx_command_buffer_use_graphics_program(
		rtdx_get_current_context(),
		rtdx_command_buffer_from_handle(command_buffer),
		rtdx_graphics_program_from_handle(program));
}

void rtCmdSetScissor(rt_command_buffer command_buffer, u32 x, u32 y, u32 width, u32 height) {
	rtdx_command_buffer_set_scissor(
		rtdx_get_current_context(),
		rtdx_command_buffer_from_handle(command_buffer),
		x,
		y,
		width,
		height);
}

void rtCmdUniformBuffer(rt_command_buffer command_buffer, rt_uniform_location location, rt_buffer buffer, u64 offset, u64 size) {
	rtdx_command_buffer_uniform_buffer(
		rtdx_get_current_context(),
		rtdx_command_buffer_from_handle(command_buffer),
		location,
		rtdx_buffer_from_handle(buffer),
		offset,
		size);
}

void rtCmdUniformTexture(rt_command_buffer command_buffer, rt_uniform_location location, rt_texture_view texture_view) {
	rtdx_command_buffer_uniform_texture(
		rtdx_get_current_context(),
		rtdx_command_buffer_from_handle(command_buffer),
		location,
		rtdx_texture_view_from_handle(texture_view));
}

void rtCmdBindVertexBuffer(rt_command_buffer command_buffer, rt_buffer buffer, u64 offset) {
	rtdx_command_buffer_bind_vertex_buffer(
		rtdx_get_current_context(),
		rtdx_command_buffer_from_handle(command_buffer),
		rtdx_buffer_from_handle(buffer),
		offset);
}

void rtCmdDraw(rt_command_buffer command_buffer, u32 vertex_count, u32 first_vertex) {
	rtdx_command_buffer_draw(
		rtdx_get_current_context(),
		rtdx_command_buffer_from_handle(command_buffer),
		vertex_count,
		first_vertex);
}

void rtCmdEndRendering(rt_command_buffer command_buffer) {
	rtdx_command_buffer_end_rendering(
		rtdx_get_current_context(),
		rtdx_command_buffer_from_handle(command_buffer));
}

void rtCmdEnd(rt_command_buffer command_buffer) {
	rtdx_command_buffer_end(
		rtdx_get_current_context(),
		rtdx_command_buffer_from_handle(command_buffer));
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTDX_DEFINE_RESOURCE_PRIVATE(command_buffer)

static void rtdx_command_buffer_release_recorded_resources(struct rtdx_command_buffer* command_buffer);
static void rtdx_command_buffer_clear_uniform_slot(rtdx_uniform_slot* slot);
static rtdx_uniform_slot* rtdx_command_buffer_uniform_slot(struct rtdx_command_buffer* command_buffer, u32 index);
static bool rtdx_command_buffer_record_texture_view(struct rtdx_command_buffer* command_buffer, struct rtdx_texture_view* texture_view);
static struct rtdx_command_buffer* rtdx_command_buffer_node_create(struct rtdx_context* ctx);
static bool rtdx_command_buffer_prepare(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, struct rtdx_queue* queue);

static constexpr u32 RTDX_COMMAND_BUFFER_DESCRIPTOR_COUNT = 1024;

void rtdx_command_buffer_init(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer) {
	rtdx_init_resource_base(ctx, RTDX_RESOURCE_BASE(command_buffer), RT_RESOURCE_COMMAND_BUFFER);
	command_buffer->queue = NULL;
}

void rtdx_command_buffer_finish(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer) {
	struct rtdx_command_buffer* node = command_buffer->next;
	rtdx_command_buffer_node_release(command_buffer->active);
	command_buffer->active = NULL;
	while (node) {
		struct rtdx_command_buffer* next = node->next;
		node->next = NULL;
		rtdx_command_buffer_node_release(node);
		node = next;
	}
	command_buffer->next = NULL;
	rtdx_finish_resource_base(ctx, RTDX_RESOURCE_BASE(command_buffer));
}

static void rtdx_command_buffer_release_recorded_resources(struct rtdx_command_buffer* command_buffer) {
	if (!command_buffer) {
		return;
	}
	rtdx_buffer_storage_release(command_buffer->vertex_buffer_storage);
	command_buffer->vertex_buffer_storage = NULL;
	rtdx_release_resource(command_buffer->graphics_program);
	command_buffer->graphics_program = NULL;
	for (u32 i = 0; i < command_buffer->uniform_slot_count; i++) {
		rtdx_command_buffer_clear_uniform_slot(&command_buffer->uniform_slots[i]);
	}
	free(command_buffer->uniform_slots);
	command_buffer->uniform_slots = NULL;
	command_buffer->uniform_slot_count = 0;
	for (u32 i = 0; i < command_buffer->recorded_texture_view_count; ++i) {
		rtdx_release_resource(command_buffer->recorded_texture_views[i]);
	}
	free(command_buffer->recorded_texture_views);
	command_buffer->recorded_texture_views = NULL;
	command_buffer->recorded_texture_view_count = 0;
	command_buffer->recorded_texture_view_capacity = 0;
	rtdx_release(&command_buffer->d3d_srv_heap);
	rtdx_release(&command_buffer->d3d_sampler_heap);
	rtdx_release_resource(command_buffer->color_texture_view);
	command_buffer->color_texture_view = NULL;
	rtdx_release_resource(command_buffer->depth_texture_view);
	command_buffer->depth_texture_view = NULL;
}

static void rtdx_command_buffer_clear_uniform_slot(rtdx_uniform_slot* slot) {
	if (!slot) {
		return;
	}
	if (slot->kind == RTDX_UNIFORM_SLOT_BUFFER) {
		rtdx_buffer_storage_release(slot->buffer.storage);
	}
	if (slot->kind == RTDX_UNIFORM_SLOT_TEXTURE) {
		rtdx_release_resource(slot->texture.view);
	}
	*slot = {};
}

static rtdx_uniform_slot* rtdx_command_buffer_uniform_slot(struct rtdx_command_buffer* command_buffer, u32 index) {
	if (index < command_buffer->uniform_slot_count) {
		return &command_buffer->uniform_slots[index];
	}

	u32 count = command_buffer->uniform_slot_count ? command_buffer->uniform_slot_count : 8;
	while (count <= index) {
		count *= 2;
	}
	void* slots = realloc(command_buffer->uniform_slots, sizeof(command_buffer->uniform_slots[0]) * count);
	if (!slots) {
		rtdx_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate command buffer uniform slots");
		return NULL;
	}

	command_buffer->uniform_slots = (rtdx_uniform_slot*)slots;
	memset(&command_buffer->uniform_slots[command_buffer->uniform_slot_count], 0,
		   sizeof(command_buffer->uniform_slots[0]) * (count - command_buffer->uniform_slot_count));
	command_buffer->uniform_slot_count = count;
	return &command_buffer->uniform_slots[index];
}

static bool rtdx_command_buffer_record_texture_view(struct rtdx_command_buffer* command_buffer, struct rtdx_texture_view* texture_view) {
	if (command_buffer->recorded_texture_view_count >= command_buffer->recorded_texture_view_capacity) {
		u32 capacity = command_buffer->recorded_texture_view_capacity ? command_buffer->recorded_texture_view_capacity * 2 : 16;
		void* views = realloc(command_buffer->recorded_texture_views, sizeof(command_buffer->recorded_texture_views[0]) * capacity);
		if (!views) {
			rtdx_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate recorded texture view list");
			return false;
		}
		command_buffer->recorded_texture_views = (struct rtdx_texture_view**)views;
		command_buffer->recorded_texture_view_capacity = capacity;
	}

	rtdx_retain_resource(texture_view);
	command_buffer->recorded_texture_views[command_buffer->recorded_texture_view_count++] = texture_view;
	return true;
}

static bool rtdx_command_buffer_prepare_descriptor_heaps(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer) {
	if (!command_buffer->d3d_srv_heap) {
		D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
		heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heap_desc.NumDescriptors = RTDX_COMMAND_BUFFER_DESCRIPTOR_COUNT;
		heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		HRESULT result = ctx->d3d_device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&command_buffer->d3d_srv_heap));
		if (FAILED(result)) {
			rtdx_throwf(rtdx_error_from_hresult(result), "CreateDescriptorHeap(SRV) failed: 0x%08x", (u32)result);
			return false;
		}
	}
	if (!command_buffer->d3d_sampler_heap) {
		D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
		heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		heap_desc.NumDescriptors = RTDX_COMMAND_BUFFER_DESCRIPTOR_COUNT;
		heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		HRESULT result = ctx->d3d_device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&command_buffer->d3d_sampler_heap));
		if (FAILED(result)) {
			rtdx_throwf(rtdx_error_from_hresult(result), "CreateDescriptorHeap(sampler) failed: 0x%08x", (u32)result);
			return false;
		}
	}
	return true;
}

static D3D12_CPU_DESCRIPTOR_HANDLE rtdx_heap_cpu_handle(struct rtdx_context* ctx, ID3D12DescriptorHeap* heap, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handle = heap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += (SIZE_T)index * ctx->d3d_device->GetDescriptorHandleIncrementSize(type);
	return handle;
}

static D3D12_GPU_DESCRIPTOR_HANDLE rtdx_heap_gpu_handle(struct rtdx_context* ctx, ID3D12DescriptorHeap* heap, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handle = heap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += (UINT64)index * ctx->d3d_device->GetDescriptorHandleIncrementSize(type);
	return handle;
}

static struct rtdx_command_buffer* rtdx_command_buffer_node_create(struct rtdx_context* ctx) {
	struct rtdx_command_buffer* node = (struct rtdx_command_buffer*)calloc(1, sizeof(*node));
	if (!node) {
		rtdx_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate command buffer node");
		return NULL;
	}

	HRESULT result = ctx->d3d_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&node->d3d_allocator));
	if (FAILED(result)) {
		free(node);
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateCommandAllocator failed: 0x%08x", (u32)result);
		return NULL;
	}

	result = ctx->d3d_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		node->d3d_allocator,
		NULL,
		IID_PPV_ARGS(&node->d3d_command_list));
	if (FAILED(result)) {
		rtdx_release(&node->d3d_allocator);
		free(node);
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateCommandList failed: 0x%08x", (u32)result);
		return NULL;
	}
	node->d3d_command_list->Close();

	rtdx_init_resource_base(ctx, RTDX_RESOURCE_BASE(node), RT_RESOURCE_COMMAND_BUFFER);
	return node;
}

static void rtdx_command_buffer_recycle_node(struct rtdx_command_buffer* command_buffer, struct rtdx_command_buffer* node) {
	if (!node) {
		return;
	}
	node->next = command_buffer->next;
	command_buffer->next = node;
}

static struct rtdx_command_buffer* rtdx_command_buffer_take_reusable_node(struct rtdx_command_buffer* command_buffer) {
	struct rtdx_command_buffer** link = &command_buffer->next;
	while (*link) {
		struct rtdx_command_buffer* node = *link;
		if (rtdx_atomic_load(&node->base.ref_count) == 1) {
			*link = node->next;
			node->next = NULL;
			return node;
		}
		link = &node->next;
	}
	return NULL;
}

static bool rtdx_command_buffer_prepare(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, struct rtdx_queue* queue) {
	rtdx_queue_collect(ctx, queue);

	rtdx_command_buffer_recycle_node(command_buffer, command_buffer->active);
	command_buffer->active = NULL;

	struct rtdx_command_buffer* node = rtdx_command_buffer_take_reusable_node(command_buffer);
	if (!node) {
		node = rtdx_command_buffer_node_create(ctx);
	}
	if (!node) {
		return false;
	}

	rtdx_command_buffer_release_recorded_resources(node);
	command_buffer->active = node;
	return true;
}

void rtdx_command_buffer_begin(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, struct rtdx_queue* queue) {
	command_buffer->queue = queue;
	if (!rtdx_command_buffer_prepare(ctx, command_buffer, queue)) {
		return;
	}

	struct rtdx_command_buffer* node = command_buffer->active;
	HRESULT result = node->d3d_allocator->Reset();
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12CommandAllocator::Reset failed: 0x%08x", (u32)result);
		return;
	}

	result = node->d3d_command_list->Reset(node->d3d_allocator, NULL);
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12GraphicsCommandList::Reset failed: 0x%08x", (u32)result);
		return;
	}

	command_buffer->recording = true;
	command_buffer->framebuffer = NULL;
	command_buffer->graphics_program = NULL;
	command_buffer->vertex_buffer = NULL;
	command_buffer->color_texture_view = NULL;
	command_buffer->depth_texture_view = NULL;
	node->descriptor_cursor = 0;
}

static void rtdx_command_buffer_transition_texture(struct rtdx_command_buffer* command_buffer, struct rtdx_texture_view* view, D3D12_RESOURCE_STATES next_state) {
	if (!view || !view->d3d_resource || view->state == next_state) {
		return;
	}

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = view->d3d_resource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = view->state;
	barrier.Transition.StateAfter = next_state;
	command_buffer->d3d_command_list->ResourceBarrier(1, &barrier);
	view->state = next_state;
}

void rtdx_command_buffer_begin_rendering(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, struct rtdx_framebuffer* framebuffer) {

	struct rtdx_command_buffer* node = command_buffer->active;
	struct rtdx_texture_view* color_view = framebuffer->color_views[0];
	struct rtdx_texture_view* depth_view = framebuffer->depth_view;
	rtdx_command_buffer_transition_texture(node, color_view, D3D12_RESOURCE_STATE_RENDER_TARGET);
	if (depth_view) {
		rtdx_command_buffer_transition_texture(node, depth_view, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}
	D3D12_CPU_DESCRIPTOR_HANDLE* dsv = depth_view ? &depth_view->dsv : NULL;
	node->d3d_command_list->OMSetRenderTargets(1, &color_view->rtv, FALSE, dsv);

	command_buffer->framebuffer = framebuffer;
	rtdx_retain_resource(color_view);
	rtdx_retain_resource(depth_view);
	rtdx_release_resource(node->color_texture_view);
	rtdx_release_resource(node->depth_texture_view);
	node->color_texture_view = color_view;
	node->depth_texture_view = depth_view;
	command_buffer->color_texture_view = color_view;
	command_buffer->depth_texture_view = depth_view;
}

void rtdx_command_buffer_clear_color(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, u32 color_index, f32 r, f32 g, f32 b, f32 a) {
	struct rtdx_command_buffer* node = command_buffer ? command_buffer->active : NULL;
	if (!node || !command_buffer->recording || !command_buffer->framebuffer) {
		rtdx_throwf(RT_IMPROPER_USAGE, "clear color requires active rendering");
		return;
	}

	struct rtdx_framebuffer* framebuffer = command_buffer->framebuffer;
	if (!framebuffer || color_index >= framebuffer->color_texture_count) {
		rtdx_throwf(RT_IMPROPER_USAGE, "color attachment index is out of range");
		return;
	}

	struct rtdx_texture_view* color_view = framebuffer->color_views[color_index];
	if (!color_view || !color_view->rtv.ptr) {
		rtdx_throwf(RT_IMPROPER_USAGE, "color attachment view is invalid");
		return;
	}

	f32 clear_color[] = { r, g, b, a };
	node->d3d_command_list->ClearRenderTargetView(color_view->rtv, clear_color, 0, NULL);
}

void rtdx_command_buffer_clear_depth(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, f32 depth) {
	struct rtdx_command_buffer* node = command_buffer ? command_buffer->active : NULL;
	if (!node || !command_buffer->recording || !command_buffer->framebuffer) {
		rtdx_throwf(RT_IMPROPER_USAGE, "clear depth requires active rendering");
		return;
	}

	struct rtdx_texture_view* depth_view = node->depth_texture_view;
	if (!depth_view || !depth_view->dsv.ptr) {
		rtdx_throwf(RT_IMPROPER_USAGE, "depth attachment view is invalid");
		return;
	}

	node->d3d_command_list->ClearDepthStencilView(depth_view->dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, NULL);
}

void rtdx_command_buffer_clear_stencil(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, u32 stencil) {
	rtdx_throwf(RT_UNSUPPORTED_FEATURE, "stencil clear is not implemented yet");
}

void rtdx_command_buffer_use_graphics_program(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, struct rtdx_graphics_program* program) {
	struct rtdx_command_buffer* node = command_buffer->active;
	struct rtdx_texture_view* color_view = node ? node->color_texture_view : NULL;
	struct rtdx_texture_view* depth_view = node ? node->depth_texture_view : NULL;

	if (!color_view) {
		rtdx_throwf(RT_IMPROPER_USAGE, "command buffer has no color attachment");
		return;
	}

	DXGI_FORMAT depth_format = depth_view ? depth_view->dxgi_format : DXGI_FORMAT_UNKNOWN;
	if (!rtdx_graphics_program_prepare(ctx, program, color_view->dxgi_format, depth_format)) {
		return;
	}

	D3D12_VIEWPORT viewport = {};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = (f32)color_view->width;
	viewport.Height = (f32)color_view->height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	node->d3d_command_list->RSSetViewports(1, &viewport);

	D3D12_RECT scissor = {};
	scissor.left = 0;
	scissor.top = 0;
	scissor.right = (LONG)color_view->width;
	scissor.bottom = (LONG)color_view->height;
	node->d3d_command_list->RSSetScissorRects(1, &scissor);

	node->d3d_command_list->SetGraphicsRootSignature(program->d3d_root_signature);
	node->d3d_command_list->SetPipelineState(program->d3d_pipeline);
	node->d3d_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	if (node->graphics_program != program) {
		rtdx_retain_resource(program);
		rtdx_release_resource(node->graphics_program);
		node->graphics_program = program;
	}
	command_buffer->graphics_program = program;
}

void rtdx_command_buffer_set_scissor(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, u32 x, u32 y, u32 width, u32 height) {
	(void)ctx;
	struct rtdx_command_buffer* node = command_buffer ? command_buffer->active : NULL;
	if (!node || !command_buffer->recording || !command_buffer->framebuffer) {
		rtdx_throwf(RT_IMPROPER_USAGE, "setting a scissor requires active rendering");
		return;
	}

	D3D12_RECT scissor = {};
	scissor.left = (LONG)x;
	scissor.top = (LONG)y;
	scissor.right = (LONG)(x + width);
	scissor.bottom = (LONG)(y + height);
	node->d3d_command_list->RSSetScissorRects(1, &scissor);
}

void rtdx_command_buffer_uniform_buffer(
	struct rtdx_context* ctx,
	struct rtdx_command_buffer* command_buffer,
	rt_uniform_location location,
	struct rtdx_buffer* buffer,
	u64 offset,
	u64 size) {
	struct rtdx_command_buffer* node = command_buffer ? command_buffer->active : NULL;
	struct rtdx_uniform_location* internal_location = (struct rtdx_uniform_location*)location;

	if (!node || !command_buffer->recording || !command_buffer->framebuffer) {
		rtdx_throwf(RT_IMPROPER_USAGE, "setting a uniform buffer requires active rendering");
		return;
	}
	if (!internal_location) {
		rtdx_throwf(RT_IMPROPER_USAGE, "uniform location is NULL");
		return;
	}
	if (internal_location->kind != RTDX_UNIFORM_LOCATION_BUFFER) {
		rtdx_throwf(RT_IMPROPER_USAGE, "uniform location does not accept a buffer");
		return;
	}
	if (!command_buffer->graphics_program || command_buffer->graphics_program != internal_location->program) {
		rtdx_throwf(RT_IMPROPER_USAGE, "uniform location does not belong to the active graphics program");
		return;
	}
	if (!buffer || !buffer->storage || !buffer->storage->d3d_resource) {
		rtdx_throwf(RT_IMPROPER_USAGE, "uniform buffer has no storage");
		return;
	}
	if (!(buffer->storage->usage & RT_BUFFER_USAGE_UNIFORM) || (buffer->storage->usage & RT_BUFFER_USAGE_STAGING)) {
		rtdx_throwf(RT_IMPROPER_USAGE, "buffer usage is not compatible with uniform binding");
		return;
	}
	if (size == 0) {
		rtdx_throwf(RT_IMPROPER_USAGE, "uniform buffer binding size is zero");
		return;
	}
	if (offset > buffer->storage->size || size > buffer->storage->size - offset) {
		rtdx_throwf(RT_IMPROPER_USAGE, "uniform buffer range is out of bounds");
		return;
	}
	D3D12_GPU_VIRTUAL_ADDRESS gpu_address = buffer->storage->d3d_resource->GetGPUVirtualAddress() + offset;
	node->d3d_command_list->SetGraphicsRootConstantBufferView(internal_location->root_parameter, gpu_address);
	rtdx_uniform_slot* slot = rtdx_command_buffer_uniform_slot(node, internal_location->slot);
	if (!slot) {
		return;
	}
	if (slot->kind == RTDX_UNIFORM_SLOT_BUFFER &&
		slot->buffer.storage == buffer->storage &&
		slot->buffer.offset == offset &&
		slot->buffer.size == size) {
		return;
	}
	rtdx_command_buffer_clear_uniform_slot(slot);
	rtdx_buffer_storage_retain(buffer->storage);
	slot->kind = RTDX_UNIFORM_SLOT_BUFFER;
	slot->buffer.storage = buffer->storage;
	slot->buffer.offset = offset;
	slot->buffer.size = size;
}

void rtdx_command_buffer_uniform_texture(
	struct rtdx_context* ctx,
	struct rtdx_command_buffer* command_buffer,
	rt_uniform_location location,
	struct rtdx_texture_view* texture_view) {
	struct rtdx_command_buffer* node = command_buffer ? command_buffer->active : NULL;
	struct rtdx_uniform_location* internal_location = (struct rtdx_uniform_location*)location;

	if (!node || !command_buffer->recording || !command_buffer->framebuffer) {
		rtdx_throwf(RT_IMPROPER_USAGE, "setting a uniform texture requires active rendering");
		return;
	}
	if (!internal_location) {
		rtdx_throwf(RT_IMPROPER_USAGE, "uniform location is NULL");
		return;
	}
	if (internal_location->kind != RTDX_UNIFORM_LOCATION_TEXTURE) {
		rtdx_throwf(RT_IMPROPER_USAGE, "uniform location does not accept a texture");
		return;
	}
	if (!command_buffer->graphics_program || command_buffer->graphics_program != internal_location->program) {
		rtdx_throwf(RT_IMPROPER_USAGE, "uniform location does not belong to the active graphics program");
		return;
	}
	if (!texture_view || !texture_view->d3d_resource) {
		rtdx_throwf(RT_IMPROPER_USAGE, "uniform texture view is invalid");
		return;
	}
	if (!rtdx_texture_view_prepare_sampler(ctx, texture_view)) {
		return;
	}
	if (!rtdx_command_buffer_prepare_descriptor_heaps(ctx, node)) {
		return;
	}
	if (node->descriptor_cursor >= RTDX_COMMAND_BUFFER_DESCRIPTOR_COUNT) {
		rtdx_throwf(RT_OUT_OF_DEVICE_MEMORY, "command buffer descriptor heap is full");
		return;
	}

	rtdx_command_buffer_transition_texture(node, texture_view, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	u32 descriptor_index = node->descriptor_cursor++;
	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.Format = texture_view->dxgi_format;
	srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv_desc.Texture2D.MostDetailedMip = 0;
	srv_desc.Texture2D.MipLevels = 1;
	D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu = rtdx_heap_cpu_handle(ctx, node->d3d_srv_heap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, descriptor_index);
	ctx->d3d_device->CreateShaderResourceView(texture_view->d3d_resource, &srv_desc, srv_cpu);
	D3D12_CPU_DESCRIPTOR_HANDLE sampler_cpu = rtdx_heap_cpu_handle(ctx, node->d3d_sampler_heap, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, descriptor_index);
	ctx->d3d_device->CopyDescriptorsSimple(1, sampler_cpu, texture_view->sampler_cpu, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	ID3D12DescriptorHeap* heaps[] = { node->d3d_srv_heap, node->d3d_sampler_heap };
	node->d3d_command_list->SetDescriptorHeaps(2, heaps);
	node->d3d_command_list->SetGraphicsRootDescriptorTable(
		internal_location->root_parameter,
		rtdx_heap_gpu_handle(ctx, node->d3d_srv_heap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, descriptor_index));
	node->d3d_command_list->SetGraphicsRootDescriptorTable(
		internal_location->sampler_root_parameter,
		rtdx_heap_gpu_handle(ctx, node->d3d_sampler_heap, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, descriptor_index));

	rtdx_uniform_slot* slot = rtdx_command_buffer_uniform_slot(node, internal_location->slot);
	if (!slot) {
		return;
	}
	if (!rtdx_command_buffer_record_texture_view(node, texture_view)) {
		return;
	}
	rtdx_command_buffer_clear_uniform_slot(slot);
	rtdx_retain_resource(texture_view);
	slot->kind = RTDX_UNIFORM_SLOT_TEXTURE;
	slot->texture.view = texture_view;
}

void rtdx_command_buffer_bind_vertex_buffer(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, struct rtdx_buffer* buffer, u64 offset) {
	struct rtdx_command_buffer* node = command_buffer->active;
	if (!buffer || !buffer->storage || !buffer->storage->d3d_resource) {
		rtdx_throwf(RT_IMPROPER_USAGE, "vertex buffer has no storage");
		return;
	}
	if (!(buffer->storage->usage & RT_BUFFER_USAGE_VERTEX) || (buffer->storage->usage & RT_BUFFER_USAGE_STAGING)) {
		rtdx_throwf(RT_IMPROPER_USAGE, "buffer usage is not compatible with vertex binding");
		return;
	}
	if (offset > buffer->storage->size) {
		rtdx_throwf(RT_IMPROPER_USAGE, "vertex buffer offset is out of range");
		return;
	}

	D3D12_VERTEX_BUFFER_VIEW view = buffer->storage->vertex_view;
	view.BufferLocation += offset;
	view.SizeInBytes -= (UINT)offset;
	if (command_buffer->graphics_program) {
		struct rtdx_graphics_program* program = command_buffer->graphics_program;
		view.StrideInBytes = program->vertex_layout.stride;
	}
	node->d3d_command_list->IASetVertexBuffers(0, 1, &view);
	rtdx_buffer_storage_retain(buffer->storage);
	rtdx_buffer_storage_release(node->vertex_buffer_storage);
	node->vertex_buffer_storage = buffer->storage;
	command_buffer->vertex_buffer = buffer;
}

void rtdx_command_buffer_draw(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, u32 vertex_count, u32 first_vertex) {
	command_buffer->active->d3d_command_list->DrawInstanced(vertex_count, 1, first_vertex, 0);
}

void rtdx_command_buffer_end_rendering(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer) {
	command_buffer->framebuffer = NULL;
}

void rtdx_command_buffer_end(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer) {
	struct rtdx_command_buffer* node = command_buffer->active;

	HRESULT result = node->d3d_command_list->Close();
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12GraphicsCommandList::Close failed: 0x%08x", (u32)result);
		return;
	}
	command_buffer->recording = false;
}

void rtdx_command_buffer_node_retain(struct rtdx_command_buffer* command_buffer) {
	if (!command_buffer) {
		return;
	}
	rtdx_atomic_inc(&command_buffer->base.ref_count);
}

void rtdx_command_buffer_node_release(struct rtdx_command_buffer* command_buffer) {
	if (!command_buffer) {
		return;
	}
	if (rtdx_atomic_dec(&command_buffer->base.ref_count) != 0) {
		return;
	}

	rtdx_command_buffer_release_recorded_resources(command_buffer);
	rtdx_release(&command_buffer->d3d_command_list);
	rtdx_release(&command_buffer->d3d_allocator);
	rtdx_finish_resource_base(command_buffer->base.ctx, RTDX_RESOURCE_BASE(command_buffer));
	rtdx_atomic_bool_finish(&command_buffer->base.zombie);
	rtdx_atomic_u32_finish(&command_buffer->base.job_count);
	rtdx_atomic_u32_finish(&command_buffer->base.ref_count);
	free(command_buffer);
}

struct rtdx_command_buffer* rtdx_command_buffer_active_node(struct rtdx_command_buffer* command_buffer) {
	return command_buffer ? command_buffer->active : NULL;
}
