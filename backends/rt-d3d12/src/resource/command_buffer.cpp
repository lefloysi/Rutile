#include "command_buffer.hpp"
#include "context.hpp"
#include "error.hpp"
#include "resource/buffer.hpp"
#include "resource/framebuffer.hpp"
#include "resource/program.hpp"
#include "resource/sampler.hpp"
#include "resource/texture.hpp"

#include <stdlib.h>
#include <string.h>

static void rtd3d12_lower_texture_revision_copy(ID3D12GraphicsCommandList* command_list, rtd3d12_image_base* source, rtd3d12_image_base* target);
void rtd3d12_lower_bind_program_data(rtd3d12_context* ctx, rtd3d12_command_lower_state* state, ID3D12GraphicsCommandList* command_list, const rtd3d12_program_data_mapping& mapping, ID3D12Resource* resource, bool storage);

static void rtd3d12_set_root_descriptor_table(
	rtd3d12_command_lower_state* state,
	ID3D12GraphicsCommandList* command_list,
	u32 parameter,
	D3D12_GPU_DESCRIPTOR_HANDLE handle
) {
	if (state && state->program && state->program->d3d_compute_pipeline)
		command_list->SetComputeRootDescriptorTable(parameter, handle);
	else
		command_list->SetGraphicsRootDescriptorTable(parameter, handle);
}

static void rtd3d12_buffer_node_retain(rt_buffer_t* buffer) {
	if (buffer) {
		(buffer)->retain();
	}
}

static void rtd3d12_buffer_node_release(rt_buffer_t* buffer) {
	if (buffer) {
		(buffer)->release();
	}
}

/*===============================================================================================*/
/*                                                                                                */
/*===============================================================================================*/

rt_command_buffer_t* rtCommandBufferCreate(void) {
	rtd3d12_begin_errorable_operation();
	return rtd3d12::create_resource<rt_command_buffer_t>(rtd3d12_get_current_context());
}

void rtCommandBufferDestroy(rt_command_buffer_t* command_buffer) {
	if (command_buffer) command_buffer->retire();
}

void rtCommandBufferReset(rt_command_buffer_t* command_buffer) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_reset(command_buffer);
}

void rtCommandBufferBegin(rt_command_buffer_t* command_buffer) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_begin(command_buffer);
}

void rtCommandBufferContinue(rt_command_buffer_t* command_buffer) { rtd3d12_begin_errorable_operation(); rtd3d12_command_buffer_continue(command_buffer, false); }
void rtCommandBufferContinueRendering(rt_command_buffer_t* command_buffer) { rtd3d12_begin_errorable_operation(); rtd3d12_command_buffer_continue(command_buffer, true); }
void rtCommandBufferEnd(rt_command_buffer_t* command_buffer) { rtd3d12_begin_errorable_operation(); rtd3d12_command_buffer_end(command_buffer); }
void rtCmdExecute(rt_command_buffer_t* command_buffer, rt_command_buffer_t* secondary) { rtd3d12_begin_errorable_operation(); rtd3d12_command_buffer_execute(command_buffer, secondary); }

void rtCmdBeginRendering(rt_command_buffer_t* command_buffer, rt_framebuffer_t* framebuffer) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_begin_rendering(command_buffer, framebuffer);
}

void rtCmdClearColor(rt_command_buffer_t* command_buffer, rt::location* location, f32 r, f32 g, f32 b, f32 a) {
	rtd3d12_begin_errorable_operation();
	rt_program_t* program = rtd3d12_location_program(location);
	const rtd3d12_program_output_mapping* mapping = program && location && program->output_mappings[location->address]
		? &*program->output_mappings[location->address] : nullptr;
	rtd3d12_command_buffer_clear_color(command_buffer, mapping ? mapping->binding : 0, r, g, b, a);
}

void rtCmdClearDepth(rt_command_buffer_t* command_buffer, f32 depth) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_clear_depth(command_buffer, depth);
}

void rtCmdClearStencil(rt_command_buffer_t* command_buffer, usize stencil) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_clear_stencil(command_buffer, static_cast<u32>(stencil));
}

void rtCmdClear(rt_command_buffer_t* command_buffer, rt::clear attachments) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_clear(command_buffer, attachments);
}

void rtCmdSetViewport(rt_command_buffer_t* command_buffer, usize x, usize y, usize width, usize height, f32 min_depth, f32 max_depth) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_set_viewport(command_buffer, x, y, width, height, min_depth, max_depth);
}

void rtCmdSetScissor(rt_command_buffer_t* command_buffer, usize x, usize y, usize width, usize height) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_set_scissor(command_buffer, x, y, width, height);
}

void rtCmdEndRendering(rt_command_buffer_t* command_buffer) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_end_rendering(command_buffer);
}

void rtCmdUseProgram(rt_command_buffer_t* command_buffer, rt_program_t* program) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_use_program(command_buffer, program);
}

void rtCmdUniformData(rt_command_buffer_t* command_buffer, rt::location* location, const u08* data, usize size) {
	rtd3d12_begin_errorable_operation();
	if (command_buffer) command_buffer->uniform_data(location, data, size);
}

void rtCmdStorageData(rt_command_buffer_t* command_buffer, rt::location* location, const u08* data, usize size) {
	rtd3d12_begin_errorable_operation();
	if (command_buffer) command_buffer->storage_data(location, data, size);
}

void rtCmdBindBuffer(rt_command_buffer_t* command_buffer, rt::location* location, rt_buffer_t* buffer, rt::buffer_range range) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_bind_buffer(command_buffer, location, buffer, range.offset, range.size);
}

void rtCmdBindTexture(rt_command_buffer_t* command_buffer, rt::location* location, rt_texture_view_t* texture_view) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_bind_texture(command_buffer, location, texture_view);
}

void rtCmdBindSampler(rt_command_buffer_t* command_buffer, rt::location* location, rt_sampler_t* sampler) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_bind_sampler(command_buffer, location, sampler);
}

void rtCmdVertexBuffer(rt_command_buffer_t* command_buffer, rt::location* location, rt_buffer_t* buffer, rt::buffer_range range) {
	rtd3d12_begin_errorable_operation();
	(void)range.size;
	rtd3d12_command_buffer_vertex_buffer(command_buffer, location, buffer, range.offset);
}

void rtCmdIndexBuffer(rt_command_buffer_t* command_buffer, rt_buffer_t* buffer, rt::buffer_range range, rt::index_format format) {
	rtd3d12_begin_errorable_operation();
	(void)range.size;
	rtd3d12_command_buffer_index_buffer(command_buffer, buffer, range.offset, format);
}

void rtCmdDraw(rt_command_buffer_t* command_buffer, usize vertex_count, usize first_vertex) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_draw(command_buffer, vertex_count, first_vertex);
}

void rtCmdDrawInstanced(rt_command_buffer_t* command_buffer, usize vertex_count, usize instance_count, usize first_vertex, usize first_instance) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_draw_instanced(command_buffer, vertex_count, instance_count, first_vertex, first_instance);
}

void rtCmdDrawIndexed(rt_command_buffer_t* command_buffer, usize index_count, usize first_index, usize vertex_offset) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_draw_indexed(command_buffer, index_count, first_index, vertex_offset);
}

void rtCmdDrawIndexedInstanced(rt_command_buffer_t* command_buffer, usize index_count, usize instance_count, usize first_index, usize vertex_offset, usize first_instance) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_draw_indexed_instanced(command_buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void rtCmdDispatch(rt_command_buffer_t* command_buffer, usize group_count_x, usize group_count_y, usize group_count_z) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_dispatch(command_buffer, group_count_x, group_count_y, group_count_z);
}

void rtCmdBufferData(rt_command_buffer_t* command_buffer, rt_buffer_t* buffer, rt::buffer_range range, const u08* data) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_buffer_data(command_buffer, buffer, range, data);
}

void rtCmdBufferCopy(rt_command_buffer_t* command_buffer, rt_buffer_t* src, rt::buffer_range src_range, rt_buffer_t* dst, rt::buffer_range dst_range) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_buffer_copy(command_buffer, src, src_range, dst, dst_range);
}

void rtCmdBufferCopyToTexture(rt_command_buffer_t* command_buffer, rt_buffer_t* src, rt::buffer_range src_range, rt_texture_t* dst, rt::texture_range dst_range) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_buffer_copy_to_texture(command_buffer, src, src_range, dst, dst_range);
}

void rtCmdBufferBarrier(rt_command_buffer_t* command_buffer, rt_buffer_t* buffer, rt::buffer_range range, rt::access src, rt::access dst) {
	rtd3d12_begin_errorable_operation();
	rtd3d12_command_buffer_buffer_barrier(command_buffer, buffer, range, src, dst);
}

/*===============================================================================================*/
/*                                                                                                */
/*===============================================================================================*/

void rtd3d12_command_transition_buffer(ID3D12GraphicsCommandList* command_list, rt_buffer_t* buffer, D3D12_RESOURCE_STATES state) {
	if (!buffer || !buffer->d3d_resource || buffer->state == state) {
		return;
	}
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = buffer->d3d_resource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = buffer->state;
	barrier.Transition.StateAfter = state;
	command_list->ResourceBarrier(1, &barrier);
	buffer->state = state;
}

void rtd3d12_command_transition_image(ID3D12GraphicsCommandList* command_list, rtd3d12_image_base* image, D3D12_RESOURCE_STATES state) {
	if (!image || !image->d3d_resource) {
		return;
	}
	const usize count = rtd3d12_texture_subresource_count(image);
	for (usize layer = 0; layer < image->layer_count; ++layer) {
		for (usize mip = 0; mip < image->mip_count; ++mip) {
			D3D12_RESOURCE_STATES before = rtd3d12_texture_subresource_state(image, mip, layer);
			if (before == state) {
				continue;
			}
			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Transition.pResource = image->d3d_resource;
			barrier.Transition.Subresource = static_cast<UINT>(layer * image->mip_count + mip);
			barrier.Transition.StateBefore = before;
			barrier.Transition.StateAfter = state;
			command_list->ResourceBarrier(1, &barrier);
			rtd3d12_texture_set_subresource_state(image, mip, layer, state);
		}
	}
	if (!count) {
		image->state = state;
	}
}

void rtd3d12_command_transition_image_range(ID3D12GraphicsCommandList* command_list, rtd3d12_image_base* image, rt::texture_range range, D3D12_RESOURCE_STATES state) {
	if (!image || !image->d3d_resource) {
		return;
	}
	for (usize layer = 0; layer < range.layer_count; ++layer) {
		for (usize mip = 0; mip < range.mip_count; ++mip) {
			const usize actual_mip = range.base_mip + mip;
			const usize actual_layer = range.base_layer + layer;
			D3D12_RESOURCE_STATES before = rtd3d12_texture_subresource_state(image, actual_mip, actual_layer);
			if (before == state) {
				continue;
			}
			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Transition.pResource = image->d3d_resource;
			barrier.Transition.Subresource = static_cast<UINT>(actual_layer * image->mip_count + actual_mip);
			barrier.Transition.StateBefore = before;
			barrier.Transition.StateAfter = state;
			command_list->ResourceBarrier(1, &barrier);
			rtd3d12_texture_set_subresource_state(image, actual_mip, actual_layer, state);
		}
	}
}

void rtd3d12_command_buffer_init(rtd3d12_context* ctx, rt_command_buffer_t* command_buffer) {
	command_buffer->clear_depth_value = 1.0f;
}

rt_command_buffer_t::~rt_command_buffer_t() {
	rtd3d12_command_buffer_release_resources(this);
	rtd3d12::release_bytes(ir_data);
}

usize rtd3d12_command_record_size(rtd3d12_command_opcode opcode) {
	usize size = sizeof(rtd3d12_command_header);
	switch (opcode) {
	case rtd3d12_command_opcode::begin_rendering:
		size += sizeof(rtd3d12_ir_framebuffer);
		break;
	case rtd3d12_command_opcode::clear_color:
		size += sizeof(rtd3d12_ir_clear_color);
		break;
	case rtd3d12_command_opcode::clear_depth:
		size += sizeof(rtd3d12_ir_clear_depth);
		break;
	case rtd3d12_command_opcode::clear_stencil:
		size += sizeof(rtd3d12_ir_clear_stencil);
		break;
	case rtd3d12_command_opcode::set_viewport:
		size += sizeof(rtd3d12_ir_viewport);
		break;
	case rtd3d12_command_opcode::set_scissor:
		size += sizeof(rtd3d12_ir_scissor);
		break;
	case rtd3d12_command_opcode::end_rendering:
		break;
	case rtd3d12_command_opcode::use_program:
		size += sizeof(rtd3d12_ir_program);
		break;
	case rtd3d12_command_opcode::uniform_data:
	case rtd3d12_command_opcode::storage_data:
		size += sizeof(rtd3d12_ir_program_data);
		break;
	case rtd3d12_command_opcode::bind_buffer:
		size += sizeof(rtd3d12_ir_buffer);
		break;
	case rtd3d12_command_opcode::bind_texture:
		size += sizeof(rtd3d12_ir_texture);
		break;
	case rtd3d12_command_opcode::bind_sampler:
		size += sizeof(rtd3d12_ir_sampler);
		break;
	case rtd3d12_command_opcode::vertex_buffer:
		size += sizeof(rtd3d12_ir_vertex_buffer);
		break;
	case rtd3d12_command_opcode::index_buffer:
		size += sizeof(rtd3d12_ir_index_buffer);
		break;
	case rtd3d12_command_opcode::draw:
		size += sizeof(rtd3d12_ir_draw);
		break;
	case rtd3d12_command_opcode::draw_instanced:
		size += sizeof(rtd3d12_ir_draw_instanced);
		break;
	case rtd3d12_command_opcode::draw_indexed:
		size += sizeof(rtd3d12_ir_draw_indexed);
		break;
	case rtd3d12_command_opcode::draw_indexed_instanced:
		size += sizeof(rtd3d12_ir_draw_indexed_instanced);
		break;
	case rtd3d12_command_opcode::dispatch:
		size += sizeof(rtd3d12_ir_dispatch);
		break;
	case rtd3d12_command_opcode::buffer_data:
		size += sizeof(rtd3d12_ir_buffer_data);
		break;
	case rtd3d12_command_opcode::buffer_copy:
		size += sizeof(rtd3d12_ir_buffer_copy);
		break;
	case rtd3d12_command_opcode::buffer_barrier:
		size += sizeof(rtd3d12_ir_buffer_barrier);
		break;
	case rtd3d12_command_opcode::buffer_copy_to_texture:
		size += sizeof(rtd3d12_ir_buffer_copy_to_texture);
		break;
	case rtd3d12_command_opcode::texture_copy:
		size += sizeof(rtd3d12_ir_texture_copy);
		break;
	case rtd3d12_command_opcode::texture_data:
		size += sizeof(rtd3d12_ir_texture_data);
		break;
	case rtd3d12_command_opcode::texture_copy_to_buffer:
		size += sizeof(rtd3d12_ir_texture_copy_to_buffer);
		break;
	case rtd3d12_command_opcode::texture_barrier:
		size += sizeof(rtd3d12_ir_texture_barrier);
		break;
	}
	return (size + alignof(void*) - 1) & ~(alignof(void*) - 1);
}

void* rtd3d12_command_append(rt_command_buffer_t* command_buffer, rtd3d12_command_opcode opcode) {
	if (!command_buffer || !command_buffer->recording) {
		return nullptr;
	}
	usize size = rtd3d12_command_record_size(opcode);
	if (command_buffer->ir_capacity - command_buffer->ir_size < size) {
		usize required = command_buffer->ir_size + size;
		usize capacity = 1;
		while (capacity < required) {
			capacity <<= 1;
		}
		std::byte* data = rtd3d12::resize_bytes(reinterpret_cast<std::byte*>(command_buffer->ir_data), capacity);
		if (!data) {
			return nullptr;
		}
		command_buffer->ir_data = reinterpret_cast<u08*>(data);
		command_buffer->ir_capacity = capacity;
	}
	rtd3d12_command_header* command = reinterpret_cast<rtd3d12_command_header*>(command_buffer->ir_data + command_buffer->ir_size);
	command->opcode = opcode;
	command_buffer->ir_size += size;
	return command + 1;
}

void rtd3d12_command_buffer_release_resources(rt_command_buffer_t* command_buffer) {
	for (usize offset = 0; offset < command_buffer->ir_size; (void)0) {
		rtd3d12_command_header* header = reinterpret_cast<rtd3d12_command_header*>(command_buffer->ir_data + offset);
		void* payload = header + 1;
		switch (header->opcode) {
		case rtd3d12_command_opcode::begin_rendering: {
			rtd3d12_ir_framebuffer* command = static_cast<rtd3d12_ir_framebuffer*>(payload);
			rt_framebuffer_t* framebuffer = command->framebuffer;
			if (framebuffer) {
				(framebuffer)->release();
			}
			break;
		}
		case rtd3d12_command_opcode::use_program: {
			rt_program_t* program = static_cast<rtd3d12_ir_program*>(payload)->program;
			if (program) {
				(program)->release();
			}
			break;
		}
		case rtd3d12_command_opcode::uniform_data:
		case rtd3d12_command_opcode::storage_data: {
			rtd3d12_ir_program_data* command = static_cast<rtd3d12_ir_program_data*>(payload);
			rtd3d12::release_bytes(command->bytes);
			if (command->resource) command->resource->Release();
			if (command->upload) command->upload->Release();
			break;
		}
		case rtd3d12_command_opcode::bind_buffer: {
			rtd3d12_ir_buffer* command = static_cast<rtd3d12_ir_buffer*>(payload);
			rtd3d12_buffer_node_release(command->buffer);
			break;
		}
		case rtd3d12_command_opcode::bind_texture: {
			rtd3d12_ir_texture* command = static_cast<rtd3d12_ir_texture*>(payload);
			rt_texture_view_t* texture_view = command->texture_view;
			if (texture_view) {
				(texture_view)->release();
			}
			if (command->image) {
				(command->image)->release();
			}
			if (command->sampler_heap) {
				command->sampler_heap->Release();
				command->sampler_heap = nullptr;
			}
			break;
		}
		case rtd3d12_command_opcode::bind_sampler: {
			rtd3d12_ir_sampler* command = static_cast<rtd3d12_ir_sampler*>(payload);
			if (command->sampler) command->sampler->release();
			break;
		}
		case rtd3d12_command_opcode::vertex_buffer: {
			rtd3d12_ir_vertex_buffer* command = static_cast<rtd3d12_ir_vertex_buffer*>(payload);
			rtd3d12_buffer_node_release(command->buffer);
			break;
		}
		case rtd3d12_command_opcode::index_buffer: {
			rtd3d12_buffer_node_release(static_cast<rtd3d12_ir_index_buffer*>(payload)->buffer);
			break;
		}
		case rtd3d12_command_opcode::buffer_data: {
			rtd3d12_ir_buffer_data* command = static_cast<rtd3d12_ir_buffer_data*>(payload);
			rtd3d12_buffer_node_release(command->copy_source);
			rtd3d12_buffer_node_release(command->target);
			if (command->upload) {
				command->upload->Release();
				command->upload = nullptr;
			}
			break;
		}
		case rtd3d12_command_opcode::buffer_copy: {
			rtd3d12_ir_buffer_copy* command = static_cast<rtd3d12_ir_buffer_copy*>(payload);
			rtd3d12_buffer_node_release(command->source);
			rtd3d12_buffer_node_release(command->target_copy_source);
			rtd3d12_buffer_node_release(command->target);
			break;
		}
		case rtd3d12_command_opcode::buffer_barrier:
			rtd3d12_buffer_node_release(static_cast<rtd3d12_ir_buffer_barrier*>(payload)->buffer);
			break;
		case rtd3d12_command_opcode::buffer_copy_to_texture: {
			rtd3d12_ir_buffer_copy_to_texture* command = static_cast<rtd3d12_ir_buffer_copy_to_texture*>(payload);
			rtd3d12_buffer_node_release(command->source);
			if (command->target_copy_source) {
				(command->target_copy_source)->release();
			}
			if (command->target) {
				(command->target)->release();
			}
			if (command->staging) {
				command->staging->Release();
				command->staging = nullptr;
			}
			break;
		}
		case rtd3d12_command_opcode::texture_copy: {
			rtd3d12_ir_texture_copy* command = static_cast<rtd3d12_ir_texture_copy*>(payload);
			if (command->source) {
				(command->source)->release();
			}
			if (command->target_copy_source) {
				(command->target_copy_source)->release();
			}
			if (command->target) {
				(command->target)->release();
			}
			break;
		}
		case rtd3d12_command_opcode::texture_data: {
			rtd3d12_ir_texture_data* command = static_cast<rtd3d12_ir_texture_data*>(payload);
			if (command->copy_source) {
				(command->copy_source)->release();
			}
			if (command->target) {
				(command->target)->release();
			}
			rtd3d12::release_bytes(command->data);
			if (command->upload) {
				command->upload->Release();
				command->upload = nullptr;
			}
			break;
		}
		case rtd3d12_command_opcode::texture_copy_to_buffer: {
			rtd3d12_ir_texture_copy_to_buffer* command = static_cast<rtd3d12_ir_texture_copy_to_buffer*>(payload);
			if (command->source) {
				(command->source)->release();
			}
			rtd3d12_buffer_node_release(command->target_copy_source);
			rtd3d12_buffer_node_release(command->target);
			if (command->staging) {
				command->staging->Release();
				command->staging = nullptr;
			}
			break;
		}
		case rtd3d12_command_opcode::texture_barrier:
			if (static_cast<rtd3d12_ir_texture_barrier*>(payload)->image) {
				(static_cast<rtd3d12_ir_texture_barrier*>(payload)->image)->release();
			}
			break;
		default:
			break;
		}
		offset += rtd3d12_command_record_size(header->opcode);
	}
	command_buffer->ir_size = 0;
}

void rtd3d12_command_buffer_reset(rt_command_buffer_t* command_buffer) {
	if (!command_buffer) {
		return;
	}
	rtd3d12_command_buffer_release_resources(command_buffer);
	command_buffer->recording = false;
	command_buffer->executable = false;
	command_buffer->continuation = false;
	command_buffer->rendering_continuation = false;
	command_buffer->rendering = false;
	command_buffer->active_framebuffer = nullptr;
}

static void rtd3d12_command_buffer_begin_mode(rt_command_buffer_t* command_buffer, bool continuation, bool rendering_continuation) {
	if (!command_buffer) {
		return;
	}
	rtd3d12_command_buffer_release_resources(command_buffer);
	command_buffer->recording = true;
	command_buffer->executable = false;
	command_buffer->continuation = continuation;
	command_buffer->rendering_continuation = rendering_continuation;
	command_buffer->rendering = false;
}

void rtd3d12_command_buffer_begin(rt_command_buffer_t* command_buffer) { rtd3d12_command_buffer_begin_mode(command_buffer, false, false); }
void rtd3d12_command_buffer_continue(rt_command_buffer_t* command_buffer, bool rendering) { rtd3d12_command_buffer_begin_mode(command_buffer, true, rendering); }

static void rtd3d12_command_buffer_retain_payload(rtd3d12_command_opcode opcode, void* payload) {
	switch (opcode) {
	case rtd3d12_command_opcode::begin_rendering: {
		rtd3d12_ir_framebuffer* command = static_cast<rtd3d12_ir_framebuffer*>(payload);
		rt_framebuffer_t* framebuffer = command->framebuffer;
		if (framebuffer) {
			(framebuffer)->retain();
		}
		break;
	}
	case rtd3d12_command_opcode::use_program: {
		rt_program_t* program = static_cast<rtd3d12_ir_program*>(payload)->program;
		if (program) {
			(program)->retain();
		}
		break;
	}
	case rtd3d12_command_opcode::uniform_data:
	case rtd3d12_command_opcode::storage_data: {
		rtd3d12_ir_program_data* command = static_cast<rtd3d12_ir_program_data*>(payload);
		if (command->resource) command->resource->AddRef();
		if (command->upload) command->upload->AddRef();
		break;
	}
	case rtd3d12_command_opcode::bind_buffer: {
		rtd3d12_ir_buffer* command = static_cast<rtd3d12_ir_buffer*>(payload);
		rtd3d12_buffer_node_retain(command->buffer);
		break;
	}
	case rtd3d12_command_opcode::bind_texture: {
		rtd3d12_ir_texture* command = static_cast<rtd3d12_ir_texture*>(payload);
		if (command->texture_view) {
			(command->texture_view)->retain();
		}
		if (command->image) {
			(command->image)->retain();
		}
		if (command->sampler_heap) {
			command->sampler_heap->AddRef();
		}
		break;
	}
	case rtd3d12_command_opcode::bind_sampler: {
		rtd3d12_ir_sampler* command = static_cast<rtd3d12_ir_sampler*>(payload);
		if (command->sampler) command->sampler->retain();
		break;
	}
	case rtd3d12_command_opcode::vertex_buffer: {
		rtd3d12_ir_vertex_buffer* command = static_cast<rtd3d12_ir_vertex_buffer*>(payload);
		rtd3d12_buffer_node_retain(command->buffer);
		break;
	}
	case rtd3d12_command_opcode::index_buffer:
		rtd3d12_buffer_node_retain(static_cast<rtd3d12_ir_index_buffer*>(payload)->buffer);
		break;
	case rtd3d12_command_opcode::buffer_data: {
		rtd3d12_ir_buffer_data* command = static_cast<rtd3d12_ir_buffer_data*>(payload);
		rtd3d12_buffer_node_retain(command->copy_source);
		rtd3d12_buffer_node_retain(command->target);
		if (command->upload) {
			command->upload->AddRef();
		}
		break;
	}
	case rtd3d12_command_opcode::buffer_copy: {
		rtd3d12_ir_buffer_copy* command = static_cast<rtd3d12_ir_buffer_copy*>(payload);
		rtd3d12_buffer_node_retain(command->source);
		rtd3d12_buffer_node_retain(command->target_copy_source);
		rtd3d12_buffer_node_retain(command->target);
		break;
	}
	case rtd3d12_command_opcode::buffer_barrier:
		rtd3d12_buffer_node_retain(static_cast<rtd3d12_ir_buffer_barrier*>(payload)->buffer);
		break;
	case rtd3d12_command_opcode::buffer_copy_to_texture: {
		rtd3d12_ir_buffer_copy_to_texture* c = static_cast<rtd3d12_ir_buffer_copy_to_texture*>(payload);
		rtd3d12_buffer_node_retain(c->source);
		if (c->target_copy_source) {
			(c->target_copy_source)->retain();
		}
		if (c->target) {
			(c->target)->retain();
		}
		if (c->staging) {
			c->staging->AddRef();
		}
		break;
	}
	case rtd3d12_command_opcode::texture_copy: {
		rtd3d12_ir_texture_copy* c = static_cast<rtd3d12_ir_texture_copy*>(payload);
		if (c->source) {
			(c->source)->retain();
		}
		if (c->target_copy_source) {
			(c->target_copy_source)->retain();
		}
		if (c->target) {
			(c->target)->retain();
		}
		break;
	}
	case rtd3d12_command_opcode::texture_data: {
		rtd3d12_ir_texture_data* c = static_cast<rtd3d12_ir_texture_data*>(payload);
		if (c->copy_source) {
			(c->copy_source)->retain();
		}
		if (c->target) {
			(c->target)->retain();
		}
		if (c->upload) {
			c->upload->AddRef();
		}
		if (c->data_size) {
			u08* copy = reinterpret_cast<u08*>(rtd3d12::allocate_bytes(c->data_size));
			if (!copy) {
				c->data_size = 0;
				c->data = nullptr;
			} else {
				memcpy(copy, c->data, c->data_size);
				c->data = copy;
			}
		}
		break;
	}
	case rtd3d12_command_opcode::texture_copy_to_buffer: {
		rtd3d12_ir_texture_copy_to_buffer* c = static_cast<rtd3d12_ir_texture_copy_to_buffer*>(payload);
		if (c->source) {
			(c->source)->retain();
		}
		rtd3d12_buffer_node_retain(c->target_copy_source);
		rtd3d12_buffer_node_retain(c->target);
		if (c->staging) {
			c->staging->AddRef();
		}
		break;
	}
	case rtd3d12_command_opcode::texture_barrier: {
		rtd3d12_image_base* image = static_cast<rtd3d12_ir_texture_barrier*>(payload)->image;
		if (image) {
			(image)->retain();
		}
		break;
	}
	default:
		break;
	}
}

rt_command_buffer_t* rtd3d12_command_buffer_snapshot_create(const rt_command_buffer_t* command_buffer) {
	if (!command_buffer || !command_buffer->ir_size) {
		return nullptr;
	}
	rt_command_buffer_t* snapshot = rtd3d12::create_resource<rt_command_buffer_t>(command_buffer->ctx);
	if (!snapshot) {
		return nullptr;
	}
	snapshot->ir_data = reinterpret_cast<u08*>(rtd3d12::allocate_bytes(command_buffer->ir_size));
	if (!snapshot->ir_data) {
		delete snapshot;
		return nullptr;
	}
	snapshot->ir_size = command_buffer->ir_size;
	snapshot->ir_capacity = command_buffer->ir_size;
	memcpy(snapshot->ir_data, command_buffer->ir_data, snapshot->ir_size);
	for (usize offset = 0; offset < snapshot->ir_size;) {
		rtd3d12_command_header* header = reinterpret_cast<rtd3d12_command_header*>(snapshot->ir_data + offset);
		if (header->opcode == rtd3d12_command_opcode::uniform_data || header->opcode == rtd3d12_command_opcode::storage_data) {
			rtd3d12_ir_program_data* command = reinterpret_cast<rtd3d12_ir_program_data*>(header + 1);
			std::byte* bytes = rtd3d12::allocate_bytes(command->size);
			if (!bytes) {
				delete snapshot;
				return nullptr;
			}
			memcpy(bytes, command->bytes, command->size);
			command->bytes = bytes;
		}
		rtd3d12_command_buffer_retain_payload(header->opcode, header + 1);
		offset += rtd3d12_command_record_size(header->opcode);
	}
	return snapshot;
}

void rtd3d12_command_buffer_execute(rt_command_buffer_t* command_buffer, rt_command_buffer_t* secondary) {
	if (!command_buffer || !secondary || command_buffer == secondary || !command_buffer->recording || !secondary->executable || !secondary->continuation || (secondary->rendering_continuation && !command_buffer->rendering)) {
		return;
	}
	for (usize offset = 0; offset < secondary->ir_size;) {
		rtd3d12_command_header* source = reinterpret_cast<rtd3d12_command_header*>(secondary->ir_data + offset);
		const usize record_size = rtd3d12_command_record_size(source->opcode);
		void* destination = rtd3d12_command_append(command_buffer, source->opcode);
		if (!destination) {
			return;
		}
		memcpy(destination, source + 1, record_size - sizeof(*source));
		if (source->opcode == rtd3d12_command_opcode::uniform_data || source->opcode == rtd3d12_command_opcode::storage_data) {
			rtd3d12_ir_program_data* command = static_cast<rtd3d12_ir_program_data*>(destination);
			std::byte* bytes = rtd3d12::allocate_bytes(command->size);
			if (!bytes) {
				command_buffer->ir_size -= record_size;
				return;
			}
			memcpy(bytes, command->bytes, command->size);
			command->bytes = bytes;
		}
		rtd3d12_command_buffer_retain_payload(source->opcode, destination);
		offset += record_size;
	}
}
void rtd3d12_command_buffer_begin_rendering(rt_command_buffer_t* command_buffer, rt_framebuffer_t* framebuffer) {
	if (!command_buffer || !command_buffer->recording || command_buffer->continuation || command_buffer->rendering || !framebuffer || !rtd3d12_framebuffer_valid(framebuffer)) {
		return;
	}
	rtd3d12_ir_framebuffer* command = static_cast<rtd3d12_ir_framebuffer*>(rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::begin_rendering));
	if (!command) {
		return;
	}
	*command = {};
	command->framebuffer = framebuffer;
	(framebuffer)->retain();
	command_buffer->active_framebuffer = framebuffer;
	command_buffer->rendering = true;
}
void rtd3d12_command_buffer_clear_color(rt_command_buffer_t* command_buffer, u32 index, f32 r, f32 g, f32 b, f32 a) {
	if (!command_buffer || !command_buffer->recording || !command_buffer->rendering || index >= 8) {
		return;
	}
	command_buffer->clear_colors[index][0] = r;
	command_buffer->clear_colors[index][1] = g;
	command_buffer->clear_colors[index][2] = b;
	command_buffer->clear_colors[index][3] = a;
}
void rtd3d12_command_buffer_clear_depth(rt_command_buffer_t* command_buffer, f32 depth) {
	if (command_buffer && command_buffer->recording && command_buffer->rendering) {
		command_buffer->clear_depth_value = depth;
	}
}
void rtd3d12_command_buffer_clear_stencil(rt_command_buffer_t* command_buffer, u32 stencil) {
	if (command_buffer && command_buffer->recording && command_buffer->rendering) {
		command_buffer->clear_stencil_value = stencil;
	}
}
void rtd3d12_command_buffer_clear(rt_command_buffer_t* command_buffer, rt::clear attachments) {
	if (!command_buffer || !command_buffer->recording || !command_buffer->rendering) {
		return;
	}
	if (static_cast<u32>(attachments & rt::clear::color) != 0) {
		const u32 count = command_buffer->active_framebuffer ? command_buffer->active_framebuffer->color_texture_count : 0;
		for (u32 index = 0; index < count; ++index) {
			rtd3d12_ir_clear_color* c = static_cast<rtd3d12_ir_clear_color*>(rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::clear_color));
			if (c) {
				*c = { index, command_buffer->clear_colors[index][0], command_buffer->clear_colors[index][1], command_buffer->clear_colors[index][2], command_buffer->clear_colors[index][3] };
			}
		}
	}
	if (static_cast<u32>(attachments & rt::clear::depth) != 0) {
		rtd3d12_ir_clear_depth* c = static_cast<rtd3d12_ir_clear_depth*>(rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::clear_depth));
		if (c) {
			c->depth = command_buffer->clear_depth_value;
		}
	}
	if (static_cast<u32>(attachments & rt::clear::stencil) != 0) {
		rtd3d12_ir_clear_stencil* c = static_cast<rtd3d12_ir_clear_stencil*>(rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::clear_stencil));
		if (c) {
			c->stencil = command_buffer->clear_stencil_value;
		}
	}
}
void rtd3d12_command_buffer_set_viewport(rt_command_buffer_t* command_buffer, usize x, usize y, usize width, usize height, f32 min_depth, f32 max_depth) {
	rtd3d12_ir_viewport* command = static_cast<rtd3d12_ir_viewport*>(rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::set_viewport));
	if (!command) {
		return;
	}
	*command = { x, y, width, height, min_depth, max_depth };
}
void rtd3d12_command_buffer_set_scissor(rt_command_buffer_t* command_buffer, usize x, usize y, usize width, usize height) {
	rtd3d12_ir_scissor* command = static_cast<rtd3d12_ir_scissor*>(rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::set_scissor));
	if (!command) {
		return;
	}
	*command = { x, y, width, height };
}
void rtd3d12_command_buffer_end_rendering(rt_command_buffer_t* command_buffer) {
	if (!command_buffer || command_buffer->continuation || !command_buffer->rendering) {
		return;
	}
	rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::end_rendering);
	command_buffer->rendering = false;
	command_buffer->active_framebuffer = nullptr;
}
void rtd3d12_command_buffer_use_program(rt_command_buffer_t* command_buffer, rt_program_t* program) {
	rtd3d12_ir_program* command = static_cast<rtd3d12_ir_program*>(rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::use_program));
	if (!command) {
		return;
	}
	command->program = program;
	if (program) {
		(program)->retain();
	}
}
void rtd3d12_command_buffer_bind_buffer(rt_command_buffer_t* command_buffer, rt::location* location, rt_buffer_t* buffer, usize offset, usize size) {
	rtd3d12_ir_buffer* command = static_cast<rtd3d12_ir_buffer*>(rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::bind_buffer));
	if (!command) {
		return;
	}
	rt_buffer_t* node = buffer ? buffer->active : nullptr;
	*command = { location ? location->address : u08{0}, node, offset, size };
	rtd3d12_buffer_node_retain(node);
}
void rtd3d12_command_buffer_bind_texture(rt_command_buffer_t* command_buffer, rt::location* location, rt_texture_view_t* texture_view) {
	if (!command_buffer || !command_buffer->recording || !texture_view || !rtd3d12_texture_view_refresh(rtd3d12_get_current_context(), texture_view) || !rtd3d12_texture_view_prepare_sampler(rtd3d12_get_current_context(), texture_view)) {
		return;
	}
	rtd3d12_image_base* image = texture_view->image;
	rtd3d12_ir_texture* command = static_cast<rtd3d12_ir_texture*>(rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::bind_texture));
	if (!command) {
		return;
	}
	*command = { location ? location->address : u08{0}, texture_view, image, texture_view->d3d_sampler_heap, texture_view->sampler_cpu };
	(texture_view)->retain();
	if (image) {
		(image)->retain();
	}
	if (command->sampler_heap) {
		command->sampler_heap->AddRef();
	}
}

void rtd3d12_command_buffer_bind_sampler(rt_command_buffer_t* command_buffer, rt::location* location, rt_sampler_t* sampler) {
	if (!command_buffer || !command_buffer->recording || !sampler || !rtd3d12_sampler_prepare(sampler)) return;
	rtd3d12_ir_sampler* command = static_cast<rtd3d12_ir_sampler*>(rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::bind_sampler));
	if (!command) return;
	*command = { location ? location->address : u08{0}, sampler, sampler->cpu };
	sampler->retain();
}
void rtd3d12_command_buffer_vertex_buffer(rt_command_buffer_t* command_buffer, rt::location* location, rt_buffer_t* buffer, usize offset) {
	rtd3d12_ir_vertex_buffer* command = static_cast<rtd3d12_ir_vertex_buffer*>(rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::vertex_buffer));
	if (!command) {
		return;
	}
	rt_buffer_t* node = buffer ? buffer->active : nullptr;
	*command = { location ? location->address : u08{0}, node, offset };
	rtd3d12_buffer_node_retain(node);
}

void rt_command_buffer_t::uniform_data(rt::location* location, const u08* data, usize size) {
	rt_program_t* program = rtd3d12_location_program(location);
	const rtd3d12_program_data_mapping* mapping = program && location && program->uniform_data_mappings[location->address]
		? &*program->uniform_data_mappings[location->address] : nullptr;
	if (!recording || !mapping || !data || size != mapping->byte_size) {
		rtd3d12_fail(rt::error::improper_usage, "program data write does not match reflected location: name={}, bytes={}, expected={}, recording={}", mapping ? mapping->name.c_str() : "<missing>", size, mapping ? mapping->byte_size : 0, recording);
		return;
	}
	rtd3d12_ir_program_data* command = static_cast<rtd3d12_ir_program_data*>(rtd3d12_command_append(this, rtd3d12_command_opcode::uniform_data));
	if (!command) return;
	command->bytes = rtd3d12::allocate_bytes(size);
	if (!command->bytes) {
		ir_size -= rtd3d12_command_record_size(rtd3d12_command_opcode::uniform_data);
		return;
	}
	memcpy(command->bytes, data, size);
	command->address = location->address;
	command->size = size;
	command->resource = nullptr;
	command->upload = nullptr;
}

void rt_command_buffer_t::storage_data(rt::location* location, const u08* data, usize size) {
	rt_program_t* program = rtd3d12_location_program(location);
	const rtd3d12_program_data_mapping* mapping = program && location && program->storage_data_mappings[location->address]
		? &*program->storage_data_mappings[location->address] : nullptr;
	if (!recording || !mapping || !data || size != mapping->byte_size) {
		rtd3d12_fail(rt::error::improper_usage, "program data write does not match reflected location: name={}, bytes={}, expected={}, recording={}", mapping ? mapping->name.c_str() : "<missing>", size, mapping ? mapping->byte_size : 0, recording);
		return;
	}
	rtd3d12_ir_program_data* command = static_cast<rtd3d12_ir_program_data*>(rtd3d12_command_append(this, rtd3d12_command_opcode::storage_data));
	if (!command) return;
	command->bytes = rtd3d12::allocate_bytes(size);
	if (!command->bytes) {
		ir_size -= rtd3d12_command_record_size(rtd3d12_command_opcode::storage_data);
		return;
	}
	memcpy(command->bytes, data, size);
	command->address = location->address;
	command->size = size;
	command->resource = nullptr;
	command->upload = nullptr;
}
void rtd3d12_command_buffer_index_buffer(rt_command_buffer_t* command_buffer, rt_buffer_t* buffer, usize offset, rt::index_format format) {
	rtd3d12_ir_index_buffer* command = static_cast<rtd3d12_ir_index_buffer*>(rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::index_buffer));
	if (!command) {
		return;
	}
	rt_buffer_t* node = buffer ? buffer->active : nullptr;
	*command = { node, offset, format };
	rtd3d12_buffer_node_retain(node);
}
void rtd3d12_command_buffer_draw(rt_command_buffer_t* command_buffer, usize count, usize first) {
	rtd3d12_ir_draw* command = static_cast<rtd3d12_ir_draw*>(rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::draw));
	if (!command) {
		return;
	}
	*command = { count, first };
}
void rtd3d12_command_buffer_draw_instanced(rt_command_buffer_t* command_buffer, usize count, usize instances, usize first, usize first_instance) {
	rtd3d12_ir_draw_instanced* command = static_cast<rtd3d12_ir_draw_instanced*>(rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::draw_instanced));
	if (!command) {
		return;
	}
	*command = { count, instances, first, first_instance };
}
void rtd3d12_command_buffer_draw_indexed(rt_command_buffer_t* command_buffer, usize count, usize first, usize vertex_offset) {
	rtd3d12_ir_draw_indexed* command = static_cast<rtd3d12_ir_draw_indexed*>(rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::draw_indexed));
	if (!command) {
		return;
	}
	*command = { count, first, vertex_offset };
}
void rtd3d12_command_buffer_draw_indexed_instanced(rt_command_buffer_t* command_buffer, usize count, usize instances, usize first, usize vertex_offset, usize first_instance) {
	rtd3d12_ir_draw_indexed_instanced* command = static_cast<rtd3d12_ir_draw_indexed_instanced*>(rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::draw_indexed_instanced));
	if (!command) {
		return;
	}
	*command = { count, instances, first, vertex_offset, first_instance };
}
void rtd3d12_command_buffer_end(rt_command_buffer_t* command_buffer) {
	if (!command_buffer || !command_buffer->recording || command_buffer->rendering) {
		return;
	}
	command_buffer->recording = false;
	command_buffer->executable = true;
}

static ID3D12Resource* rtd3d12_command_buffer_upload(rtd3d12_context* ctx, const u08* data, usize size) {
	if (!data || !size) {
		return nullptr;
	}
	D3D12_HEAP_PROPERTIES heap = {};
	heap.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = size;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ID3D12Resource* upload = nullptr;
	HRESULT result = ctx->d3d_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload));
	if (FAILED(result)) {
		rtd3d12_fail(rtd3d12_error_from_hresult(result), "CreateCommittedResource(command upload) failed: 0x{:08x}", static_cast<u32>(result));
		return nullptr;
	}
	void* mapped = nullptr;
	result = upload->Map(0, nullptr, &mapped);
	if (FAILED(result)) {
		if (upload) {
			upload->Release();
			upload = nullptr;
		}
		rtd3d12_fail(rtd3d12_error_from_hresult(result), "ID3D12Resource::Map(command upload) failed: 0x{:08x}", static_cast<u32>(result));
		return nullptr;
	}
	memcpy(mapped, data, size);
	upload->Unmap(0, nullptr);
	return upload;
}

void rtd3d12_command_buffer_buffer_data(rt_command_buffer_t* command_buffer, rt_buffer_t* buffer, rt::buffer_range range, const u08* data) {
	if (!command_buffer || !command_buffer->recording || !buffer || !buffer->active || !data || !range.size || range.offset > buffer->active->size || range.size > buffer->active->size - range.offset) {
		return;
	}
	rtd3d12_buffer_write write = rtd3d12_buffer_write_begin(rtd3d12_get_current_context(), buffer);
	if (!write.target) {
		return;
	}
	ID3D12Resource* upload = rtd3d12_command_buffer_upload(rtd3d12_get_current_context(), data, range.size);
	if (!upload) {
		return;
	}
	rtd3d12_ir_buffer_data* command = static_cast<rtd3d12_ir_buffer_data*>(rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::buffer_data));
	if (!command) {
		if (upload) {
			upload->Release();
			upload = nullptr;
		}
		return;
	}
	*command = { write.source, write.target, range, upload };
	rtd3d12_buffer_node_retain(write.source);
	rtd3d12_buffer_node_retain(write.target);
	rtd3d12_buffer_write_commit(buffer, &write);
}

void rtd3d12_command_buffer_buffer_copy(rt_command_buffer_t* command_buffer, rt_buffer_t* src, rt::buffer_range src_range, rt_buffer_t* dst, rt::buffer_range dst_range) {
	if (!command_buffer || !command_buffer->recording || !src || !dst || !src->active || !dst->active || !src_range.size || src_range.size != dst_range.size || src_range.offset > src->active->size || src_range.size > src->active->size - src_range.offset || dst_range.offset > dst->active->size || dst_range.size > dst->active->size - dst_range.offset) {
		return;
	}
	rtd3d12_buffer_write write = rtd3d12_buffer_write_begin(rtd3d12_get_current_context(), dst);
	if (!write.target) {
		return;
	}
	rtd3d12_ir_buffer_copy* command = static_cast<rtd3d12_ir_buffer_copy*>(rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::buffer_copy));
	if (!command) {
		return;
	}
	*command = { src->active, src_range, write.source, write.target, dst_range };
	rtd3d12_buffer_node_retain(command->source);
	rtd3d12_buffer_node_retain(command->target_copy_source);
	rtd3d12_buffer_node_retain(command->target);
	rtd3d12_buffer_write_commit(dst, &write);
}

void rtd3d12_command_buffer_buffer_barrier(rt_command_buffer_t* command_buffer, rt_buffer_t* buffer, rt::buffer_range range, rt::access src, rt::access dst) {
	if (!command_buffer || !command_buffer->recording || !buffer || !buffer->active || range.offset > buffer->active->size || range.size > buffer->active->size - range.offset) {
		return;
	}
	rtd3d12_ir_buffer_barrier* command = static_cast<rtd3d12_ir_buffer_barrier*>(rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::buffer_barrier));
	if (!command) {
		return;
	}
	*command = { buffer->active, src, dst };
	rtd3d12_buffer_node_retain(command->buffer);
}

static bool rtd3d12_texture_range_valid(rtd3d12_image_base* image, rt::texture_range range) {
	if (!image || !image->d3d_resource || !range.mip_count || !range.layer_count || !range.extent.width || !range.extent.height || !range.extent.depth || range.base_mip >= image->mip_count || range.mip_count > image->mip_count - range.base_mip) {
		return false;
	}
	const rt::texture_aspect available = rtd3d12_texture_format_is_depth(image->dxgi_format)
													  ? (image->dxgi_format == DXGI_FORMAT_D24_UNORM_S8_UINT || image->dxgi_format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT
															 ? (rt::texture_aspect::depth | rt::texture_aspect::stencil)
															 : rt::texture_aspect::depth)
													  : rt::texture_aspect::color;
	if (range.aspects == rt::texture_aspect::none || static_cast<u32>(range.aspects & ~available) != 0) {
		return false;
	}
	const usize layers = (image->type == rt::texture_type::texture_1d_array || image->type == rt::texture_type::texture_2d_array) ? image->layer_count : 1;
	if (range.base_layer >= layers || range.layer_count > layers - range.base_layer) {
		return false;
	}
	if (image->type == rt::texture_type::texture_1d || image->type == rt::texture_type::texture_1d_array) {
		if (range.offset.height || range.extent.height != 1 || range.offset.depth || range.extent.depth != 1) {
			return false;
		}
	}
	if (image->type != rt::texture_type::texture_3d && (range.offset.depth || range.extent.depth != 1)) {
		return false;
	}
	for (usize mip = 0; mip < range.mip_count; ++mip) {
		const usize actual_mip = range.base_mip + mip;
		const usize width = image->width >> actual_mip ? image->width >> actual_mip : 1;
		const usize height = image->type == rt::texture_type::texture_1d || image->type == rt::texture_type::texture_1d_array ? 1 : (image->height >> actual_mip ? image->height >> actual_mip : 1);
		const usize depth = image->type == rt::texture_type::texture_3d ? (image->depth >> actual_mip ? image->depth >> actual_mip : 1) : 1;
		if (range.offset.width > width || range.extent.width > width - range.offset.width || range.offset.height > height || range.extent.height > height - range.offset.height || range.offset.depth > depth || range.extent.depth > depth - range.offset.depth) {
			return false;
		}
	}
	return true;
}

static bool rtd3d12_texture_range_copy_supported(const rtd3d12_image_base* image, rt::texture_range range) {
	/* D3D12 copy locations address a complete subresource. Depth/stencil formats
	 * therefore transfer the complete packed depth-stencil texel, never one
	 * aspect while silently copying the other. */
	if (!image || (image->dxgi_format != DXGI_FORMAT_D24_UNORM_S8_UINT && image->dxgi_format != DXGI_FORMAT_D32_FLOAT_S8X24_UINT)) {
		return true;
	}
	return range.aspects == (rt::texture_aspect::depth | rt::texture_aspect::stencil);
}

static usize rtd3d12_command_texture_range_bytes(const rtd3d12_image_base* image, rt::texture_range range);

void rtd3d12_command_buffer_buffer_copy_to_texture(rt_command_buffer_t* cb, rt_buffer_t* src, rt::buffer_range src_range, rt_texture_t* dst, rt::texture_range dst_range) {
	rtd3d12_image_base* original = dst ? dst->active : nullptr;
	if (!cb || !cb->recording || !src || !src->active || !rtd3d12_texture_range_valid(original, dst_range) || !rtd3d12_texture_range_copy_supported(original, dst_range) || src_range.offset > src->active->size || src_range.size > src->active->size - src_range.offset) {
		return;
	}
	rtd3d12_texture_write write = rtd3d12_texture_write_begin(rtd3d12_get_current_context(), dst);
	if (!write.target) {
		return;
	}
	rtd3d12_ir_buffer_copy_to_texture* c = static_cast<rtd3d12_ir_buffer_copy_to_texture*>(rtd3d12_command_append(cb, rtd3d12_command_opcode::buffer_copy_to_texture));
	if (!c) {
		return;
	}
	*c = { src->active, src_range, write.source, write.target, dst_range, nullptr };
	rtd3d12_buffer_node_retain(c->source);
	if (write.source) {
		(write.source)->retain();
	}
	(write.target)->retain();
}

void rtd3d12_command_buffer_texture_copy(rt_command_buffer_t* cb, rt_texture_t* src, rt::texture_range src_range, rt_texture_t* dst, rt::texture_range dst_range) {
	rtd3d12_image_base* source = src ? src->active : nullptr;
	rtd3d12_image_base* original = dst ? dst->active : nullptr;
	if (!cb || !cb->recording || !rtd3d12_texture_range_valid(source, src_range) || !rtd3d12_texture_range_valid(original, dst_range) || !rtd3d12_texture_range_copy_supported(source, src_range) || !rtd3d12_texture_range_copy_supported(original, dst_range) || src_range.mip_count != dst_range.mip_count || src_range.layer_count != dst_range.layer_count || src_range.extent.width != dst_range.extent.width || src_range.extent.height != dst_range.extent.height || src_range.extent.depth != dst_range.extent.depth) {
		return;
	}
	rtd3d12_texture_write write = rtd3d12_texture_write_begin(rtd3d12_get_current_context(), dst);
	if (!write.target) {
		return;
	}
	rtd3d12_ir_texture_copy* c = static_cast<rtd3d12_ir_texture_copy*>(rtd3d12_command_append(cb, rtd3d12_command_opcode::texture_copy));
	if (!c) {
		return;
	}
	*c = { source, src_range, write.source, write.target, dst_range };
	(source)->retain();
	if (write.source) {
		(write.source)->retain();
	}
	(write.target)->retain();
}

void rtd3d12_command_buffer_texture_data(rt_command_buffer_t* cb, rt_texture_t* texture, rt::texture_range range, const u08* data) {
	rtd3d12_image_base* original = texture ? texture->active : nullptr;
	if (!cb || !cb->recording || !data || !rtd3d12_texture_range_valid(original, range) || !rtd3d12_texture_range_copy_supported(original, range)) {
		return;
	}
	rtd3d12_texture_write write = rtd3d12_texture_write_begin(rtd3d12_get_current_context(), texture);
	if (!write.target) {
		return;
	}
	rtd3d12_image_base* target = write.target;
	u32 bpp = target->dxgi_format == DXGI_FORMAT_R8G8B8A8_UNORM || target->dxgi_format == DXGI_FORMAT_B8G8R8A8_UNORM || target->dxgi_format == DXGI_FORMAT_D32_FLOAT || target->dxgi_format == DXGI_FORMAT_D24_UNORM_S8_UINT ? 4 : target->dxgi_format == DXGI_FORMAT_D16_UNORM			 ? 2
																																																							   : target->dxgi_format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT ? 8
																																																																						 : 0;
	usize size = range.extent.width * range.extent.height * range.extent.depth * range.mip_count * range.layer_count * bpp;
	if (!bpp || !size) {
		rtd3d12_fail(rt::error::unsupported_feature, "D3D12 recorded texture upload format is unsupported");
		return;
	}
	u08* copy = reinterpret_cast<u08*>(rtd3d12::allocate_bytes(size));
	if (!copy) {
		return;
	}
	memcpy(copy, data, size);
	rtd3d12_ir_texture_data* c = static_cast<rtd3d12_ir_texture_data*>(rtd3d12_command_append(cb, rtd3d12_command_opcode::texture_data));
	if (!c) {
		rtd3d12::release_bytes(copy);
		return;
	}
	*c = { write.source, target, range, copy, size, nullptr };
	if (write.source) {
		(write.source)->retain();
	}
	(target)->retain();
}

void rtd3d12_command_buffer_texture_copy_to_buffer(rt_command_buffer_t* cb, rt_texture_t* src, rt::texture_range src_range, rt_buffer_t* dst, rt::buffer_range dst_range) {
	rtd3d12_image_base* source = src ? src->active : nullptr;
	const usize packed_size = source ? rtd3d12_command_texture_range_bytes(source, src_range) : 0;
	if (!cb || !cb->recording || !dst || !dst->active || !rtd3d12_texture_range_valid(source, src_range) || !rtd3d12_texture_range_copy_supported(source, src_range) || !packed_size || dst_range.offset > dst->active->size || dst_range.size < packed_size || packed_size > dst->active->size - dst_range.offset) {
		return;
	}
	rt::buffer_range packed_dst_range = { packed_size, dst_range.offset };
	rtd3d12_buffer_write write = rtd3d12_buffer_write_begin(rtd3d12_get_current_context(), dst);
	if (!write.target) {
		return;
	}
	rtd3d12_ir_texture_copy_to_buffer* c = static_cast<rtd3d12_ir_texture_copy_to_buffer*>(rtd3d12_command_append(cb, rtd3d12_command_opcode::texture_copy_to_buffer));
	if (!c) {
		return;
	}
	*c = { source, src_range, write.source, write.target, packed_dst_range, nullptr };
	(source)->retain();
	rtd3d12_buffer_node_retain(write.source);
	rtd3d12_buffer_node_retain(c->target);
}

void rtd3d12_command_buffer_texture_barrier(rt_command_buffer_t* cb, rt_texture_t* texture, rt::texture_range range, rt::access src, rt::access dst) {
	rtd3d12_image_base* image = texture ? texture->active : nullptr;
	if (!cb || !cb->recording || !rtd3d12_texture_range_valid(image, range)) {
		return;
	}
	rtd3d12_ir_texture_barrier* c = static_cast<rtd3d12_ir_texture_barrier*>(rtd3d12_command_append(cb, rtd3d12_command_opcode::texture_barrier));
	if (!c) {
		return;
	}
	*c = { image, range, src, dst };
	(image)->retain();
}

void rtd3d12_lower_begin_rendering(rtd3d12_command_lower_state* state, ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_framebuffer* command) {
	state->framebuffer = command->framebuffer;
	state->color_count = state->framebuffer ? state->framebuffer->color_texture_count : 0;
	for (usize index = 0; index < state->color_count; index++) {
		rt_texture_view_t* view = state->framebuffer->color_views[index];
		if (!rtd3d12_texture_view_refresh(rtd3d12_get_current_context(), view)) {
			return;
		}
		state->color_images[index] = view->image;
		state->color_rtvs[index] = view->rtv;
	}
	rt_texture_view_t* depth_view = state->framebuffer ? state->framebuffer->depth_view : nullptr;
	if (depth_view && !rtd3d12_texture_view_refresh(rtd3d12_get_current_context(), depth_view)) {
		return;
	}
	state->depth_image = depth_view ? depth_view->image : nullptr;
	state->depth_dsv = depth_view ? depth_view->dsv : D3D12_CPU_DESCRIPTOR_HANDLE{};
	rt_texture_view_t* stencil_view = state->framebuffer ? state->framebuffer->stencil_view : nullptr;
	if (stencil_view && !rtd3d12_texture_view_refresh(rtd3d12_get_current_context(), stencil_view)) {
		return;
	}
	state->stencil_image = stencil_view ? stencil_view->image : nullptr;
	state->stencil_dsv = stencil_view ? stencil_view->dsv : D3D12_CPU_DESCRIPTOR_HANDLE{};
	if (!state->framebuffer || (!state->color_count && !state->depth_image && !state->stencil_image)) {
		return;
	}
	for (usize i = 0; i < state->color_count; ++i) {
		rtd3d12_command_transition_image(command_list, state->color_images[i], D3D12_RESOURCE_STATE_RENDER_TARGET);
	}
	if (state->depth_image) {
		rtd3d12_command_transition_image(command_list, state->depth_image, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}
	if (state->stencil_image && state->stencil_image != state->depth_image) {
		rtd3d12_command_transition_image(command_list, state->stencil_image, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}
	/* D3D12 exposes one DSV slot. A combined depth-stencil view is intentionally
	 * represented by the same view in both Rutile attachment slots. */
	D3D12_CPU_DESCRIPTOR_HANDLE* dsv = state->depth_image ? &state->depth_dsv : (state->stencil_image ? &state->stencil_dsv : nullptr);
	command_list->OMSetRenderTargets(static_cast<UINT>(state->color_count), state->color_count ? state->color_rtvs : nullptr, FALSE, dsv);
	rtd3d12_image_base* extent_image = state->color_count ? state->color_images[0] : (state->depth_image ? state->depth_image : state->stencil_image);
	D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<f32>(extent_image->width), static_cast<f32>(extent_image->height), 0.0f, 1.0f };
	D3D12_RECT scissor = { 0, 0, static_cast<LONG>(extent_image->width), static_cast<LONG>(extent_image->height) };
	command_list->RSSetViewports(1, &viewport);
	command_list->RSSetScissorRects(1, &scissor);
}

void rtd3d12_lower_clear_color(rtd3d12_command_lower_state* state, ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_clear_color* command) {
	f32 color[] = { command->r, command->g, command->b, command->a };
	if (command->index < state->color_count) {
		command_list->ClearRenderTargetView(state->color_rtvs[command->index], color, 0, nullptr);
	}
}

void rtd3d12_lower_clear_depth(rtd3d12_command_lower_state* state, ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_clear_depth* command) {
	if (state->depth_image) {
		command_list->ClearDepthStencilView(state->depth_dsv, D3D12_CLEAR_FLAG_DEPTH, command->depth, 0, 0, nullptr);
	}
}

void rtd3d12_lower_clear_stencil(rtd3d12_command_lower_state* state, ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_clear_stencil* command) {
	if (state->stencil_image) {
		command_list->ClearDepthStencilView(state->stencil_dsv, D3D12_CLEAR_FLAG_STENCIL, 0.0f, command->stencil, 0, nullptr);
	}
}

void rtd3d12_lower_end_rendering(rtd3d12_command_lower_state* state, ID3D12GraphicsCommandList* command_list) {
	for (usize index = 0; index < state->color_count; index++) {
		rtd3d12_image_base* image = state->color_images[index];
		if (image && image->swapchain_image) {
			rtd3d12_command_transition_image(command_list, image, D3D12_RESOURCE_STATE_PRESENT);
		}
	}
}

void rtd3d12_lower_set_viewport(ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_viewport* command) {
	D3D12_VIEWPORT viewport = {
		static_cast<f32>(command->x),
		static_cast<f32>(command->y),
		static_cast<f32>(command->width),
		static_cast<f32>(command->height),
		command->min_depth,
		command->max_depth,
	};
	command_list->RSSetViewports(1, &viewport);
}

void rtd3d12_lower_set_scissor(ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_scissor* command) {
	D3D12_RECT scissor = {
		static_cast<LONG>(command->x),
		static_cast<LONG>(command->y),
		static_cast<LONG>(command->x + command->width),
		static_cast<LONG>(command->y + command->height),
	};
	command_list->RSSetScissorRects(1, &scissor);
}

static void rtd3d12_lower_remember_buffer_binding(rtd3d12_command_lower_state* state, const rtd3d12_ir_buffer* command) {
	for (usize index = 0; index < state->pending_buffer_count; ++index) {
		if (state->pending_buffers[index].address == command->address) {
			state->pending_buffers[index] = *command;
			return;
		}
	}
	if (state->pending_buffer_count < sizeof(state->pending_buffers) / sizeof(state->pending_buffers[0])) {
		state->pending_buffers[state->pending_buffer_count++] = *command;
	}
}

static void rtd3d12_lower_remember_texture_binding(rtd3d12_command_lower_state* state, const rtd3d12_ir_texture* command) {
	for (usize index = 0; index < state->pending_texture_count; ++index) {
		if (state->pending_textures[index].address == command->address) {
			state->pending_textures[index] = *command;
			return;
		}
	}
	if (state->pending_texture_count < sizeof(state->pending_textures) / sizeof(state->pending_textures[0])) {
		state->pending_textures[state->pending_texture_count++] = *command;
	}
}

void rtd3d12_lower_use_program(rtd3d12_context* ctx, rtd3d12_command_lower_state* state, ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_program* command) {
	state->program = command->program;
	if (!state->program || !state->program->d3d_root_signature || !state->program->d3d_pipeline) {
		state->program = nullptr;
		return;
	}
	if (state->program->d3d_compute_pipeline) {
		command_list->SetComputeRootSignature(state->program->d3d_root_signature);
		command_list->SetPipelineState(state->program->d3d_pipeline);
	} else {
		if (!state->framebuffer || !state->color_count || !state->program->prepare(state->color_images[0]->dxgi_format, state->depth_image ? state->depth_image->dxgi_format : DXGI_FORMAT_UNKNOWN)) {
			state->program = nullptr;
			return;
		}
		command_list->SetGraphicsRootSignature(state->program->d3d_root_signature);
		command_list->SetPipelineState(state->program->d3d_pipeline);
		command_list->IASetPrimitiveTopology(state->program->d3d_primitive_topology);
	}
	for (usize index = 0; index < state->pending_buffer_count; ++index) {
		rtd3d12_lower_bind_buffer(ctx, state, command_list, &state->pending_buffers[index]);
	}
	for (usize index = 0; index < state->pending_texture_count; ++index) {
		rtd3d12_lower_bind_texture(ctx, state, command_list, &state->pending_textures[index]);
	}
	for (u32 address = 0; address < 256; address++) {
		if (state->program->uniform_data_mappings[address] && state->uniform_resources[address])
			rtd3d12_lower_bind_program_data(ctx, state, command_list, *state->program->uniform_data_mappings[address], state->uniform_resources[address], false);
		if (state->program->storage_data_mappings[address] && state->storage_resources[address])
			rtd3d12_lower_bind_program_data(ctx, state, command_list, *state->program->storage_data_mappings[address], state->storage_resources[address], true);
	}
}

void rtd3d12_command_buffer_dispatch(rt_command_buffer_t* command_buffer, usize group_count_x, usize group_count_y, usize group_count_z) {
	rtd3d12_ir_dispatch* command = static_cast<rtd3d12_ir_dispatch*>(rtd3d12_command_append(command_buffer, rtd3d12_command_opcode::dispatch));
	if (!command) return;
	*command = { group_count_x, group_count_y, group_count_z };
}

void rtd3d12_lower_bind_buffer(rtd3d12_context* ctx, rtd3d12_command_lower_state* state, ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_buffer* command) {
	if (!command->buffer || command->offset > command->buffer->size || !command->size || command->size > command->buffer->size - command->offset) {
		return;
	}
	rtd3d12_lower_remember_buffer_binding(state, command);
	if (!state->program) {
		return;
	}
	if (!state->program->descriptor_mappings[command->address]) {
		return;
	}
	const rtd3d12_program_descriptor_mapping& mapping = *state->program->descriptor_mappings[command->address];
	if (mapping.type != rtd3d12_descriptor_type::constant_buffer && mapping.type != rtd3d12_descriptor_type::storage_buffer) return;
	if (!state->resource_heap || !command->buffer->d3d_resource) {
		return;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE cpu = state->resource_heap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE gpu = state->resource_heap->GetGPUDescriptorHandleForHeapStart();
	cpu.ptr += state->resource_index * state->resource_step;
	gpu.ptr += state->resource_index++ * state->resource_step;
	if (mapping.type == rtd3d12_descriptor_type::constant_buffer) {
		/* A D3D12 CBV uses a 256-byte-aligned address and rounded byte size.
		 * The public range is logical, but it must fit in the physical backing
		 * after D3D12's rounding. */
		const u64 cbv_size = static_cast<u64>(command->size + 255u) & ~UINT64_C(255);
		const u64 allocation_size = command->buffer->d3d_resource->GetDesc().Width;
		if (command->offset % 256u || cbv_size > UINT_MAX || command->offset > allocation_size || cbv_size > allocation_size - command->offset) {
			rtd3d12_fail(rt::error::improper_usage, "uniform buffer range is not representable as a D3D12 constant-buffer view");
			return;
		}
		rtd3d12_command_transition_buffer(command_list, command->buffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {
			command->buffer->d3d_resource->GetGPUVirtualAddress() + command->offset,
			static_cast<UINT>(cbv_size),
		};
		ctx->d3d_device->CreateConstantBufferView(&desc, cpu);
		rtd3d12_set_root_descriptor_table(state, command_list, mapping.root_parameter, gpu);
	} else {
		if (command->offset % 4u || command->size % 4u) {
			rtd3d12_fail(rt::error::improper_usage, "storage buffer range is not representable as a D3D12 raw UAV");
			return;
		}
		rtd3d12_command_transition_buffer(command_list, command->buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
		desc.Format = DXGI_FORMAT_R32_TYPELESS;
		desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		desc.Buffer.FirstElement = command->offset / 4u;
		desc.Buffer.NumElements = command->size / 4u;
		desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
		ctx->d3d_device->CreateUnorderedAccessView(command->buffer->d3d_resource, nullptr, &desc, cpu);
		rtd3d12_set_root_descriptor_table(state, command_list, mapping.root_parameter, gpu);
	}
}

void rtd3d12_lower_bind_texture(rtd3d12_context* ctx, rtd3d12_command_lower_state* state, ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_texture* command) {
	rt_texture_view_t* texture_view = command->texture_view;
	rtd3d12_image_base* image = command->image;
	if (!texture_view || !image || !image->d3d_resource) {
		return;
	}
	rtd3d12_lower_remember_texture_binding(state, command);
	if (!state->program || !state->resource_heap) {
		return;
	}
	if (!state->program->descriptor_mappings[command->address]) {
		return;
	}
	const rtd3d12_program_descriptor_mapping& mapping = *state->program->descriptor_mappings[command->address];
	if (mapping.type != rtd3d12_descriptor_type::texture && mapping.type != rtd3d12_descriptor_type::storage_texture) return;
	if (mapping.type == rtd3d12_descriptor_type::storage_texture) {
		D3D12_CPU_DESCRIPTOR_HANDLE cpu = state->resource_heap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE gpu = state->resource_heap->GetGPUDescriptorHandleForHeapStart();
		cpu.ptr += state->resource_index * state->resource_step;
		gpu.ptr += state->resource_index++ * state->resource_step;
		rtd3d12_command_transition_image(command_list, image, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
		uav.Format = image->dxgi_format;
		switch (image->type) {
		case rt::texture_type::texture_1d: uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D; break;
		case rt::texture_type::texture_2d: uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D; break;
		case rt::texture_type::texture_3d: uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D; uav.Texture3D.WSize = static_cast<UINT>(image->depth); break;
		default: return;
		}
		ctx->d3d_device->CreateUnorderedAccessView(image->d3d_resource, nullptr, &uav, cpu);
		rtd3d12_set_root_descriptor_table(state, command_list, mapping.root_parameter, gpu);
		if (state->program->d3d_compute_pipeline) return;
	}
	if (!state->sampler_heap) return;
	D3D12_CPU_DESCRIPTOR_HANDLE resource_cpu = state->resource_heap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE resource_gpu = state->resource_heap->GetGPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE sampler_cpu = state->sampler_heap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE sampler_gpu = state->sampler_heap->GetGPUDescriptorHandleForHeapStart();
	resource_cpu.ptr += state->resource_index * state->resource_step;
	resource_gpu.ptr += state->resource_index++ * state->resource_step;
	sampler_cpu.ptr += state->sampler_index * state->sampler_step;
	sampler_gpu.ptr += state->sampler_index++ * state->sampler_step;
	rtd3d12_command_transition_image(command_list, image, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
	srv.Format = image->dxgi_format;
	if (rtd3d12_texture_format_is_depth(image->dxgi_format)) {
		switch (image->dxgi_format) {
		case DXGI_FORMAT_D16_UNORM:
			srv.Format = DXGI_FORMAT_R16_UNORM;
			break;
		case DXGI_FORMAT_D32_FLOAT:
			srv.Format = DXGI_FORMAT_R32_FLOAT;
			break;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
			srv.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			break;
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			srv.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
			break;
		default:
			break;
		}
	}
	srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	switch (image->type) {
	case rt::texture_type::texture_1d:
		srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
		srv.Texture1D.MipLevels = static_cast<UINT>(image->mip_count);
		break;
	case rt::texture_type::texture_1d_array:
		srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
		srv.Texture1DArray.ArraySize = static_cast<UINT>(image->layer_count);
		srv.Texture1DArray.MipLevels = static_cast<UINT>(image->mip_count);
		break;
	case rt::texture_type::texture_2d:
		srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv.Texture2D.MipLevels = static_cast<UINT>(image->mip_count);
		break;
	case rt::texture_type::texture_2d_array:
		srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srv.Texture2DArray.ArraySize = static_cast<UINT>(image->layer_count);
		srv.Texture2DArray.MipLevels = static_cast<UINT>(image->mip_count);
		break;
	case rt::texture_type::texture_3d:
		srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		srv.Texture3D.MipLevels = static_cast<UINT>(image->mip_count);
		break;
	default:
		return;
	}
	ctx->d3d_device->CreateShaderResourceView(image->d3d_resource, &srv, resource_cpu);
	ctx->d3d_device->CopyDescriptorsSimple(1, sampler_cpu, command->sampler_cpu, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	rtd3d12_set_root_descriptor_table(state, command_list, mapping.type == rtd3d12_descriptor_type::storage_texture ? mapping.sampled_root_parameter : mapping.root_parameter, resource_gpu);
	rtd3d12_set_root_descriptor_table(state, command_list, mapping.sampler_root_parameter, sampler_gpu);
}

void rtd3d12_lower_vertex_buffer(rtd3d12_command_lower_state* state, ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_vertex_buffer* command) {
	if (!command->buffer || !state->program) {
		return;
	}
	if (!state->program->input_mappings[command->address]) {
		return;
	}
	const rtd3d12_program_input_mapping& mapping = *state->program->input_mappings[command->address];
	if (mapping.vertex_input >= state->program->vertex_layout.input_count) return;

	rtd3d12_command_transition_buffer(command_list, command->buffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	D3D12_VERTEX_BUFFER_VIEW view = command->buffer->vertex_view;
	view.BufferLocation += command->offset;
	view.SizeInBytes -= static_cast<UINT>(command->offset);
	view.StrideInBytes = static_cast<UINT>(state->program->vertex_layout.inputs[mapping.vertex_input].stride);
	command_list->IASetVertexBuffers(static_cast<UINT>(mapping.vertex_input), 1, &view);
}

void rtd3d12_lower_index_buffer(ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_index_buffer* command) {
	if (!command->buffer) {
		return;
	}

	rtd3d12_command_transition_buffer(command_list, command->buffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	D3D12_INDEX_BUFFER_VIEW view = {
		command->buffer->d3d_resource->GetGPUVirtualAddress() + command->offset,
		static_cast<UINT>(command->buffer->size - command->offset),
		command->format == rt::index_format::u16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
	};
	command_list->IASetIndexBuffer(&view);
}

void rtd3d12_lower_draw(ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_draw* command) {
	command_list->DrawInstanced(command->count, 1, command->first, 0);
}

void rtd3d12_lower_draw_instanced(ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_draw_instanced* command) {
	command_list->DrawInstanced(command->count, command->instances, command->first, command->first_instance);
}

void rtd3d12_lower_draw_indexed(ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_draw_indexed* command) {
	command_list->DrawIndexedInstanced(command->count, 1, command->first, command->vertex_offset, 0);
}

void rtd3d12_lower_draw_indexed_instanced(ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_draw_indexed_instanced* command) {
	command_list->DrawIndexedInstanced(command->count, command->instances, command->first, command->vertex_offset, command->first_instance);
}

void rtd3d12_lower_dispatch(ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_dispatch* command) {
	command_list->Dispatch(static_cast<UINT>(command->group_count_x), static_cast<UINT>(command->group_count_y), static_cast<UINT>(command->group_count_z));
}

void rtd3d12_lower_buffer_data(ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_buffer_data* command) {
	if (!command || !command->target || !command->target->d3d_resource || !command->upload) {
		return;
	}
	if (command->copy_source && command->copy_source != command->target) {
		rtd3d12_command_transition_buffer(command_list, command->copy_source, D3D12_RESOURCE_STATE_COPY_SOURCE);
		rtd3d12_command_transition_buffer(command_list, command->target, D3D12_RESOURCE_STATE_COPY_DEST);
		command_list->CopyBufferRegion(command->target->d3d_resource, 0, command->copy_source->d3d_resource, 0, command->target->size);
	}
	rtd3d12_command_transition_buffer(command_list, command->target, D3D12_RESOURCE_STATE_COPY_DEST);
	command_list->CopyBufferRegion(command->target->d3d_resource, command->range.offset, command->upload, 0, command->range.size);
}

void rtd3d12_lower_buffer_copy(ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_buffer_copy* command) {
	if (!command || !command->source || !command->target || !command->source->d3d_resource || !command->target->d3d_resource) {
		return;
	}
	if (command->target_copy_source && command->target_copy_source != command->target) {
		rtd3d12_command_transition_buffer(command_list, command->target_copy_source, D3D12_RESOURCE_STATE_COPY_SOURCE);
		rtd3d12_command_transition_buffer(command_list, command->target, D3D12_RESOURCE_STATE_COPY_DEST);
		command_list->CopyBufferRegion(command->target->d3d_resource, 0, command->target_copy_source->d3d_resource, 0, command->target->size);
	}
	rtd3d12_command_transition_buffer(command_list, command->source, D3D12_RESOURCE_STATE_COPY_SOURCE);
	rtd3d12_command_transition_buffer(command_list, command->target, D3D12_RESOURCE_STATE_COPY_DEST);
	command_list->CopyBufferRegion(command->target->d3d_resource, command->dst_range.offset, command->source->d3d_resource, command->src_range.offset, command->src_range.size);
}

static u32 rtd3d12_command_texture_bpp(DXGI_FORMAT format) {
	return (format == DXGI_FORMAT_R8G8B8A8_UNORM || format == DXGI_FORMAT_B8G8R8A8_UNORM || format == DXGI_FORMAT_D32_FLOAT || format == DXGI_FORMAT_D24_UNORM_S8_UINT) ? 4 : format == DXGI_FORMAT_D16_UNORM		   ? 2
																																										  : format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT ? 8
																																																					   : 0;
}

static UINT rtd3d12_command_texture_subresource(const rtd3d12_image_base* image, usize mip, usize layer) {
	return static_cast<UINT>(layer * image->mip_count + mip);
}

static usize rtd3d12_command_texture_range_bytes(const rtd3d12_image_base* image, rt::texture_range range) {
	const u32 bpp = image ? rtd3d12_command_texture_bpp(image->dxgi_format) : 0;
	return bpp ? range.extent.width * range.extent.height * range.extent.depth * range.mip_count * range.layer_count * bpp : 0;
}

static D3D12_PLACED_SUBRESOURCE_FOOTPRINT rtd3d12_command_texture_footprint(rtd3d12_image_base* image, rt::texture_range range, u64 offset) {
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT result = {};
	const u32 bpp = rtd3d12_command_texture_bpp(image->dxgi_format);
	result.Offset = offset;
	result.Footprint.Format = image->d3d_resource->GetDesc().Format;
	result.Footprint.Width = static_cast<UINT>(range.extent.width);
	result.Footprint.Height = static_cast<UINT>(range.extent.height);
	result.Footprint.Depth = static_cast<UINT>(range.extent.depth);
	result.Footprint.RowPitch = static_cast<UINT>((range.extent.width * bpp + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1));
	return result;
}

static void rtd3d12_lower_texture_revision_copy(ID3D12GraphicsCommandList* command_list, rtd3d12_image_base* source, rtd3d12_image_base* target) {
	if (!source || !target || source == target || !source->d3d_resource || !target->d3d_resource) {
		return;
	}
	rtd3d12_command_transition_image(command_list, source, D3D12_RESOURCE_STATE_COPY_SOURCE);
	rtd3d12_command_transition_image(command_list, target, D3D12_RESOURCE_STATE_COPY_DEST);
	command_list->CopyResource(target->d3d_resource, source->d3d_resource);
}

static D3D12_RESOURCE_STATES rtd3d12_access_state(rt::access access) {
	if (access.type == rt::access_type::write) {
		if (static_cast<u32>(access.pipeline_stage & rt::stage::color_attachment) != 0) {
			return D3D12_RESOURCE_STATE_RENDER_TARGET;
		}
		if (static_cast<u32>(access.pipeline_stage & rt::stage::depth_stencil_attachment) != 0) {
			return D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}
		if (static_cast<u32>(access.pipeline_stage & rt::stage::transfer) != 0) {
			return D3D12_RESOURCE_STATE_COPY_DEST;
		}
		return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}
	if (static_cast<u32>(access.pipeline_stage & rt::stage::transfer) != 0) {
		return D3D12_RESOURCE_STATE_COPY_SOURCE;
	}
	if (static_cast<u32>(access.pipeline_stage & rt::stage::depth_stencil_attachment) != 0) {
		return D3D12_RESOURCE_STATE_DEPTH_READ;
	}
	D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
	if (static_cast<u32>(access.pipeline_stage & rt::stage::fragment) != 0) {
		state = static_cast<D3D12_RESOURCE_STATES>(state | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	if (static_cast<u32>(access.pipeline_stage & (rt::stage::vertex | rt::stage::compute)) != 0) {
		state = static_cast<D3D12_RESOURCE_STATES>(state | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}
	return state == D3D12_RESOURCE_STATE_COMMON ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : state;
}

static void rtd3d12_lower_uav_barrier(ID3D12GraphicsCommandList* command_list, ID3D12Resource* resource) {
	if (!resource) {
		return;
	}
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.UAV.pResource = resource;
	command_list->ResourceBarrier(1, &barrier);
}

static void rtd3d12_lower_buffer_barrier(ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_buffer_barrier* command) {
	if (!command || !command->buffer) {
		return;
	}
	/* A transition carries layout/access state. A UAV barrier additionally makes
	 * preceding unordered writes visible when the source access says write. */
	if (command->src.type == rt::access_type::write && rtd3d12_access_state(command->src) == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
		rtd3d12_lower_uav_barrier(command_list, command->buffer->d3d_resource);
	}
	rtd3d12_command_transition_buffer(command_list, command->buffer, rtd3d12_access_state(command->dst));
}

static void rtd3d12_lower_texture_barrier(ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_texture_barrier* command) {
	if (!command || !command->image) {
		return;
	}
	if (command->src.type == rt::access_type::write && rtd3d12_access_state(command->src) == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
		rtd3d12_lower_uav_barrier(command_list, command->image->d3d_resource);
	}
	rtd3d12_command_transition_image_range(command_list, command->image, command->range, rtd3d12_access_state(command->dst));
}

void rtd3d12_lower_texture_copy(ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_texture_copy* command) {
	if (!command || !command->source || !command->target || command->source->dxgi_format != command->target->dxgi_format) {
		return;
	}
	rtd3d12_lower_texture_revision_copy(command_list, command->target_copy_source, command->target);
	rtd3d12_command_transition_image_range(command_list, command->source, command->src_range, D3D12_RESOURCE_STATE_COPY_SOURCE);
	rtd3d12_command_transition_image_range(command_list, command->target, command->dst_range, D3D12_RESOURCE_STATE_COPY_DEST);
	const usize mip_count = command->src_range.mip_count < command->dst_range.mip_count ? command->src_range.mip_count : command->dst_range.mip_count;
	const usize layer_count = command->src_range.layer_count < command->dst_range.layer_count ? command->src_range.layer_count : command->dst_range.layer_count;
	for (usize mip = 0; mip < mip_count; ++mip)
		for (usize layer = 0; layer < layer_count; ++layer) {
			D3D12_TEXTURE_COPY_LOCATION src = {};
			src.pResource = command->source->d3d_resource;
			src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			src.SubresourceIndex = rtd3d12_command_texture_subresource(command->source, command->src_range.base_mip + mip, command->src_range.base_layer + layer);
			D3D12_TEXTURE_COPY_LOCATION dst = {};
			dst.pResource = command->target->d3d_resource;
			dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dst.SubresourceIndex = rtd3d12_command_texture_subresource(command->target, command->dst_range.base_mip + mip, command->dst_range.base_layer + layer);
			D3D12_BOX box = { static_cast<UINT>(command->src_range.offset.width), static_cast<UINT>(command->src_range.offset.height), static_cast<UINT>(command->src_range.offset.depth), static_cast<UINT>(command->src_range.offset.width + command->src_range.extent.width), static_cast<UINT>(command->src_range.offset.height + command->src_range.extent.height), static_cast<UINT>(command->src_range.offset.depth + command->src_range.extent.depth) };
			command_list->CopyTextureRegion(&dst, static_cast<UINT>(command->dst_range.offset.width), static_cast<UINT>(command->dst_range.offset.height), static_cast<UINT>(command->dst_range.offset.depth), &src, &box);
		}
}

void rtd3d12_lower_texture_data(rtd3d12_context* ctx, ID3D12GraphicsCommandList* command_list, rtd3d12_ir_texture_data* command) {
	if (!command || !command->target || !command->data || command->upload) {
		return;
	}
	rtd3d12_lower_texture_revision_copy(command_list, command->copy_source, command->target);
	u32 bpp = rtd3d12_command_texture_bpp(command->target->dxgi_format);
	if (!bpp) {
		return;
	}
	const usize region_bytes = command->range.extent.width * command->range.extent.height * command->range.extent.depth * bpp;
	const usize region_count = command->range.mip_count * command->range.layer_count;
	if (!region_bytes || command->data_size < region_bytes * region_count) {
		rtd3d12_fail(rt::error::improper_usage, "texture upload bytes are too small");
		return;
	}
	const u64 row_pitch = (command->range.extent.width * bpp + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
	const u64 region_size = row_pitch * command->range.extent.height * command->range.extent.depth;
	const u64 total = region_size * region_count;
	D3D12_HEAP_PROPERTIES heap = {};
	heap.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC buffer = {};
	buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	buffer.Width = total;
	buffer.Height = 1;
	buffer.DepthOrArraySize = 1;
	buffer.MipLevels = 1;
	buffer.SampleDesc.Count = 1;
	buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	HRESULT result = ctx->d3d_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&command->upload));
	if (FAILED(result)) {
		rtd3d12_fail(rtd3d12_error_from_hresult(result), "CreateCommittedResource(texture command upload) failed: 0x{:08x}", static_cast<u32>(result));
		return;
	}
	void* mapped = nullptr;
	result = command->upload->Map(0, nullptr, &mapped);
	if (FAILED(result)) {
		rtd3d12_fail(rtd3d12_error_from_hresult(result), "Map(texture command upload) failed: 0x{:08x}", static_cast<u32>(result));
		return;
	}
	for (usize region = 0; region < region_count; ++region)
		for (usize z = 0; z < command->range.extent.depth; ++z)
			for (usize y = 0; y < command->range.extent.height; ++y) {
				memcpy(static_cast<u08*>(mapped) + region * region_size + (z * command->range.extent.height + y) * row_pitch, command->data + region * region_bytes + (z * command->range.extent.height + y) * command->range.extent.width * bpp, command->range.extent.width * bpp);
			}
	command->upload->Unmap(0, nullptr);
	rtd3d12_command_transition_image_range(command_list, command->target, command->range, D3D12_RESOURCE_STATE_COPY_DEST);
	usize region = 0;
	for (usize mip = 0; mip < command->range.mip_count; ++mip)
		for (usize layer = 0; layer < command->range.layer_count; ++layer, ++region) {
			D3D12_TEXTURE_COPY_LOCATION src = {};
			src.pResource = command->upload;
			src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			src.PlacedFootprint = rtd3d12_command_texture_footprint(command->target, command->range, region * region_size);
			D3D12_TEXTURE_COPY_LOCATION dst = {};
			dst.pResource = command->target->d3d_resource;
			dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dst.SubresourceIndex = rtd3d12_command_texture_subresource(command->target, command->range.base_mip + mip, command->range.base_layer + layer);
			command_list->CopyTextureRegion(&dst, static_cast<UINT>(command->range.offset.width), static_cast<UINT>(command->range.offset.height), static_cast<UINT>(command->range.offset.depth), &src, nullptr);
		}
}

void rtd3d12_lower_buffer_copy_to_texture(rtd3d12_context* ctx, ID3D12GraphicsCommandList* command_list, rtd3d12_ir_buffer_copy_to_texture* command) {
	if (!command || !command->source || !command->source->d3d_resource || !command->target || command->staging) {
		return;
	}
	rtd3d12_lower_texture_revision_copy(command_list, command->target_copy_source, command->target);
	u32 bpp = rtd3d12_command_texture_bpp(command->target->dxgi_format);
	const usize packed_size = rtd3d12_command_texture_range_bytes(command->target, command->dst_range);
	if (!bpp || command->src_range.size < packed_size) {
		rtd3d12_fail(rt::error::improper_usage, "buffer source range is too small for texture copy");
		return;
	}
	const usize region_count = command->dst_range.mip_count * command->dst_range.layer_count;
	const usize region_bytes = packed_size / region_count;
	const u64 row_pitch = (command->dst_range.extent.width * bpp + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
	const u64 region_size = row_pitch * command->dst_range.extent.height * command->dst_range.extent.depth;
	const u64 total = region_size * region_count;
	D3D12_HEAP_PROPERTIES heap = {};
	heap.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC buffer = {};
	buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	buffer.Width = total;
	buffer.Height = 1;
	buffer.DepthOrArraySize = 1;
	buffer.MipLevels = 1;
	buffer.SampleDesc.Count = 1;
	buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	HRESULT result = ctx->d3d_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&command->staging));
	if (FAILED(result)) {
		rtd3d12_fail(rtd3d12_error_from_hresult(result), "CreateCommittedResource(buffer texture upload) failed: 0x{:08x}", static_cast<u32>(result));
		return;
	}
	void* mapped = nullptr;
	result = command->staging->Map(0, nullptr, &mapped);
	if (FAILED(result)) {
		rtd3d12_fail(rtd3d12_error_from_hresult(result), "Map(buffer texture upload) failed: 0x{:08x}", static_cast<u32>(result));
		return;
	}
	if (command->source->memory_type != rt::memory_type::host) {
		command->staging->Unmap(0, nullptr);
		rtd3d12_fail(rt::error::unsupported_feature, "device-memory buffer to texture copies require a host-visible source");
		return;
	}
	const u08* source = nullptr;
	void* mapped_source = nullptr;
	D3D12_RANGE read_range = { static_cast<SIZE_T>(command->src_range.offset), static_cast<SIZE_T>(command->src_range.offset + command->src_range.size) };
	result = command->source->d3d_resource->Map(0, &read_range, &mapped_source);
	if (FAILED(result)) {
		command->staging->Unmap(0, nullptr);
		rtd3d12_fail(rtd3d12_error_from_hresult(result), "Map(host buffer texture source) failed: 0x{:08x}", static_cast<u32>(result));
		return;
	}
	source = static_cast<const u08*>(mapped_source) + command->src_range.offset;
	for (usize region = 0; region < region_count; ++region)
		for (usize z = 0; z < command->dst_range.extent.depth; ++z)
			for (usize y = 0; y < command->dst_range.extent.height; ++y) {
				memcpy(static_cast<u08*>(mapped) + region * region_size + (z * command->dst_range.extent.height + y) * row_pitch, source + region * region_bytes + (z * command->dst_range.extent.height + y) * command->dst_range.extent.width * bpp, command->dst_range.extent.width * bpp);
			}
	command->staging->Unmap(0, nullptr);
	if (mapped_source) {
		D3D12_RANGE write_range = { 0, 0 };
		command->source->d3d_resource->Unmap(0, &write_range);
	}
	rtd3d12_command_transition_image_range(command_list, command->target, command->dst_range, D3D12_RESOURCE_STATE_COPY_DEST);
	usize region = 0;
	for (usize mip = 0; mip < command->dst_range.mip_count; ++mip)
		for (usize layer = 0; layer < command->dst_range.layer_count; ++layer, ++region) {
			D3D12_TEXTURE_COPY_LOCATION src = {};
			src.pResource = command->staging;
			src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			src.PlacedFootprint = rtd3d12_command_texture_footprint(command->target, command->dst_range, region * region_size);
			D3D12_TEXTURE_COPY_LOCATION dst = {};
			dst.pResource = command->target->d3d_resource;
			dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dst.SubresourceIndex = rtd3d12_command_texture_subresource(command->target, command->dst_range.base_mip + mip, command->dst_range.base_layer + layer);
			command_list->CopyTextureRegion(&dst, static_cast<UINT>(command->dst_range.offset.width), static_cast<UINT>(command->dst_range.offset.height), static_cast<UINT>(command->dst_range.offset.depth), &src, nullptr);
		}
}

void rtd3d12_lower_texture_copy_to_buffer(rtd3d12_context* ctx, ID3D12GraphicsCommandList* command_list, rtd3d12_ir_texture_copy_to_buffer* command) {
	if (!command || !command->source || !command->target || !command->source->d3d_resource || !command->target->d3d_resource) {
		return;
	}
	const usize packed_size = rtd3d12_command_texture_range_bytes(command->source, command->src_range);
	if (!packed_size || command->dst_range.size < packed_size) {
		rtd3d12_fail(rt::error::improper_usage, "texture copy destination buffer range is too small");
		return;
	}
	if (command->target_copy_source && command->target_copy_source != command->target) {
		rtd3d12_command_transition_buffer(command_list, command->target_copy_source, D3D12_RESOURCE_STATE_COPY_SOURCE);
		rtd3d12_command_transition_buffer(command_list, command->target, D3D12_RESOURCE_STATE_COPY_DEST);
		command_list->CopyBufferRegion(command->target->d3d_resource, 0, command->target_copy_source->d3d_resource, 0, command->target->size);
	}
	const D3D12_RESOURCE_DESC texture_desc = command->source->d3d_resource->GetDesc();
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints;
	usize staging_size = 0;
	for (usize mip = 0; mip < command->src_range.mip_count; ++mip)
		for (usize layer = 0; layer < command->src_range.layer_count; ++layer) {
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
			u32 rows = 0;
			u64 row_size = 0;
			u64 total_size = 0;
			const UINT subresource = rtd3d12_command_texture_subresource(command->source, command->src_range.base_mip + mip, command->src_range.base_layer + layer);
			ctx->d3d_device->GetCopyableFootprints(&texture_desc, subresource, 1, staging_size, &footprint, &rows, &row_size, &total_size);
			footprints.push_back(footprint);
			staging_size = static_cast<usize>(footprint.Offset + total_size);
		}
	if (!command->staging) {
		D3D12_HEAP_PROPERTIES heap = {};
		heap.Type = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = staging_size;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		HRESULT result = ctx->d3d_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&command->staging));
		if (FAILED(result)) {
			rtd3d12_fail(rtd3d12_error_from_hresult(result), "CreateCommittedResource(texture buffer staging) failed: 0x{:08x}", static_cast<u32>(result));
			return;
		}
	}
	D3D12_RESOURCE_BARRIER staging_barrier = {};
	staging_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	staging_barrier.Transition.pResource = command->staging;
	staging_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	staging_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	staging_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	command_list->ResourceBarrier(1, &staging_barrier);
	rtd3d12_command_transition_image_range(command_list, command->source, command->src_range, D3D12_RESOURCE_STATE_COPY_SOURCE);
	rtd3d12_command_transition_buffer(command_list, command->target, D3D12_RESOURCE_STATE_COPY_DEST);
	usize packed_offset = command->dst_range.offset;
	usize index = 0;
	const u32 bpp = rtd3d12_command_texture_bpp(command->source->dxgi_format);
	for (usize mip = 0; mip < command->src_range.mip_count; ++mip)
		for (usize layer = 0; layer < command->src_range.layer_count; ++layer, ++index) {
			const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint = footprints[index];
			const UINT subresource = rtd3d12_command_texture_subresource(command->source, command->src_range.base_mip + mip, command->src_range.base_layer + layer);
			D3D12_TEXTURE_COPY_LOCATION src = {};
			src.pResource = command->source->d3d_resource;
			src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			src.SubresourceIndex = subresource;
			D3D12_TEXTURE_COPY_LOCATION dst = {};
			dst.pResource = command->staging;
			dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			dst.PlacedFootprint = footprint;
			D3D12_BOX box = { static_cast<UINT>(command->src_range.offset.width), static_cast<UINT>(command->src_range.offset.height), static_cast<UINT>(command->src_range.offset.depth), static_cast<UINT>(command->src_range.offset.width + command->src_range.extent.width), static_cast<UINT>(command->src_range.offset.height + command->src_range.extent.height), static_cast<UINT>(command->src_range.offset.depth + command->src_range.extent.depth) };
			command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);
		}
	staging_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	staging_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	command_list->ResourceBarrier(1, &staging_barrier);
	index = 0;
	for (usize mip = 0; mip < command->src_range.mip_count; ++mip)
		for (usize layer = 0; layer < command->src_range.layer_count; ++layer, ++index) {
			const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint = footprints[index];
			const usize row_bytes = command->src_range.extent.width * bpp;
			for (usize z = 0; z < command->src_range.extent.depth; ++z)
				for (usize y = 0; y < command->src_range.extent.height; ++y) {
					command_list->CopyBufferRegion(command->target->d3d_resource, packed_offset, command->staging, footprint.Offset + (z * command->src_range.extent.height + y) * footprint.Footprint.RowPitch, row_bytes);
					packed_offset += row_bytes;
				}
		}
	staging_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	staging_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
	command_list->ResourceBarrier(1, &staging_barrier);
}

static void rtd3d12_lower_bind_sampler(rtd3d12_context* ctx, rtd3d12_command_lower_state* state, ID3D12GraphicsCommandList* command_list, const rtd3d12_ir_sampler* command) {
	if (!command || !command->sampler || !state->program || !state->sampler_heap || !state->program->descriptor_mappings[command->address]) return;
	const rtd3d12_program_descriptor_mapping& mapping = *state->program->descriptor_mappings[command->address];
	if (mapping.type != rtd3d12_descriptor_type::texture) return;
	D3D12_CPU_DESCRIPTOR_HANDLE cpu = state->sampler_heap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE gpu = state->sampler_heap->GetGPUDescriptorHandleForHeapStart();
	cpu.ptr += state->sampler_index * state->sampler_step;
	gpu.ptr += state->sampler_index++ * state->sampler_step;
	ctx->d3d_device->CopyDescriptorsSimple(1, cpu, command->cpu, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	rtd3d12_set_root_descriptor_table(state, command_list, mapping.sampler_root_parameter, gpu);
}

void rtd3d12_lower_bind_program_data(rtd3d12_context* ctx, rtd3d12_command_lower_state* state, ID3D12GraphicsCommandList* command_list, const rtd3d12_program_data_mapping& mapping, ID3D12Resource* resource, bool storage) {
	if (!resource || !state->resource_heap) {
		return;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE cpu = state->resource_heap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE gpu = state->resource_heap->GetGPUDescriptorHandleForHeapStart();
	cpu.ptr += state->resource_index * state->resource_step;
	gpu.ptr += state->resource_index++ * state->resource_step;
	if (storage) {
		D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
		desc.Format = DXGI_FORMAT_R32_TYPELESS;
		desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		desc.Buffer.NumElements = static_cast<UINT>((mapping.block_size + 3u) / 4u);
		desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
		ctx->d3d_device->CreateUnorderedAccessView(resource, nullptr, &desc, cpu);
	} else {
		D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
		desc.BufferLocation = resource->GetGPUVirtualAddress();
		desc.SizeInBytes = static_cast<UINT>((mapping.block_size + 255u) & ~usize(255u));
		ctx->d3d_device->CreateConstantBufferView(&desc, cpu);
	}
	rtd3d12_set_root_descriptor_table(state, command_list, mapping.root_parameter, gpu);
}

void rtd3d12_command_buffer_lower(rtd3d12_context* ctx, rt_command_buffer_t* command_buffer, ID3D12GraphicsCommandList* command_list, ID3D12DescriptorHeap** resource_heap, ID3D12DescriptorHeap** sampler_heap) {
	const usize begin = 0;
	const usize end = command_buffer ? command_buffer->ir_size : 0;
	usize resource_bind_count = 0;
	usize sampler_bind_count = 0;
	usize program_count = 0;
	for (usize offset = begin; offset < end; (void)0) {
		rtd3d12_command_header* header = reinterpret_cast<rtd3d12_command_header*>(command_buffer->ir_data + offset);
		if (header->opcode == rtd3d12_command_opcode::bind_buffer || header->opcode == rtd3d12_command_opcode::bind_texture || header->opcode == rtd3d12_command_opcode::uniform_data || header->opcode == rtd3d12_command_opcode::storage_data) {
			resource_bind_count++;
		}
		if (header->opcode == rtd3d12_command_opcode::bind_texture || header->opcode == rtd3d12_command_opcode::bind_sampler) {
			sampler_bind_count++;
		}
		if (header->opcode == rtd3d12_command_opcode::use_program) {
			program_count++;
		}
		offset += rtd3d12_command_record_size(header->opcode);
	}
	/* A root-signature selection invalidates descriptor-table arguments. Keep
	 * enough descriptors for every retained resource binding to be replayed at
	 * each recorded program selection. */
	if (program_count == SIZE_MAX ||
		(resource_bind_count && resource_bind_count > UINT_MAX / (program_count + 1)) ||
		(sampler_bind_count && sampler_bind_count > UINT_MAX / (program_count + 1))) {
		rtd3d12_fail(rt::error::improper_usage, "command buffer requires too many D3D12 descriptors");
		return;
	}
	const usize resource_count = resource_bind_count * (program_count + 1);
	const usize sampler_count = sampler_bind_count * (program_count + 1);

	if (resource_count) {
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = static_cast<UINT>(resource_count);
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		HRESULT result = ctx->d3d_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(resource_heap));
		if (FAILED(result)) {
			rtd3d12_fail(rtd3d12_error_from_hresult(result), "CreateDescriptorHeap(resource) failed: 0x{:08x}", static_cast<u32>(result));
			return;
		}
	}
	if (sampler_count) {
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		desc.NumDescriptors = static_cast<UINT>(sampler_count);
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		HRESULT result = ctx->d3d_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(sampler_heap));
		if (FAILED(result)) {
			rtd3d12_fail(rtd3d12_error_from_hresult(result), "CreateDescriptorHeap(sampler) failed: 0x{:08x}", static_cast<u32>(result));
			return;
		}
	}
	if (*resource_heap) {
		ID3D12DescriptorHeap* heaps[] = { *resource_heap, *sampler_heap };
		command_list->SetDescriptorHeaps(*sampler_heap ? 2u : 1u, heaps);
	}

	rtd3d12_command_lower_state state = {};
	state.resource_heap = *resource_heap;
	state.sampler_heap = *sampler_heap;
	state.resource_step = state.resource_heap ? ctx->d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) : 0;
	state.sampler_step = state.sampler_heap ? ctx->d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER) : 0;
	for (usize offset = begin; offset < end;) {
		rtd3d12_command_header* header = reinterpret_cast<rtd3d12_command_header*>(command_buffer->ir_data + offset);
		void* payload = header + 1;
		switch (header->opcode) {
		case rtd3d12_command_opcode::begin_rendering:
			rtd3d12_lower_begin_rendering(&state, command_list, static_cast<rtd3d12_ir_framebuffer*>(payload));
			break;
		case rtd3d12_command_opcode::clear_color:
			rtd3d12_lower_clear_color(&state, command_list, static_cast<rtd3d12_ir_clear_color*>(payload));
			break;
		case rtd3d12_command_opcode::clear_depth:
			rtd3d12_lower_clear_depth(&state, command_list, static_cast<rtd3d12_ir_clear_depth*>(payload));
			break;
		case rtd3d12_command_opcode::clear_stencil:
			rtd3d12_lower_clear_stencil(&state, command_list, static_cast<rtd3d12_ir_clear_stencil*>(payload));
			break;
		case rtd3d12_command_opcode::end_rendering:
			rtd3d12_lower_end_rendering(&state, command_list);
			break;
		case rtd3d12_command_opcode::set_viewport:
			rtd3d12_lower_set_viewport(command_list, static_cast<rtd3d12_ir_viewport*>(payload));
			break;
		case rtd3d12_command_opcode::set_scissor:
			rtd3d12_lower_set_scissor(command_list, static_cast<rtd3d12_ir_scissor*>(payload));
			break;
		case rtd3d12_command_opcode::use_program:
			rtd3d12_lower_use_program(ctx, &state, command_list, static_cast<rtd3d12_ir_program*>(payload));
			break;
		case rtd3d12_command_opcode::uniform_data:
		case rtd3d12_command_opcode::storage_data: {
			rtd3d12_ir_program_data* command = static_cast<rtd3d12_ir_program_data*>(payload);
			if (!state.program) break;
			auto& mappings = header->opcode == rtd3d12_command_opcode::uniform_data
				? state.program->uniform_data_mappings : state.program->storage_data_mappings;
			if (!mappings[command->address]) break;
			const rtd3d12_program_data_mapping& mapping = *mappings[command->address];
			if (command->size != mapping.byte_size || mapping.byte_offset > mapping.block_size || command->size > mapping.block_size - mapping.byte_offset) {
				break;
			}
			u32 block_address = command->address;
			for (u32 address = 0; address < command->address; address++) {
				if (mappings[address] && mappings[address]->binding == mapping.binding) {
					block_address = address;
					break;
				}
			}
			std::vector<std::byte>& block = header->opcode == rtd3d12_command_opcode::uniform_data ? state.uniform_blocks[block_address] : state.storage_blocks[block_address];
			if (block.size() != mapping.block_size) block.resize(mapping.block_size);
			memcpy(block.data() + mapping.byte_offset, command->bytes, command->size);
			if (command->resource) {
				command->resource->Release();
				command->resource = nullptr;
			}
			if (command->upload) {
				command->upload->Release();
				command->upload = nullptr;
			}
			if (header->opcode == rtd3d12_command_opcode::uniform_data) {
				const usize padded_size = (mapping.block_size + 255u) & ~usize(255u);
				std::vector<std::byte> padded(padded_size);
				memcpy(padded.data(), block.data(), block.size());
				command->resource = rtd3d12_command_buffer_upload(ctx, reinterpret_cast<const u08*>(padded.data()), padded.size());
				state.uniform_resources[block_address] = command->resource;
				rtd3d12_lower_bind_program_data(ctx, &state, command_list, mapping, command->resource, false);
			} else {
				command->upload = rtd3d12_command_buffer_upload(ctx, reinterpret_cast<const u08*>(block.data()), block.size());
				D3D12_HEAP_PROPERTIES heap = {};
				heap.Type = D3D12_HEAP_TYPE_DEFAULT;
				D3D12_RESOURCE_DESC desc = {};
				desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
				desc.Width = (mapping.block_size + 3u) & ~usize(3u);
				desc.Height = 1;
				desc.DepthOrArraySize = 1;
				desc.MipLevels = 1;
				desc.SampleDesc.Count = 1;
				desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
				desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
				HRESULT result = ctx->d3d_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&command->resource));
				if (FAILED(result)) {
					rtd3d12_fail(rtd3d12_error_from_hresult(result), "CreateCommittedResource(program storage) failed: 0x{:08x}", static_cast<u32>(result));
					break;
				}
				if (!command->upload) break;
				command_list->CopyBufferRegion(command->resource, 0, command->upload, 0, mapping.block_size);
				D3D12_RESOURCE_BARRIER barrier = {};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Transition.pResource = command->resource;
				barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
				barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				command_list->ResourceBarrier(1, &barrier);
				state.storage_resources[block_address] = command->resource;
				rtd3d12_lower_bind_program_data(ctx, &state, command_list, mapping, command->resource, true);
			}
			break;
		}
		case rtd3d12_command_opcode::bind_buffer:
			rtd3d12_lower_bind_buffer(ctx, &state, command_list, static_cast<rtd3d12_ir_buffer*>(payload));
			break;
		case rtd3d12_command_opcode::bind_texture:
			rtd3d12_lower_bind_texture(ctx, &state, command_list, static_cast<rtd3d12_ir_texture*>(payload));
			break;
		case rtd3d12_command_opcode::bind_sampler:
			rtd3d12_lower_bind_sampler(ctx, &state, command_list, static_cast<rtd3d12_ir_sampler*>(payload));
			break;
		case rtd3d12_command_opcode::vertex_buffer:
			rtd3d12_lower_vertex_buffer(&state, command_list, static_cast<rtd3d12_ir_vertex_buffer*>(payload));
			break;
		case rtd3d12_command_opcode::index_buffer:
			rtd3d12_lower_index_buffer(command_list, static_cast<rtd3d12_ir_index_buffer*>(payload));
			break;
		case rtd3d12_command_opcode::draw:
			if (state.program && state.program->d3d_pipeline) rtd3d12_lower_draw(command_list, static_cast<rtd3d12_ir_draw*>(payload));
			break;
		case rtd3d12_command_opcode::draw_instanced:
			if (state.program && state.program->d3d_pipeline) rtd3d12_lower_draw_instanced(command_list, static_cast<rtd3d12_ir_draw_instanced*>(payload));
			break;
		case rtd3d12_command_opcode::draw_indexed:
			if (state.program && state.program->d3d_pipeline) rtd3d12_lower_draw_indexed(command_list, static_cast<rtd3d12_ir_draw_indexed*>(payload));
			break;
	case rtd3d12_command_opcode::draw_indexed_instanced:
			if (state.program && state.program->d3d_pipeline) rtd3d12_lower_draw_indexed_instanced(command_list, static_cast<rtd3d12_ir_draw_indexed_instanced*>(payload));
			break;
		case rtd3d12_command_opcode::dispatch:
			rtd3d12_lower_dispatch(command_list, static_cast<rtd3d12_ir_dispatch*>(payload));
			break;
		case rtd3d12_command_opcode::buffer_data:
			rtd3d12_lower_buffer_data(command_list, static_cast<rtd3d12_ir_buffer_data*>(payload));
			break;
		case rtd3d12_command_opcode::buffer_copy:
			rtd3d12_lower_buffer_copy(command_list, static_cast<rtd3d12_ir_buffer_copy*>(payload));
			break;
		case rtd3d12_command_opcode::buffer_copy_to_texture:
			rtd3d12_lower_buffer_copy_to_texture(ctx, command_list, static_cast<rtd3d12_ir_buffer_copy_to_texture*>(payload));
			break;
		case rtd3d12_command_opcode::buffer_barrier:
			rtd3d12_lower_buffer_barrier(command_list, static_cast<rtd3d12_ir_buffer_barrier*>(payload));
			break;
		case rtd3d12_command_opcode::texture_copy:
			rtd3d12_lower_texture_copy(command_list, static_cast<rtd3d12_ir_texture_copy*>(payload));
			break;
		case rtd3d12_command_opcode::texture_data:
			rtd3d12_lower_texture_data(ctx, command_list, static_cast<rtd3d12_ir_texture_data*>(payload));
			break;
		case rtd3d12_command_opcode::texture_copy_to_buffer:
			rtd3d12_lower_texture_copy_to_buffer(ctx, command_list, static_cast<rtd3d12_ir_texture_copy_to_buffer*>(payload));
			break;
		case rtd3d12_command_opcode::texture_barrier:
			rtd3d12_lower_texture_barrier(command_list, static_cast<rtd3d12_ir_texture_barrier*>(payload));
			break;
		default:
			break;
		}
		offset += rtd3d12_command_record_size(header->opcode);
	}
}
