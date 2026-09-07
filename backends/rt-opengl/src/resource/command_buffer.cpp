#include "resource/command_buffer.h"

#include "context.h"
#include "error.h"
#include "execution/execution.h"
#include "execution/internal.hpp"
#include "glad/gl.h"
#include "resource/queue.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

struct rtgl_program_data_block {
	struct rtgl_program* program;
	rtgl_location_kind kind;
	u32 binding;
	GLuint buffer;
	std::vector<u08> bytes;
};

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rtgl_recorded_command* rtgl_command_buffer_append(struct rtgl_command_buffer* command_buffer) {
	if (!command_buffer || !command_buffer->recording) {
		return NULL;
	}
	if (command_buffer->command_count == command_buffer->command_capacity) {
		u32 next_capacity = command_buffer->command_capacity ? command_buffer->command_capacity * 2 : 16;
		rtgl_recorded_command* next = (rtgl_recorded_command*)realloc(
			command_buffer->commands,
			sizeof(*command_buffer->commands) * (usize)next_capacity
		);
		if (!next) {
			RTGL_CHECK_ALLOC(next, sizeof(*command_buffer->commands) * (usize)next_capacity, "OpenGL recorded commands");
			return NULL;
		}
		command_buffer->commands = next;
		command_buffer->command_capacity = next_capacity;
	}
	rtgl_recorded_command* command = &command_buffer->commands[command_buffer->command_count++];
	memset(command, 0, sizeof(*command));
	command->size = sizeof(*command);
	return command;
}

void rtgl_command_buffer_release_command(struct rtgl_command_buffer* command_buffer, rtgl_recorded_command* command) {
	switch (command->kind) {
	case RTGL_RECORDED_COMMAND_BEGIN_RENDERING:
		rtgl_release_resource(command->data.begin_rendering.framebuffer);
		for (u32 color = 0; color < RTGL_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS; ++color) {
			rtgl_texture_image_release(command_buffer->base.ctx, command->data.begin_rendering.color_images[color]);
			rtgl_texture_image_release(command_buffer->base.ctx, command->data.begin_rendering.color_copy_sources[color]);
		}
		rtgl_texture_image_release(command_buffer->base.ctx, command->data.begin_rendering.depth_image);
		rtgl_texture_image_release(command_buffer->base.ctx, command->data.begin_rendering.depth_copy_source);
		rtgl_texture_image_release(command_buffer->base.ctx, command->data.begin_rendering.stencil_image);
		rtgl_texture_image_release(command_buffer->base.ctx, command->data.begin_rendering.stencil_copy_source);
		break;
	case RTGL_RECORDED_COMMAND_USE_PROGRAM:
		rtgl_release_resource(command->data.use_program.program);
		break;
	case RTGL_RECORDED_COMMAND_UNIFORM_DATA:
	case RTGL_RECORDED_COMMAND_STORAGE_DATA:
		free(command->data.program_data.bytes);
		break;
	case RTGL_RECORDED_COMMAND_BIND_BUFFER:
		rtgl_buffer_storage_release(command->data.bind_buffer.storage);
		break;
	case RTGL_RECORDED_COMMAND_BIND_TEXTURE:
		rtgl_release_resource(command->data.bind_texture.texture_view);
		rtgl_texture_image_release(command_buffer->base.ctx, command->data.bind_texture.image);
		break;
	case RTGL_RECORDED_COMMAND_VERTEX_BUFFER:
		rtgl_buffer_storage_release(command->data.vertex_buffer.storage);
		break;
	case RTGL_RECORDED_COMMAND_INDEX_BUFFER:
		rtgl_buffer_storage_release(command->data.index_buffer.storage);
		break;
	case RTGL_RECORDED_COMMAND_BUFFER_DATA:
		rtgl_buffer_storage_release(command->data.buffer_data.storage);
		rtgl_buffer_storage_release(command->data.buffer_data.copy_source);
		free(command->data.buffer_data.data);
		break;
	case RTGL_RECORDED_COMMAND_BUFFER_COPY:
		rtgl_buffer_storage_release(command->data.buffer_copy.src);
		rtgl_buffer_storage_release(command->data.buffer_copy.dst);
		rtgl_buffer_storage_release(command->data.buffer_copy.dst_copy_source);
		break;
	case RTGL_RECORDED_COMMAND_BUFFER_COPY_TO_TEXTURE:
		rtgl_buffer_storage_release(command->data.buffer_copy_to_texture.src);
		rtgl_texture_image_release(command_buffer->base.ctx, command->data.buffer_copy_to_texture.dst);
		rtgl_texture_image_release(command_buffer->base.ctx, command->data.buffer_copy_to_texture.copy_source);
		break;
	case RTGL_RECORDED_COMMAND_TEXTURE_DATA:
		rtgl_texture_image_release(command_buffer->base.ctx, command->data.texture_data.image);
		rtgl_texture_image_release(command_buffer->base.ctx, command->data.texture_data.copy_source);
		free(command->data.texture_data.data);
		break;
	case RTGL_RECORDED_COMMAND_TEXTURE_COPY:
		rtgl_texture_image_release(command_buffer->base.ctx, command->data.texture_copy.src);
		rtgl_texture_image_release(command_buffer->base.ctx, command->data.texture_copy.dst);
		rtgl_texture_image_release(command_buffer->base.ctx, command->data.texture_copy.dst_copy_source);
		break;
	case RTGL_RECORDED_COMMAND_TEXTURE_COPY_TO_BUFFER:
		rtgl_texture_image_release(command_buffer->base.ctx, command->data.texture_copy_to_buffer.src);
		rtgl_buffer_storage_release(command->data.texture_copy_to_buffer.dst);
		rtgl_buffer_storage_release(command->data.texture_copy_to_buffer.dst_copy_source);
		break;
	default:
		break;
	}
}

void rtgl_command_buffer_clear_commands(struct rtgl_command_buffer* command_buffer) {
	for (u32 i = 0; i < command_buffer->command_count; i++) {
		rtgl_command_buffer_release_command(command_buffer, &command_buffer->commands[i]);
	}
	command_buffer->command_count = 0;
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_command_buffer rtCommandBufferCreate(void) {
	rtgl_begin_errorable_operation();
	return rtgl_command_buffer_to_handle(rtgl_command_buffer_create(rtgl_get_current_context()));
}

void rtCommandBufferDestroy(rt_command_buffer command_buffer) {
	rtgl_command_buffer_destroy(rtgl_get_current_context(), rtgl_command_buffer_from_handle(command_buffer));
}

void rtgl_command_buffer_reset(struct rtgl_command_buffer* command_buffer) {
	if (!command_buffer) {
		return;
	}
	rtgl_command_buffer_clear_commands(command_buffer);
	command_buffer->recording = false;
	command_buffer->executable = false;
	command_buffer->continuation = false;
	command_buffer->rendering_continuation = false;
	command_buffer->rendering = false;
	memset(command_buffer->clear_colors, 0, sizeof(command_buffer->clear_colors));
	command_buffer->clear_depth = 1.0f;
	command_buffer->clear_stencil = 0;
}

void rtgl_command_buffer_begin_mode(struct rtgl_command_buffer* command_buffer, bool continuation, bool rendering_continuation) {
	if (!command_buffer) {
		return;
	}
	rtgl_command_buffer_clear_commands(command_buffer);
	command_buffer->recording = true;
	command_buffer->executable = false;
	command_buffer->continuation = continuation;
	command_buffer->rendering_continuation = rendering_continuation;
	command_buffer->rendering = false;
}

void rtgl_command_buffer_begin(struct rtgl_command_buffer* command_buffer) {
	rtgl_command_buffer_begin_mode(command_buffer, false, false);
}

void rtgl_command_buffer_continue(struct rtgl_command_buffer* command_buffer, bool rendering) {
	rtgl_command_buffer_begin_mode(command_buffer, true, rendering);
}

void rtgl_command_buffer_begin_rendering(struct rtgl_command_buffer* command_buffer, struct rtgl_framebuffer* framebuffer) {
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_BEGIN_RENDERING;
	command->data.begin_rendering.framebuffer = framebuffer;
	rtgl_retain_resource(framebuffer);
	for (u32 color = 0; framebuffer && color < framebuffer->color_texture_count; ++color) {
		struct rtgl_texture_view* view = framebuffer->color_views[color];
		struct rtgl_image_base* copy_source = NULL;
		struct rtgl_image_base* image = view && view->texture
			? rtgl_texture_prepare_write(command_buffer->base.ctx, view->texture, &copy_source)
			: view ? view->image : NULL;
		command->data.begin_rendering.color_images[color] = image;
		command->data.begin_rendering.color_copy_sources[color] = copy_source;
		rtgl_texture_image_retain(image);
	}
	struct rtgl_texture_view* depth_view = framebuffer ? framebuffer->depth_view : NULL;
	struct rtgl_image_base* depth_copy_source = NULL;
	command->data.begin_rendering.depth_image = depth_view && depth_view->texture
		? rtgl_texture_prepare_write(command_buffer->base.ctx, depth_view->texture, &depth_copy_source)
		: depth_view ? depth_view->image : NULL;
	command->data.begin_rendering.depth_copy_source = depth_copy_source;
	rtgl_texture_image_retain(command->data.begin_rendering.depth_image);
	struct rtgl_texture_view* stencil_view = framebuffer ? framebuffer->stencil_view : NULL;
	if (depth_view && stencil_view && depth_view->texture && stencil_view->texture == depth_view->texture) {
		command->data.begin_rendering.stencil_image = command->data.begin_rendering.depth_image;
		command->data.begin_rendering.stencil_copy_source = command->data.begin_rendering.depth_copy_source;
		rtgl_texture_image_retain(command->data.begin_rendering.stencil_image);
		rtgl_texture_image_retain(command->data.begin_rendering.stencil_copy_source);
	} else {
		struct rtgl_image_base* stencil_copy_source = NULL;
		command->data.begin_rendering.stencil_image = stencil_view && stencil_view->texture
			? rtgl_texture_prepare_write(command_buffer->base.ctx, stencil_view->texture, &stencil_copy_source)
			: stencil_view ? stencil_view->image : NULL;
		command->data.begin_rendering.stencil_copy_source = stencil_copy_source;
		rtgl_texture_image_retain(command->data.begin_rendering.stencil_image);
	}
	command_buffer->rendering = true;
}

void rtgl_command_buffer_clear_color(struct rtgl_command_buffer* command_buffer, struct rt_location_t* location, f32 r, f32 g, f32 b, f32 a) {
	struct rtgl_program* program = rtgl_location_program(location);
	struct rtgl_program_mapping* mapping = rtgl_program_mapping(program, location);
	if (!command_buffer) {
		return;
	}
	/* A null location addresses the primary color attachment, consistently with
	 * the Vulkan and D3D12 backends. */
	const usize color_index = mapping ? mapping->binding : 0;
	if (mapping && (mapping->kind != RTGL_LOCATION_MAPPING_OUTPUT || color_index >= RTGL_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS)) {
		return;
	}
	command_buffer->clear_colors[color_index][0] = r;
	command_buffer->clear_colors[color_index][1] = g;
	command_buffer->clear_colors[color_index][2] = b;
	command_buffer->clear_colors[color_index][3] = a;
}

void rtgl_command_buffer_clear_depth(struct rtgl_command_buffer* command_buffer, f32 depth) {
	if (command_buffer) {
		command_buffer->clear_depth = depth;
	}
}

void rtgl_command_buffer_clear_stencil(struct rtgl_command_buffer* command_buffer, usize stencil) {
	if (command_buffer) {
		command_buffer->clear_stencil = stencil;
	}
}

void rtgl_command_buffer_clear(struct rtgl_command_buffer* command_buffer, enum rt_clear_flag attachments) {
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_CLEAR;
	command->data.clear.attachments = attachments;
	memcpy(command->data.clear.colors, command_buffer->clear_colors, sizeof(command->data.clear.colors));
	command->data.clear.depth = command_buffer->clear_depth;
	command->data.clear.stencil = command_buffer->clear_stencil;
}

void rtgl_command_buffer_set_viewport(struct rtgl_command_buffer* command_buffer, usize x, usize y, usize width, usize height, f32 min_depth, f32 max_depth) {
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_SET_VIEWPORT;
	command->data.set_viewport = { x, y, width, height, min_depth, max_depth };
}

void rtgl_command_buffer_use_program(struct rtgl_command_buffer* command_buffer, struct rtgl_program* program) {
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_USE_PROGRAM;
	command->data.use_program.program = program;
	rtgl_retain_resource(command->data.use_program.program);
}

void rtgl_command_buffer_set_scissor(struct rtgl_command_buffer* command_buffer, usize x, usize y, usize width, usize height) {
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_SET_SCISSOR;
	command->data.set_scissor = { x, y, width, height };
}

void rtgl_command_buffer_bind_buffer(struct rtgl_command_buffer* command_buffer, struct rt_location_t* location, struct rtgl_buffer* buffer, rt_buffer_range range) {
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_BIND_BUFFER;
	command->data.bind_buffer.address = location ? location->address : 0;
	command->data.bind_buffer.storage = buffer ? buffer->storage : NULL;
	command->data.bind_buffer.offset = range.offset;
	command->data.bind_buffer.size = range.size;
	rtgl_buffer_storage_retain(command->data.bind_buffer.storage);
}

void rtgl_command_buffer_bind_texture(struct rtgl_command_buffer* command_buffer, struct rt_location_t* location, struct rtgl_texture_view* texture_view) {
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_BIND_TEXTURE;
	command->data.bind_texture.address = location ? location->address : 0;
	command->data.bind_texture.texture_view = texture_view;
	command->data.bind_texture.image = texture_view ? texture_view->image : NULL;
	command->data.bind_texture.mag_filter = texture_view ? texture_view->mag_filter : RT_FILTER_LINEAR;
	command->data.bind_texture.min_filter = texture_view ? texture_view->min_filter : RT_FILTER_LINEAR;
	command->data.bind_texture.mip_filter = texture_view ? texture_view->mip_filter : RT_MIP_FILTER_NONE;
	command->data.bind_texture.address_u = texture_view ? texture_view->address_u : RT_ADDRESS_REPEAT;
	command->data.bind_texture.address_v = texture_view ? texture_view->address_v : RT_ADDRESS_REPEAT;
	command->data.bind_texture.address_w = texture_view ? texture_view->address_w : RT_ADDRESS_REPEAT;
	command->data.bind_texture.max_anisotropy = texture_view ? texture_view->max_anisotropy : 1;
	command->data.bind_texture.min_lod = texture_view ? texture_view->min_lod : 0.0f;
	command->data.bind_texture.max_lod = texture_view ? texture_view->max_lod : 1000.0f;
	command->data.bind_texture.lod_bias = texture_view ? texture_view->lod_bias : 0.0f;
	rtgl_retain_resource(command->data.bind_texture.texture_view);
	rtgl_texture_image_retain(command->data.bind_texture.image);
}

void rtgl_command_buffer_vertex_buffer(struct rtgl_command_buffer* command_buffer, struct rt_location_t* location, struct rtgl_buffer* buffer, rt_buffer_range range) {
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_VERTEX_BUFFER;
	command->data.vertex_buffer.address = location ? location->address : 0;
	command->data.vertex_buffer.storage = buffer ? buffer->storage : NULL;
	command->data.vertex_buffer.offset = range.offset;
	command->data.vertex_buffer.size = range.size;
	rtgl_buffer_storage_retain(command->data.vertex_buffer.storage);
}

void rtgl_command_buffer_index_buffer(struct rtgl_command_buffer* command_buffer, struct rtgl_buffer* buffer, rt_buffer_range range, enum rt_index_format format) {
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_INDEX_BUFFER;
	command->data.index_buffer.storage = buffer ? buffer->storage : NULL;
	command->data.index_buffer.offset = range.offset;
	command->data.index_buffer.size = range.size;
	command->data.index_buffer.format = format;
	rtgl_buffer_storage_retain(command->data.index_buffer.storage);
}

void rtgl_command_buffer_draw(struct rtgl_command_buffer* command_buffer, usize vertex_count, usize first_vertex) {
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_DRAW;
	command->data.draw = { vertex_count, first_vertex };
}

void rtgl_command_buffer_draw_instanced(struct rtgl_command_buffer* command_buffer, usize vertex_count, usize instance_count, usize first_vertex, usize first_instance) {
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_DRAW_INSTANCED;
	command->data.draw_instanced = { vertex_count, instance_count, first_vertex, first_instance };
}

void rtgl_command_buffer_draw_indexed(struct rtgl_command_buffer* command_buffer, usize index_count, usize first_index, usize vertex_offset) {
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_DRAW_INDEXED;
	command->data.draw_indexed = { index_count, first_index, vertex_offset };
}

void rtgl_command_buffer_draw_indexed_instanced(struct rtgl_command_buffer* command_buffer, usize index_count, usize instance_count, usize first_index, usize vertex_offset, usize first_instance) {
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_DRAW_INDEXED_INSTANCED;
	command->data.draw_indexed_instanced = { index_count, instance_count, first_index, vertex_offset, first_instance };
}

void rtgl_command_buffer_end_rendering(struct rtgl_command_buffer* command_buffer) {
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_END_RENDERING;
	command_buffer->rendering = false;
}

static bool rtgl_texture_range_level(const struct rtgl_image_base* image, rt_texture_range range, usize level, rt_texture_range* out) {
	if (!image || !out || !range.extent.width || !range.extent.height || !range.extent.depth || !range.mip_count || !range.layer_count || level >= range.mip_count || range.base_mip + level >= image->mip_levels)
		return false;
	enum rt_texture_aspect_flag supported_aspects = RT_TEXTURE_ASPECT_COLOR;
	if (image->format == RT_D16_UNORM || image->format == RT_D32_SFLOAT)
		supported_aspects = RT_TEXTURE_ASPECT_DEPTH;
	if (image->format == RT_S8_UINT)
		supported_aspects = RT_TEXTURE_ASPECT_STENCIL;
	if (image->format == RT_D24_UNORM_S8_UINT || image->format == RT_D32_SFLOAT_S8_UINT)
		supported_aspects = (enum rt_texture_aspect_flag)(RT_TEXTURE_ASPECT_DEPTH | RT_TEXTURE_ASPECT_STENCIL);
	if (!range.aspects || (range.aspects & ~supported_aspects))
		return false;
	*out = range;
	out->base_mip = range.base_mip + level;
	out->mip_count = 1;
	out->offset.width = range.offset.width >> level;
	out->offset.height = range.offset.height >> level;
	out->offset.depth = range.offset.depth >> level;
	out->extent.width = range.extent.width >> level;
	out->extent.height = range.extent.height >> level;
	out->extent.depth = range.extent.depth >> level;
	if (!out->extent.width)
		out->extent.width = 1;
	if (!out->extent.height)
		out->extent.height = 1;
	if (!out->extent.depth)
		out->extent.depth = 1;
	const usize mip_width = image->width >> out->base_mip ? image->width >> out->base_mip : 1;
	const usize mip_height = image->height >> out->base_mip ? image->height >> out->base_mip : 1;
	const usize mip_depth = image->depth >> out->base_mip ? image->depth >> out->base_mip : 1;
	const bool array = image->type == RT_TEXTURE_1D_ARRAY || image->type == RT_TEXTURE_2D_ARRAY;
	if ((array && (range.base_layer >= image->depth || range.layer_count > image->depth - range.base_layer)) || (!array && (range.base_layer || range.layer_count != 1)))
		return false;
	if ((image->type == RT_TEXTURE_1D || image->type == RT_TEXTURE_1D_ARRAY) && (out->offset.height || out->offset.depth || out->extent.height != 1 || out->extent.depth != 1))
		return false;
	if ((image->type == RT_TEXTURE_2D || image->type == RT_TEXTURE_2D_ARRAY) && (out->offset.depth || out->extent.depth != 1))
		return false;
	if (out->offset.width > mip_width || out->extent.width > mip_width - out->offset.width)
		return false;
	if ((image->type != RT_TEXTURE_1D && image->type != RT_TEXTURE_1D_ARRAY) && (out->offset.height > mip_height || out->extent.height > mip_height - out->offset.height))
		return false;
	if (image->type == RT_TEXTURE_3D && (out->offset.depth > mip_depth || out->extent.depth > mip_depth - out->offset.depth))
		return false;
	return true;
}

static usize rtgl_texture_range_byte_count_image(const struct rtgl_image_base* image, rt_texture_range range) {
	if (!image)
		return 0;
	const usize bytes_per_texel = rtgl_texture_format_aspect_size(image->format, range.aspects);
	if (!bytes_per_texel)
		return 0;
	usize total = 0;
	for (usize level = 0; level < range.mip_count; level++) {
		rt_texture_range mip;
		if (!rtgl_texture_range_level(image, range, level, &mip))
			return 0;
		const usize layers = image->type == RT_TEXTURE_1D_ARRAY || image->type == RT_TEXTURE_2D_ARRAY ? mip.layer_count : 1;
		total += mip.extent.width * mip.extent.height * mip.extent.depth * layers * bytes_per_texel;
	}
	return total;
}

static usize rtgl_texture_range_byte_count(const struct rtgl_texture* texture, rt_texture_range range) {
	return texture ? rtgl_texture_range_byte_count_image(texture->image, range) : 0;
}

void rtgl_command_buffer_buffer_data(struct rtgl_command_buffer* command_buffer, struct rtgl_buffer* buffer, rt_buffer_range range, const u08* data) {
	if (!buffer || !buffer->storage || !data || !range.size || range.offset > buffer->storage->size || range.size > buffer->storage->size - range.offset) {
		return;
	}
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	command->data.buffer_data.data = (u08*)malloc(range.size);
	if (!command->data.buffer_data.data) {
		command_buffer->command_count--;
		return;
	}
	memcpy(command->data.buffer_data.data, data, range.size);
	struct rtgl_buffer_storage* previous = buffer->storage;
	command->kind = RTGL_RECORDED_COMMAND_BUFFER_DATA;
	command->data.buffer_data.range = range;
	command->data.buffer_data.storage = rtgl_buffer_prepare_write(command_buffer->base.ctx, buffer);
	if (!command->data.buffer_data.storage) {
		free(command->data.buffer_data.data);
		command_buffer->command_count--;
		return;
	}
	rtgl_buffer_storage_retain(command->data.buffer_data.storage);
	if (command->data.buffer_data.storage != previous) {
		command->data.buffer_data.copy_source = previous;
		rtgl_buffer_storage_retain(previous);
	}
}

void rtgl_command_buffer_buffer_copy(struct rtgl_command_buffer* command_buffer, struct rtgl_buffer* src, rt_buffer_range src_range, struct rtgl_buffer* dst, rt_buffer_range dst_range) {
	if (!src || !src->storage || !dst || !dst->storage || src_range.size != dst_range.size || !src_range.size || src_range.offset > src->storage->size || src_range.size > src->storage->size - src_range.offset || dst_range.offset > dst->storage->size || dst_range.size > dst->storage->size - dst_range.offset) {
		return;
	}
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	struct rtgl_buffer_storage* source = src->storage;
	rtgl_buffer_storage_retain(source);
	struct rtgl_buffer_storage* previous_target = dst->storage;
	struct rtgl_buffer_storage* target = rtgl_buffer_prepare_write(command_buffer->base.ctx, dst);
	if (!target) {
		rtgl_buffer_storage_release(source);
		command_buffer->command_count--;
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_BUFFER_COPY;
	command->data.buffer_copy.src = source;
	command->data.buffer_copy.src_range = src_range;
	command->data.buffer_copy.dst = target;
	command->data.buffer_copy.dst_copy_source = target != previous_target ? previous_target : NULL;
	command->data.buffer_copy.dst_range = dst_range;
	rtgl_buffer_storage_retain(target);
	rtgl_buffer_storage_retain(command->data.buffer_copy.dst_copy_source);
}

void rtgl_command_buffer_texture_data(struct rtgl_command_buffer* command_buffer, struct rtgl_texture* texture, rt_texture_range range, const u08* data) {
	const usize size = rtgl_texture_range_byte_count(texture, range);
	if (!size || !data) {
		return;
	}
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	command->data.texture_data.data = (u08*)malloc(size);
	if (!command->data.texture_data.data) {
		command_buffer->command_count--;
		return;
	}
	struct rtgl_image_base* copy_source = NULL;
	struct rtgl_image_base* image = rtgl_texture_prepare_write(command_buffer->base.ctx, texture, &copy_source);
	if (!image) {
		free(command->data.texture_data.data);
		command_buffer->command_count--;
		return;
	}
	memcpy(command->data.texture_data.data, data, size);
	command->kind = RTGL_RECORDED_COMMAND_TEXTURE_DATA;
	command->data.texture_data.image = image;
	command->data.texture_data.copy_source = copy_source;
	command->data.texture_data.range = range;
	rtgl_texture_image_retain(image);
}

void rtgl_command_buffer_buffer_copy_to_texture(struct rtgl_command_buffer* command_buffer, struct rtgl_buffer* src, rt_buffer_range src_range, struct rtgl_texture* dst, rt_texture_range dst_range) {
	const usize required_size = rtgl_texture_range_byte_count(dst, dst_range);
	if (!src || !src->storage || !dst || !required_size || src_range.size < required_size || src_range.offset > src->storage->size || src_range.size > src->storage->size - src_range.offset) {
		return;
	}
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	struct rtgl_image_base* copy_source = NULL;
	struct rtgl_image_base* target = rtgl_texture_prepare_write(command_buffer->base.ctx, dst, &copy_source);
	if (!target) {
		command_buffer->command_count--;
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_BUFFER_COPY_TO_TEXTURE;
	command->data.buffer_copy_to_texture.src = src->storage;
	command->data.buffer_copy_to_texture.src_range = src_range;
	command->data.buffer_copy_to_texture.dst = target;
	command->data.buffer_copy_to_texture.copy_source = copy_source;
	command->data.buffer_copy_to_texture.dst_range = dst_range;
	rtgl_buffer_storage_retain(src->storage);
	rtgl_texture_image_retain(target);
}

void rtgl_command_buffer_texture_copy(struct rtgl_command_buffer* command_buffer, struct rtgl_texture* src, rt_texture_range src_range, struct rtgl_texture* dst, rt_texture_range dst_range) {
	if (!src || !dst || !src->image || !dst->image || src->image->type != dst->image->type || src->image->format != dst->image->format || src_range.aspects != dst_range.aspects || src_range.extent.width != dst_range.extent.width || src_range.extent.height != dst_range.extent.height || src_range.extent.depth != dst_range.extent.depth || src_range.mip_count != dst_range.mip_count || src_range.layer_count != dst_range.layer_count || !rtgl_texture_range_byte_count(src, src_range) || !rtgl_texture_range_byte_count(dst, dst_range)) {
		return;
	}
	if (((src->image->format == RT_D24_UNORM_S8_UINT || src->image->format == RT_D32_SFLOAT_S8_UINT) && src_range.aspects != (enum rt_texture_aspect_flag)(RT_TEXTURE_ASPECT_DEPTH | RT_TEXTURE_ASPECT_STENCIL)) || ((dst->image->format == RT_D24_UNORM_S8_UINT || dst->image->format == RT_D32_SFLOAT_S8_UINT) && dst_range.aspects != (enum rt_texture_aspect_flag)(RT_TEXTURE_ASPECT_DEPTH | RT_TEXTURE_ASPECT_STENCIL))) {
		rtgl_throwf(RT_UNSUPPORTED_FEATURE, "OpenGL texture copies require both depth and stencil aspects for packed depth-stencil textures");
		return;
	}
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	struct rtgl_image_base* source = src->image;
	rtgl_texture_image_retain(source);
	struct rtgl_image_base* dst_copy_source = NULL;
	struct rtgl_image_base* target = rtgl_texture_prepare_write(command_buffer->base.ctx, dst, &dst_copy_source);
	if (!target) {
		rtgl_texture_image_release(command_buffer->base.ctx, source);
		command_buffer->command_count--;
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_TEXTURE_COPY;
	command->data.texture_copy.src = source;
	command->data.texture_copy.src_range = src_range;
	command->data.texture_copy.dst = target;
	command->data.texture_copy.dst_copy_source = dst_copy_source;
	command->data.texture_copy.dst_range = dst_range;
	rtgl_texture_image_retain(target);
}

void rtgl_command_buffer_texture_copy_to_buffer(struct rtgl_command_buffer* command_buffer, struct rtgl_texture* src, rt_texture_range src_range, struct rtgl_buffer* dst, rt_buffer_range dst_range) {
	const usize required_size = rtgl_texture_range_byte_count(src, src_range);
	if (!src || !dst || !dst->storage || !required_size || dst_range.size < required_size || dst_range.offset > dst->storage->size || dst_range.size > dst->storage->size - dst_range.offset) {
		return;
	}
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	struct rtgl_buffer_storage* previous_target = dst->storage;
	struct rtgl_buffer_storage* target = rtgl_buffer_prepare_write(command_buffer->base.ctx, dst);
	if (!target) {
		command_buffer->command_count--;
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_TEXTURE_COPY_TO_BUFFER;
	command->data.texture_copy_to_buffer.src = src->image;
	command->data.texture_copy_to_buffer.src_range = src_range;
	command->data.texture_copy_to_buffer.dst = target;
	command->data.texture_copy_to_buffer.dst_copy_source = target != previous_target ? previous_target : NULL;
	command->data.texture_copy_to_buffer.dst_range = dst_range;
	rtgl_texture_image_retain(command->data.texture_copy_to_buffer.src);
	rtgl_buffer_storage_retain(target);
	rtgl_buffer_storage_retain(command->data.texture_copy_to_buffer.dst_copy_source);
}

void rtgl_command_buffer_barrier(struct rtgl_command_buffer* command_buffer, rt_access src, rt_access dst, rtgl_recorded_command_kind kind) {
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	command->kind = kind;
	command->data.barrier.src = src;
	command->data.barrier.dst = dst;
}

void rtgl_command_buffer_end(struct rtgl_command_buffer* command_buffer) {
	if (!command_buffer) {
		return;
	}
	if (command_buffer->rendering) {
		return;
	}
	command_buffer->recording = false;
	command_buffer->executable = true;
}

void rtgl_command_buffer_retain_command(rtgl_recorded_command* command) {
	switch (command->kind) {
	case RTGL_RECORDED_COMMAND_BEGIN_RENDERING:
		rtgl_retain_resource(command->data.begin_rendering.framebuffer);
		for (u32 color = 0; color < RTGL_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS; ++color) {
			rtgl_texture_image_retain(command->data.begin_rendering.color_images[color]);
			rtgl_texture_image_retain(command->data.begin_rendering.color_copy_sources[color]);
		}
		rtgl_texture_image_retain(command->data.begin_rendering.depth_image);
		rtgl_texture_image_retain(command->data.begin_rendering.depth_copy_source);
		rtgl_texture_image_retain(command->data.begin_rendering.stencil_image);
		rtgl_texture_image_retain(command->data.begin_rendering.stencil_copy_source);
		break;
	case RTGL_RECORDED_COMMAND_USE_PROGRAM:
		rtgl_retain_resource(command->data.use_program.program);
		break;
	case RTGL_RECORDED_COMMAND_UNIFORM_DATA:
	case RTGL_RECORDED_COMMAND_STORAGE_DATA:
		break;
	case RTGL_RECORDED_COMMAND_BIND_BUFFER:
		rtgl_buffer_storage_retain(command->data.bind_buffer.storage);
		break;
	case RTGL_RECORDED_COMMAND_BIND_TEXTURE:
		rtgl_retain_resource(command->data.bind_texture.texture_view);
		rtgl_texture_image_retain(command->data.bind_texture.image);
		break;
	case RTGL_RECORDED_COMMAND_VERTEX_BUFFER:
		rtgl_buffer_storage_retain(command->data.vertex_buffer.storage);
		break;
	case RTGL_RECORDED_COMMAND_INDEX_BUFFER:
		rtgl_buffer_storage_retain(command->data.index_buffer.storage);
		break;
	case RTGL_RECORDED_COMMAND_BUFFER_DATA:
		rtgl_buffer_storage_retain(command->data.buffer_data.storage);
		rtgl_buffer_storage_retain(command->data.buffer_data.copy_source);
		break;
	case RTGL_RECORDED_COMMAND_BUFFER_COPY:
		rtgl_buffer_storage_retain(command->data.buffer_copy.src);
		rtgl_buffer_storage_retain(command->data.buffer_copy.dst);
		rtgl_buffer_storage_retain(command->data.buffer_copy.dst_copy_source);
		break;
	case RTGL_RECORDED_COMMAND_BUFFER_COPY_TO_TEXTURE:
		rtgl_buffer_storage_retain(command->data.buffer_copy_to_texture.src);
		rtgl_texture_image_retain(command->data.buffer_copy_to_texture.dst);
		rtgl_texture_image_retain(command->data.buffer_copy_to_texture.copy_source);
		break;
	case RTGL_RECORDED_COMMAND_TEXTURE_DATA:
		rtgl_texture_image_retain(command->data.texture_data.image);
		rtgl_texture_image_retain(command->data.texture_data.copy_source);
		break;
	case RTGL_RECORDED_COMMAND_TEXTURE_COPY:
		rtgl_texture_image_retain(command->data.texture_copy.src);
		rtgl_texture_image_retain(command->data.texture_copy.dst);
		rtgl_texture_image_retain(command->data.texture_copy.dst_copy_source);
		break;
	case RTGL_RECORDED_COMMAND_TEXTURE_COPY_TO_BUFFER:
		rtgl_texture_image_retain(command->data.texture_copy_to_buffer.src);
		rtgl_buffer_storage_retain(command->data.texture_copy_to_buffer.dst);
		rtgl_buffer_storage_retain(command->data.texture_copy_to_buffer.dst_copy_source);
		break;
	default:
		break;
	}
}

bool rtgl_command_buffer_clone_command(rtgl_recorded_command* destination, const rtgl_recorded_command* source) {
	*destination = *source;
	if (source->kind == RTGL_RECORDED_COMMAND_BUFFER_DATA) {
		const usize size = source->data.buffer_data.range.size;
		destination->data.buffer_data.data = (u08*)malloc(size);
		if (!destination->data.buffer_data.data) {
			memset(destination, 0, sizeof(*destination));
			return false;
		}
		memcpy(destination->data.buffer_data.data, source->data.buffer_data.data, size);
	} else if (source->kind == RTGL_RECORDED_COMMAND_UNIFORM_DATA || source->kind == RTGL_RECORDED_COMMAND_STORAGE_DATA) {
		destination->data.program_data.bytes = (u08*)malloc(source->data.program_data.size);
		if (!destination->data.program_data.bytes) {
			memset(destination, 0, sizeof(*destination));
			return false;
		}
		memcpy(destination->data.program_data.bytes, source->data.program_data.bytes, source->data.program_data.size);
	} else if (source->kind == RTGL_RECORDED_COMMAND_TEXTURE_DATA) {
		const usize data_size = rtgl_texture_range_byte_count_image(source->data.texture_data.image, source->data.texture_data.range);
		if (!data_size) {
			memset(destination, 0, sizeof(*destination));
			return false;
		}
		destination->data.texture_data.data = (u08*)malloc(data_size);
		if (!destination->data.texture_data.data) {
			memset(destination, 0, sizeof(*destination));
			return false;
		}
		memcpy(destination->data.texture_data.data, source->data.texture_data.data, data_size);
	}
	rtgl_command_buffer_retain_command(destination);
	return true;
}

bool rtgl_command_buffer_append_copy(struct rtgl_command_buffer* command_buffer, const rtgl_recorded_command* source) {
	rtgl_recorded_command* destination = rtgl_command_buffer_append(command_buffer);
	if (!destination) {
		return false;
	}
	if (!rtgl_command_buffer_clone_command(destination, source)) {
		command_buffer->command_count--;
		return false;
	}
	return true;
}

void rtCommandBufferReset(rt_command_buffer command_buffer) { rtgl_begin_errorable_operation(); rtgl_command_buffer_reset(rtgl_command_buffer_from_handle(command_buffer)); }
void rtCommandBufferBegin(rt_command_buffer command_buffer) { rtgl_begin_errorable_operation(); rtgl_command_buffer_begin(rtgl_command_buffer_from_handle(command_buffer)); }
void rtCommandBufferContinue(rt_command_buffer command_buffer) { rtgl_begin_errorable_operation(); rtgl_command_buffer_continue(rtgl_command_buffer_from_handle(command_buffer), false); }
void rtCommandBufferContinueRendering(rt_command_buffer command_buffer) { rtgl_begin_errorable_operation(); rtgl_command_buffer_continue(rtgl_command_buffer_from_handle(command_buffer), true); }
void rtCommandBufferEnd(rt_command_buffer command_buffer) { rtgl_begin_errorable_operation(); rtgl_command_buffer_end(rtgl_command_buffer_from_handle(command_buffer)); }

void rtCmdExecute(rt_command_buffer command_buffer, rt_command_buffer secondary) {
	rtgl_begin_errorable_operation();
	struct rtgl_command_buffer* parent = rtgl_command_buffer_from_handle(command_buffer);
	struct rtgl_command_buffer* child = rtgl_command_buffer_from_handle(secondary);
	if (!parent || !child || parent == child || !parent->recording || !child->executable || !child->continuation || (child->rendering_continuation && !parent->rendering)) {
		return;
	}
	for (u32 index = 0; index < child->command_count; index++) {
		if (!rtgl_command_buffer_append_copy(parent, &child->commands[index])) {
			return;
		}
	}
}

void rtCmdBeginRendering(rt_command_buffer command_buffer, rt_framebuffer framebuffer) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_begin_rendering(rtgl_command_buffer_from_handle(command_buffer), rtgl_framebuffer_from_handle(framebuffer));
}

void rtCmdClearColor(rt_command_buffer command_buffer, rt_location location, f32 r, f32 g, f32 b, f32 a) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_clear_color(rtgl_command_buffer_from_handle(command_buffer), location, r, g, b, a);
}

void rtCmdClearDepth(rt_command_buffer command_buffer, f32 depth) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_clear_depth(rtgl_command_buffer_from_handle(command_buffer), depth);
}

void rtCmdClearStencil(rt_command_buffer command_buffer, usize stencil) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_clear_stencil(rtgl_command_buffer_from_handle(command_buffer), stencil);
}

void rtCmdClear(rt_command_buffer command_buffer, enum rt_clear_flag attachments) { rtgl_begin_errorable_operation(); rtgl_command_buffer_clear(rtgl_command_buffer_from_handle(command_buffer), attachments); }

void rtCmdSetViewport(rt_command_buffer command_buffer, usize x, usize y, usize width, usize height, f32 min_depth, f32 max_depth) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_set_viewport(rtgl_command_buffer_from_handle(command_buffer), x, y, width, height, min_depth, max_depth);
}

void rtCmdUseProgram(rt_command_buffer command_buffer, rt_program program) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_use_program(rtgl_command_buffer_from_handle(command_buffer), rtgl_program_from_handle(program));
}

void rtgl_command_buffer_dispatch(struct rtgl_command_buffer* command_buffer, usize group_count_x, usize group_count_y, usize group_count_z) {
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) return;
	command->kind = RTGL_RECORDED_COMMAND_DISPATCH;
	command->data.dispatch = { group_count_x, group_count_y, group_count_z };
}

static void rtgl_command_buffer_bind_sampler(struct rtgl_command_buffer* command_buffer, struct rt_location_t* location, const struct rtgl_sampler* sampler) {
	if (!command_buffer || !command_buffer->recording || !sampler) return;
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) return;
	command->kind = RTGL_RECORDED_COMMAND_BIND_SAMPLER;
	command->data.bind_sampler.address = location ? location->address : 0;
	command->data.bind_sampler.mag_filter = sampler->mag_filter;
	command->data.bind_sampler.min_filter = sampler->min_filter;
	command->data.bind_sampler.mip_filter = sampler->mip_filter;
	command->data.bind_sampler.address_u = sampler->address_u;
	command->data.bind_sampler.address_v = sampler->address_v;
	command->data.bind_sampler.address_w = sampler->address_w;
	command->data.bind_sampler.max_anisotropy = sampler->max_anisotropy;
	command->data.bind_sampler.min_lod = sampler->min_lod;
	command->data.bind_sampler.max_lod = sampler->max_lod;
	command->data.bind_sampler.lod_bias = sampler->lod_bias;
}

void rtgl_command_buffer_program_data(struct rtgl_command_buffer* command_buffer, struct rt_location_t* location, const u08* data, usize size, rtgl_location_kind expected_kind, rtgl_recorded_command_kind command_kind) {
	struct rtgl_program* location_program = rtgl_location_program(location);
	const struct rtgl_program_mapping* mapping = rtgl_program_mapping(location_program, location);
	if (!command_buffer || !command_buffer->recording || !mapping || mapping->kind != expected_kind || !data || size != mapping->byte_size) {
		rtgl_throwf(RT_IMPROPER_USAGE, "program data write does not match its reflected location");
		return;
	}
	rtgl_recorded_command* command = rtgl_command_buffer_append(command_buffer);
	if (!command) {
		return;
	}
	command->kind = command_kind;
	command->data.program_data.address = location->address;
	command->data.program_data.bytes = (u08*)malloc(size);
	command->data.program_data.size = size;
	if (!command->data.program_data.bytes) {
		command_buffer->command_count--;
		rtgl_throwf(RT_OUT_OF_HOST_MEMORY, "failed to copy program data into the command buffer");
		return;
	}
	memcpy(command->data.program_data.bytes, data, size);
}

void rtCmdUniformData(rt_command_buffer command_buffer, rt_location location, const u08* data, usize size) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_program_data(rtgl_command_buffer_from_handle(command_buffer), location, data, size, RTGL_LOCATION_MAPPING_UNIFORM_DATA, RTGL_RECORDED_COMMAND_UNIFORM_DATA);
}

void rtCmdStorageData(rt_command_buffer command_buffer, rt_location location, const u08* data, usize size) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_program_data(rtgl_command_buffer_from_handle(command_buffer), location, data, size, RTGL_LOCATION_MAPPING_STORAGE_DATA, RTGL_RECORDED_COMMAND_STORAGE_DATA);
}

void rtCmdSetScissor(rt_command_buffer command_buffer, usize x, usize y, usize width, usize height) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_set_scissor(rtgl_command_buffer_from_handle(command_buffer), x, y, width, height);
}

void rtCmdBindBuffer(rt_command_buffer command_buffer, rt_location location, rt_buffer buffer, rt_buffer_range range) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_bind_buffer(rtgl_command_buffer_from_handle(command_buffer), location, rtgl_buffer_from_handle(buffer), range);
}

void rtCmdBindTexture(rt_command_buffer command_buffer, rt_location location, rt_texture_view texture_view) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_bind_texture(rtgl_command_buffer_from_handle(command_buffer), location, rtgl_texture_view_from_handle(texture_view));
}

void rtCmdBindSampler(rt_command_buffer command_buffer, rt_location location, rt_sampler sampler) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_bind_sampler(rtgl_command_buffer_from_handle(command_buffer), location, rtgl_sampler_from_handle(sampler));
}

void rtCmdVertexBuffer(rt_command_buffer command_buffer, rt_location location, rt_buffer buffer, rt_buffer_range range) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_vertex_buffer(rtgl_command_buffer_from_handle(command_buffer), location, rtgl_buffer_from_handle(buffer), range);
}

void rtCmdIndexBuffer(rt_command_buffer command_buffer, rt_buffer buffer, rt_buffer_range range, enum rt_index_format format) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_index_buffer(rtgl_command_buffer_from_handle(command_buffer), rtgl_buffer_from_handle(buffer), range, format);
}

void rtCmdDraw(rt_command_buffer command_buffer, usize vertex_count, usize first_vertex) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_draw(rtgl_command_buffer_from_handle(command_buffer), vertex_count, first_vertex);
}

void rtCmdDrawInstanced(rt_command_buffer command_buffer, usize vertex_count, usize instance_count, usize first_vertex, usize first_instance) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_draw_instanced(rtgl_command_buffer_from_handle(command_buffer), vertex_count, instance_count, first_vertex, first_instance);
}

void rtCmdDrawIndexed(rt_command_buffer command_buffer, usize index_count, usize first_index, usize vertex_offset) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_draw_indexed(rtgl_command_buffer_from_handle(command_buffer), index_count, first_index, vertex_offset);
}

void rtCmdDrawIndexedInstanced(rt_command_buffer command_buffer, usize index_count, usize instance_count, usize first_index, usize vertex_offset, usize first_instance) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_draw_indexed_instanced(rtgl_command_buffer_from_handle(command_buffer), index_count, instance_count, first_index, vertex_offset, first_instance);
}

void rtCmdEndRendering(rt_command_buffer command_buffer) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_end_rendering(rtgl_command_buffer_from_handle(command_buffer));
}

void rtCmdBufferData(rt_command_buffer command_buffer, rt_buffer buffer, rt_buffer_range range, const u08* data) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_buffer_data(rtgl_command_buffer_from_handle(command_buffer), rtgl_buffer_from_handle(buffer), range, data);
}

void rtCmdBufferCopy(rt_command_buffer command_buffer, rt_buffer src, rt_buffer_range src_range, rt_buffer dst, rt_buffer_range dst_range) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_buffer_copy(rtgl_command_buffer_from_handle(command_buffer), rtgl_buffer_from_handle(src), src_range, rtgl_buffer_from_handle(dst), dst_range);
}

void rtCmdBufferCopyToTexture(rt_command_buffer command_buffer, rt_buffer src, rt_buffer_range src_range, rt_texture dst, rt_texture_range dst_range) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_buffer_copy_to_texture(rtgl_command_buffer_from_handle(command_buffer), rtgl_buffer_from_handle(src), src_range, rtgl_texture_from_handle(dst), dst_range);
}

void rtCmdBufferBarrier(rt_command_buffer command_buffer, rt_buffer buffer, rt_buffer_range range, rt_access src, rt_access dst) {
	rtgl_begin_errorable_operation();
	(void)buffer;
	(void)range;
	rtgl_command_buffer_barrier(rtgl_command_buffer_from_handle(command_buffer), src, dst, RTGL_RECORDED_COMMAND_BUFFER_BARRIER);
}

void rtCmdTextureData(rt_command_buffer command_buffer, rt_texture texture, rt_texture_range range, const u08* data) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_texture_data(rtgl_command_buffer_from_handle(command_buffer), rtgl_texture_from_handle(texture), range, data);
}

void rtCmdTextureCopy(rt_command_buffer command_buffer, rt_texture src, rt_texture_range src_range, rt_texture dst, rt_texture_range dst_range) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_texture_copy(rtgl_command_buffer_from_handle(command_buffer), rtgl_texture_from_handle(src), src_range, rtgl_texture_from_handle(dst), dst_range);
}

void rtCmdTextureCopyToBuffer(rt_command_buffer command_buffer, rt_texture src, rt_texture_range src_range, rt_buffer dst, rt_buffer_range dst_range) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_texture_copy_to_buffer(rtgl_command_buffer_from_handle(command_buffer), rtgl_texture_from_handle(src), src_range, rtgl_buffer_from_handle(dst), dst_range);
}

void rtCmdTextureBarrier(rt_command_buffer command_buffer, rt_texture texture, rt_texture_range range, rt_access src, rt_access dst) {
	rtgl_begin_errorable_operation();
	(void)texture;
	(void)range;
	rtgl_command_buffer_barrier(rtgl_command_buffer_from_handle(command_buffer), src, dst, RTGL_RECORDED_COMMAND_TEXTURE_BARRIER);
}

struct rtgl_command_buffer* rtgl_command_buffer_create(struct rtgl_context* ctx) {
	struct rtgl_command_buffer* command_buffer = RTGL_ALLOC_RESOURCE(struct rtgl_command_buffer);
	if (command_buffer) {
		rtgl_command_buffer_init(ctx, command_buffer);
	}
	return command_buffer;
}

void rtgl_command_buffer_destroy(struct rtgl_context*, struct rtgl_command_buffer* command_buffer) {
	if (command_buffer) {
		rtgl_resource_retire(RTGL_RESOURCE_BASE(command_buffer));
	}
}

void rtgl_command_buffer_init(struct rtgl_context* ctx, struct rtgl_command_buffer* command_buffer) {
	rtgl_init_resource_base(ctx, RTGL_RESOURCE_BASE(command_buffer), RTGL_RESOURCE_COMMAND_BUFFER);
}

void rtgl_command_buffer_finish(struct rtgl_command_buffer* command_buffer) {
	rtgl_command_buffer_clear_commands(command_buffer);
	free(command_buffer->commands);
	command_buffer->commands = NULL;
	command_buffer->command_capacity = 0;
	rtgl_finish_resource_base(RTGL_RESOURCE_BASE(command_buffer));
}

struct rtgl_command_buffer_submission {
	struct rtgl_context* ctx;
	rtgl_recorded_command* commands;
	u32 command_count;
	rt_timepoint* wait_timepoints;
	usize wait_count;
};

void rtgl_command_buffer_submission_destroy(struct rtgl_command_buffer_submission* submission) {
	if (!submission) {
		return;
	}
	struct rtgl_command_buffer command_buffer;
	memset(&command_buffer, 0, sizeof(command_buffer));
	command_buffer.base.ctx = submission->ctx;
	for (u32 i = 0; i < submission->command_count; i++) {
		rtgl_command_buffer_release_command(&command_buffer, &submission->commands[i]);
	}
	free(submission->commands);
	free(submission->wait_timepoints);
	free(submission);
}

void rtgl_command_buffer_mark_writes(struct rtgl_context* ctx, const rtgl_recorded_command* commands, u32 command_count, rt_timepoint timepoint) {
	for (u32 index = 0; index < command_count; index++) {
		const rtgl_recorded_command* command = &commands[index];
		switch (command->kind) {
		case RTGL_RECORDED_COMMAND_BUFFER_DATA:
			rtgl_buffer_storage_mark_write(ctx, command->data.buffer_data.storage, timepoint);
			break;
		case RTGL_RECORDED_COMMAND_BUFFER_COPY:
			rtgl_buffer_storage_mark_write(ctx, command->data.buffer_copy.dst, timepoint);
			break;
		case RTGL_RECORDED_COMMAND_TEXTURE_COPY_TO_BUFFER:
			rtgl_buffer_storage_mark_write(ctx, command->data.texture_copy_to_buffer.dst, timepoint);
			break;
		default:
			break;
		}
	}
}

rt_timepoint rtgl_command_buffer_submit(struct rtgl_context* ctx, struct rtgl_queue* queue, struct rtgl_command_buffer* command_buffer) {
	rt_timepoint* wait_timepoints = queue->wait_timepoints;
	const usize wait_count = queue->wait_count;
	queue->wait_timepoints = NULL;
	queue->wait_count = 0;
	queue->wait_capacity = 0;
	rt_timepoint done = rtgl_queue_signal(queue);

	struct rtgl_command_buffer_submission* submission = (struct rtgl_command_buffer_submission*)calloc(1, sizeof(*submission));
	if (!submission) {
		RTGL_CHECK_ALLOC(submission, sizeof(*submission), "OpenGL command buffer submission");
		rtgl_execution_queue_complete(ctx, queue, done.value);
		free(wait_timepoints);
		return done;
	}
	submission->ctx = ctx;
	submission->wait_timepoints = wait_timepoints;
	submission->wait_count = wait_count;
	if (!command_buffer) {
		goto submit;
	}
	submission->command_count = command_buffer->command_count;
	if (submission->command_count) {
		submission->commands = (rtgl_recorded_command*)calloc(submission->command_count, sizeof(*submission->commands));
		if (!submission->commands) {
			RTGL_CHECK_ALLOC(submission->commands, sizeof(*submission->commands) * (usize)submission->command_count, "OpenGL submitted commands");
			rtgl_command_buffer_submission_destroy(submission);
			rtgl_execution_queue_complete(ctx, queue, done.value);
			return done;
		}
		for (u32 index = 0; index < submission->command_count; index++) {
			if (!rtgl_command_buffer_clone_command(&submission->commands[index], &command_buffer->commands[index])) {
				submission->command_count = index;
				rtgl_command_buffer_submission_destroy(submission);
				rtgl_execution_queue_complete(ctx, queue, done.value);
				return done;
			}
		}
	}

submit:
	rtgl_command_buffer_mark_writes(ctx, submission->commands, submission->command_count, done);
	rtgl_retain_resource(command_buffer);
	if (!rtgl_execution_submit_async(ctx, [command_buffer, submission, queue, value = rtgl_timepoint_queue_value(done)](struct rtgl_context* exec_ctx) {
			for (usize index = 0; index < submission->wait_count; index++) {
				if (!submission->wait_timepoints[index].value || rtgl_timepoint_reached(exec_ctx, submission->wait_timepoints[index])) {
					continue;
				}
				if (!exec_ctx->execution.stopping) {
					rtgl_execution_defer_current_command();
					return;
				}
				rtgl_command_buffer_submission_destroy(submission);
				if (command_buffer) {
					rtgl_resource_release(RTGL_RESOURCE_BASE(command_buffer));
				}
				rtgl_execution_queue_complete(exec_ctx, queue, value);
				return;
			}
			if (command_buffer) {
				/* Execute a retained snapshot. The logical command buffer remains executable. */
				struct rtgl_command_buffer command_view = *command_buffer;
				command_view.commands = submission->commands;
				command_view.command_count = submission->command_count;
				rtgl_command_buffer_execute(exec_ctx, &command_view, queue, value);
			} else {
				rtgl_execution_queue_complete(exec_ctx, queue, value);
			}
			rtgl_command_buffer_submission_destroy(submission);
			if (command_buffer) {
				rtgl_resource_release(RTGL_RESOURCE_BASE(command_buffer));
			}
		})) {
		rtgl_command_buffer_submission_destroy(submission);
		if (command_buffer) {
			rtgl_resource_release(RTGL_RESOURCE_BASE(command_buffer));
		}
		rtgl_execution_queue_complete(ctx, queue, rtgl_timepoint_queue_value(done));
	}
	return done;
}

static GLint rtgl_vertex_attribute_components(enum rt_format format) {
	switch (format) {
	case RT_R32_SFLOAT:
		return 1;
	case RT_RG32_SFLOAT:
		return 2;
	case RT_RGB32_SFLOAT:
		return 3;
	case RT_RGBA32_SFLOAT:
		return 4;
	default:
		return 0;
	}
}

static GLenum rtgl_blend_factor(enum rt_blend_factor factor) {
	switch (factor) {
	case RT_BLEND_ZERO:
		return GL_ZERO;
	case RT_BLEND_ONE:
		return GL_ONE;
	case RT_BLEND_SRC_COLOR:
		return GL_SRC_COLOR;
	case RT_BLEND_ONE_MINUS_SRC_COLOR:
		return GL_ONE_MINUS_SRC_COLOR;
	case RT_BLEND_DST_COLOR:
		return GL_DST_COLOR;
	case RT_BLEND_ONE_MINUS_DST_COLOR:
		return GL_ONE_MINUS_DST_COLOR;
	case RT_BLEND_SRC_ALPHA:
		return GL_SRC_ALPHA;
	case RT_BLEND_ONE_MINUS_SRC_ALPHA:
		return GL_ONE_MINUS_SRC_ALPHA;
	case RT_BLEND_DST_ALPHA:
		return GL_DST_ALPHA;
	case RT_BLEND_ONE_MINUS_DST_ALPHA:
		return GL_ONE_MINUS_DST_ALPHA;
	default:
		return GL_ONE;
	}
}

static GLenum rtgl_blend_op(enum rt_blend_op op) {
	switch (op) {
	case RT_BLEND_OP_ADD:
		return GL_FUNC_ADD;
	case RT_BLEND_OP_SUBTRACT:
		return GL_FUNC_SUBTRACT;
	case RT_BLEND_OP_REVERSE_SUBTRACT:
		return GL_FUNC_REVERSE_SUBTRACT;
	case RT_BLEND_OP_MIN:
		return GL_MIN;
	case RT_BLEND_OP_MAX:
		return GL_MAX;
	default:
		return GL_FUNC_ADD;
	}
}

void rtgl_bind_uniform_block(struct rtgl_program* program, const struct rtgl_program_mapping* mapping) {
	if (!program || !program->gl_program || !mapping || mapping->kind != RTGL_LOCATION_MAPPING_UNIFORM_BUFFER) {
		return;
	}
	GLuint block = glGetUniformBlockIndex(program->gl_program, mapping->name);
	if (block != GL_INVALID_INDEX) {
		glUniformBlockBinding(program->gl_program, block, mapping->binding);
	}
}

void rtCmdDispatch(rt_command_buffer command_buffer, usize group_count_x, usize group_count_y, usize group_count_z) {
	rtgl_begin_errorable_operation();
	rtgl_command_buffer_dispatch(rtgl_command_buffer_from_handle(command_buffer), group_count_x, group_count_y, group_count_z);
}

void rtgl_bind_uniform_texture(struct rtgl_context* ctx, struct rtgl_program* program, struct rtgl_program_mapping* mapping) {
	if (!program || !program->gl_program || !mapping) {
		return;
	}
	mapping->gl_location = glGetUniformLocation(program->gl_program, mapping->name);
	if (mapping->gl_location < 0) {
		return;
	}
	glProgramUniform1i(program->gl_program, mapping->gl_location, (GLint)mapping->binding);
}

static void rtgl_bind_vertex_layout(struct rtgl_context* ctx, struct rtgl_program* program, struct rtgl_buffer_storage* const* storages, const u64* offsets, GLuint vao) {
	if (!program || !program->vertex_layout.input_count) {
		return;
	}
	u32 attribute_location = 0;
	for (u32 input_index = 0; input_index < program->vertex_layout.input_count; input_index++) {
		const rt_vertex_input* input = &program->vertex_layout.inputs[input_index];
		if (!storages[input_index]) {
			attribute_location += (u32)input->attribute_count;
			continue;
		}
		glVertexArrayVertexBuffer(vao, input_index, storages[input_index]->gl_buffer, (GLintptr)offsets[input_index], (GLsizei)input->stride);
		glVertexArrayBindingDivisor(vao, input_index, input->rate == RT_VERTEX_RATE_INSTANCE ? 1 : 0);
		for (u32 attribute_index = 0; attribute_index < input->attribute_count; attribute_index++, attribute_location++) {
			const rt_vertex_attribute* attribute = &input->attributes[attribute_index];
			GLint components = rtgl_vertex_attribute_components(attribute->format);
			if (!components) {
				rtgl_throwf(RT_UNSUPPORTED_FEATURE, "unsupported OpenGL vertex attribute format");
				return;
			}
			glEnableVertexArrayAttrib(vao, attribute_location);
			glVertexArrayAttribFormat(vao, attribute_location, components, GL_FLOAT, GL_FALSE, attribute->offset);
			glVertexArrayAttribBinding(vao, attribute_location, input_index);
		}
	}
}

static GLbitfield rtgl_barrier_bits(rt_access access) {
	GLbitfield bits = 0;
	if (access.stage & RT_STAGE_TRANSFER) {
		bits |= GL_BUFFER_UPDATE_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT | GL_PIXEL_BUFFER_BARRIER_BIT;
	}
	if (access.stage & RT_STAGE_VERTEX) {
		bits |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_UNIFORM_BARRIER_BIT;
	}
	if (access.stage & (RT_STAGE_FRAGMENT | RT_STAGE_COMPUTE)) {
		bits |= GL_UNIFORM_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
	}
	if (access.stage & RT_STAGE_COLOR_ATTACHMENT) {
		bits |= GL_FRAMEBUFFER_BARRIER_BIT;
	}
	if (access.stage & RT_STAGE_DEPTH_STENCIL_ATTACHMENT) {
		bits |= GL_FRAMEBUFFER_BARRIER_BIT;
	}
	return bits;
}

static GLenum rtgl_snapshot_sampler_filter(enum rt_filter filter, enum rt_mip_filter mip_filter) {
	if (mip_filter == RT_MIP_FILTER_NONE)
		return filter == RT_FILTER_NEAREST ? GL_NEAREST : GL_LINEAR;
	if (mip_filter == RT_MIP_FILTER_NEAREST)
		return filter == RT_FILTER_NEAREST ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_NEAREST;
	return filter == RT_FILTER_NEAREST ? GL_NEAREST_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_LINEAR;
}

static GLenum rtgl_snapshot_sampler_address(enum rt_address_mode address) {
	return address == RT_ADDRESS_CLAMP ? GL_CLAMP_TO_EDGE : address == RT_ADDRESS_MIRROR ? GL_MIRRORED_REPEAT
																						 : GL_REPEAT;
}

static void rtgl_texture_copy_one(const struct rtgl_image_base* src, rt_texture_range src_range, const struct rtgl_image_base* dst, rt_texture_range dst_range) {
	GLint src_y = (GLint)src_range.offset.height;
	GLint src_z = (GLint)src_range.offset.depth;
	GLint dst_y = (GLint)dst_range.offset.height;
	GLint dst_z = (GLint)dst_range.offset.depth;
	GLsizei width = (GLsizei)src_range.extent.width;
	GLsizei height = (GLsizei)src_range.extent.height;
	GLsizei depth = (GLsizei)src_range.extent.depth;
	if (src->type == RT_TEXTURE_1D_ARRAY) {
		src_y = (GLint)src_range.base_layer;
		height = (GLsizei)src_range.layer_count;
		depth = 1;
	}
	if (dst->type == RT_TEXTURE_1D_ARRAY) {
		dst_y = (GLint)dst_range.base_layer;
	}
	if (src->type == RT_TEXTURE_2D_ARRAY) {
		src_z = (GLint)src_range.base_layer;
		depth = (GLsizei)src_range.layer_count;
	}
	if (dst->type == RT_TEXTURE_2D_ARRAY) {
		dst_z = (GLint)dst_range.base_layer;
	}
	if (src->type == RT_TEXTURE_1D) {
		height = 1;
		depth = 1;
	}
	if (src->type == RT_TEXTURE_2D) {
		depth = 1;
	}
	glCopyImageSubData(src->gl_texture, src->gl_target, (GLint)src_range.base_mip, (GLint)src_range.offset.width, src_y, src_z, dst->gl_texture, dst->gl_target, (GLint)dst_range.base_mip, (GLint)dst_range.offset.width, dst_y, dst_z, width, height, depth);
}

static void rtgl_texture_copy_range(const struct rtgl_image_base* src, rt_texture_range src_range, const struct rtgl_image_base* dst, rt_texture_range dst_range) {
	for (usize level = 0; level < src_range.mip_count; level++) {
		rt_texture_range src_mip;
		rt_texture_range dst_mip;
		if (!rtgl_texture_range_level(src, src_range, level, &src_mip) || !rtgl_texture_range_level(dst, dst_range, level, &dst_mip))
			return;
		rtgl_texture_copy_one(src, src_mip, dst, dst_mip);
	}
}

void rtgl_command_buffer_execute(struct rtgl_context* ctx, struct rtgl_command_buffer* command_buffer, struct rtgl_queue* queue, u64 complete_value) {
	struct rtgl_framebuffer* framebuffer = NULL;
	struct rtgl_image_base* color_image = NULL;
	struct rtgl_image_base* color_images[RTGL_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS] = { 0 };
	struct rtgl_image_base* depth_image = NULL;
	struct rtgl_image_base* stencil_image = NULL;
	u32 color_count = 0;
	struct rtgl_program* program = NULL;
	struct rtgl_buffer_storage* vertex_storages[RTGL_MAX_VERTEX_ATTRIBUTES] = { 0 };
	u64 vertex_offsets[RTGL_MAX_VERTEX_ATTRIBUTES] = { 0 };
	struct rtgl_buffer_storage* index_storage = NULL;
	u64 index_offset = 0;
	enum rt_index_format index_format = RT_INDEX_U16;
	std::vector<GLuint> sampler_snapshots;
	std::vector<rtgl_program_data_block> program_data_blocks;

	u32 command_offset = 0;
	while (command_offset < command_buffer->command_count) {
		const rtgl_recorded_command* command = &command_buffer->commands[command_offset];
		if (command->size != sizeof(*command)) {
			break;
		}
		switch (command->kind) {
		case RTGL_RECORDED_COMMAND_BEGIN_RENDERING: {
			framebuffer = command->data.begin_rendering.framebuffer;
			color_count = framebuffer ? framebuffer->color_texture_count : 0;
			for (u32 color = 0; color < color_count; color++) {
				color_images[color] = command->data.begin_rendering.color_images[color];
				if (color_images[color] && command->data.begin_rendering.color_copy_sources[color])
					rtgl_texture_image_copy(color_images[color], command->data.begin_rendering.color_copy_sources[color]);
			}
			color_image = color_count ? color_images[0] : NULL;
			depth_image = command->data.begin_rendering.depth_image;
			if (depth_image && command->data.begin_rendering.depth_copy_source)
				rtgl_texture_image_copy(depth_image, command->data.begin_rendering.depth_copy_source);
			stencil_image = command->data.begin_rendering.stencil_image;
			if (stencil_image && command->data.begin_rendering.stencil_copy_source &&
				stencil_image != depth_image)
				rtgl_texture_image_copy(stencil_image, command->data.begin_rendering.stencil_copy_source);
			if (!framebuffer || !color_image || !color_image->gl_texture) {
				break;
			}
			GLsizei width = (GLsizei)color_image->width;
			GLsizei height = (GLsizei)color_image->height;
			for (u32 color = 0; color < color_count; color++) {
				struct rtgl_image_base* image = color_images[color];
				glNamedFramebufferTexture(framebuffer->gl_framebuffer, GL_COLOR_ATTACHMENT0 + color, image ? image->gl_texture : 0, 0);
			}
			glNamedFramebufferTexture(framebuffer->gl_framebuffer, GL_DEPTH_ATTACHMENT, depth_image ? depth_image->gl_texture : 0, 0);
			glNamedFramebufferTexture(framebuffer->gl_framebuffer, GL_STENCIL_ATTACHMENT, stencil_image ? stencil_image->gl_texture : 0, 0);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer->gl_framebuffer);
			glViewport(0, 0, width, height);
			if (color_image->gl_internal_format == GL_SRGB8_ALPHA8)
				glEnable(GL_FRAMEBUFFER_SRGB);
			else
				glDisable(GL_FRAMEBUFFER_SRGB);
			glDisable(GL_SCISSOR_TEST);
			glDisable(GL_STENCIL_TEST);
			glDisable(GL_BLEND);
			glDisable(GL_CULL_FACE);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glDepthMask(GL_TRUE);
			if (depth_image && depth_image->gl_texture) {
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_LESS);
			} else
				glDisable(GL_DEPTH_TEST);
			break;
		}
		case RTGL_RECORDED_COMMAND_CLEAR:
			if (!framebuffer) {
				break;
			}
			if (command->data.clear.attachments & RT_CLEAR_COLOR) {
				for (u32 color = 0; color < color_count; color++) {
					if (color_images[color]) {
						glClearNamedFramebufferfv(framebuffer->gl_framebuffer, GL_COLOR, (GLint)color, command->data.clear.colors[color]);
					}
				}
			}
			if ((command->data.clear.attachments & RT_CLEAR_DEPTH) && depth_image && depth_image->gl_texture) {
				glClearNamedFramebufferfv(framebuffer->gl_framebuffer, GL_DEPTH, 0, &command->data.clear.depth);
			}
			if ((command->data.clear.attachments & RT_CLEAR_STENCIL) && stencil_image && stencil_image->gl_texture) {
				const GLuint stencil = (GLuint)command->data.clear.stencil;
				glClearNamedFramebufferuiv(framebuffer->gl_framebuffer, GL_STENCIL, 0, &stencil);
			}
			break;
		case RTGL_RECORDED_COMMAND_SET_VIEWPORT:
			glViewport((GLint)command->data.set_viewport.x, (GLint)command->data.set_viewport.y, (GLsizei)command->data.set_viewport.width, (GLsizei)command->data.set_viewport.height);
			glDepthRangef(command->data.set_viewport.min_depth, command->data.set_viewport.max_depth);
			break;
		case RTGL_RECORDED_COMMAND_USE_PROGRAM:
			program = command->data.use_program.program;
			rtgl_program_prepare(ctx, program);
			if (!program || !program->gl_program)
				break;
			glUseProgram(program->gl_program);
			if (program->tessellated) {
				glPatchParameteri(GL_PATCH_VERTICES, (GLint)program->patch_vertices);
			}
			if (program->cull_mode == RT_CULL_NONE)
				glDisable(GL_CULL_FACE);
			else {
				glEnable(GL_CULL_FACE);
				glCullFace(program->cull_mode == RT_CULL_FRONT ? GL_FRONT : GL_BACK);
			}
			glFrontFace(program->front_face == RT_FRONT_FACE_CW ? GL_CW : GL_CCW);
			glPolygonMode(GL_FRONT_AND_BACK, program->fill_mode == RT_FILL_WIREFRAME ? GL_LINE : GL_FILL);
			if (program->blend_enabled) {
				glEnable(GL_BLEND);
				glBlendFuncSeparate(rtgl_blend_factor(program->src_color_blend), rtgl_blend_factor(program->dst_color_blend), rtgl_blend_factor(program->src_alpha_blend), rtgl_blend_factor(program->dst_alpha_blend));
				glBlendEquationSeparate(rtgl_blend_op(program->color_blend_op), rtgl_blend_op(program->alpha_blend_op));
			} else
				glDisable(GL_BLEND);
			break;
		case RTGL_RECORDED_COMMAND_UNIFORM_DATA:
		case RTGL_RECORDED_COMMAND_STORAGE_DATA: {
			if (!program || !program->location_occupied[command->data.program_data.address]) {
				break;
			}
			struct rtgl_program_mapping* mapping = &program->mappings[command->data.program_data.address];
			const rtgl_location_kind expected = command->kind == RTGL_RECORDED_COMMAND_UNIFORM_DATA ? RTGL_LOCATION_MAPPING_UNIFORM_DATA : RTGL_LOCATION_MAPPING_STORAGE_DATA;
			if (mapping->kind != expected || command->data.program_data.size != mapping->byte_size || mapping->byte_offset > mapping->block_size || command->data.program_data.size > mapping->block_size - mapping->byte_offset) {
				break;
			}
			rtgl_program_data_block* block = nullptr;
			for (rtgl_program_data_block& candidate : program_data_blocks) {
				if (candidate.program == program && candidate.kind == mapping->kind && candidate.binding == mapping->binding) {
					block = &candidate;
					break;
				}
			}
			if (!block) {
				program_data_blocks.push_back({ program, mapping->kind, mapping->binding, 0, std::vector<u08>(mapping->block_size) });
				block = &program_data_blocks.back();
				glCreateBuffers(1, &block->buffer);
				glNamedBufferStorage(block->buffer, (GLsizeiptr)mapping->block_size, nullptr, GL_DYNAMIC_STORAGE_BIT);
			}
			memcpy(block->bytes.data() + mapping->byte_offset, command->data.program_data.bytes, command->data.program_data.size);
			glNamedBufferSubData(block->buffer, (GLintptr)mapping->byte_offset, (GLsizeiptr)command->data.program_data.size, command->data.program_data.bytes);
			glBindBufferBase(mapping->kind == RTGL_LOCATION_MAPPING_UNIFORM_DATA ? GL_UNIFORM_BUFFER : GL_SHADER_STORAGE_BUFFER, mapping->binding, block->buffer);
			break;
		}
		case RTGL_RECORDED_COMMAND_SET_SCISSOR:
			glEnable(GL_SCISSOR_TEST);
			if (color_image) {
				u32 height = color_image->height;
				u32 y = command->data.set_scissor.y + command->data.set_scissor.height <= height ? height - command->data.set_scissor.y - command->data.set_scissor.height : 0;
				glScissor((GLint)command->data.set_scissor.x, (GLint)y, (GLsizei)command->data.set_scissor.width, (GLsizei)command->data.set_scissor.height);
			} else
				glScissor((GLint)command->data.set_scissor.x, (GLint)command->data.set_scissor.y, (GLsizei)command->data.set_scissor.width, (GLsizei)command->data.set_scissor.height);
			break;
		case RTGL_RECORDED_COMMAND_BIND_BUFFER:
			if (program && program->location_occupied[command->data.bind_buffer.address] && command->data.bind_buffer.storage) {
				struct rtgl_program_mapping* mapping = &program->mappings[command->data.bind_buffer.address];
				if (mapping->kind == RTGL_LOCATION_MAPPING_UNIFORM_BUFFER) {
					rtgl_bind_uniform_block(program, mapping);
					glBindBufferRange(GL_UNIFORM_BUFFER, mapping->binding, command->data.bind_buffer.storage->gl_buffer, (GLintptr)command->data.bind_buffer.offset, (GLsizeiptr)command->data.bind_buffer.size);
				} else if (mapping->kind == RTGL_LOCATION_MAPPING_STORAGE_BUFFER) {
					glBindBufferRange(GL_SHADER_STORAGE_BUFFER, mapping->binding, command->data.bind_buffer.storage->gl_buffer, (GLintptr)command->data.bind_buffer.offset, (GLsizeiptr)command->data.bind_buffer.size);
				}
			}
			break;
		case RTGL_RECORDED_COMMAND_BIND_TEXTURE:
			if (program && program->location_occupied[command->data.bind_texture.address] && command->data.bind_texture.image && command->data.bind_texture.image->gl_texture) {
				struct rtgl_program_mapping* mapping = &program->mappings[command->data.bind_texture.address];
				if (mapping->kind == RTGL_LOCATION_MAPPING_STORAGE_TEXTURE_BUFFER) {
					glBindImageTexture(mapping->binding, command->data.bind_texture.image->gl_texture, 0, GL_FALSE, 0, GL_READ_WRITE, command->data.bind_texture.image->gl_internal_format);
				}
				if (mapping->kind == RTGL_LOCATION_MAPPING_TEXTURE) rtgl_bind_uniform_texture(ctx, program, mapping);
				GLuint sampler = 0;
				glCreateSamplers(1, &sampler);
				glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, command->data.bind_texture.mag_filter == RT_FILTER_NEAREST ? GL_NEAREST : GL_LINEAR);
				glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, rtgl_snapshot_sampler_filter(command->data.bind_texture.min_filter, command->data.bind_texture.mip_filter));
				glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, rtgl_snapshot_sampler_address(command->data.bind_texture.address_u));
				glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, rtgl_snapshot_sampler_address(command->data.bind_texture.address_v));
				glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, rtgl_snapshot_sampler_address(command->data.bind_texture.address_w));
				glSamplerParameterf(sampler, GL_TEXTURE_MIN_LOD, command->data.bind_texture.min_lod);
				glSamplerParameterf(sampler, GL_TEXTURE_MAX_LOD, command->data.bind_texture.max_lod);
				glSamplerParameterf(sampler, GL_TEXTURE_LOD_BIAS, command->data.bind_texture.lod_bias);
#if defined(GL_TEXTURE_MAX_ANISOTROPY)
				glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY, (GLfloat)command->data.bind_texture.max_anisotropy);
#endif
				sampler_snapshots.push_back(sampler);
				glBindTextureUnit(mapping->binding, command->data.bind_texture.image->gl_texture);
				glBindSampler(mapping->binding, sampler);
			}
			break;
		case RTGL_RECORDED_COMMAND_BIND_SAMPLER:
			if (program && program->location_occupied[command->data.bind_sampler.address]) {
				struct rtgl_program_mapping* mapping = &program->mappings[command->data.bind_sampler.address];
				rtgl_bind_uniform_texture(ctx, program, mapping);
				GLuint sampler = 0;
				glCreateSamplers(1, &sampler);
				glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, command->data.bind_sampler.mag_filter == RT_FILTER_NEAREST ? GL_NEAREST : GL_LINEAR);
				glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, rtgl_snapshot_sampler_filter(command->data.bind_sampler.min_filter, command->data.bind_sampler.mip_filter));
				glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, rtgl_snapshot_sampler_address(command->data.bind_sampler.address_u));
				glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, rtgl_snapshot_sampler_address(command->data.bind_sampler.address_v));
				glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, rtgl_snapshot_sampler_address(command->data.bind_sampler.address_w));
				glSamplerParameterf(sampler, GL_TEXTURE_MIN_LOD, command->data.bind_sampler.min_lod);
				glSamplerParameterf(sampler, GL_TEXTURE_MAX_LOD, command->data.bind_sampler.max_lod);
				glSamplerParameterf(sampler, GL_TEXTURE_LOD_BIAS, command->data.bind_sampler.lod_bias);
				sampler_snapshots.push_back(sampler);
				glBindSampler(mapping->binding, sampler);
			}
			break;
		case RTGL_RECORDED_COMMAND_VERTEX_BUFFER:
			if (program && program->location_occupied[command->data.vertex_buffer.address] && program->mappings[command->data.vertex_buffer.address].kind == RTGL_LOCATION_MAPPING_VERTEX_STREAM) {
				const u32 stream = program->mappings[command->data.vertex_buffer.address].binding;
				if (stream < RTGL_MAX_VERTEX_ATTRIBUTES) {
					vertex_storages[stream] = command->data.vertex_buffer.storage;
					vertex_offsets[stream] = command->data.vertex_buffer.offset;
				}
			}
			break;
		case RTGL_RECORDED_COMMAND_INDEX_BUFFER:
			index_storage = command->data.index_buffer.storage;
			index_offset = command->data.index_buffer.offset;
			index_format = command->data.index_buffer.format;
			break;
		case RTGL_RECORDED_COMMAND_DRAW:
			if (program && command->data.draw.vertex_count) {
				GLuint vao = 0;
				glCreateVertexArrays(1, &vao);
				rtgl_bind_vertex_layout(ctx, program, vertex_storages, vertex_offsets, vao);
				glBindVertexArray(vao);
				glDrawArrays(program->tessellated ? GL_PATCHES : GL_TRIANGLES, (GLint)command->data.draw.first_vertex, (GLsizei)command->data.draw.vertex_count);
				glBindVertexArray(0);
				glDeleteVertexArrays(1, &vao);
			}
			break;
		case RTGL_RECORDED_COMMAND_DRAW_INSTANCED:
			if (program && command->data.draw_instanced.vertex_count && command->data.draw_instanced.instance_count) {
				GLuint vao = 0;
				glCreateVertexArrays(1, &vao);
				rtgl_bind_vertex_layout(ctx, program, vertex_storages, vertex_offsets, vao);
				glBindVertexArray(vao);
				glDrawArraysInstancedBaseInstance(program->tessellated ? GL_PATCHES : GL_TRIANGLES, (GLint)command->data.draw_instanced.first_vertex, (GLsizei)command->data.draw_instanced.vertex_count, (GLsizei)command->data.draw_instanced.instance_count, command->data.draw_instanced.first_instance);
				glBindVertexArray(0);
				glDeleteVertexArrays(1, &vao);
			}
			break;
		case RTGL_RECORDED_COMMAND_DRAW_INDEXED:
		case RTGL_RECORDED_COMMAND_DRAW_INDEXED_INSTANCED:
			if (program && index_storage) {
				const bool instanced = command->kind == RTGL_RECORDED_COMMAND_DRAW_INDEXED_INSTANCED;
				const u32 index_count = instanced ? command->data.draw_indexed_instanced.index_count : command->data.draw_indexed.index_count;
				const u32 first_index = instanced ? command->data.draw_indexed_instanced.first_index : command->data.draw_indexed.first_index;
				const i32 vertex_offset = instanced ? command->data.draw_indexed_instanced.vertex_offset : command->data.draw_indexed.vertex_offset;
				const GLenum type = index_format == RT_INDEX_U32 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
				const usize index_size = index_format == RT_INDEX_U32 ? sizeof(u32) : sizeof(u16);
				GLuint vao = 0;
				glCreateVertexArrays(1, &vao);
				glVertexArrayElementBuffer(vao, index_storage->gl_buffer);
				rtgl_bind_vertex_layout(ctx, program, vertex_storages, vertex_offsets, vao);
				glBindVertexArray(vao);
				const void* indices = (const void*)(uintptr_t)(index_offset + (usize)first_index * index_size);
				if (instanced) {
					glDrawElementsInstancedBaseVertexBaseInstance(program->tessellated ? GL_PATCHES : GL_TRIANGLES, (GLsizei)index_count, type, indices, (GLsizei)command->data.draw_indexed_instanced.instance_count, vertex_offset, command->data.draw_indexed_instanced.first_instance);
				} else {
					glDrawElementsBaseVertex(program->tessellated ? GL_PATCHES : GL_TRIANGLES, (GLsizei)index_count, type, indices, vertex_offset);
				}
				glBindVertexArray(0);
				glDeleteVertexArrays(1, &vao);
			}
			break;
		case RTGL_RECORDED_COMMAND_DISPATCH:
			if (program) {
				glUseProgram(program->gl_program);
				glDispatchCompute((GLuint)command->data.dispatch.group_count_x, (GLuint)command->data.dispatch.group_count_y, (GLuint)command->data.dispatch.group_count_z);
			}
			break;
		case RTGL_RECORDED_COMMAND_END_RENDERING:
			framebuffer = NULL;
			color_image = NULL;
			memset(color_images, 0, sizeof(color_images));
			depth_image = NULL;
			stencil_image = NULL;
			color_count = 0;
			glDisable(GL_FRAMEBUFFER_SRGB);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			break;
		case RTGL_RECORDED_COMMAND_BUFFER_DATA:
			if (command->data.buffer_data.storage) {
				if (command->data.buffer_data.copy_source) {
					glMemoryBarrier(GL_ALL_BARRIER_BITS);
					memcpy(command->data.buffer_data.storage->shadow_data, command->data.buffer_data.copy_source->shadow_data, command->data.buffer_data.storage->size);
					glCopyNamedBufferSubData(command->data.buffer_data.copy_source->gl_buffer, command->data.buffer_data.storage->gl_buffer, 0, 0, (GLsizeiptr)command->data.buffer_data.storage->size);
				}
				memcpy(command->data.buffer_data.storage->shadow_data + command->data.buffer_data.range.offset, command->data.buffer_data.data, command->data.buffer_data.range.size);
				rtgl_execution_buffer_subdata(ctx, command->data.buffer_data.storage, command->data.buffer_data.range.offset, command->data.buffer_data.range.size, command->data.buffer_data.data);
			}
			break;
		case RTGL_RECORDED_COMMAND_BUFFER_COPY:
			if (command->data.buffer_copy.src && command->data.buffer_copy.dst) {
				glMemoryBarrier(GL_ALL_BARRIER_BITS);
				if (command->data.buffer_copy.dst_copy_source) {
					glMemoryBarrier(GL_ALL_BARRIER_BITS);
					memcpy(command->data.buffer_copy.dst->shadow_data, command->data.buffer_copy.dst_copy_source->shadow_data, command->data.buffer_copy.dst->size);
					glCopyNamedBufferSubData(command->data.buffer_copy.dst_copy_source->gl_buffer, command->data.buffer_copy.dst->gl_buffer, 0, 0, (GLsizeiptr)command->data.buffer_copy.dst->size);
				}
				memcpy(command->data.buffer_copy.dst->shadow_data + command->data.buffer_copy.dst_range.offset, command->data.buffer_copy.src->shadow_data + command->data.buffer_copy.src_range.offset, command->data.buffer_copy.src_range.size);
				glCopyNamedBufferSubData(command->data.buffer_copy.src->gl_buffer, command->data.buffer_copy.dst->gl_buffer, (GLintptr)command->data.buffer_copy.src_range.offset, (GLintptr)command->data.buffer_copy.dst_range.offset, (GLsizeiptr)command->data.buffer_copy.src_range.size);
			}
			break;
		case RTGL_RECORDED_COMMAND_BUFFER_COPY_TO_TEXTURE:
			if (command->data.buffer_copy_to_texture.src && command->data.buffer_copy_to_texture.dst) {
				const rt_texture_range range = command->data.buffer_copy_to_texture.dst_range;
				if (command->data.buffer_copy_to_texture.copy_source) {
					rtgl_texture_image_copy(command->data.buffer_copy_to_texture.dst, command->data.buffer_copy_to_texture.copy_source);
				}
				rtgl_execution_texture_subdata(ctx, command->data.buffer_copy_to_texture.dst, range, command->data.buffer_copy_to_texture.src->shadow_data + command->data.buffer_copy_to_texture.src_range.offset);
			}
			break;
		case RTGL_RECORDED_COMMAND_TEXTURE_DATA:
			if (command->data.texture_data.image) {
				const rt_texture_range range = command->data.texture_data.range;
				if (command->data.texture_data.copy_source) {
					rtgl_texture_image_copy(command->data.texture_data.image, command->data.texture_data.copy_source);
				}
				rtgl_execution_texture_subdata(ctx, command->data.texture_data.image, range, command->data.texture_data.data);
			}
			break;
		case RTGL_RECORDED_COMMAND_TEXTURE_COPY:
			if (command->data.texture_copy.src && command->data.texture_copy.dst) {
				const rt_texture_range src_range = command->data.texture_copy.src_range;
				const rt_texture_range dst_range = command->data.texture_copy.dst_range;
				if (command->data.texture_copy.dst_copy_source) {
					rtgl_texture_image_copy(command->data.texture_copy.dst, command->data.texture_copy.dst_copy_source);
				}
				rtgl_texture_copy_range(command->data.texture_copy.src, src_range, command->data.texture_copy.dst, dst_range);
			}
			break;
		case RTGL_RECORDED_COMMAND_TEXTURE_COPY_TO_BUFFER:
			if (command->data.texture_copy_to_buffer.src && command->data.texture_copy_to_buffer.dst) {
				const rt_texture_range range = command->data.texture_copy_to_buffer.src_range;
				glMemoryBarrier(GL_ALL_BARRIER_BITS);
				if (command->data.texture_copy_to_buffer.dst_copy_source) {
					glMemoryBarrier(GL_ALL_BARRIER_BITS);
					memcpy(command->data.texture_copy_to_buffer.dst->shadow_data, command->data.texture_copy_to_buffer.dst_copy_source->shadow_data, command->data.texture_copy_to_buffer.dst->size);
					glCopyNamedBufferSubData(command->data.texture_copy_to_buffer.dst_copy_source->gl_buffer, command->data.texture_copy_to_buffer.dst->gl_buffer, 0, 0, (GLsizeiptr)command->data.texture_copy_to_buffer.dst->size);
				}
				rtgl_execution_texture_read(ctx, command->data.texture_copy_to_buffer.src, range, command->data.texture_copy_to_buffer.dst->shadow_data + command->data.texture_copy_to_buffer.dst_range.offset, command->data.texture_copy_to_buffer.dst_range.size);
				rtgl_execution_buffer_subdata(ctx, command->data.texture_copy_to_buffer.dst, command->data.texture_copy_to_buffer.dst_range.offset, command->data.texture_copy_to_buffer.dst_range.size, command->data.texture_copy_to_buffer.dst->shadow_data + command->data.texture_copy_to_buffer.dst_range.offset);
			}
			break;
		case RTGL_RECORDED_COMMAND_BUFFER_BARRIER:
		case RTGL_RECORDED_COMMAND_TEXTURE_BARRIER: {
			const GLbitfield bits = rtgl_barrier_bits(command->data.barrier.dst);
			if (bits) {
				glMemoryBarrier(bits);
			}
			break;
		}
		}
		command_offset += command->size / sizeof(*command);
	}
	if (!sampler_snapshots.empty()) {
		glDeleteSamplers((GLsizei)sampler_snapshots.size(), sampler_snapshots.data());
	}
	for (rtgl_program_data_block& block : program_data_blocks) {
		glDeleteBuffers(1, &block.buffer);
	}
	rtgl_execution_lock(ctx);
	rtgl_execution_queue_complete_locked(queue, complete_value);
	rtgl_execution_unlock(ctx);
}
