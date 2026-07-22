#include "resource/command_buffer.h"

#include "context.h"
#include "error.h"
#include "execution.h"
#include "execution_internal.hpp"
#include "glad/gl.h"
#include "resource/resource.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static rtgl_recorded_command* rtgl_command_buffer_append(struct rtgl_command_buffer* command_buffer) {
	if (command_buffer->command_count == command_buffer->command_capacity) {
		u32 next_capacity = command_buffer->command_capacity ? command_buffer->command_capacity * 2 : 16;
		rtgl_recorded_command* next_commands = (rtgl_recorded_command*)realloc(command_buffer->commands, sizeof(*command_buffer->commands) * next_capacity);
		RTGL_CHECK_ALLOC(next_commands, sizeof(*command_buffer->commands) * (usize)next_capacity, "OpenGL recorded commands");
		if (!next_commands) {
			return NULL;
		}
		command_buffer->commands = next_commands;
		command_buffer->command_capacity = next_capacity;
	}

	rtgl_recorded_command* command = &command_buffer->commands[command_buffer->command_count++];
	memset(command, 0, sizeof(*command));
	return command;
}

rt_command_buffer rtCommandBufferCreate(void) {
	return rtgl_command_buffer_to_handle(rtgl_command_buffer_create(rtgl_get_current_context()));
}

void rtCommandBufferDestroy(rt_command_buffer command_buffer) {
	rtgl_command_buffer_destroy(rtgl_get_current_context(), rtgl_command_buffer_from_handle(command_buffer));
}

void rtCmdBegin(rt_command_buffer command_buffer, rt_queue queue) {
	struct rtgl_command_buffer* internal = rtgl_command_buffer_from_handle(command_buffer);

	rtgl_command_buffer_clear_recorded_resources(internal);
	internal->queue = rtgl_queue_from_handle(queue);
	internal->recording = true;
	internal->rendering = false;
}

void rtCmdBeginRendering(rt_command_buffer command_buffer, rt_framebuffer framebuffer) {
	struct rtgl_command_buffer* internal = rtgl_command_buffer_from_handle(command_buffer);
	rtgl_recorded_command* command;

	if (!internal->recording) {
		rtgl_throwf(RT_IMPROPER_USAGE, "rtCmdBeginRendering called before rtCmdBegin");
		return;
	}
	command = rtgl_command_buffer_append(internal);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_BEGIN_RENDERING;
	command->begin_rendering.framebuffer = rtgl_framebuffer_from_handle(framebuffer);
	if (command->begin_rendering.framebuffer) {
		rtgl_retain_resource(command->begin_rendering.framebuffer);
		command->begin_rendering.color_view = command->begin_rendering.framebuffer->color_views[0];
		command->begin_rendering.depth_view = command->begin_rendering.framebuffer->depth_view;
		rtgl_retain_resource(command->begin_rendering.color_view);
		rtgl_retain_resource(command->begin_rendering.depth_view);
	}
	internal->rendering = true;
}

void rtCmdClearColor(rt_command_buffer command_buffer, u32 color_index, f32 r, f32 g, f32 b, f32 a) {
	struct rtgl_command_buffer* internal = rtgl_command_buffer_from_handle(command_buffer);
	rtgl_recorded_command* command;

	if (!internal->rendering) {
		rtgl_throwf(RT_IMPROPER_USAGE, "rtCmdClearColor called outside rendering");
		return;
	}
	command = rtgl_command_buffer_append(internal);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_CLEAR_COLOR;
	command->clear_color.color_index = color_index;
	command->clear_color.values[0] = r;
	command->clear_color.values[1] = g;
	command->clear_color.values[2] = b;
	command->clear_color.values[3] = a;
}

void rtCmdEndRendering(rt_command_buffer command_buffer) {
	struct rtgl_command_buffer* internal = rtgl_command_buffer_from_handle(command_buffer);

	if (!internal->rendering) {
		rtgl_throwf(RT_IMPROPER_USAGE, "rtCmdEndRendering called outside rendering");
		return;
	}
	rtgl_recorded_command* command = rtgl_command_buffer_append(internal);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_END_RENDERING;
	internal->rendering = false;
}

void rtCmdEnd(rt_command_buffer command_buffer) {
	struct rtgl_command_buffer* internal = rtgl_command_buffer_from_handle(command_buffer);

	internal->recording = false;
	internal->rendering = false;
}

void rtCmdClearDepth(rt_command_buffer command_buffer, f32 depth) {
	struct rtgl_command_buffer* internal = rtgl_command_buffer_from_handle(command_buffer);
	rtgl_recorded_command* command;

	if (!internal->rendering) {
		rtgl_throwf(RT_IMPROPER_USAGE, "rtCmdClearDepth called outside rendering");
		return;
	}
	command = rtgl_command_buffer_append(internal);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_CLEAR_DEPTH;
	command->clear_depth.depth = depth;
}

void rtCmdClearStencil(rt_command_buffer, u32) {
	rtgl_throwf(RT_UNSUPPORTED_FEATURE, "OpenGL stencil clear is not implemented");
}

void rtCmdUseGraphicsProgram(rt_command_buffer command_buffer, rt_graphics_program program) {
	struct rtgl_command_buffer* internal = rtgl_command_buffer_from_handle(command_buffer);
	rtgl_recorded_command* command = rtgl_command_buffer_append(internal);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_USE_GRAPHICS_PROGRAM;
	command->use_graphics_program.program = rtgl_graphics_program_from_handle(program);
	if (command->use_graphics_program.program) {
		rtgl_retain_resource(command->use_graphics_program.program);
	}
}

void rtCmdSetScissor(rt_command_buffer command_buffer, u32 x, u32 y, u32 width, u32 height) {
	struct rtgl_command_buffer* internal = rtgl_command_buffer_from_handle(command_buffer);
	rtgl_recorded_command* command = rtgl_command_buffer_append(internal);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_SET_SCISSOR;
	command->set_scissor.x = x;
	command->set_scissor.y = y;
	command->set_scissor.width = width;
	command->set_scissor.height = height;
}

void rtCmdUniformBuffer(rt_command_buffer command_buffer, rt_uniform_location location, rt_buffer buffer, u64 offset, u64 size) {
	struct rtgl_command_buffer* internal = rtgl_command_buffer_from_handle(command_buffer);
	rtgl_recorded_command* command = rtgl_command_buffer_append(internal);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_UNIFORM_BUFFER;
	command->uniform_buffer.location = (rtgl_uniform_location*)location;
	command->uniform_buffer.buffer = rtgl_buffer_from_handle(buffer);
	command->uniform_buffer.offset = offset;
	command->uniform_buffer.size = size;
	if (command->uniform_buffer.buffer) {
		rtgl_retain_resource(command->uniform_buffer.buffer);
	}
}

void rtCmdUniformTexture(rt_command_buffer command_buffer, rt_uniform_location location, rt_texture_view texture_view) {
	struct rtgl_command_buffer* internal = rtgl_command_buffer_from_handle(command_buffer);
	rtgl_recorded_command* command = rtgl_command_buffer_append(internal);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_UNIFORM_TEXTURE;
	command->uniform_texture.location = (rtgl_uniform_location*)location;
	command->uniform_texture.texture_view = rtgl_texture_view_from_handle(texture_view);
	if (command->uniform_texture.texture_view) {
		rtgl_retain_resource(command->uniform_texture.texture_view);
	}
}

void rtCmdStorageBuffer(rt_command_buffer command_buffer, u32 binding, rt_buffer buffer, u64 offset, u64 size) {
	struct rtgl_command_buffer* internal = rtgl_command_buffer_from_handle(command_buffer);
	rtgl_recorded_command* command = rtgl_command_buffer_append(internal);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_STORAGE_BUFFER;
	command->storage_buffer.binding = binding;
	command->storage_buffer.buffer = rtgl_buffer_from_handle(buffer);
	command->storage_buffer.offset = offset;
	command->storage_buffer.size = size;
	if (command->storage_buffer.buffer) {
		rtgl_retain_resource(command->storage_buffer.buffer);
	}
}

void rtCmdStorageTexture(rt_command_buffer, u32, rt_texture_view) {
	rtgl_throwf(RT_UNSUPPORTED_FEATURE, "OpenGL storage textures are not implemented");
}

void rtCmdBindVertexBuffer(rt_command_buffer command_buffer, rt_buffer buffer, u64 offset) {
	struct rtgl_command_buffer* internal = rtgl_command_buffer_from_handle(command_buffer);
	rtgl_recorded_command* command = rtgl_command_buffer_append(internal);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_BIND_VERTEX_BUFFER;
	command->bind_vertex_buffer.buffer = rtgl_buffer_from_handle(buffer);
	command->bind_vertex_buffer.offset = offset;
	if (command->bind_vertex_buffer.buffer) {
		rtgl_retain_resource(command->bind_vertex_buffer.buffer);
	}
}

void rtCmdDraw(rt_command_buffer command_buffer, u32 vertex_count, u32 first_vertex) {
	struct rtgl_command_buffer* internal = rtgl_command_buffer_from_handle(command_buffer);
	rtgl_recorded_command* command = rtgl_command_buffer_append(internal);
	if (!command) {
		return;
	}
	command->kind = RTGL_RECORDED_COMMAND_DRAW;
	command->draw.vertex_count = vertex_count;
	command->draw.first_vertex = first_vertex;
}

RTGL_DEFINE_RESOURCE_PRIVATE(command_buffer)

void rtgl_command_buffer_init(struct rtgl_context* ctx, struct rtgl_command_buffer* command_buffer) {
	rtgl_init_resource_base(ctx, RTGL_RESOURCE_BASE(command_buffer), RTGL_RESOURCE_COMMAND_BUFFER);
}

void rtgl_command_buffer_finish(struct rtgl_command_buffer* command_buffer) {
	rtgl_command_buffer_clear_recorded_resources(command_buffer);
	free(command_buffer->commands);
	command_buffer->commands = NULL;
	command_buffer->command_capacity = 0;
	rtgl_finish_resource_base(RTGL_RESOURCE_BASE(command_buffer));
}

struct rtgl_timepoint rtgl_command_buffer_submit(struct rtgl_context* ctx, struct rtgl_queue* queue, struct rtgl_command_buffer* command_buffer) {
	struct rtgl_timepoint done = rtgl_queue_signal(queue);

	if (!command_buffer) {
		rtgl_execution_queue_complete(ctx, queue, done.value);
		return done;
	}

	rtgl_retain_resource(command_buffer);
	if (!rtgl_execution_submit_async(ctx, [command_buffer, queue, value = done.value](struct rtgl_context* exec_ctx) {
		rtgl_command_buffer_execute(exec_ctx, command_buffer, queue, value);
		rtgl_resource_release(RTGL_RESOURCE_BASE(command_buffer));
	})) {
		rtgl_resource_release(RTGL_RESOURCE_BASE(command_buffer));
		rtgl_execution_queue_complete(ctx, queue, done.value);
	}
	return done;
}

void rtgl_command_buffer_clear_recorded_resources(struct rtgl_command_buffer* command_buffer) {
	if (!command_buffer) {
		return;
	}
	for (u32 i = 0; i < command_buffer->command_count; i++) {
		rtgl_recorded_command* command = &command_buffer->commands[i];
		if (command->kind == RTGL_RECORDED_COMMAND_BEGIN_RENDERING) {
			rtgl_release_resource(command->begin_rendering.color_view);
			rtgl_release_resource(command->begin_rendering.depth_view);
			rtgl_release_resource(command->begin_rendering.framebuffer);
		} else if (command->kind == RTGL_RECORDED_COMMAND_USE_GRAPHICS_PROGRAM) {
			rtgl_release_resource(command->use_graphics_program.program);
		} else if (command->kind == RTGL_RECORDED_COMMAND_UNIFORM_BUFFER) {
			rtgl_release_resource(command->uniform_buffer.buffer);
		} else if (command->kind == RTGL_RECORDED_COMMAND_UNIFORM_TEXTURE) {
			rtgl_release_resource(command->uniform_texture.texture_view);
		} else if (command->kind == RTGL_RECORDED_COMMAND_STORAGE_BUFFER) {
			rtgl_release_resource(command->storage_buffer.buffer);
		} else if (command->kind == RTGL_RECORDED_COMMAND_BIND_VERTEX_BUFFER) {
			rtgl_release_resource(command->bind_vertex_buffer.buffer);
		}
	}
	command_buffer->command_count = 0;
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

static void rtgl_bind_uniform_block(rtgl_uniform_location* location) {
	if (!location || !location->program || !location->program->gl_program || location->kind != RTGL_UNIFORM_LOCATION_UNIFORM_BUFFER) {
		return;
	}
	const GLuint block = glGetUniformBlockIndex(location->program->gl_program, location->name);
	if (block != GL_INVALID_INDEX) {
		glUniformBlockBinding(location->program->gl_program, block, location->binding);
	}
}

static void rtgl_bind_uniform_texture(rtgl_uniform_location* location) {
	if (!location || !location->program || !location->program->gl_program) {
		return;
	}
	location->gl_location = glGetUniformLocation(location->program->gl_program, location->name);
	if (location->gl_location >= 0) {
		struct rtgl_context* ctx = rtgl_get_current_context();
		if (ctx && ctx->execution.separate_shader_objects) {
			glProgramUniform1i(location->program->gl_program, location->gl_location, (GLint)location->binding);
		} else {
			glUniform1i(location->gl_location, (GLint)location->binding);
		}
	}
}

static void rtgl_command_buffer_bind_vertex_layout(struct rtgl_context* ctx, struct rtgl_graphics_program* program, struct rtgl_buffer* vertex_buffer, u64 offset, GLuint vao) {
	if (!program || !vertex_buffer || !program->vertex_layout.attribute_count) {
		return;
	}

	if (ctx->execution.direct_state_access) {
		glVertexArrayVertexBuffer(vao, 0, vertex_buffer->gl_buffer, (GLintptr)offset, (GLsizei)program->vertex_layout.stride);
	} else {
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer->gl_buffer);
	}
	for (u32 i = 0; i < program->vertex_layout.attribute_count; i++) {
		const rt_vertex_attribute* attribute = &program->vertex_attributes[i];
		GLint components = rtgl_vertex_attribute_components(attribute->format);
		if (!components) {
			rtgl_throwf(RT_UNSUPPORTED_FEATURE, "unsupported OpenGL vertex attribute format");
			return;
		}
		if (ctx->execution.direct_state_access) {
			glEnableVertexArrayAttrib(vao, i);
			glVertexArrayAttribFormat(vao, i, components, GL_FLOAT, GL_FALSE, (GLuint)attribute->offset);
			glVertexArrayAttribBinding(vao, i, 0);
		} else {
			glEnableVertexAttribArray(i);
			glVertexAttribPointer(i, components, GL_FLOAT, GL_FALSE, (GLsizei)program->vertex_layout.stride, (const void*)(uintptr_t)(offset + attribute->offset));
		}
	}
	if (!ctx->execution.direct_state_access) {
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
}

void rtgl_command_buffer_execute(struct rtgl_context* ctx, struct rtgl_command_buffer* command_buffer, struct rtgl_queue* queue, u64 complete_value) {
	struct rtgl_framebuffer* framebuffer = NULL;
	struct rtgl_texture_view* depth_view = NULL;
	struct rtgl_graphics_program* program = NULL;
	struct rtgl_buffer* vertex_buffer = NULL;
	u64 vertex_buffer_offset = 0;
	GLuint vao = 0;

	for (u32 i = 0; i < command_buffer->command_count; i++) {
		const rtgl_recorded_command* command = &command_buffer->commands[i];
		switch (command->kind) {
		case RTGL_RECORDED_COMMAND_BEGIN_RENDERING:
			framebuffer = command->begin_rendering.framebuffer;
			depth_view = command->begin_rendering.depth_view;
			if (framebuffer && rtgl_texture_view_valid(command->begin_rendering.color_view)) {
				GLsizei width = (GLsizei)command->begin_rendering.color_view->image->width;
				GLsizei height = (GLsizei)command->begin_rendering.color_view->image->height;
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer->gl_framebuffer);
				glViewport(0, 0, width, height);
				if (command->begin_rendering.color_view->image->gl_internal_format == GL_SRGB8_ALPHA8) {
					glEnable(GL_FRAMEBUFFER_SRGB);
				} else {
					glDisable(GL_FRAMEBUFFER_SRGB);
				}
				glDisable(GL_SCISSOR_TEST);
				glDisable(GL_STENCIL_TEST);
				glDisable(GL_BLEND);
				glDisable(GL_CULL_FACE);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				glDepthMask(GL_TRUE);
				if (rtgl_texture_view_valid(depth_view)) {
					glEnable(GL_DEPTH_TEST);
					glDepthFunc(GL_LESS);
				} else {
					glDisable(GL_DEPTH_TEST);
				}
			}
			break;
		case RTGL_RECORDED_COMMAND_CLEAR_COLOR:
			if (framebuffer && command->clear_color.color_index < framebuffer->color_texture_count) {
				if (ctx->execution.direct_state_access) {
					glClearNamedFramebufferfv(framebuffer->gl_framebuffer, GL_COLOR, (GLint)command->clear_color.color_index, command->clear_color.values);
				} else {
					glClearBufferfv(GL_COLOR, (GLint)command->clear_color.color_index, command->clear_color.values);
				}
			}
			break;
		case RTGL_RECORDED_COMMAND_CLEAR_DEPTH:
			if (framebuffer && rtgl_texture_view_valid(depth_view)) {
				if (ctx->execution.direct_state_access) {
					glClearNamedFramebufferfv(framebuffer->gl_framebuffer, GL_DEPTH, 0, &command->clear_depth.depth);
				} else {
					glClearBufferfv(GL_DEPTH, 0, &command->clear_depth.depth);
				}
			}
			break;
		case RTGL_RECORDED_COMMAND_USE_GRAPHICS_PROGRAM:
			program = command->use_graphics_program.program;
			rtgl_graphics_program_prepare(ctx, program);
			if (program && program->gl_program) {
				glUseProgram(program->gl_program);
				if (program->cull_mode == RT_CULL_NONE) {
					glDisable(GL_CULL_FACE);
				} else {
					glEnable(GL_CULL_FACE);
					glCullFace(program->cull_mode == RT_CULL_FRONT ? GL_FRONT : GL_BACK);
				}
				glFrontFace(program->front_face == RT_FRONT_FACE_CW ? GL_CW : GL_CCW);
				glPolygonMode(GL_FRONT_AND_BACK, program->fill_mode == RT_FILL_WIREFRAME ? GL_LINE : GL_FILL);
				if (program->blend_enabled) {
					glEnable(GL_BLEND);
					glBlendFuncSeparate(
						rtgl_blend_factor(program->src_color_blend),
						rtgl_blend_factor(program->dst_color_blend),
						rtgl_blend_factor(program->src_alpha_blend),
						rtgl_blend_factor(program->dst_alpha_blend));
					glBlendEquationSeparate(
						rtgl_blend_op(program->color_blend_op),
						rtgl_blend_op(program->alpha_blend_op));
				} else {
					glDisable(GL_BLEND);
				}
			}
			break;
		case RTGL_RECORDED_COMMAND_SET_SCISSOR:
			glEnable(GL_SCISSOR_TEST);
			if (framebuffer && framebuffer->color_views[0]) {
				const u32 framebuffer_height = framebuffer->color_views[0]->image ? framebuffer->color_views[0]->image->height : 0;
				const u32 y = command->set_scissor.y + command->set_scissor.height <= framebuffer_height
					? framebuffer_height - command->set_scissor.y - command->set_scissor.height
					: 0;
				glScissor((GLint)command->set_scissor.x, (GLint)y, (GLsizei)command->set_scissor.width, (GLsizei)command->set_scissor.height);
			} else {
				glScissor((GLint)command->set_scissor.x, (GLint)command->set_scissor.y, (GLsizei)command->set_scissor.width, (GLsizei)command->set_scissor.height);
			}
			break;
		case RTGL_RECORDED_COMMAND_UNIFORM_BUFFER:
			if (command->uniform_buffer.location && command->uniform_buffer.buffer) {
				rtgl_bind_uniform_block(command->uniform_buffer.location);
				glBindBufferRange(GL_UNIFORM_BUFFER, command->uniform_buffer.location->binding, command->uniform_buffer.buffer->gl_buffer, (GLintptr)command->uniform_buffer.offset, (GLsizeiptr)command->uniform_buffer.size);
			}
			break;
		case RTGL_RECORDED_COMMAND_UNIFORM_TEXTURE:
			if (command->uniform_texture.location && rtgl_texture_view_valid(command->uniform_texture.texture_view)) {
				rtgl_bind_uniform_texture(command->uniform_texture.location);
				if (ctx->execution.direct_state_access) {
					glBindTextureUnit(command->uniform_texture.location->binding, command->uniform_texture.texture_view->image->gl_texture);
				} else {
					glActiveTexture(GL_TEXTURE0 + command->uniform_texture.location->binding);
					glBindTexture(GL_TEXTURE_2D, command->uniform_texture.texture_view->image->gl_texture);
				}
			}
			break;
		case RTGL_RECORDED_COMMAND_STORAGE_BUFFER:
			if (command->storage_buffer.buffer) {
				if (ctx->execution.shader_storage_buffer) {
					glBindBufferRange(GL_SHADER_STORAGE_BUFFER, command->storage_buffer.binding, command->storage_buffer.buffer->gl_buffer, (GLintptr)command->storage_buffer.offset, (GLsizeiptr)command->storage_buffer.size);
				} else {
					rtgl_throwf(RT_UNSUPPORTED_FEATURE, "OpenGL storage buffers require GL 4.3 or ARB_shader_storage_buffer_object");
				}
			}
			break;
		case RTGL_RECORDED_COMMAND_BIND_VERTEX_BUFFER:
			vertex_buffer = command->bind_vertex_buffer.buffer;
			vertex_buffer_offset = command->bind_vertex_buffer.offset;
			break;
		case RTGL_RECORDED_COMMAND_DRAW:
			if (program && vertex_buffer && command->draw.vertex_count) {
				if (ctx->execution.direct_state_access) {
					glCreateVertexArrays(1, &vao);
				} else {
					glGenVertexArrays(1, &vao);
				}
				rtgl_command_buffer_bind_vertex_layout(ctx, program, vertex_buffer, vertex_buffer_offset, vao);
				glBindVertexArray(vao);
				glDrawArrays(GL_TRIANGLES, (GLint)command->draw.first_vertex, (GLsizei)command->draw.vertex_count);
				glBindVertexArray(0);
				glDeleteVertexArrays(1, &vao);
				vao = 0;
			}
			break;
		case RTGL_RECORDED_COMMAND_END_RENDERING:
			framebuffer = NULL;
			glDisable(GL_FRAMEBUFFER_SRGB);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			break;
		}
	}

	rtgl_execution_lock(ctx);
	rtgl_execution_queue_complete_locked(queue, complete_value);
	rtgl_execution_unlock(ctx);
}
