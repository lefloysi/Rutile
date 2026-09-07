#include "command_buffer.h"
#include "context.h"
#include "error.h"
#include "queue.h"

#include <assert.h>
#include <intrin.h>
#include <stdlib.h>
#include <string.h>

/*===============================================================================================*/
/*                                                                                                */
/*===============================================================================================*/

rt_command_buffer rtCommandBufferCreate(void) {
	rtvk_begin_errorable_operation();
	return rtvk_command_buffer_to_handle(rtvk_command_buffer_create(rtvk_get_current_context()));
}

void rtCommandBufferDestroy(rt_command_buffer command_buffer) {
	rtvk_command_buffer_destroy(rtvk_get_current_context(), rtvk_command_buffer_from_handle(command_buffer));
}

void rtCommandBufferReset(rt_command_buffer command_buffer) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_reset(rtvk_command_buffer_from_handle(command_buffer));
}

void rtCommandBufferBegin(rt_command_buffer command_buffer) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_begin(rtvk_command_buffer_from_handle(command_buffer));
}

void rtCommandBufferContinue(rt_command_buffer command_buffer) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_continue(rtvk_command_buffer_from_handle(command_buffer));
}

void rtCommandBufferContinueRendering(rt_command_buffer command_buffer) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_continue_rendering(rtvk_command_buffer_from_handle(command_buffer));
}

void rtCommandBufferEnd(rt_command_buffer command_buffer) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_end(rtvk_command_buffer_from_handle(command_buffer));
}

void rtCmdExecute(rt_command_buffer command_buffer, rt_command_buffer secondary) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_execute(
		rtvk_command_buffer_from_handle(command_buffer),
		rtvk_command_buffer_from_handle(secondary)
	);
}

void rtCmdBufferData(rt_command_buffer command_buffer, rt_buffer buffer, rt_buffer_range range, const u08* data) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_buffer_data(
		rtvk_command_buffer_from_handle(command_buffer),
		rtvk_buffer_from_handle(buffer),
		range,
		data
	);
}

void rtCmdBufferCopy(rt_command_buffer command_buffer, rt_buffer src, rt_buffer_range src_range, rt_buffer dst, rt_buffer_range dst_range) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_buffer_copy(
		rtvk_command_buffer_from_handle(command_buffer),
		rtvk_buffer_from_handle(src),
		src_range,
		rtvk_buffer_from_handle(dst),
		dst_range
	);
}

void rtCmdBufferCopyToTexture(rt_command_buffer command_buffer, rt_buffer src, rt_buffer_range src_range, rt_texture dst, rt_texture_range dst_range) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_buffer_copy_to_texture(
		rtvk_command_buffer_from_handle(command_buffer),
		rtvk_buffer_from_handle(src),
		src_range,
		rtvk_texture_from_handle(dst),
		dst_range
	);
}

void rtCmdBufferBarrier(rt_command_buffer command_buffer, rt_buffer buffer, rt_buffer_range range, rt_access src, rt_access dst) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_buffer_barrier(
		rtvk_command_buffer_from_handle(command_buffer),
		rtvk_buffer_from_handle(buffer),
		range,
		src,
		dst
	);
}

void rtCmdTextureCopy(rt_command_buffer command_buffer, rt_texture src, rt_texture_range src_range, rt_texture dst, rt_texture_range dst_range) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_texture_copy(
		rtvk_command_buffer_from_handle(command_buffer),
		rtvk_texture_from_handle(src),
		src_range,
		rtvk_texture_from_handle(dst),
		dst_range
	);
}

void rtCmdTextureData(rt_command_buffer command_buffer, rt_texture texture, rt_texture_range range, const u08* data) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_texture_data(
		rtvk_command_buffer_from_handle(command_buffer),
		rtvk_texture_from_handle(texture),
		range,
		data
	);
}

void rtCmdTextureCopyToBuffer(rt_command_buffer command_buffer, rt_texture src, rt_texture_range src_range, rt_buffer dst, rt_buffer_range dst_range) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_texture_copy_to_buffer(
		rtvk_command_buffer_from_handle(command_buffer),
		rtvk_texture_from_handle(src),
		src_range,
		rtvk_buffer_from_handle(dst),
		dst_range
	);
}

void rtCmdTextureBarrier(rt_command_buffer command_buffer, rt_texture texture, rt_texture_range range, rt_access src, rt_access dst) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_texture_barrier(
		rtvk_command_buffer_from_handle(command_buffer),
		rtvk_texture_from_handle(texture),
		range,
		src,
		dst
	);
}

void rtCmdBeginRendering(rt_command_buffer command_buffer, rt_framebuffer framebuffer) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_begin_rendering(
		rtvk_command_buffer_from_handle(command_buffer),
		rtvk_framebuffer_from_handle(framebuffer)
	);
}

void rtCmdClearColor(rt_command_buffer command_buffer, rt_location location, f32 r, f32 g, f32 b, f32 a) {
	rtvk_begin_errorable_operation();
	struct rtvk_program_output_mapping* mapping = rtvk_program_output_mapping(rtvk_location_program(location), location);
	rtvk_command_buffer_clear_color(rtvk_command_buffer_from_handle(command_buffer), mapping ? mapping->attachment : 0, r, g, b, a);
}

void rtCmdClearDepth(rt_command_buffer command_buffer, f32 depth) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_clear_depth(rtvk_command_buffer_from_handle(command_buffer), depth);
}

void rtCmdClearStencil(rt_command_buffer command_buffer, usize stencil) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_clear_stencil(rtvk_command_buffer_from_handle(command_buffer), stencil);
}

void rtCmdClear(rt_command_buffer command_buffer, enum rt_clear_flag attachments) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_clear(rtvk_command_buffer_from_handle(command_buffer), attachments);
}

void rtCmdSetViewport(rt_command_buffer command_buffer, usize x, usize y, usize width, usize height, f32 min_depth, f32 max_depth) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_set_viewport(rtvk_command_buffer_from_handle(command_buffer), x, y, width, height, min_depth, max_depth);
}

void rtCmdSetScissor(rt_command_buffer command_buffer, usize x, usize y, usize width, usize height) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_set_scissor(rtvk_command_buffer_from_handle(command_buffer), x, y, width, height);
}

void rtCmdEndRendering(rt_command_buffer command_buffer) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_end_rendering(rtvk_command_buffer_from_handle(command_buffer));
}

void rtCmdUseProgram(rt_command_buffer command_buffer, rt_program program) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_use_program(
		rtvk_command_buffer_from_handle(command_buffer),
		rtvk_program_from_handle(program)
	);
}

void rtCmdBindBuffer(rt_command_buffer command_buffer, rt_location location, rt_buffer buffer, rt_buffer_range range) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_bind_buffer(
		rtvk_command_buffer_from_handle(command_buffer),
		location,
		rtvk_buffer_from_handle(buffer),
		range.offset,
		range.size
	);
}

void rtCmdBindTexture(rt_command_buffer command_buffer, rt_location location, rt_texture_view texture_view) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_bind_texture(
		rtvk_command_buffer_from_handle(command_buffer),
		location,
		rtvk_texture_view_from_handle(texture_view)
	);
}

void rtCmdBindSampler(rt_command_buffer command_buffer, rt_location location, rt_sampler sampler) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_bind_sampler(
		rtvk_command_buffer_from_handle(command_buffer),
		location,
		rtvk_sampler_from_handle(sampler)
	);
}

void rtCmdVertexBuffer(rt_command_buffer command_buffer, rt_location location, rt_buffer buffer, rt_buffer_range range) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_vertex_buffer(
		rtvk_command_buffer_from_handle(command_buffer),
		location,
		rtvk_buffer_from_handle(buffer),
		range
	);
}

void rtCmdIndexBuffer(rt_command_buffer command_buffer, rt_buffer buffer, rt_buffer_range range, enum rt_index_format format) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_index_buffer(rtvk_command_buffer_from_handle(command_buffer), rtvk_buffer_from_handle(buffer), range, format);
}

void rtCmdDraw(rt_command_buffer command_buffer, usize vertex_count, usize first_vertex) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_draw(rtvk_command_buffer_from_handle(command_buffer), vertex_count, first_vertex);
}

void rtCmdDrawInstanced(rt_command_buffer command_buffer, usize vertex_count, usize instance_count, usize first_vertex, usize first_instance) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_draw_instanced(
		rtvk_command_buffer_from_handle(command_buffer),
		vertex_count,
		instance_count,
		first_vertex,
		first_instance
	);
}

void rtCmdDrawIndexed(rt_command_buffer command_buffer, usize index_count, usize first_index, usize vertex_offset) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_draw_indexed(rtvk_command_buffer_from_handle(command_buffer), index_count, first_index, vertex_offset);
}

void rtCmdDrawIndexedInstanced(rt_command_buffer command_buffer, usize index_count, usize instance_count, usize first_index, usize vertex_offset, usize first_instance) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_draw_indexed_instanced(
		rtvk_command_buffer_from_handle(command_buffer),
		index_count,
		instance_count,
		first_index,
		vertex_offset,
		first_instance
	);
}

void rtCmdDispatch(rt_command_buffer command_buffer, usize group_count_x, usize group_count_y, usize group_count_z) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_dispatch(rtvk_command_buffer_from_handle(command_buffer), group_count_x, group_count_y, group_count_z);
}

/*===============================================================================================*/
/*                                                                                                */
/*===============================================================================================*/

usize rtvk_command_payload_size(rtvk_command_opcode opcode) {
	switch (opcode) {
	case RTVK_COMMAND_BUFFER_DATA:
		return sizeof(struct rtvk_ir_buffer_data);
	case RTVK_COMMAND_BUFFER_COPY:
		return sizeof(struct rtvk_ir_buffer_copy);
	case RTVK_COMMAND_BUFFER_COPY_TO_TEXTURE:
		return sizeof(struct rtvk_ir_buffer_copy_to_texture);
	case RTVK_COMMAND_BUFFER_BARRIER:
		return sizeof(struct rtvk_ir_buffer_barrier);
	case RTVK_COMMAND_TEXTURE_COPY:
		return sizeof(struct rtvk_ir_texture_copy);
	case RTVK_COMMAND_TEXTURE_DATA:
		return sizeof(struct rtvk_ir_texture_data);
	case RTVK_COMMAND_TEXTURE_COPY_TO_BUFFER:
		return sizeof(struct rtvk_ir_texture_copy_to_buffer);
	case RTVK_COMMAND_TEXTURE_BARRIER:
		return sizeof(struct rtvk_ir_texture_barrier);
	case RTVK_COMMAND_EXECUTE:
		return sizeof(struct rtvk_ir_execute);
	case RTVK_COMMAND_BEGIN_RENDERING:
		return sizeof(struct rtvk_ir_framebuffer);
	case RTVK_COMMAND_CLEAR_COLOR:
		return sizeof(struct rtvk_ir_clear_color);
	case RTVK_COMMAND_CLEAR_DEPTH:
		return sizeof(struct rtvk_ir_clear_depth);
	case RTVK_COMMAND_CLEAR_STENCIL:
		return sizeof(struct rtvk_ir_clear_stencil);
	case RTVK_COMMAND_CLEAR:
		return sizeof(struct rtvk_ir_clear);
	case RTVK_COMMAND_SET_VIEWPORT:
		return sizeof(struct rtvk_ir_viewport);
	case RTVK_COMMAND_SET_SCISSOR:
		return sizeof(struct rtvk_ir_scissor);
	case RTVK_COMMAND_END_RENDERING:
		return 0;
	case RTVK_COMMAND_USE_PROGRAM:
		return sizeof(struct rtvk_ir_program);
	case RTVK_COMMAND_UNIFORM_DATA:
	case RTVK_COMMAND_STORAGE_DATA:
		return sizeof(struct rtvk_ir_program_data);
	case RTVK_COMMAND_BIND_BUFFER:
		return sizeof(struct rtvk_ir_buffer);
	case RTVK_COMMAND_BIND_TEXTURE:
		return sizeof(struct rtvk_ir_texture);
	case RTVK_COMMAND_BIND_SAMPLER:
		return sizeof(struct rtvk_ir_sampler);
	case RTVK_COMMAND_VERTEX_BUFFER:
		return sizeof(struct rtvk_ir_vertex_buffer);
	case RTVK_COMMAND_INDEX_BUFFER:
		return sizeof(struct rtvk_ir_index_buffer);
	case RTVK_COMMAND_DRAW:
		return sizeof(struct rtvk_ir_draw);
	case RTVK_COMMAND_DRAW_INSTANCED:
		return sizeof(struct rtvk_ir_draw_instanced);
	case RTVK_COMMAND_DRAW_INDEXED:
		return sizeof(struct rtvk_ir_draw_indexed);
	case RTVK_COMMAND_DRAW_INDEXED_INSTANCED:
		return sizeof(struct rtvk_ir_draw_indexed_instanced);
	case RTVK_COMMAND_DISPATCH:
		return sizeof(struct rtvk_ir_dispatch);
	default:
		return 0;
	}
}

usize rtvk_command_record_size(rtvk_command_opcode opcode) {
	const usize alignment = _Alignof(void*);
	const usize size = sizeof(struct rtvk_command_header) + rtvk_command_payload_size(opcode);
	return (size + alignment - 1) & ~(alignment - 1);
}

void* rtvk_command_append(struct rtvk_command_buffer* command_buffer, rtvk_command_opcode opcode) {
	const usize size = rtvk_command_record_size(opcode);
	if (command_buffer->ir_capacity - command_buffer->ir_size < size) {
		usize required_size = command_buffer->ir_size + size;
		unsigned long most_significant_bit = 0;
		_BitScanReverse64(&most_significant_bit, required_size - 1);
		usize capacity = (usize)1 << (most_significant_bit + 1);
		if (capacity < 4096) {
			capacity = 4096;
		}
		void* data = realloc(command_buffer->ir_data, capacity);
		if (!data) {
			rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for command IR", capacity);
			return NULL;
		}
		command_buffer->ir_data = data;
		command_buffer->ir_capacity = capacity;
	}

	struct rtvk_command_header* header = (struct rtvk_command_header*)(command_buffer->ir_data + command_buffer->ir_size);
	header->opcode = (u08)opcode;
	command_buffer->ir_size += size;
	return header + 1;
}

void rtvk_command_buffer_release_resources(struct rtvk_command_buffer* command_buffer) {
	for (usize offset = 0; offset < command_buffer->ir_size; (void)0) {
		struct rtvk_command_header* header = (struct rtvk_command_header*)(command_buffer->ir_data + offset);
		void* payload = header + 1;
		switch ((rtvk_command_opcode)header->opcode) {
		case RTVK_COMMAND_BUFFER_DATA: {
			struct rtvk_ir_buffer_data* command = payload;
			free(command->data);
			rtvk_release_resource(command->copy_source);
			rtvk_release_resource(command->buffer);
			break;
		}
		case RTVK_COMMAND_BUFFER_COPY: {
			struct rtvk_ir_buffer_copy* command = payload;
			rtvk_release_resource(command->copy_source);
			rtvk_release_resource(command->src);
			rtvk_release_resource(command->dst);
			break;
		}
		case RTVK_COMMAND_BUFFER_COPY_TO_TEXTURE: {
			struct rtvk_ir_buffer_copy_to_texture* command = payload;
			rtvk_release_resource(command->src);
			rtvk_release_resource(command->dst);
			break;
		}
		case RTVK_COMMAND_BUFFER_BARRIER:
			rtvk_release_resource(((struct rtvk_ir_buffer_barrier*)payload)->buffer);
			break;
		case RTVK_COMMAND_TEXTURE_COPY: {
			struct rtvk_ir_texture_copy* command = payload;
			rtvk_release_resource(command->copy_source);
			rtvk_release_resource(command->src);
			rtvk_release_resource(command->dst);
			break;
		}
		case RTVK_COMMAND_TEXTURE_DATA: {
			struct rtvk_ir_texture_data* command = payload;
			free(command->data);
			rtvk_release_resource(command->copy_source);
			rtvk_release_resource(command->texture);
			break;
		}
		case RTVK_COMMAND_TEXTURE_COPY_TO_BUFFER: {
			struct rtvk_ir_texture_copy_to_buffer* command = payload;
			rtvk_release_resource(command->src);
			rtvk_release_resource(command->copy_source);
			rtvk_release_resource(command->dst);
			break;
		}
		case RTVK_COMMAND_TEXTURE_BARRIER:
			rtvk_release_resource(((struct rtvk_ir_texture_barrier*)payload)->texture);
			break;
		case RTVK_COMMAND_EXECUTE:
			rtvk_release_resource(((struct rtvk_ir_execute*)payload)->command_buffer);
			break;
		case RTVK_COMMAND_BEGIN_RENDERING: {
			struct rtvk_ir_framebuffer* command = payload;
			rtvk_release_resource(command->framebuffer);
			break;
		}
		case RTVK_COMMAND_USE_PROGRAM:
			rtvk_release_resource(((struct rtvk_ir_program*)payload)->program);
			break;
		case RTVK_COMMAND_UNIFORM_DATA:
		case RTVK_COMMAND_STORAGE_DATA: {
			struct rtvk_ir_program_data* command = payload;
			free(command->bytes);
			break;
		}
		case RTVK_COMMAND_BIND_BUFFER: {
			struct rtvk_ir_buffer* command = payload;
			rtvk_release_resource(command->buffer);
			break;
		}
		case RTVK_COMMAND_BIND_TEXTURE: {
			struct rtvk_ir_texture* command = payload;
			if (command->owns_image_view) {
				vkDestroyImageView(command_buffer->base.ctx->vk_device, command->vk_image_view, VK_ALLOCATOR);
			}
			rtvk_release_resource(command->view);
			rtvk_release_resource(command->image);
			break;
		}
		case RTVK_COMMAND_BIND_SAMPLER:
			rtvk_release_resource(((struct rtvk_ir_sampler*)payload)->sampler);
			break;
		case RTVK_COMMAND_VERTEX_BUFFER: {
			struct rtvk_ir_vertex_buffer* command = payload;
			rtvk_release_resource(command->buffer);
			break;
		}
		case RTVK_COMMAND_INDEX_BUFFER:
			rtvk_release_resource(((struct rtvk_ir_index_buffer*)payload)->buffer);
			break;
		default:
			break;
		}
		offset += rtvk_command_record_size((rtvk_command_opcode)header->opcode);
	}

	free(command_buffer->ir_data);
	command_buffer->ir_data = NULL;
	command_buffer->ir_size = 0;
	command_buffer->ir_capacity = 0;
}

void rtvk_command_buffer_reset(struct rtvk_command_buffer* command_buffer) {
	rtvk_command_buffer_release_resources(command_buffer);
	command_buffer->recording = false;
	command_buffer->executable = false;
	command_buffer->continuation = false;
	command_buffer->rendering_continuation = false;
	command_buffer->rendering = false;
}

void rtvk_command_buffer_begin(struct rtvk_command_buffer* command_buffer) {
	command_buffer->recording = true;
	command_buffer->executable = false;
	command_buffer->continuation = false;
	command_buffer->rendering_continuation = false;
	command_buffer->rendering = false;
}

void rtvk_command_buffer_continue(struct rtvk_command_buffer* command_buffer) {
	command_buffer->recording = true;
	command_buffer->executable = false;
	command_buffer->continuation = true;
	command_buffer->rendering_continuation = false;
	command_buffer->rendering = false;
}

void rtvk_command_buffer_continue_rendering(struct rtvk_command_buffer* command_buffer) {
	command_buffer->recording = true;
	command_buffer->executable = false;
	command_buffer->continuation = true;
	command_buffer->rendering_continuation = true;
	command_buffer->rendering = false;
}

static bool rtvk_command_buffer_references(struct rtvk_command_buffer* command_buffer, const struct rtvk_command_buffer* searched) {
	if (!command_buffer) {
		return false;
	}
	if (command_buffer == searched || command_buffer->checking_references) {
		return true;
	}

	command_buffer->checking_references = true;
	for (usize offset = 0; offset < command_buffer->ir_size;) {
		struct rtvk_command_header* header = (struct rtvk_command_header*)(command_buffer->ir_data + offset);
		if (header->opcode == RTVK_COMMAND_EXECUTE) {
			struct rtvk_ir_execute* execute = (struct rtvk_ir_execute*)(header + 1);
			if (rtvk_command_buffer_references(execute->command_buffer, searched)) {
				command_buffer->checking_references = false;
				return true;
			}
		}
		offset += rtvk_command_record_size((rtvk_command_opcode)header->opcode);
	}
	command_buffer->checking_references = false;
	return false;
}

void rtvk_command_buffer_execute(struct rtvk_command_buffer* command_buffer, struct rtvk_command_buffer* secondary) {
	if (!command_buffer || !secondary || !command_buffer->recording || command_buffer->continuation) {
		rtvk_throwf(RT_IMPROPER_USAGE, "rtCmdExecute requires a recording primary command buffer and an executable continuation");
		return;
	}
	if (!secondary->executable || !secondary->continuation) {
		rtvk_throwf(RT_IMPROPER_USAGE, "rtCmdExecute requires an executable continuation command buffer");
		return;
	}
	if (secondary->rendering_continuation != command_buffer->rendering) {
		rtvk_throwf(RT_IMPROPER_USAGE, "rtCmdExecute rendering scope does not match the continuation command buffer");
		return;
	}
	if (rtvk_command_buffer_references(secondary, command_buffer)) {
		rtvk_throwf(RT_IMPROPER_USAGE, "rtCmdExecute would create a command-buffer execution cycle");
		return;
	}

	struct rtvk_ir_execute* command = rtvk_command_append(command_buffer, RTVK_COMMAND_EXECUTE);
	if (!command) {
		return;
	}
	command->command_buffer = secondary;
	rtvk_retain_resource(command->command_buffer);
}

void rtvk_command_buffer_buffer_data(struct rtvk_command_buffer* command_buffer, struct rtvk_buffer* buffer, rt_buffer_range range, const u08* data) {
	if (range.size && !data) {
		return;
	}

	u08* snapshot = NULL;
	if (range.size) {
		snapshot = malloc(range.size);
		if (!snapshot) {
			rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for recorded buffer data", range.size);
			return;
		}
		memcpy(snapshot, data, range.size);
	}

	struct rtvk_ir_buffer_data* command = rtvk_command_append(command_buffer, RTVK_COMMAND_BUFFER_DATA);
	if (!command) {
		free(snapshot);
		return;
	}
	struct rtvk_buffer_write write = rtvk_buffer_write_begin(rtvk_get_current_context(), buffer);
	command->copy_source = write.source;
	command->buffer = write.target;
	command->range = range;
	command->data = snapshot;
	rtvk_retain_resource(command->copy_source);
	rtvk_retain_resource(command->buffer);
	rtvk_buffer_write_commit(buffer, &write);
}

void rtvk_command_buffer_buffer_copy(struct rtvk_command_buffer* command_buffer, struct rtvk_buffer* src, rt_buffer_range src_range, struct rtvk_buffer* dst, rt_buffer_range dst_range) {
	struct rtvk_ir_buffer_copy* command = rtvk_command_append(command_buffer, RTVK_COMMAND_BUFFER_COPY);
	if (!command) {
		return;
	}
	command->src = rtvk_buffer_active_node(src);
	rtvk_retain_resource(command->src);
	struct rtvk_buffer_write write = rtvk_buffer_write_begin(rtvk_get_current_context(), dst);
	command->copy_source = write.source;
	command->dst = write.target;
	command->src_range = src_range;
	command->dst_range = dst_range;
	rtvk_retain_resource(command->copy_source);
	rtvk_retain_resource(command->dst);
	rtvk_buffer_write_commit(dst, &write);
}

void rtvk_command_buffer_buffer_copy_to_texture(struct rtvk_command_buffer* command_buffer, struct rtvk_buffer* src, rt_buffer_range src_range, struct rtvk_texture* dst, rt_texture_range dst_range) {
	struct rtvk_ir_buffer_copy_to_texture* command = rtvk_command_append(command_buffer, RTVK_COMMAND_BUFFER_COPY_TO_TEXTURE);
	if (!command) {
		return;
	}
	command->src = rtvk_buffer_active_node(src);
	command->dst = rtvk_texture_active_node(dst);
	command->src_range = src_range;
	command->dst_range = dst_range;
	rtvk_retain_resource(command->src);
	rtvk_retain_resource(command->dst);
}

void rtvk_command_buffer_buffer_barrier(struct rtvk_command_buffer* command_buffer, struct rtvk_buffer* buffer, rt_buffer_range range, rt_access src, rt_access dst) {
	struct rtvk_ir_buffer_barrier* command = rtvk_command_append(command_buffer, RTVK_COMMAND_BUFFER_BARRIER);
	if (!command) {
		return;
	}
	command->buffer = rtvk_buffer_active_node(buffer);
	command->range = range;
	command->src = src;
	command->dst = dst;
	rtvk_retain_resource(command->buffer);
}

usize rtvk_texture_bytes_per_texel(VkFormat format) {
	switch (format) {
	case VK_FORMAT_R8_UNORM:
	case VK_FORMAT_R8_SINT:
	case VK_FORMAT_R8_UINT:
	case VK_FORMAT_S8_UINT:
		return 1;
	case VK_FORMAT_R8G8_UNORM:
	case VK_FORMAT_R8G8_SINT:
	case VK_FORMAT_R8G8_UINT:
	case VK_FORMAT_R16_UNORM:
	case VK_FORMAT_R16_SFLOAT:
	case VK_FORMAT_R16_SINT:
	case VK_FORMAT_R16_UINT:
	case VK_FORMAT_D16_UNORM:
		return 2;
	case VK_FORMAT_R8G8B8_UNORM:
	case VK_FORMAT_R8G8B8_SINT:
	case VK_FORMAT_R8G8B8_UINT:
		return 3;
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_R8G8B8A8_SINT:
	case VK_FORMAT_R8G8B8A8_UINT:
	case VK_FORMAT_R8G8B8A8_SRGB:
	case VK_FORMAT_B8G8R8A8_UNORM:
	case VK_FORMAT_B8G8R8A8_SRGB:
	case VK_FORMAT_R16G16_UNORM:
	case VK_FORMAT_R16G16_SFLOAT:
	case VK_FORMAT_R16G16_SINT:
	case VK_FORMAT_R16G16_UINT:
	case VK_FORMAT_R32_SFLOAT:
	case VK_FORMAT_R32_SINT:
	case VK_FORMAT_R32_UINT:
	case VK_FORMAT_D32_SFLOAT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
		return 4;
	case VK_FORMAT_R16G16B16_UNORM:
	case VK_FORMAT_R16G16B16_SFLOAT:
	case VK_FORMAT_R16G16B16_SINT:
	case VK_FORMAT_R16G16B16_UINT:
		return 6;
	case VK_FORMAT_R16G16B16A16_UNORM:
	case VK_FORMAT_R16G16B16A16_SFLOAT:
	case VK_FORMAT_R16G16B16A16_SINT:
	case VK_FORMAT_R16G16B16A16_UINT:
	case VK_FORMAT_R32G32_SFLOAT:
	case VK_FORMAT_R32G32_SINT:
	case VK_FORMAT_R32G32_UINT:
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		return 8;
	case VK_FORMAT_R32G32B32_SFLOAT:
	case VK_FORMAT_R32G32B32_SINT:
	case VK_FORMAT_R32G32B32_UINT:
		return 12;
	case VK_FORMAT_R32G32B32A32_SFLOAT:
	case VK_FORMAT_R32G32B32A32_SINT:
	case VK_FORMAT_R32G32B32A32_UINT:
		return 16;
	default:
		return 0;
	}
}

VkImageAspectFlags rtvk_texture_range_aspect(VkFormat format, enum rt_texture_aspect_flag aspects) {
	VkImageAspectFlags result = 0;
	if (aspects & RT_TEXTURE_ASPECT_COLOR) {
		result |= VK_IMAGE_ASPECT_COLOR_BIT;
	}
	if (aspects & RT_TEXTURE_ASPECT_DEPTH) {
		result |= VK_IMAGE_ASPECT_DEPTH_BIT;
	}
	if (aspects & RT_TEXTURE_ASPECT_STENCIL) {
		result |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	if (!result) {
		return rtvk_texture_format_aspect(format);
	}
	return result;
}

usize rtvk_texture_range_data_size(VkFormat format, rt_texture_range range) {
	usize texel_size = rtvk_texture_bytes_per_texel(format);
	return range.mip_count * range.layer_count * range.extent.width * range.extent.height * range.extent.depth * texel_size;
}

void rtvk_command_buffer_texture_copy(struct rtvk_command_buffer* command_buffer, struct rtvk_texture* src, rt_texture_range src_range, struct rtvk_texture* dst, rt_texture_range dst_range) {
	struct rtvk_ir_texture_copy* command = rtvk_command_append(command_buffer, RTVK_COMMAND_TEXTURE_COPY);
	if (!command) {
		return;
	}
	command->src = rtvk_texture_active_node(src);
	rtvk_retain_resource(command->src);
	struct rtvk_texture_write write = rtvk_texture_write_begin(rtvk_get_current_context(), dst);
	command->copy_source = write.source;
	command->dst = write.target;
	command->src_range = src_range;
	command->dst_range = dst_range;
	rtvk_retain_resource(command->copy_source);
	rtvk_retain_resource(command->dst);
	rtvk_texture_write_commit(dst, &write);
}

void rtvk_command_buffer_texture_data(struct rtvk_command_buffer* command_buffer, struct rtvk_texture* texture, rt_texture_range range, const u08* data) {
	struct rtvk_texture* node = rtvk_texture_active_node(texture);
	usize data_size = node ? rtvk_texture_range_data_size(node->base.vk_format, range) : 0;
	if (data_size && !data) {
		return;
	}

	u08* snapshot = NULL;
	if (data_size) {
		snapshot = malloc(data_size);
		if (!snapshot) {
			rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for recorded texture data", data_size);
			return;
		}
		memcpy(snapshot, data, data_size);
	}

	struct rtvk_ir_texture_data* command = rtvk_command_append(command_buffer, RTVK_COMMAND_TEXTURE_DATA);
	if (!command) {
		free(snapshot);
		return;
	}
	struct rtvk_texture_write write = rtvk_texture_write_begin(rtvk_get_current_context(), texture);
	command->copy_source = write.source;
	command->texture = write.target;
	command->range = range;
	command->data = snapshot;
	command->data_size = data_size;
	rtvk_retain_resource(command->copy_source);
	rtvk_retain_resource(command->texture);
	rtvk_texture_write_commit(texture, &write);
}

void rtvk_command_buffer_texture_copy_to_buffer(struct rtvk_command_buffer* command_buffer, struct rtvk_texture* src, rt_texture_range src_range, struct rtvk_buffer* dst, rt_buffer_range dst_range) {
	struct rtvk_ir_texture_copy_to_buffer* command = rtvk_command_append(command_buffer, RTVK_COMMAND_TEXTURE_COPY_TO_BUFFER);
	if (!command) {
		return;
	}
	command->src = rtvk_texture_active_node(src);
	struct rtvk_buffer_write write = rtvk_buffer_write_begin(rtvk_get_current_context(), dst);
	command->copy_source = write.source;
	command->dst = write.target;
	command->src_range = src_range;
	command->dst_range = dst_range;
	rtvk_retain_resource(command->src);
	rtvk_retain_resource(command->copy_source);
	rtvk_retain_resource(command->dst);
	rtvk_buffer_write_commit(dst, &write);
}

void rtvk_command_buffer_texture_barrier(struct rtvk_command_buffer* command_buffer, struct rtvk_texture* texture, rt_texture_range range, rt_access src, rt_access dst) {
	struct rtvk_ir_texture_barrier* command = rtvk_command_append(command_buffer, RTVK_COMMAND_TEXTURE_BARRIER);
	if (!command) {
		return;
	}
	command->texture = rtvk_texture_active_node(texture);
	command->range = range;
	command->src = src;
	command->dst = dst;
	rtvk_retain_resource(command->texture);
}

void rtvk_command_buffer_begin_rendering(struct rtvk_command_buffer* command_buffer, struct rtvk_framebuffer* framebuffer) {
	if (!command_buffer || !framebuffer || !command_buffer->recording || command_buffer->continuation || command_buffer->rendering) {
		rtvk_throwf(RT_IMPROPER_USAGE, "rtCmdBeginRendering requires a recording primary command buffer outside a rendering scope");
		return;
	}

	struct rtvk_ir_framebuffer* command = rtvk_command_append(command_buffer, RTVK_COMMAND_BEGIN_RENDERING);
	if (!command) {
		return;
	}
	command->framebuffer = framebuffer;
	rtvk_retain_resource(command->framebuffer);
	command_buffer->rendering = true;
}

void rtCmdUniformData(rt_command_buffer command_buffer, rt_location location, const u08* data, usize size) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_program_data(rtvk_command_buffer_from_handle(command_buffer), location, data, size, RTVK_DATA_UNIFORM, RTVK_COMMAND_UNIFORM_DATA);
}

void rtCmdStorageData(rt_command_buffer command_buffer, rt_location location, const u08* data, usize size) {
	rtvk_begin_errorable_operation();
	rtvk_command_buffer_program_data(rtvk_command_buffer_from_handle(command_buffer), location, data, size, RTVK_DATA_STORAGE, RTVK_COMMAND_STORAGE_DATA);
}

void rtvk_command_buffer_clear_color(struct rtvk_command_buffer* command_buffer, u32 index, f32 r, f32 g, f32 b, f32 a) {
	struct rtvk_ir_clear_color* command = rtvk_command_append(command_buffer, RTVK_COMMAND_CLEAR_COLOR);
	if (!command) {
		return;
	}
	*command = (struct rtvk_ir_clear_color){ index, r, g, b, a };
}

void rtvk_command_buffer_clear_depth(struct rtvk_command_buffer* command_buffer, f32 depth) {
	struct rtvk_ir_clear_depth* command = rtvk_command_append(command_buffer, RTVK_COMMAND_CLEAR_DEPTH);
	if (!command) {
		return;
	}
	command->depth = depth;
}

void rtvk_command_buffer_clear_stencil(struct rtvk_command_buffer* command_buffer, u32 stencil) {
	struct rtvk_ir_clear_stencil* command = rtvk_command_append(command_buffer, RTVK_COMMAND_CLEAR_STENCIL);
	if (!command) {
		return;
	}
	command->stencil = stencil;
}

void rtvk_command_buffer_clear(struct rtvk_command_buffer* command_buffer, enum rt_clear_flag attachments) {
	struct rtvk_ir_clear* command = rtvk_command_append(command_buffer, RTVK_COMMAND_CLEAR);
	if (!command) {
		return;
	}
	command->attachments = attachments;
}

void rtvk_command_buffer_set_viewport(struct rtvk_command_buffer* command_buffer, u32 x, u32 y, u32 width, u32 height, f32 min_depth, f32 max_depth) {
	struct rtvk_ir_viewport* command = rtvk_command_append(command_buffer, RTVK_COMMAND_SET_VIEWPORT);
	if (!command) {
		return;
	}
	*command = (struct rtvk_ir_viewport){ x, y, width, height, min_depth, max_depth };
}

void rtvk_command_buffer_set_scissor(struct rtvk_command_buffer* command_buffer, u32 x, u32 y, u32 width, u32 height) {
	struct rtvk_ir_scissor* command = rtvk_command_append(command_buffer, RTVK_COMMAND_SET_SCISSOR);
	if (!command) {
		return;
	}
	*command = (struct rtvk_ir_scissor){ x, y, width, height };
}

void rtvk_command_buffer_end_rendering(struct rtvk_command_buffer* command_buffer) {
	if (!command_buffer || !command_buffer->recording || command_buffer->continuation || !command_buffer->rendering) {
		rtvk_throwf(RT_IMPROPER_USAGE, "rtCmdEndRendering requires an active rendering scope in a primary command buffer");
		return;
	}

	struct rtvk_command_header* command = rtvk_command_append(command_buffer, RTVK_COMMAND_END_RENDERING);
	if (!command) {
		return;
	}
	command_buffer->rendering = false;
}

void rtvk_command_buffer_use_program(struct rtvk_command_buffer* command_buffer, struct rtvk_program* program) {
	struct rtvk_ir_program* command = rtvk_command_append(command_buffer, RTVK_COMMAND_USE_PROGRAM);
	if (!command) {
		return;
	}
	command->program = program;
	rtvk_retain_resource(command->program);
}

void rtvk_command_buffer_bind_buffer(struct rtvk_command_buffer* command_buffer, rt_location location, struct rtvk_buffer* buffer, usize offset, usize size) {
	struct rtvk_ir_buffer* command = rtvk_command_append(command_buffer, RTVK_COMMAND_BIND_BUFFER);
	if (!command) {
		return;
	}
	*command = (struct rtvk_ir_buffer){ location ? location->address : 0, rtvk_buffer_active_node(buffer), offset, size };
	rtvk_retain_resource(command->buffer);
}

void rtvk_command_buffer_bind_texture(struct rtvk_command_buffer* command_buffer, rt_location location, struct rtvk_texture_view* view) {
	struct rtvk_ir_texture* command = rtvk_command_append(command_buffer, RTVK_COMMAND_BIND_TEXTURE);
	if (!command) {
		return;
	}
	struct rtvk_image_base* image = view && view->texture
		? &view->texture->active->base
		: view ? view->image : NULL;
	VkImageView image_view = image ? rtvk_image_view_create(rtvk_get_current_context(), image) : VK_NULL_HANDLE;
	*command = (struct rtvk_ir_texture){
		.address = location ? location->address : 0,
		.view = view,
		.image = image,
		.vk_image_view = image_view,
		.owns_image_view = image_view != VK_NULL_HANDLE,
	};
	rtvk_retain_resource(command->view);
	rtvk_retain_resource(command->image);
}

void rtvk_command_buffer_bind_sampler(struct rtvk_command_buffer* command_buffer, rt_location location, struct rtvk_sampler* sampler) {
	struct rtvk_ir_sampler* command = rtvk_command_append(command_buffer, RTVK_COMMAND_BIND_SAMPLER);
	if (!command) return;
	struct rtvk_sampler* node = rtvk_sampler_active_node(sampler);
	*command = (struct rtvk_ir_sampler){ location ? location->address : 0, node, node ? node->vk_sampler : VK_NULL_HANDLE };
	rtvk_retain_resource(command->sampler);
}

void rtvk_command_buffer_vertex_buffer(struct rtvk_command_buffer* command_buffer, rt_location location, struct rtvk_buffer* buffer, rt_buffer_range range) {
	struct rtvk_ir_vertex_buffer* command = rtvk_command_append(command_buffer, RTVK_COMMAND_VERTEX_BUFFER);
	if (!command) {
		return;
	}
	*command = (struct rtvk_ir_vertex_buffer){ location ? location->address : 0, rtvk_buffer_active_node(buffer), range };
	rtvk_retain_resource(command->buffer);
}

void rtvk_command_buffer_index_buffer(struct rtvk_command_buffer* command_buffer, struct rtvk_buffer* buffer, rt_buffer_range range, enum rt_index_format format) {
	struct rtvk_ir_index_buffer* command = rtvk_command_append(command_buffer, RTVK_COMMAND_INDEX_BUFFER);
	if (!command) {
		return;
	}
	*command = (struct rtvk_ir_index_buffer){ rtvk_buffer_active_node(buffer), range, format };
	rtvk_retain_resource(command->buffer);
}

void rtvk_command_buffer_draw(struct rtvk_command_buffer* command_buffer, u32 count, u32 first) {
	struct rtvk_ir_draw* command = rtvk_command_append(command_buffer, RTVK_COMMAND_DRAW);
	if (!command) {
		return;
	}
	*command = (struct rtvk_ir_draw){ count, first };
}

void rtvk_command_buffer_draw_instanced(struct rtvk_command_buffer* command_buffer, u32 count, u32 instances, u32 first, u32 first_instance) {
	struct rtvk_ir_draw_instanced* command = rtvk_command_append(command_buffer, RTVK_COMMAND_DRAW_INSTANCED);
	if (!command) {
		return;
	}
	*command = (struct rtvk_ir_draw_instanced){ count, instances, first, first_instance };
}

void rtvk_command_buffer_draw_indexed(struct rtvk_command_buffer* command_buffer, u32 count, u32 first, i32 vertex_offset) {
	struct rtvk_ir_draw_indexed* command = rtvk_command_append(command_buffer, RTVK_COMMAND_DRAW_INDEXED);
	if (!command) {
		return;
	}
	*command = (struct rtvk_ir_draw_indexed){ count, first, vertex_offset };
}

void rtvk_command_buffer_draw_indexed_instanced(struct rtvk_command_buffer* command_buffer, u32 count, u32 instances, u32 first, i32 vertex_offset, u32 first_instance) {
	struct rtvk_ir_draw_indexed_instanced* command = rtvk_command_append(command_buffer, RTVK_COMMAND_DRAW_INDEXED_INSTANCED);
	if (!command) {
		return;
	}
	*command = (struct rtvk_ir_draw_indexed_instanced){ count, instances, first, vertex_offset, first_instance };
}

void rtvk_command_buffer_dispatch(struct rtvk_command_buffer* command_buffer, usize group_count_x, usize group_count_y, usize group_count_z) {
	struct rtvk_ir_dispatch* command = rtvk_command_append(command_buffer, RTVK_COMMAND_DISPATCH);
	if (!command) return;
	*command = (struct rtvk_ir_dispatch){ (u32)group_count_x, (u32)group_count_y, (u32)group_count_z };
}

void rtvk_command_buffer_end(struct rtvk_command_buffer* command_buffer) {
	if (!command_buffer || !command_buffer->recording || command_buffer->rendering) {
		rtvk_throwf(RT_IMPROPER_USAGE, "rtCommandBufferEnd requires a recording command buffer outside a rendering scope");
		return;
	}

	command_buffer->recording = false;
	command_buffer->executable = rtvk_error() == RT_SUCCESS;
}

/*===============================================================================================*/
/*                                                                                                */
/*===============================================================================================*/

RTVK_DEFINE_RESOURCE_PRIVATE(command_buffer)

void rtvk_command_buffer_init(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer) {
	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(command_buffer), command_buffer, rtvk_command_buffer_finalize_resource);
}

void rtvk_command_buffer_finish(struct rtvk_command_buffer* command_buffer) {
	rtvk_command_buffer_release_resources(command_buffer);
}

/*===============================================================================================*/
/*                                                                                                */
/*===============================================================================================*/

struct rtvk_bound_descriptor {
	u08 address;
	struct rtvk_buffer* buffer;
	VkBuffer vk_buffer;
	struct rtvk_ir_texture texture;
	struct rtvk_ir_sampler sampler;
	usize offset;
	usize size;
};

struct rtvk_program_data_block {
	struct rtvk_program* program;
	rtvk_program_data_kind kind;
	u32 binding;
	struct rtvk_lowered_staging_buffer* buffer;
	u08* bytes;
	usize size;
};

struct rtvk_lower_state {
	struct rtvk_bound_descriptor* bound_descriptors;
	struct rtvk_buffer** vertex_buffers;
	struct rtvk_program_data_block* program_data_blocks;
	VkDeviceSize* vertex_offsets;
	const struct rtvk_framebuffer* framebuffer;
	struct rtvk_program* program;
	struct rtvk_buffer* index_buffer;
	usize descriptor_count;
	usize descriptor_capacity;
	usize vertex_buffer_capacity;
	usize program_data_count;
	usize program_data_capacity;
	usize index_offset;
	u32 render_width;
	u32 render_height;
	VkViewport viewport;
	VkRect2D scissor;
	VkClearValue clear_colors[RTVK_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
	VkClearValue clear_depth_stencil;
	enum rt_index_format index_format;
	bool rendering;
	bool descriptors_dirty;
	bool scissor_set;
	bool viewport_set;
};

void rtvk_lower_reserve_descriptors(struct rtvk_lower_state* state, usize count) {
	if (state->descriptor_capacity >= count) {
		return;
	}

	struct rtvk_bound_descriptor* descriptors = realloc(state->bound_descriptors, count * sizeof(*descriptors));
	if (!descriptors) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for descriptor binding state", count * sizeof(*descriptors));
		return;
	}

	memset(descriptors + state->descriptor_capacity, 0, (count - state->descriptor_capacity) * sizeof(*descriptors));
	state->bound_descriptors = descriptors;
	state->descriptor_capacity = count;
}

struct rtvk_bound_descriptor* rtvk_lower_find_descriptor(
	struct rtvk_lower_state* state,
	u08 address
) {
	for (usize index = 0; index < state->descriptor_count; index++) {
		struct rtvk_bound_descriptor* descriptor = &state->bound_descriptors[index];
		if (descriptor->address == address) {
			return descriptor;
		}
	}
	return NULL;
}

struct rtvk_bound_descriptor* rtvk_lower_add_descriptor(
	struct rtvk_lower_state* state,
	u08 address
) {
	struct rtvk_bound_descriptor* descriptor = rtvk_lower_find_descriptor(state, address);
	if (descriptor) {
		return descriptor;
	}

	rtvk_lower_reserve_descriptors(state, state->descriptor_count + 1);
	if (rtvk_error() != RT_SUCCESS) {
		return NULL;
	}

	descriptor = &state->bound_descriptors[state->descriptor_count++];
	*descriptor = (struct rtvk_bound_descriptor){ .address = address };
	return descriptor;
}

void rtvk_lower_reserve_vertex_buffers(struct rtvk_lower_state* state, usize count) {
	if (state->vertex_buffer_capacity >= count) {
		return;
	}

	struct rtvk_buffer** buffers = calloc(count, sizeof(*buffers));
	VkDeviceSize* offsets = calloc(count, sizeof(*offsets));
	if (!buffers || !offsets) {
		free(buffers);
		free(offsets);
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for vertex binding state", count * (sizeof(*buffers) + sizeof(*offsets)));
		return;
	}

	memcpy(buffers, state->vertex_buffers, sizeof(*buffers) * state->vertex_buffer_capacity);
	memcpy(offsets, state->vertex_offsets, sizeof(*offsets) * state->vertex_buffer_capacity);
	free(state->vertex_buffers);
	free(state->vertex_offsets);
	state->vertex_buffers = buffers;
	state->vertex_offsets = offsets;
	state->vertex_buffer_capacity = count;
}

void rtvk_lower_state_finish(struct rtvk_lower_state* state) {
	free(state->bound_descriptors);
	free(state->vertex_buffers);
	free(state->vertex_offsets);
	for (usize index = 0; index < state->program_data_count; index++) {
		free(state->program_data_blocks[index].bytes);
	}
	free(state->program_data_blocks);
}

struct rtvk_bound_descriptor* rtvk_lower_find_program_descriptor(struct rtvk_lower_state* state, const struct rtvk_program_descriptor_mapping* mapping) {
	for (usize index = 0; index < state->descriptor_count; index++) {
		struct rtvk_bound_descriptor* descriptor = &state->bound_descriptors[index];
		if (!state->program->descriptor_mapping_occupied[descriptor->address]) {
			continue;
		}
		const struct rtvk_program_descriptor_mapping* candidate = &state->program->descriptor_mappings[descriptor->address];
		if (candidate->binding == mapping->binding && rtvk_program_descriptor_type(candidate->kind) == rtvk_program_descriptor_type(mapping->kind)) {
			return descriptor;
		}
	}
	return NULL;
}

void rtvk_lower_reserve_program_data(struct rtvk_lower_state* state, usize count) {
	if (state->program_data_capacity >= count) {
		return;
	}
	struct rtvk_program_data_block* blocks = realloc(state->program_data_blocks, count * sizeof(*blocks));
	if (!blocks) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for program data state", count * sizeof(*blocks));
		return;
	}
	state->program_data_blocks = blocks;
	state->program_data_capacity = count;
}

void rtvk_lowered_command_buffer_add_resource_job(struct rtvk_lowered_command_buffer* lowered, struct rtvk_resource_base* resource) {
	if (!resource) {
		return;
	}

	for (struct rtvk_lowered_resource_job* job = lowered->resource_jobs; job; job = job->next) {
		if (job->base.resource == resource) {
			return;
		}
	}

	struct rtvk_lowered_resource_job* job = calloc(1, sizeof(*job));
	if (!job) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for Vulkan resource job", sizeof(*job));
		return;
	}

	job->base.resource = resource;
	rtvk_resource_job_begin(resource);
	job->next = lowered->resource_jobs;
	lowered->resource_jobs = job;
}

void rtvk_lowered_command_buffer_release_resource_jobs(struct rtvk_lowered_command_buffer* lowered) {
	while (lowered->resource_jobs) {
		struct rtvk_lowered_resource_job* job = lowered->resource_jobs;
		lowered->resource_jobs = job->next;
		rtvk_resource_job_end(job->base.resource);
		free(job);
	}
}

struct rtvk_lowered_staging_buffer* rtvk_lowered_command_buffer_create_host_buffer(struct rtvk_context* ctx, struct rtvk_lowered_command_buffer* lowered, usize size, VkBufferUsageFlags usage) {
	struct rtvk_lowered_staging_buffer* staging = calloc(1, sizeof(*staging));
	if (!staging) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for recorded buffer staging metadata", sizeof(*staging));
		return NULL;
	}

	VkBufferCreateInfo buffer_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	buffer_info.size = size ? size : 1;
	buffer_info.usage = usage;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VmaAllocationCreateInfo allocation_info = { 0 };
	allocation_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	VkResult result = vmaCreateBuffer(ctx->vma_allocator, &buffer_info, &allocation_info, &staging->vk_buffer, &staging->vma_allocation, NULL);
	if (result != VK_SUCCESS) {
		free(staging);
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return NULL;
	}

	staging->next = lowered->staging_buffers;
	lowered->staging_buffers = staging;
	return staging;
}

VkPipelineStageFlags rtvk_access_stage_mask(enum rt_stage_flag stage, bool destination) {
	VkPipelineStageFlags mask = 0;
	if (stage & RT_STAGE_TRANSFER) {
		mask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	if (stage & RT_STAGE_VERTEX) {
		mask |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
	}
	if (stage & RT_STAGE_FRAGMENT) {
		mask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	if (stage & RT_STAGE_COMPUTE) {
		mask |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	}
	if (stage & RT_STAGE_COLOR_ATTACHMENT) {
		mask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	}
	if (stage & RT_STAGE_DEPTH_STENCIL_ATTACHMENT) {
		mask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	}
	if (stage & RT_STAGE_ALL) {
		mask |= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	}
	if (!mask) {
		return destination ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	}
	return mask;
}

VkAccessFlags rtvk_access_mask(rt_access access) {
	if (access.type == RT_ACCESS_READ) {
		return VK_ACCESS_MEMORY_READ_BIT;
	}
	if (access.type == RT_ACCESS_WRITE) {
		return VK_ACCESS_MEMORY_WRITE_BIT;
	}
	return 0;
}

bool rtvk_lowered_command_buffer_add_image_view(struct rtvk_lowered_command_buffer* lowered, VkImageView image_view) {
	struct rtvk_lowered_image_view* entry = malloc(sizeof(*entry));
	if (!entry) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to retain a bound image view until submission completes");
		return false;
	}
	entry->vk_image_view = image_view;
	entry->next = lowered->image_views;
	lowered->image_views = entry;
	return true;
}

VkImageLayout rtvk_access_image_layout(rt_access access, VkImageLayout current_layout) {
	if (access.stage & RT_STAGE_TRANSFER) {
		return access.type == RT_ACCESS_READ ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	}
	if (access.stage & (RT_STAGE_VERTEX | RT_STAGE_FRAGMENT | RT_STAGE_COMPUTE)) {
		/* rt_access identifies a shader stage and read/write direction, but not
		 * whether the shader binding is sampled or storage. GENERAL is valid for
		 * both and therefore preserves the explicit barrier contract. */
		return VK_IMAGE_LAYOUT_GENERAL;
	}
	if (access.stage & RT_STAGE_COLOR_ATTACHMENT) {
		return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}
	if (access.stage & RT_STAGE_DEPTH_STENCIL_ATTACHMENT) {
		return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}
	return current_layout;
}

void rtvk_lowered_command_buffer_end(struct rtvk_lowered_command_buffer* lowered) {
	VkResult result = vkEndCommandBuffer(lowered->vk_command_buffer);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}
}

static VkExtent3D rtvk_framebuffer_extent(const struct rtvk_framebuffer* framebuffer) {
	for (usize index = 0; index < framebuffer->color_texture_count; index++) {
		struct rtvk_texture_view* view = framebuffer->color_views[index];
		if (view && view->image) {
			return (VkExtent3D){
				view->image->width,
				view->image->height,
				view->image->depth,
			};
		}
	}
	if (framebuffer->depth_view && framebuffer->depth_view->image) {
		return (VkExtent3D){
			framebuffer->depth_view->image->width,
			framebuffer->depth_view->image->height,
			framebuffer->depth_view->image->depth,
		};
	}
	if (framebuffer->stencil_view && framebuffer->stencil_view->image) {
		return (VkExtent3D){
			framebuffer->stencil_view->image->width,
			framebuffer->stencil_view->image->height,
			framebuffer->stencil_view->image->depth,
		};
	}
	return (VkExtent3D){ 0 };
}

void rtvk_command_buffer_program_data(struct rtvk_command_buffer* command_buffer, rt_location location, const u08* data, usize size, rtvk_program_data_kind expected_kind, rtvk_command_opcode opcode) {
	struct rtvk_program* program = rtvk_location_program(location);
	const struct rtvk_program_data_mapping* mapping = rtvk_program_data_mapping(program, location);
	if (!command_buffer || !command_buffer->recording || !mapping || mapping->kind != expected_kind || !data || !size) {
		rtvk_throwf(RT_IMPROPER_USAGE, "program data write does not match its reflected location");
		return;
	}
	u08* bytes = malloc(size);
	if (!bytes) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to copy program data into the command buffer");
		return;
	}
	memcpy(bytes, data, size);
	struct rtvk_ir_program_data* command = rtvk_command_append(command_buffer, opcode);
	if (!command) {
		free(bytes);
		return;
	}
	*command = (struct rtvk_ir_program_data){ location->address, bytes, size };
}

void rtvk_lower_begin_rendering(VkCommandBuffer command_buffer, const struct rtvk_framebuffer* framebuffer) {
	VkRenderingAttachmentInfo colors[RTVK_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS] = { 0 };
	VkExtent3D extent = rtvk_framebuffer_extent(framebuffer);
	for (usize index = 0; index < framebuffer->color_texture_count; index++) {
		struct rtvk_texture_view* view = framebuffer->color_views[index];
		struct rtvk_image_base* image = view ? view->image : NULL;
		colors[index].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		if (!image) {
			colors[index].imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colors[index].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colors[index].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			continue;
		}
		rtvk_image_transition_layout(command_buffer, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		colors[index].imageView = view->vk_image_view;
		colors[index].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colors[index].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		colors[index].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	}

	VkRenderingAttachmentInfo depth = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
	if (framebuffer->depth_view && framebuffer->depth_view->image) {
		rtvk_image_transition_layout(command_buffer, framebuffer->depth_view->image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		depth.imageView = framebuffer->depth_view->vk_image_view;
		depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	}

	VkRenderingAttachmentInfo stencil = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
	if (framebuffer->stencil_view && framebuffer->stencil_view->image) {
		rtvk_image_transition_layout(command_buffer, framebuffer->stencil_view->image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		stencil.imageView = framebuffer->stencil_view->vk_image_view;
		stencil.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		stencil.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		stencil.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	}

	VkRenderingInfo info = { VK_STRUCTURE_TYPE_RENDERING_INFO };
	info.renderArea.extent.width = extent.width;
	info.renderArea.extent.height = extent.height;
	info.layerCount = 1;
	info.colorAttachmentCount = framebuffer->color_texture_count;
	info.pColorAttachments = colors;
	info.pDepthAttachment = framebuffer->depth_view && framebuffer->depth_view->image ? &depth : NULL;
	info.pStencilAttachment = framebuffer->stencil_view && framebuffer->stencil_view->image ? &stencil : NULL;
	vkCmdBeginRendering(command_buffer, &info);
}

void rtvk_lower_bind_descriptors(struct rtvk_context* ctx, struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state) {
	if (!state->descriptors_dirty) {
		return;
	}
	if (!state->program || !state->program->vk_descriptor_set_layout) {
		state->descriptors_dirty = false;
		return;
	}
	VkDescriptorSetLayout layout = state->program->vk_descriptor_set_layout;
	VkDescriptorSetAllocateInfo allocate_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocate_info.descriptorPool = lowered->vk_descriptor_pool;
	allocate_info.descriptorSetCount = 1;
	allocate_info.pSetLayouts = &layout;
	VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
	VkResult result = vkAllocateDescriptorSets(ctx->vk_device, &allocate_info, &descriptor_set);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}

	u32 descriptor_count = 0;
	for (u32 location_index = 0; location_index < 256; location_index++) {
		if (rtvk_program_descriptor_is_first(state->program, location_index)) {
			descriptor_count++;
			if (state->program->descriptor_mappings[location_index].sampled_alias) descriptor_count++;
		}
	}

	VkDescriptorBufferInfo* buffer_infos = calloc(descriptor_count, sizeof(*buffer_infos));
	VkDescriptorImageInfo* image_infos = calloc(descriptor_count, sizeof(*image_infos));
	VkWriteDescriptorSet* writes = calloc(descriptor_count, sizeof(*writes));
	if (!buffer_infos || !image_infos || !writes) {
		free(buffer_infos);
		free(image_infos);
		free(writes);
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for descriptor writes", (usize)descriptor_count * (sizeof(*buffer_infos) + sizeof(*image_infos) + sizeof(*writes)));
		return;
	}

	u32 descriptor_index = 0;
	for (u32 location_index = 0; location_index < 256; location_index++) {
		struct rtvk_program_descriptor_mapping* mapping = &state->program->descriptor_mappings[location_index];
		if (!rtvk_program_descriptor_is_first(state->program, location_index)) {
			continue;
		}
		writes[descriptor_index] = (VkWriteDescriptorSet){ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		writes[descriptor_index].dstSet = descriptor_set;
		writes[descriptor_index].dstBinding = mapping->binding;
		writes[descriptor_index].descriptorCount = 1;
		const struct rtvk_bound_descriptor* descriptor = rtvk_lower_find_program_descriptor(state, mapping);
		if (mapping->kind == RTVK_DESCRIPTOR_TEXTURE || mapping->kind == RTVK_DESCRIPTOR_STORAGE_TEXTURE) {
			const struct rtvk_ir_texture* texture = descriptor ? &descriptor->texture : NULL;
			if (!texture || !texture->vk_image_view) {
				free(buffer_infos);
				free(image_infos);
				free(writes);
				rtvk_throwf(RT_IMPROPER_USAGE, "program resource %s is not bound to a texture view", state->program->location_names[location_index]);
				return;
			}
			image_infos[descriptor_index].imageView = texture->vk_image_view;
			if (mapping->kind == RTVK_DESCRIPTOR_TEXTURE) {
				image_infos[descriptor_index].sampler = descriptor->sampler.vk_sampler
					? descriptor->sampler.vk_sampler
					: texture->view->vk_sampler;
				image_infos[descriptor_index].imageLayout = texture->image->vk_layout;
				writes[descriptor_index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			} else {
				image_infos[descriptor_index].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
				writes[descriptor_index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			}
			writes[descriptor_index].pImageInfo = &image_infos[descriptor_index];
		} else if (mapping->kind == RTVK_DESCRIPTOR_SAMPLER) {
			const struct rtvk_ir_sampler* sampler = descriptor ? &descriptor->sampler : NULL;
			if (!sampler || !sampler->vk_sampler) {
				free(buffer_infos);
				free(image_infos);
				free(writes);
				rtvk_throwf(RT_IMPROPER_USAGE, "program resource %s is not bound to a sampler", state->program->location_names[location_index]);
				return;
			}
			image_infos[descriptor_index].sampler = sampler->vk_sampler;
			writes[descriptor_index].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			writes[descriptor_index].pImageInfo = &image_infos[descriptor_index];
		} else {
			VkBuffer buffer = descriptor ? descriptor->vk_buffer : VK_NULL_HANDLE;
			usize buffer_size = descriptor ? descriptor->size : 0;
			usize buffer_offset = descriptor ? descriptor->offset : 0;
			if (!buffer && (mapping->kind == RTVK_DESCRIPTOR_BUFFER || mapping->kind == RTVK_DESCRIPTOR_STORAGE_BUFFER)) {
				const rtvk_program_data_kind data_kind = mapping->kind == RTVK_DESCRIPTOR_BUFFER ? RTVK_DATA_UNIFORM : RTVK_DATA_STORAGE;
				for (usize block_index = 0; block_index < state->program_data_count; block_index++) {
					const struct rtvk_program_data_block* block = &state->program_data_blocks[block_index];
					if (block->program == state->program && block->kind == data_kind && block->binding == mapping->binding && block->buffer) {
						buffer = block->buffer->vk_buffer;
						buffer_size = block->size;
						buffer_offset = 0;
						break;
					}
				}
			}
			if (!buffer || !buffer_size) {
				free(buffer_infos);
				free(image_infos);
				free(writes);
				rtvk_throwf(RT_IMPROPER_USAGE, "program resource %s is not bound to a buffer range", state->program->location_names[location_index]);
				return;
			}
			buffer_infos[descriptor_index].buffer = buffer;
			buffer_infos[descriptor_index].offset = buffer_offset;
			buffer_infos[descriptor_index].range = buffer_size;
			writes[descriptor_index].descriptorType = rtvk_program_descriptor_type(mapping->kind);
			writes[descriptor_index].pBufferInfo = &buffer_infos[descriptor_index];
		}
		descriptor_index++;
		if (mapping->sampled_alias) {
			writes[descriptor_index] = (VkWriteDescriptorSet){ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			writes[descriptor_index].dstSet = descriptor_set;
			writes[descriptor_index].dstBinding = mapping->sampled_binding;
			writes[descriptor_index].descriptorCount = 1;
			image_infos[descriptor_index].imageView = image_infos[descriptor_index - 1].imageView;
			image_infos[descriptor_index].sampler = descriptor->sampler.vk_sampler ? descriptor->sampler.vk_sampler : descriptor->texture.view->vk_sampler;
			image_infos[descriptor_index].imageLayout = descriptor->texture.image->vk_layout;
			writes[descriptor_index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[descriptor_index].pImageInfo = &image_infos[descriptor_index];
			descriptor_index++;
		}
	}

	vkUpdateDescriptorSets(ctx->vk_device, descriptor_count, writes, 0, NULL);
	vkCmdBindDescriptorSets(lowered->vk_command_buffer, state->program->compute_program ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS, state->program->vk_pipeline_layout, 0, 1, &descriptor_set, 0, NULL);
	free(buffer_infos);
	free(image_infos);
	free(writes);
	state->descriptors_dirty = false;
}

void rtvk_lower_bind_program(struct rtvk_context* ctx, struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state) {
	if (state->program->compute_program) {
		vkCmdBindPipeline(lowered->vk_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, state->program->vk_compute_pipeline);
		return;
	}
	if (!state->framebuffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "a program may only be used inside a rendering scope");
		return;
	}

	VkFormat formats[RTVK_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
	for (usize index = 0; index < state->framebuffer->color_texture_count; index++) {
		formats[index] = rtvk_view_format(state->framebuffer->color_views[index]);
	}

	VkPipeline pipeline = rtvk_program_prepare(
		ctx,
		state->program,
		formats,
		state->framebuffer->color_texture_count,
		rtvk_view_format(state->framebuffer->depth_view),
		rtvk_view_format(state->framebuffer->stencil_view),
		VK_SAMPLE_COUNT_1_BIT
	);
	if (pipeline) {
		vkCmdBindPipeline(lowered->vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	}
}

void rtvk_lower_buffer_copy_prior(struct rtvk_lowered_command_buffer* lowered, struct rtvk_buffer* source, struct rtvk_buffer* target) {
	if (!source || !target) {
		return;
	}

	VkBufferCopy copy = { 0 };
	copy.size = source->size;
	vkCmdCopyBuffer(lowered->vk_command_buffer, source->vk_buffer, target->vk_buffer, 1, &copy);
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(source));
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(target));
}

void rtvk_lower_buffer_data(struct rtvk_context* ctx, struct rtvk_lowered_command_buffer* lowered, const struct rtvk_ir_buffer_data* command) {
	if (!command->buffer || !command->range.size) {
		return;
	}

	rtvk_lower_buffer_copy_prior(lowered, command->copy_source, command->buffer);

	struct rtvk_lowered_staging_buffer* staging = rtvk_lowered_command_buffer_create_host_buffer(ctx, lowered, command->range.size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	if (!staging) {
		return;
	}

	VmaAllocationInfo allocation_info;
	vmaGetAllocationInfo(ctx->vma_allocator, staging->vma_allocation, &allocation_info);
	memcpy(allocation_info.pMappedData, command->data, command->range.size);
	vmaFlushAllocation(ctx->vma_allocator, staging->vma_allocation, 0, command->range.size);

	VkBufferCopy copy = { 0 };
	copy.srcOffset = 0;
	copy.dstOffset = command->range.offset;
	copy.size = command->range.size;
	vkCmdCopyBuffer(lowered->vk_command_buffer, staging->vk_buffer, command->buffer->vk_buffer, 1, &copy);
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(command->buffer));
}

void rtvk_lower_buffer_copy(struct rtvk_lowered_command_buffer* lowered, const struct rtvk_ir_buffer_copy* command) {
	if (!command->src || !command->dst || !command->src_range.size || !command->dst_range.size) {
		return;
	}

	rtvk_lower_buffer_copy_prior(lowered, command->copy_source, command->dst);

	VkBufferCopy copy = { 0 };
	copy.srcOffset = command->src_range.offset;
	copy.dstOffset = command->dst_range.offset;
	copy.size = command->src_range.size < command->dst_range.size ? command->src_range.size : command->dst_range.size;
	vkCmdCopyBuffer(lowered->vk_command_buffer, command->src->vk_buffer, command->dst->vk_buffer, 1, &copy);
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(command->src));
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(command->dst));
}

void rtvk_lower_buffer_copy_to_texture(struct rtvk_lowered_command_buffer* lowered, const struct rtvk_ir_buffer_copy_to_texture* command) {
	if (!command->src || !command->dst || !command->src_range.size) {
		return;
	}

	rtvk_image_transition_layout(lowered->vk_command_buffer, &command->dst->base, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	usize region_size = command->dst_range.extent.width * command->dst_range.extent.height * command->dst_range.extent.depth * rtvk_texture_bytes_per_texel(command->dst->base.vk_format);
	usize buffer_offset = command->src_range.offset;
	for (usize mip = 0; mip < command->dst_range.mip_count; mip++) {
		for (usize layer = 0; layer < command->dst_range.layer_count; layer++) {
			VkBufferImageCopy copy = { 0 };
			copy.bufferOffset = buffer_offset;
			copy.imageSubresource.aspectMask = rtvk_texture_range_aspect(command->dst->base.vk_format, command->dst_range.aspects);
			copy.imageSubresource.mipLevel = (u32)(command->dst_range.base_mip + mip);
			copy.imageSubresource.baseArrayLayer = (u32)(command->dst_range.base_layer + layer);
			copy.imageSubresource.layerCount = 1;
			copy.imageOffset = (VkOffset3D){ (i32)command->dst_range.offset.width, (i32)command->dst_range.offset.height, (i32)command->dst_range.offset.depth };
			copy.imageExtent = (VkExtent3D){ (u32)command->dst_range.extent.width, (u32)command->dst_range.extent.height, (u32)command->dst_range.extent.depth };
			vkCmdCopyBufferToImage(lowered->vk_command_buffer, command->src->vk_buffer, command->dst->base.vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
			buffer_offset += region_size;
		}
	}
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(command->src));
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(command->dst));
}

void rtvk_lower_buffer_barrier(struct rtvk_lowered_command_buffer* lowered, const struct rtvk_ir_buffer_barrier* command) {
	if (!command->buffer) {
		return;
	}

	VkBufferMemoryBarrier barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
	barrier.srcAccessMask = rtvk_access_mask(command->src);
	barrier.dstAccessMask = rtvk_access_mask(command->dst);
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = command->buffer->vk_buffer;
	barrier.offset = command->range.offset;
	barrier.size = command->range.size;
	vkCmdPipelineBarrier(
		lowered->vk_command_buffer,
		rtvk_access_stage_mask(command->src.stage, false),
		rtvk_access_stage_mask(command->dst.stage, true),
		0,
		0,
		NULL,
		1,
		&barrier,
		0,
		NULL
	);
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(command->buffer));
}

void rtvk_lower_texture_copy_prior(struct rtvk_lowered_command_buffer* lowered, struct rtvk_texture* source, struct rtvk_texture* target) {
	if (!source || !target) {
		return;
	}

	rtvk_image_transition_layout(lowered->vk_command_buffer, &source->base, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	rtvk_image_transition_layout(lowered->vk_command_buffer, &target->base, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	usize layer_count = source->base.type == RT_TEXTURE_1D_ARRAY || source->base.type == RT_TEXTURE_2D_ARRAY ? source->base.depth : 1;
	for (u32 mip = 0; mip < source->base.mip_levels; mip++) {
		VkImageCopy copy = { 0 };
		copy.srcSubresource.aspectMask = rtvk_texture_format_aspect(source->base.vk_format);
		copy.srcSubresource.mipLevel = mip;
		copy.srcSubresource.layerCount = (u32)layer_count;
		copy.dstSubresource.aspectMask = rtvk_texture_format_aspect(target->base.vk_format);
		copy.dstSubresource.mipLevel = mip;
		copy.dstSubresource.layerCount = (u32)layer_count;
		copy.extent.width = source->base.width >> mip;
		copy.extent.height = source->base.height >> mip;
		copy.extent.depth = source->base.type == RT_TEXTURE_3D ? source->base.depth >> mip : 1;
		if (!copy.extent.width) {
			copy.extent.width = 1;
		}
		if (!copy.extent.height) {
			copy.extent.height = 1;
		}
		if (!copy.extent.depth) {
			copy.extent.depth = 1;
		}
		vkCmdCopyImage(lowered->vk_command_buffer, source->base.vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target->base.vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	}
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(source));
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(target));
}

void rtvk_lower_texture_copy(struct rtvk_lowered_command_buffer* lowered, const struct rtvk_ir_texture_copy* command) {
	if (!command->src || !command->dst) {
		return;
	}

	rtvk_lower_texture_copy_prior(lowered, command->copy_source, command->dst);
	rtvk_image_transition_layout(lowered->vk_command_buffer, &command->src->base, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	rtvk_image_transition_layout(lowered->vk_command_buffer, &command->dst->base, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	usize mip_count = command->src_range.mip_count < command->dst_range.mip_count ? command->src_range.mip_count : command->dst_range.mip_count;
	usize layer_count = command->src_range.layer_count < command->dst_range.layer_count ? command->src_range.layer_count : command->dst_range.layer_count;
	for (usize mip = 0; mip < mip_count; mip++) {
		for (usize layer = 0; layer < layer_count; layer++) {
			VkImageCopy copy = { 0 };
			copy.srcSubresource.aspectMask = rtvk_texture_range_aspect(command->src->base.vk_format, command->src_range.aspects);
			copy.srcSubresource.mipLevel = (u32)(command->src_range.base_mip + mip);
			copy.srcSubresource.baseArrayLayer = (u32)(command->src_range.base_layer + layer);
			copy.srcSubresource.layerCount = 1;
			copy.srcOffset = (VkOffset3D){ (i32)command->src_range.offset.width, (i32)command->src_range.offset.height, (i32)command->src_range.offset.depth };
			copy.dstSubresource.aspectMask = rtvk_texture_range_aspect(command->dst->base.vk_format, command->dst_range.aspects);
			copy.dstSubresource.mipLevel = (u32)(command->dst_range.base_mip + mip);
			copy.dstSubresource.baseArrayLayer = (u32)(command->dst_range.base_layer + layer);
			copy.dstSubresource.layerCount = 1;
			copy.dstOffset = (VkOffset3D){ (i32)command->dst_range.offset.width, (i32)command->dst_range.offset.height, (i32)command->dst_range.offset.depth };
			copy.extent = (VkExtent3D){ (u32)command->src_range.extent.width, (u32)command->src_range.extent.height, (u32)command->src_range.extent.depth };
			vkCmdCopyImage(lowered->vk_command_buffer, command->src->base.vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, command->dst->base.vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
		}
	}
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(command->src));
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(command->dst));
}

void rtvk_lower_texture_data(struct rtvk_context* ctx, struct rtvk_lowered_command_buffer* lowered, const struct rtvk_ir_texture_data* command) {
	if (!command->texture || !command->data_size) {
		return;
	}

	rtvk_lower_texture_copy_prior(lowered, command->copy_source, command->texture);

	struct rtvk_lowered_staging_buffer* staging = rtvk_lowered_command_buffer_create_host_buffer(ctx, lowered, command->data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	if (!staging) {
		return;
	}

	VmaAllocationInfo allocation_info;
	vmaGetAllocationInfo(ctx->vma_allocator, staging->vma_allocation, &allocation_info);
	memcpy(allocation_info.pMappedData, command->data, command->data_size);
	vmaFlushAllocation(ctx->vma_allocator, staging->vma_allocation, 0, command->data_size);

	rtvk_image_transition_layout(lowered->vk_command_buffer, &command->texture->base, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	usize region_size = command->range.extent.width * command->range.extent.height * command->range.extent.depth * rtvk_texture_bytes_per_texel(command->texture->base.vk_format);
	usize buffer_offset = 0;
	for (usize mip = 0; mip < command->range.mip_count; mip++) {
		for (usize layer = 0; layer < command->range.layer_count; layer++) {
			VkBufferImageCopy copy = { 0 };
			copy.bufferOffset = buffer_offset;
			copy.imageSubresource.aspectMask = rtvk_texture_range_aspect(command->texture->base.vk_format, command->range.aspects);
			copy.imageSubresource.mipLevel = (u32)(command->range.base_mip + mip);
			copy.imageSubresource.baseArrayLayer = (u32)(command->range.base_layer + layer);
			copy.imageSubresource.layerCount = 1;
			copy.imageOffset = (VkOffset3D){ (i32)command->range.offset.width, (i32)command->range.offset.height, (i32)command->range.offset.depth };
			copy.imageExtent = (VkExtent3D){ (u32)command->range.extent.width, (u32)command->range.extent.height, (u32)command->range.extent.depth };
			vkCmdCopyBufferToImage(lowered->vk_command_buffer, staging->vk_buffer, command->texture->base.vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
			buffer_offset += region_size;
		}
	}
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(command->texture));
}

void rtvk_lower_texture_copy_to_buffer(struct rtvk_lowered_command_buffer* lowered, const struct rtvk_ir_texture_copy_to_buffer* command) {
	if (!command->src || !command->dst) {
		return;
	}

	rtvk_lower_buffer_copy_prior(lowered, command->copy_source, command->dst);

	rtvk_image_transition_layout(lowered->vk_command_buffer, &command->src->base, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	usize region_size = command->src_range.extent.width * command->src_range.extent.height * command->src_range.extent.depth * rtvk_texture_bytes_per_texel(command->src->base.vk_format);
	usize buffer_offset = command->dst_range.offset;
	for (usize mip = 0; mip < command->src_range.mip_count; mip++) {
		for (usize layer = 0; layer < command->src_range.layer_count; layer++) {
			VkBufferImageCopy copy = { 0 };
			copy.bufferOffset = buffer_offset;
			copy.imageSubresource.aspectMask = rtvk_texture_range_aspect(command->src->base.vk_format, command->src_range.aspects);
			copy.imageSubresource.mipLevel = (u32)(command->src_range.base_mip + mip);
			copy.imageSubresource.baseArrayLayer = (u32)(command->src_range.base_layer + layer);
			copy.imageSubresource.layerCount = 1;
			copy.imageOffset = (VkOffset3D){ (i32)command->src_range.offset.width, (i32)command->src_range.offset.height, (i32)command->src_range.offset.depth };
			copy.imageExtent = (VkExtent3D){ (u32)command->src_range.extent.width, (u32)command->src_range.extent.height, (u32)command->src_range.extent.depth };
			vkCmdCopyImageToBuffer(lowered->vk_command_buffer, command->src->base.vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, command->dst->vk_buffer, 1, &copy);
			buffer_offset += region_size;
		}
	}
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(command->src));
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(command->dst));
}

void rtvk_lower_texture_barrier(struct rtvk_lowered_command_buffer* lowered, const struct rtvk_ir_texture_barrier* command) {
	if (!command->texture) {
		return;
	}

	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.srcAccessMask = rtvk_access_mask(command->src);
	barrier.dstAccessMask = rtvk_access_mask(command->dst);
	barrier.oldLayout = command->texture->base.vk_layout;
	barrier.newLayout = rtvk_access_image_layout(command->dst, barrier.oldLayout);
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = command->texture->base.vk_image;
	barrier.subresourceRange.aspectMask = rtvk_texture_range_aspect(command->texture->base.vk_format, command->range.aspects);
	barrier.subresourceRange.baseMipLevel = (u32)command->range.base_mip;
	barrier.subresourceRange.levelCount = (u32)command->range.mip_count;
	barrier.subresourceRange.baseArrayLayer = (u32)command->range.base_layer;
	barrier.subresourceRange.layerCount = (u32)command->range.layer_count;
	vkCmdPipelineBarrier(
		lowered->vk_command_buffer,
		rtvk_access_stage_mask(command->src.stage, false),
		rtvk_access_stage_mask(command->dst.stage, true),
		0,
		0,
		NULL,
		0,
		NULL,
		1,
		&barrier
	);
	command->texture->base.vk_layout = barrier.newLayout;
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(command->texture));
}

void rtvk_lower_begin_rendering_command(struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state, const struct rtvk_ir_framebuffer* command) {
	state->framebuffer = command->framebuffer;
	if (command->framebuffer) {
		rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(command->framebuffer));
	}
	rtvk_lower_begin_rendering(lowered->vk_command_buffer, command->framebuffer);
	VkExtent3D extent = rtvk_framebuffer_extent(command->framebuffer);
	state->render_width = extent.width;
	state->render_height = extent.height;
	state->viewport = (VkViewport){ 0.0f, (f32)extent.height, (f32)extent.width, -(f32)extent.height, 0.0f, 1.0f };
	state->scissor = (VkRect2D){ { 0, 0 }, { extent.width, extent.height } };
	state->viewport_set = true;
	state->scissor_set = true;
	vkCmdSetViewport(lowered->vk_command_buffer, 0, 1, &state->viewport);
	vkCmdSetScissor(lowered->vk_command_buffer, 0, 1, &state->scissor);
	state->rendering = true;
}

void rtvk_lower_clear_color(struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state, const struct rtvk_ir_clear_color* command) {
	(void)lowered;
	if (command->index < RTVK_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS) {
		state->clear_colors[command->index].color.float32[0] = command->r;
		state->clear_colors[command->index].color.float32[1] = command->g;
		state->clear_colors[command->index].color.float32[2] = command->b;
		state->clear_colors[command->index].color.float32[3] = command->a;
	}
}

void rtvk_lower_clear_depth(struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state, const struct rtvk_ir_clear_depth* command) {
	(void)lowered;
	state->clear_depth_stencil.depthStencil.depth = command->depth;
}

void rtvk_lower_clear_stencil(struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state, const struct rtvk_ir_clear_stencil* command) {
	(void)lowered;
	state->clear_depth_stencil.depthStencil.stencil = command->stencil;
}

void rtvk_lower_clear(struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state, const struct rtvk_ir_clear* command) {
	VkClearAttachment attachments[RTVK_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS + 2] = { 0 };
	u32 attachment_count = 0;
	if (command->attachments & RT_CLEAR_COLOR) {
		for (u32 index = 0; index < state->framebuffer->color_texture_count; index++) {
			if (!state->framebuffer->color_views[index] || !state->framebuffer->color_views[index]->image) {
				continue;
			}
			attachments[attachment_count].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			attachments[attachment_count].colorAttachment = index;
			attachments[attachment_count].clearValue = state->clear_colors[index];
			attachment_count++;
		}
	}
	if ((command->attachments & RT_CLEAR_DEPTH) && state->framebuffer->depth_view && state->framebuffer->depth_view->image) {
		attachments[attachment_count].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		attachments[attachment_count].clearValue = state->clear_depth_stencil;
		attachment_count++;
	}
	if ((command->attachments & RT_CLEAR_STENCIL) && state->framebuffer->stencil_view && state->framebuffer->stencil_view->image) {
		attachments[attachment_count].aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
		attachments[attachment_count].clearValue = state->clear_depth_stencil;
		attachment_count++;
	}
	if (!attachment_count) {
		return;
	}
	VkClearRect rect = { { { 0, 0 }, { state->render_width, state->render_height } }, 0, 1 };
	vkCmdClearAttachments(lowered->vk_command_buffer, attachment_count, attachments, 1, &rect);
}

void rtvk_lower_set_viewport(struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state, const struct rtvk_ir_viewport* command) {
	state->viewport = (VkViewport){ (f32)command->x, (f32)(command->y + command->height), (f32)command->width, -(f32)command->height, command->min_depth, command->max_depth };
	state->viewport_set = true;
	vkCmdSetViewport(lowered->vk_command_buffer, 0, 1, &state->viewport);
}

void rtvk_lower_set_scissor(struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state, const struct rtvk_ir_scissor* command) {
	state->scissor = (VkRect2D){ { (i32)command->x, (i32)command->y }, { command->width, command->height } };
	state->scissor_set = true;
	vkCmdSetScissor(lowered->vk_command_buffer, 0, 1, &state->scissor);
}

void rtvk_lower_end_rendering(struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state) {
	vkCmdEndRendering(lowered->vk_command_buffer);
	for (usize index = 0; index < state->framebuffer->color_texture_count; index++) {
		struct rtvk_texture_view* view = state->framebuffer->color_views[index];
		struct rtvk_image_base* image = view ? view->image : NULL;
		if (image && image->presentable) {
			rtvk_image_transition_layout(lowered->vk_command_buffer, image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		}
	}
	state->rendering = false;
}

void rtvk_lower_use_program(struct rtvk_context* ctx, struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state, const struct rtvk_ir_program* command) {
	state->program = command->program;
	state->descriptors_dirty = true;
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(state->program));
	rtvk_lower_bind_program(ctx, lowered, state);
}

void rtvk_lower_program_data(struct rtvk_context* ctx, struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state, const struct rtvk_ir_program_data* command, rtvk_program_data_kind expected_kind) {
	if (!state->program || !state->program->data_mapping_occupied[command->address]) {
		return;
	}
	struct rtvk_program_data_mapping* mapping = &state->program->data_mappings[command->address];
	if (mapping->kind != expected_kind || !command->size || mapping->byte_offset > mapping->block_size) {
		return;
	}
	struct rtvk_program_data_block* block = NULL;
	for (usize index = 0; index < state->program_data_count; index++) {
		struct rtvk_program_data_block* candidate = &state->program_data_blocks[index];
		if (candidate->program == state->program && candidate->kind == mapping->kind && candidate->binding == mapping->binding) {
			block = candidate;
			break;
		}
	}
	if (!block) {
		rtvk_lower_reserve_program_data(state, state->program_data_count + 1);
		if (rtvk_error() != RT_SUCCESS) {
			return;
		}
		block = &state->program_data_blocks[state->program_data_count++];
		*block = (struct rtvk_program_data_block){ state->program, mapping->kind, mapping->binding, NULL, NULL, mapping->block_size };
		block->bytes = calloc(1, mapping->block_size);
		if (!block->bytes) {
			rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for aggregate program data", mapping->block_size);
			return;
		}
	}
	const usize copy_size = command->size < mapping->byte_size ? command->size : mapping->byte_size;
	if (copy_size > mapping->block_size - mapping->byte_offset) return;
	memcpy(block->bytes + mapping->byte_offset, command->bytes, copy_size);
	const VkBufferUsageFlags usage = mapping->kind == RTVK_DATA_UNIFORM ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	block->buffer = rtvk_lowered_command_buffer_create_host_buffer(ctx, lowered, mapping->block_size, usage);
	if (!block->buffer) {
		return;
	}
	VmaAllocationInfo allocation_info;
	vmaGetAllocationInfo(ctx->vma_allocator, block->buffer->vma_allocation, &allocation_info);
	memcpy(allocation_info.pMappedData, block->bytes, block->size);
	vmaFlushAllocation(ctx->vma_allocator, block->buffer->vma_allocation, 0, block->size);
	struct rtvk_bound_descriptor* descriptor = rtvk_lower_add_descriptor(state, command->address);
	if (!descriptor) {
		return;
	}
	descriptor->buffer = NULL;
	descriptor->vk_buffer = block->buffer->vk_buffer;
	descriptor->texture = (struct rtvk_ir_texture){ 0 };
	descriptor->sampler = (struct rtvk_ir_sampler){ 0 };
	descriptor->offset = 0;
	descriptor->size = block->size;
	state->descriptors_dirty = true;
}

void rtvk_lower_bind_buffer(struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state, const struct rtvk_ir_buffer* command) {
	if (!command->buffer) {
		return;
	}
	/* Rutile allows primary command buffers to bind resources before executing a
	 * rendering continuation that selects the program. Keep the binding by its
	 * location and validate its descriptor kind once that program is active. */
	struct rtvk_bound_descriptor* descriptor = rtvk_lower_add_descriptor(
		state,
		command->address
	);
	if (!descriptor) {
		return;
	}
	descriptor->buffer = command->buffer;
	descriptor->vk_buffer = command->buffer ? command->buffer->vk_buffer : VK_NULL_HANDLE;
	descriptor->texture = (struct rtvk_ir_texture){ 0 };
	descriptor->sampler = (struct rtvk_ir_sampler){ 0 };
	descriptor->offset = command->offset;
	descriptor->size = command->size;
	state->descriptors_dirty = true;
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(command->buffer));
}

void rtvk_lower_bind_texture(struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state, struct rtvk_ir_texture* command) {
	if (!command->view || !command->image || !command->vk_image_view) {
		return;
	}
	struct rtvk_bound_descriptor* descriptor = rtvk_lower_add_descriptor(
		state,
		command->address
	);
	if (!descriptor) {
		return;
	}
	if (command->owns_image_view && !rtvk_lowered_command_buffer_add_image_view(lowered, command->vk_image_view)) {
		return;
	}
	command->owns_image_view = false;
	descriptor->texture = *command;
	descriptor->buffer = NULL;
	descriptor->vk_buffer = VK_NULL_HANDLE;
	descriptor->offset = 0;
	descriptor->size = 0;
	state->descriptors_dirty = true;
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(command->view));
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(command->image));
}

void rtvk_lower_bind_sampler(struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state, const struct rtvk_ir_sampler* command) {
	if (!command->sampler || !command->vk_sampler) return;
	struct rtvk_bound_descriptor* descriptor = rtvk_lower_add_descriptor(state, command->address);
	if (!descriptor) return;
	descriptor->sampler = *command;
	descriptor->buffer = NULL;
	descriptor->vk_buffer = VK_NULL_HANDLE;
	descriptor->offset = 0;
	descriptor->size = 0;
	state->descriptors_dirty = true;
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(command->sampler));
}

void rtvk_lower_vertex_buffer(struct rtvk_context* ctx, struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state, const struct rtvk_ir_vertex_buffer* command) {
	struct rtvk_buffer* buffer = command->buffer;
	if (!buffer || !state->program || !state->program->input_mapping_occupied[command->address]) {
		return;
	}
	struct rtvk_program_input_mapping* mapping = &state->program->input_mappings[command->address];
	const u32 binding = mapping->binding;
	rtvk_lower_reserve_vertex_buffers(state, binding + 1);
	if (rtvk_error() != RT_SUCCESS) {
		return;
	}
	state->vertex_buffers[binding] = buffer;
	state->vertex_offsets[binding] = command->range.offset;
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(command->buffer));
	vkCmdBindVertexBuffers(lowered->vk_command_buffer, binding, 1, &buffer->vk_buffer, &state->vertex_offsets[binding]);
}

void rtvk_lower_index_buffer(struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state, const struct rtvk_ir_index_buffer* command) {
	struct rtvk_buffer* buffer = command->buffer;
	if (!buffer) {
		return;
	}
	state->index_buffer = buffer;
	state->index_offset = command->range.offset;
	state->index_format = command->format;
	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(command->buffer));
	VkIndexType type = command->format == RT_INDEX_U16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
	vkCmdBindIndexBuffer(lowered->vk_command_buffer, buffer->vk_buffer, command->range.offset, type);
}

void rtvk_lower_draw(struct rtvk_context* ctx, struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state, const struct rtvk_ir_draw* command) {
	if (!state->rendering || !state->framebuffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "draw requires an active rendering scope");
		return;
	}
	if (!state->program || state->program->compute_program) {
		rtvk_throwf(RT_IMPROPER_USAGE, "draw requires a bound graphics program");
		return;
	}
	if (!state->viewport_set || !state->scissor_set) {
		rtvk_throwf(RT_IMPROPER_USAGE, "draw requires a viewport and scissor");
		return;
	}
	for (usize input_index = 0; input_index < state->program->vertex_layout.input_count; input_index++) {
		if (input_index >= state->vertex_buffer_capacity || !state->vertex_buffers[input_index]) {
			rtvk_throwf(RT_IMPROPER_USAGE, "draw is missing vertex input %zu", input_index);
			return;
		}
	}
	rtvk_lower_bind_descriptors(ctx, lowered, state);
	if (rtvk_error() != RT_SUCCESS) {
		return;
	}
	vkCmdDraw(lowered->vk_command_buffer, command->count, 1, command->first, 0);
}

void rtvk_lower_draw_instanced(struct rtvk_context* ctx, struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state, const struct rtvk_ir_draw_instanced* command) {
	rtvk_lower_bind_descriptors(ctx, lowered, state);
	if (rtvk_error() != RT_SUCCESS) {
		return;
	}
	vkCmdDraw(lowered->vk_command_buffer, command->count, command->instances, command->first, command->first_instance);
}

void rtvk_lower_draw_indexed(struct rtvk_context* ctx, struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state, const struct rtvk_ir_draw_indexed* command) {
	rtvk_lower_bind_descriptors(ctx, lowered, state);
	if (rtvk_error() != RT_SUCCESS) {
		return;
	}
	vkCmdDrawIndexed(lowered->vk_command_buffer, command->count, 1, command->first, command->vertex_offset, 0);
}

void rtvk_lower_draw_indexed_instanced(struct rtvk_context* ctx, struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state, const struct rtvk_ir_draw_indexed_instanced* command) {
	rtvk_lower_bind_descriptors(ctx, lowered, state);
	if (rtvk_error() != RT_SUCCESS) {
		return;
	}
	vkCmdDrawIndexed(lowered->vk_command_buffer, command->count, command->instances, command->first, command->vertex_offset, command->first_instance);
}

void rtvk_lower_dispatch(struct rtvk_context* ctx, struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state, const struct rtvk_ir_dispatch* command) {
	rtvk_lower_bind_descriptors(ctx, lowered, state);
	if (rtvk_error() != RT_SUCCESS) return;
	vkCmdDispatch(lowered->vk_command_buffer, command->group_count_x, command->group_count_y, command->group_count_z);
}

void rtvk_command_buffer_lower_commands(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state);

void rtvk_lower_execute(struct rtvk_context* ctx, struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state, const struct rtvk_ir_execute* command) {
	if (!command->command_buffer || !command->command_buffer->executable || !command->command_buffer->continuation) {
		rtvk_throwf(RT_IMPROPER_USAGE, "rtCmdExecute references an invalid continuation command buffer");
		return;
	}
	if (command->command_buffer->lowering) {
		rtvk_throwf(RT_IMPROPER_USAGE, "rtCmdExecute encountered a command-buffer execution cycle");
		return;
	}

	rtvk_lowered_command_buffer_add_resource_job(lowered, RTVK_RESOURCE_BASE(command->command_buffer));
	command->command_buffer->lowering = true;
	rtvk_command_buffer_lower_commands(ctx, command->command_buffer, lowered, state);
	command->command_buffer->lowering = false;
}

void rtvk_command_buffer_lower_commands(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, struct rtvk_lowered_command_buffer* lowered, struct rtvk_lower_state* state) {
	for (usize offset = 0; offset < command_buffer->ir_size; (void)0) {
		struct rtvk_command_header* header = (struct rtvk_command_header*)(command_buffer->ir_data + offset);
		void* payload = header + 1;
		switch ((rtvk_command_opcode)header->opcode) {
		case RTVK_COMMAND_BUFFER_DATA:
			rtvk_lower_buffer_data(ctx, lowered, payload);
			break;
		case RTVK_COMMAND_BUFFER_COPY:
			rtvk_lower_buffer_copy(lowered, payload);
			break;
		case RTVK_COMMAND_BUFFER_COPY_TO_TEXTURE:
			rtvk_lower_buffer_copy_to_texture(lowered, payload);
			break;
		case RTVK_COMMAND_BUFFER_BARRIER:
			rtvk_lower_buffer_barrier(lowered, payload);
			break;
		case RTVK_COMMAND_TEXTURE_COPY:
			rtvk_lower_texture_copy(lowered, payload);
			break;
		case RTVK_COMMAND_TEXTURE_DATA:
			rtvk_lower_texture_data(ctx, lowered, payload);
			break;
		case RTVK_COMMAND_TEXTURE_COPY_TO_BUFFER:
			rtvk_lower_texture_copy_to_buffer(lowered, payload);
			break;
		case RTVK_COMMAND_TEXTURE_BARRIER:
			rtvk_lower_texture_barrier(lowered, payload);
			break;
		case RTVK_COMMAND_EXECUTE:
			rtvk_lower_execute(ctx, lowered, state, payload);
			break;
		case RTVK_COMMAND_BEGIN_RENDERING:
			rtvk_lower_begin_rendering_command(lowered, state, payload);
			break;
		case RTVK_COMMAND_CLEAR_COLOR:
			rtvk_lower_clear_color(lowered, state, payload);
			break;
		case RTVK_COMMAND_CLEAR_DEPTH:
			rtvk_lower_clear_depth(lowered, state, payload);
			break;
		case RTVK_COMMAND_CLEAR_STENCIL:
			rtvk_lower_clear_stencil(lowered, state, payload);
			break;
		case RTVK_COMMAND_CLEAR:
			rtvk_lower_clear(lowered, state, payload);
			break;
		case RTVK_COMMAND_SET_VIEWPORT:
			rtvk_lower_set_viewport(lowered, state, payload);
			break;
		case RTVK_COMMAND_SET_SCISSOR:
			rtvk_lower_set_scissor(lowered, state, payload);
			break;
		case RTVK_COMMAND_END_RENDERING:
			rtvk_lower_end_rendering(lowered, state);
			break;
		case RTVK_COMMAND_USE_PROGRAM:
			rtvk_lower_use_program(ctx, lowered, state, payload);
			break;
		case RTVK_COMMAND_UNIFORM_DATA:
			rtvk_lower_program_data(ctx, lowered, state, payload, RTVK_DATA_UNIFORM);
			break;
		case RTVK_COMMAND_STORAGE_DATA:
			rtvk_lower_program_data(ctx, lowered, state, payload, RTVK_DATA_STORAGE);
			break;
		case RTVK_COMMAND_BIND_BUFFER:
			rtvk_lower_bind_buffer(lowered, state, payload);
			break;
		case RTVK_COMMAND_BIND_TEXTURE:
			rtvk_lower_bind_texture(lowered, state, payload);
			break;
		case RTVK_COMMAND_BIND_SAMPLER:
			rtvk_lower_bind_sampler(lowered, state, payload);
			break;
		case RTVK_COMMAND_VERTEX_BUFFER:
			rtvk_lower_vertex_buffer(ctx, lowered, state, payload);
			break;
		case RTVK_COMMAND_INDEX_BUFFER:
			rtvk_lower_index_buffer(lowered, state, payload);
			break;
		case RTVK_COMMAND_DRAW:
			rtvk_lower_draw(ctx, lowered, state, payload);
			break;
		case RTVK_COMMAND_DRAW_INSTANCED:
			rtvk_lower_draw_instanced(ctx, lowered, state, payload);
			break;
		case RTVK_COMMAND_DRAW_INDEXED:
			rtvk_lower_draw_indexed(ctx, lowered, state, payload);
			break;
		case RTVK_COMMAND_DRAW_INDEXED_INSTANCED:
			rtvk_lower_draw_indexed_instanced(ctx, lowered, state, payload);
			break;
		case RTVK_COMMAND_DISPATCH:
			rtvk_lower_dispatch(ctx, lowered, state, payload);
			break;
		}
		if (rtvk_error() != RT_SUCCESS) {
			return;
		}
		offset += rtvk_command_record_size((rtvk_command_opcode)header->opcode);
	}

	return;
}

void rtvk_command_buffer_lower(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, struct rtvk_lowered_command_buffer* lowered) {
	struct rtvk_lower_state state = { 0 };
	if (!command_buffer || command_buffer->lowering) {
		rtvk_throwf(RT_IMPROPER_USAGE, "command buffer lowering encountered an execution cycle");
		return;
	}

	command_buffer->lowering = true;
	rtvk_command_buffer_lower_commands(ctx, command_buffer, lowered, &state);
	command_buffer->lowering = false;
	if (rtvk_error() != RT_SUCCESS) {
		rtvk_lower_state_finish(&state);
		return;
	}

	rtvk_lowered_command_buffer_end(lowered);
	if (rtvk_error() != RT_SUCCESS) {
		rtvk_lower_state_finish(&state);
		return;
	}
	rtvk_lower_state_finish(&state);
}

void rtvk_lowered_command_buffer_destroy(struct rtvk_context* ctx, struct rtvk_lowered_command_buffer* lowered) {
	if (!lowered) {
		return;
	}
	while (lowered->staging_buffers) {
		struct rtvk_lowered_staging_buffer* staging = lowered->staging_buffers;
		lowered->staging_buffers = staging->next;
		vmaDestroyBuffer(ctx->vma_allocator, staging->vk_buffer, staging->vma_allocation);
		free(staging);
	}
	while (lowered->image_views) {
		struct rtvk_lowered_image_view* image_view = lowered->image_views;
		lowered->image_views = image_view->next;
		vkDestroyImageView(ctx->vk_device, image_view->vk_image_view, VK_ALLOCATOR);
		free(image_view);
	}
	vkDestroyDescriptorPool(ctx->vk_device, lowered->vk_descriptor_pool, VK_ALLOCATOR);
	vkDestroyCommandPool(ctx->vk_device, lowered->vk_command_pool, VK_ALLOCATOR);
	rtvk_lowered_command_buffer_release_resource_jobs(lowered);
	free(lowered);
}
