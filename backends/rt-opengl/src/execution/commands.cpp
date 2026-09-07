#include "execution/internal.hpp"

#include "glad/gl.h"
#include "glfw.h"
#include "resource/buffer.h"
#include "resource/framebuffer.h"
#include "resource/program.h"
#include "resource/queue.h"
#include "resource/resource.h"
#include "resource/swapchain.h"
#include "resource/texture.h"
#include "transpiler/transpiler.h"

#include <string.h>

static GLenum rtgl_buffer_gl_usage(void) {
	return GL_DYNAMIC_DRAW;
}

void rtgl_execution_buffer_create(struct rtgl_context* ctx, struct rtgl_buffer_storage* storage) {
	rtgl_execution_submit_sync(ctx, [storage](struct rtgl_context*) {
		glCreateBuffers(1, &storage->gl_buffer);
		glCreateTextures(GL_TEXTURE_BUFFER, 1, &storage->gl_texture_buffer);
	});
}

void rtgl_execution_buffer_delete(struct rtgl_context* ctx, struct rtgl_buffer_storage* storage) {
	rtgl_execution_submit_sync(ctx, [storage](struct rtgl_context*) {
		if (storage->gl_buffer) {
			glDeleteBuffers(1, &storage->gl_buffer);
			storage->gl_buffer = 0;
		}
		if (storage->gl_texture_buffer) {
			glDeleteTextures(1, &storage->gl_texture_buffer);
			storage->gl_texture_buffer = 0;
		}
	});
}

void rtgl_execution_buffer_data(struct rtgl_context* ctx, struct rtgl_buffer_storage* storage, usize size, const u08* bytes) {
	rtgl_execution_submit_sync(ctx, [storage, size, bytes](struct rtgl_context*) {
		glMemoryBarrier(GL_ALL_BARRIER_BITS);
		glNamedBufferData(storage->gl_buffer, (GLsizeiptr)size, bytes, rtgl_buffer_gl_usage());
	});
}

void rtgl_execution_buffer_subdata(struct rtgl_context* ctx, struct rtgl_buffer_storage* storage, u64 offset, u64 size, const u08* bytes) {
	rtgl_execution_submit_sync(ctx, [storage, offset, size, bytes](struct rtgl_context*) {
		glMemoryBarrier(GL_ALL_BARRIER_BITS);
		glNamedBufferSubData(storage->gl_buffer, (GLintptr)offset, (GLsizeiptr)size, bytes);
	});
}

void rtgl_execution_buffer_read(struct rtgl_context* ctx, struct rtgl_buffer_storage* storage, u64 offset, u64 size, u08* bytes) {
	rtgl_execution_submit_sync(ctx, [storage, offset, size, bytes](struct rtgl_context*) {
		glMemoryBarrier(GL_ALL_BARRIER_BITS);
		glGetNamedBufferSubData(storage->gl_buffer, (GLintptr)offset, (GLsizeiptr)size, bytes);
	});
}

void rtgl_execution_framebuffer_create(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer) {
	rtgl_execution_submit_sync(ctx, [framebuffer](struct rtgl_context*) {
		glCreateFramebuffers(1, &framebuffer->gl_framebuffer);
	});
}

void rtgl_execution_framebuffer_delete(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer) {
	rtgl_execution_submit_sync(ctx, [framebuffer](struct rtgl_context*) {
		if (framebuffer->gl_framebuffer) {
			glDeleteFramebuffers(1, &framebuffer->gl_framebuffer);
			framebuffer->gl_framebuffer = 0;
		}
	});
}

void rtgl_execution_framebuffer_attach_color(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer, u32 slot, struct rtgl_texture_view* view) {
	rtgl_execution_submit_sync(ctx, [framebuffer, slot, view](struct rtgl_context*) {
		GLuint texture = view && view->image ? view->image->gl_texture : 0;
		glNamedFramebufferTexture(framebuffer->gl_framebuffer, GL_COLOR_ATTACHMENT0 + slot, texture, 0);
		GLenum draw_buffers[RTGL_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
		for (u32 index = 0; index < framebuffer->color_texture_count; index++) {
			draw_buffers[index] = framebuffer->color_views[index] && framebuffer->color_views[index]->image ? GL_COLOR_ATTACHMENT0 + index : GL_NONE;
		}
		glNamedFramebufferDrawBuffers(framebuffer->gl_framebuffer, (GLsizei)framebuffer->color_texture_count, draw_buffers);
		if (texture) {
			GLenum status = glCheckNamedFramebufferStatus(framebuffer->gl_framebuffer, GL_FRAMEBUFFER);
			if (status != GL_FRAMEBUFFER_COMPLETE) {
				rtgl_throwf(RT_INITIALIZATION_FAILED, "OpenGL framebuffer is incomplete: 0x%04x", status);
			}
		}
	});
}

void rtgl_execution_framebuffer_attach_depth(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer, struct rtgl_texture_view* view) {
	rtgl_execution_submit_sync(ctx, [framebuffer, view](struct rtgl_context*) {
		GLuint texture = view && view->image ? view->image->gl_texture : 0;
		glNamedFramebufferTexture(framebuffer->gl_framebuffer, GL_DEPTH_ATTACHMENT, texture, 0);
		if (framebuffer->color_texture_count && texture) {
			GLenum status = glCheckNamedFramebufferStatus(framebuffer->gl_framebuffer, GL_FRAMEBUFFER);
			if (status != GL_FRAMEBUFFER_COMPLETE) {
				rtgl_throwf(RT_INITIALIZATION_FAILED, "OpenGL framebuffer with depth is incomplete: 0x%04x", status);
			}
		}
	});
}

static GLuint rtgl_execution_compile_spirv_shader(GLenum stage, const char* entry_point, const u32* words, u64 word_count) {
	if (!entry_point || !entry_point[0] || !words || word_count == 0) {
		return 0;
	}
	GLuint shader = glCreateShader(stage);
	glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V, words, (GLsizei)(word_count * sizeof(u32)));
	glSpecializeShader(shader, entry_point, 0, NULL, NULL);
	GLint ok = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[2048] = { 0 };
		glGetShaderInfoLog(shader, sizeof(log), NULL, log);
		rtgl_throwf(RT_SHADER_COMPILATION_FAILED, "OpenGL SPIR-V specialization failed: %s", log);
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

static rtgl_location_kind rtgl_location_mapping_kind_from_spirv(rt_spirv_location_kind kind) {
	switch (kind) {
	case RT_SPIRV_UNIFORM_DATA:
		return RTGL_LOCATION_MAPPING_UNIFORM_DATA;
	case RT_SPIRV_STORAGE_DATA:
		return RTGL_LOCATION_MAPPING_STORAGE_DATA;
	case RT_SPIRV_UNIFORM_BUFFER:
		return RTGL_LOCATION_MAPPING_UNIFORM_BUFFER;
	case RT_SPIRV_STORAGE_BUFFER:
		return RTGL_LOCATION_MAPPING_STORAGE_BUFFER;
	case RT_SPIRV_SAMPLED_TEXTURE:
	case RT_SPIRV_SAMPLER:
		return RTGL_LOCATION_MAPPING_TEXTURE;
	case RT_SPIRV_STORAGE_TEXTURE:
		return RTGL_LOCATION_MAPPING_STORAGE_TEXTURE_BUFFER;
	case RT_SPIRV_INPUT_ATTACHMENT:
		break;
	}
	return RTGL_LOCATION_MAPPING_UNIFORM_BUFFER;
}

enum rtgl_spirv_opcode {
	RTGL_SPIRV_OPCODE_NAME = 5,
	RTGL_SPIRV_OPCODE_EXECUTION_MODE = 16,
	RTGL_SPIRV_OPCODE_DECORATE = 71,
};

enum rtgl_spirv_decoration {
	RTGL_SPIRV_DECORATION_LOCATION = 30,
};

enum rtgl_spirv_execution_mode {
	RTGL_SPIRV_EXECUTION_MODE_OUTPUT_VERTICES = 26,
};

static u32 rtgl_spirv_tessellation_control_points(const u32* words, size_t word_count) {
	if (!words || word_count < 5) {
		return 0;
	}
	for (size_t word_index = 5; word_index < word_count;) {
		const u32 instruction = words[word_index];
		const u32 instruction_word_count = instruction >> 16;
		const u32 opcode = instruction & 0xffff;
		if (!instruction_word_count || word_index + instruction_word_count > word_count) {
			return 0;
		}
		if (opcode == RTGL_SPIRV_OPCODE_EXECUTION_MODE && instruction_word_count == 4 &&
			words[word_index + 2] == RTGL_SPIRV_EXECUTION_MODE_OUTPUT_VERTICES) {
			return words[word_index + 3];
		}
		word_index += instruction_word_count;
	}
	return 0;
}

static void rtgl_program_reflect_fragment_outputs(struct rtgl_program* program, const rt_spirv_program* translation) {
	size_t word_count = 0;
	const u32* words = rt_spirv_stage_words(translation, RT_SPIRV_FRAGMENT, &word_count);
	if (!words || word_count < 5) {
		return;
	}

	for (size_t word_index = 5; word_index < word_count;) {
		const u32 instruction = words[word_index];
		const u32 instruction_word_count = instruction >> 16;
		const u32 opcode = instruction & 0xffff;
		if (!instruction_word_count || word_index + instruction_word_count > word_count) {
			return;
		}
		if (opcode != RTGL_SPIRV_OPCODE_NAME || instruction_word_count < 3) {
			word_index += instruction_word_count;
			continue;
		}

		const char* shader_name = (const char*)&words[word_index + 2];
		if (strncmp(shader_name, "out_", 4) != 0) {
			word_index += instruction_word_count;
			continue;
		}

		u32 shader_location = 0;
		bool shader_location_found = false;
		for (size_t decoration_index = 5; decoration_index < word_count;) {
			const u32 decoration = words[decoration_index];
			const u32 decoration_word_count = decoration >> 16;
			const u32 decoration_opcode = decoration & 0xffff;
			if (!decoration_word_count || decoration_index + decoration_word_count > word_count) {
				return;
			}
			if (decoration_opcode == RTGL_SPIRV_OPCODE_DECORATE && decoration_word_count >= 4 && words[decoration_index + 1] == words[word_index + 1] && words[decoration_index + 2] == RTGL_SPIRV_DECORATION_LOCATION) {
				shader_location = words[decoration_index + 3];
				shader_location_found = true;
				break;
			}
			decoration_index += decoration_word_count;
		}
		if (shader_location_found) {
			const char* name = shader_name + 4;
			struct rt_location_t* location = rtgl_program_allocate_location(program, !name[0]);
			if (!location) {
				word_index += instruction_word_count;
				continue;
			}
			struct rtgl_program_mapping* mapping = rtgl_program_mapping(program, location);
			strncpy(mapping->name, name, sizeof(mapping->name) - 1);
			mapping->binding = shader_location;
			mapping->gl_location = (GLint)shader_location;
			mapping->kind = RTGL_LOCATION_MAPPING_OUTPUT;
		}
		word_index += instruction_word_count;
	}
}

static void rtgl_program_reflect_spirv(struct rtgl_program* program, const rt_spirv_program* translation) {
	rtgl_program_clear_locations(program);
	const u32 resource_count = rt_spirv_location_count(translation);
	for (u32 i = 0; i < resource_count; i++) {
		rt_spirv_location_info resource = { 0 };
		if (!rt_spirv_location(translation, i, &resource) || !resource.name || resource.descriptor_set != 0) {
			continue;
		}
		struct rt_location_t* location = rtgl_program_allocate_location(program, false);
		if (!location) {
			break;
		}
		struct rtgl_program_mapping* mapping = rtgl_program_mapping(program, location);
		strncpy(mapping->name, resource.name, sizeof(mapping->name) - 1);
		mapping->binding = resource.binding;
		mapping->gl_location = -1;
		mapping->kind = rtgl_location_mapping_kind_from_spirv(resource.kind);
		mapping->byte_offset = resource.offset;
		mapping->byte_size = resource.size;
		mapping->block_size = resource.block_size;
	}
	rtgl_program_reflect_fragment_outputs(program, translation);
}

static bool rtgl_execution_program_finalize_spirv(struct rtgl_context* exec_ctx, struct rtgl_program* program) {
	(void)exec_ctx;
	if (!program->source_bytes || program->source_size == 0) {
		return false;
	}
	char message[2048] = { 0 };
	rt_spirv_program* translation = NULL;
	rt_spirv_status status = rt_spirv_transpile(program->source_bytes, program->source_size, program->entry_point, &translation, message, sizeof(message));
	if (status != RT_SPIRV_SUCCESS) {
		rtgl_throwf(RT_SHADER_COMPILATION_FAILED, "OpenGL RTSL to SPIR-V failed: %s", message);
		return false;
	}

	size_t vertex_word_count = 0;
	size_t tessellation_control_word_count = 0;
	size_t tessellation_evaluation_word_count = 0;
	size_t fragment_word_count = 0;
	size_t compute_word_count = 0;
	const u32* vertex_words = rt_spirv_stage_words(translation, RT_SPIRV_VERTEX, &vertex_word_count);
	const u32* tessellation_control_words = rt_spirv_stage_words(translation, RT_SPIRV_TESSELLATION_CONTROL, &tessellation_control_word_count);
	const u32* tessellation_evaluation_words = rt_spirv_stage_words(translation, RT_SPIRV_TESSELLATION_EVALUATION, &tessellation_evaluation_word_count);
	const u32* fragment_words = rt_spirv_stage_words(translation, RT_SPIRV_FRAGMENT, &fragment_word_count);
	const u32* compute_words = rt_spirv_stage_words(translation, RT_SPIRV_COMPUTE, &compute_word_count);
	const char* vertex_entry_point = rt_spirv_stage_entry_point(translation, RT_SPIRV_VERTEX);
	const char* tessellation_control_entry_point = rt_spirv_stage_entry_point(translation, RT_SPIRV_TESSELLATION_CONTROL);
	const char* tessellation_evaluation_entry_point = rt_spirv_stage_entry_point(translation, RT_SPIRV_TESSELLATION_EVALUATION);
	const char* fragment_entry_point = rt_spirv_stage_entry_point(translation, RT_SPIRV_FRAGMENT);
	const char* compute_entry_point = rt_spirv_stage_entry_point(translation, RT_SPIRV_COMPUTE);
	if (compute_words && compute_word_count) {
		GLuint compute = rtgl_execution_compile_spirv_shader(GL_COMPUTE_SHADER, compute_entry_point, compute_words, compute_word_count);
		if (!compute) {
			rt_spirv_program_destroy(translation);
			return false;
		}
		GLuint gl_program = glCreateProgram();
		glAttachShader(gl_program, compute);
		glLinkProgram(gl_program);
		glDeleteShader(compute);
		GLint ok = GL_FALSE;
		glGetProgramiv(gl_program, GL_LINK_STATUS, &ok);
		if (!ok) {
			char log[2048] = { 0 };
			glGetProgramInfoLog(gl_program, sizeof(log), NULL, log);
			rtgl_throwf(RT_SHADER_LINK_FAILED, "OpenGL SPIR-V compute shader link failed: %s", log);
			glDeleteProgram(gl_program);
			rt_spirv_program_destroy(translation);
			return false;
		}
		if (program->gl_program) glDeleteProgram(program->gl_program);
		program->gl_program = gl_program;
		program->compute = true;
		program->tessellated = false;
		program->patch_vertices = 0;
		rtgl_program_reflect_spirv(program, translation);
		rt_spirv_program_destroy(translation);
		return true;
	}
	const bool has_tessellation_control = tessellation_control_words && tessellation_control_word_count;
	const bool has_tessellation_evaluation = tessellation_evaluation_words && tessellation_evaluation_word_count;
	if (has_tessellation_control != has_tessellation_evaluation) {
		rtgl_throwf(RT_SHADER_LINK_FAILED, "OpenGL tessellation requires both control and evaluation stages");
		rt_spirv_program_destroy(translation);
		return false;
	}
	const u32 patch_vertices = has_tessellation_control
		? rtgl_spirv_tessellation_control_points(tessellation_control_words, tessellation_control_word_count)
		: 0;
	if (has_tessellation_control && !patch_vertices) {
		rtgl_throwf(RT_SHADER_LINK_FAILED, "OpenGL tessellation control stage is missing an RTIR output control-point count");
		rt_spirv_program_destroy(translation);
		return false;
	}
	GLuint vertex = rtgl_execution_compile_spirv_shader(GL_VERTEX_SHADER, vertex_entry_point, vertex_words, vertex_word_count);
	GLuint tessellation_control = vertex && has_tessellation_control ? rtgl_execution_compile_spirv_shader(GL_TESS_CONTROL_SHADER, tessellation_control_entry_point, tessellation_control_words, tessellation_control_word_count) : 0;
	GLuint tessellation_evaluation = tessellation_control && has_tessellation_evaluation ? rtgl_execution_compile_spirv_shader(GL_TESS_EVALUATION_SHADER, tessellation_evaluation_entry_point, tessellation_evaluation_words, tessellation_evaluation_word_count) : 0;
	GLuint fragment = vertex && (!has_tessellation_control || tessellation_evaluation) ? rtgl_execution_compile_spirv_shader(GL_FRAGMENT_SHADER, fragment_entry_point, fragment_words, fragment_word_count) : 0;
	if (!vertex || !fragment || (has_tessellation_control && (!tessellation_control || !tessellation_evaluation))) {
		if (vertex) {
			glDeleteShader(vertex);
		}
		if (tessellation_control) {
			glDeleteShader(tessellation_control);
		}
		if (tessellation_evaluation) {
			glDeleteShader(tessellation_evaluation);
		}
		rt_spirv_program_destroy(translation);
		return false;
	}

	GLuint gl_program = glCreateProgram();
	glAttachShader(gl_program, vertex);
	if (tessellation_control) {
		glAttachShader(gl_program, tessellation_control);
		glAttachShader(gl_program, tessellation_evaluation);
	}
	glAttachShader(gl_program, fragment);
	glLinkProgram(gl_program);
	glDeleteShader(vertex);
	if (tessellation_control) {
		glDeleteShader(tessellation_control);
		glDeleteShader(tessellation_evaluation);
	}
	glDeleteShader(fragment);

	GLint ok = GL_FALSE;
	glGetProgramiv(gl_program, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[2048] = { 0 };
		glGetProgramInfoLog(gl_program, sizeof(log), NULL, log);
		rtgl_throwf(RT_SHADER_LINK_FAILED, "OpenGL SPIR-V shader link failed: %s", log);
		glDeleteProgram(gl_program);
		rt_spirv_program_destroy(translation);
		return false;
	}

	if (program->gl_program) {
		glDeleteProgram(program->gl_program);
	}
	program->gl_program = gl_program;
	program->compute = false;
	program->tessellated = has_tessellation_control;
	program->patch_vertices = patch_vertices;
	rtgl_program_reflect_spirv(program, translation);
	rt_spirv_program_destroy(translation);
	return true;
}

void rtgl_execution_program_finalize(struct rtgl_context* ctx, struct rtgl_program* program) {
	rtgl_retain_resource(program);
	bool finalized = false;
	char message[1024] = { 0 };
	rtgl_execution_submit_sync(ctx, [program, &finalized, &message](struct rtgl_context* exec_ctx) {
		finalized = rtgl_execution_program_finalize_spirv(exec_ctx, program);
		if (finalized && program->gl_program) {
			for (u32 address = 0; address < RTGL_LOCATION_ADDRESS_COUNT; address++) {
				if (!program->location_occupied[address]) {
					continue;
				}
				struct rtgl_program_mapping* mapping = &program->mappings[address];
				if (mapping->kind == RTGL_LOCATION_MAPPING_TEXTURE || mapping->kind == RTGL_LOCATION_MAPPING_STORAGE_TEXTURE_BUFFER) {
					mapping->gl_location = glGetUniformLocation(program->gl_program, mapping->name);
					if (mapping->kind == RTGL_LOCATION_MAPPING_TEXTURE && mapping->gl_location >= 0) {
						glProgramUniform1i(program->gl_program, mapping->gl_location, (GLint)mapping->binding);
					}
				} else if (mapping->kind == RTGL_LOCATION_MAPPING_STORAGE_BUFFER || mapping->kind == RTGL_LOCATION_MAPPING_STORAGE_DATA) {
					const GLuint block = glGetProgramResourceIndex(program->gl_program, GL_SHADER_STORAGE_BLOCK, mapping->name);
					if (block != GL_INVALID_INDEX) {
						glShaderStorageBlockBinding(program->gl_program, block, mapping->binding);
					}
				} else if (mapping->kind == RTGL_LOCATION_MAPPING_UNIFORM_BUFFER || mapping->kind == RTGL_LOCATION_MAPPING_UNIFORM_DATA) {
					const GLuint block = glGetUniformBlockIndex(program->gl_program, mapping->name);
					if (block != GL_INVALID_INDEX) {
						glUniformBlockBinding(program->gl_program, block, mapping->binding);
					}
				}
			}
		}
		if (!finalized) {
			strncpy(message, rtgl_error_message(), sizeof(message) - 1);
		}
		rtgl_resource_release(RTGL_RESOURCE_BASE(program));
	});
	if (!finalized) rtgl_throwf(RT_SHADER_LINK_FAILED, "%s", message[0] ? message : "OpenGL program finalization failed");
}
void rtgl_execution_program_destroy(struct rtgl_context* ctx, struct rtgl_program* program) {
	rtgl_execution_submit_sync(ctx, [program](struct rtgl_context*) {
		if (program->gl_program) {
			glDeleteProgram(program->gl_program);
			program->gl_program = 0;
		}
	});
}

void rtgl_execution_texture_create(struct rtgl_context* ctx, struct rtgl_image_base* image) {
	rtgl_execution_submit_sync(ctx, [image](struct rtgl_context*) {
		glCreateTextures(image->gl_target, 1, &image->gl_texture);
	});
}

void rtgl_execution_texture_delete(struct rtgl_context* ctx, struct rtgl_image_base* image) {
	rtgl_execution_submit_sync(ctx, [image](struct rtgl_context*) {
		if (image->gl_texture) {
			glDeleteTextures(1, &image->gl_texture);
			image->gl_texture = 0;
		}
	});
}

void rtgl_execution_framebuffer_attach_stencil(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer, struct rtgl_texture_view* view) {
	rtgl_execution_submit_sync(ctx, [framebuffer, view](struct rtgl_context*) {
		GLuint texture = view && view->image ? view->image->gl_texture : 0;
		glNamedFramebufferTexture(framebuffer->gl_framebuffer, GL_STENCIL_ATTACHMENT, texture, 0);
		if (framebuffer->color_texture_count && texture) {
			GLenum status = glCheckNamedFramebufferStatus(framebuffer->gl_framebuffer, GL_FRAMEBUFFER);
			if (status != GL_FRAMEBUFFER_COMPLETE) {
				rtgl_throwf(RT_INITIALIZATION_FAILED, "OpenGL framebuffer with stencil is incomplete: 0x%04x", status);
			}
		}
	});
}

void rtgl_execution_texture_view_delete_sampler(struct rtgl_context* ctx, struct rtgl_texture_view* view) {
	rtgl_execution_submit_sync(ctx, [view](struct rtgl_context*) {
		if (view->gl_sampler) {
			glDeleteSamplers(1, &view->gl_sampler);
			view->gl_sampler = 0;
		}
	});
}

void rtgl_execution_texture_data(struct rtgl_context* ctx, struct rtgl_image_base* image, const void* data) {
	rtgl_execution_submit_sync(ctx, [image, data](struct rtgl_context*) {
		switch (image->type) {
		case RT_TEXTURE_1D:
			glTextureStorage1D(image->gl_texture, (GLsizei)image->mip_levels, image->gl_internal_format, (GLsizei)image->width);
			break;
		case RT_TEXTURE_1D_ARRAY:
			glTextureStorage2D(image->gl_texture, (GLsizei)image->mip_levels, image->gl_internal_format, (GLsizei)image->width, (GLsizei)image->depth);
			break;
		case RT_TEXTURE_2D:
			glTextureStorage2D(image->gl_texture, (GLsizei)image->mip_levels, image->gl_internal_format, (GLsizei)image->width, (GLsizei)image->height);
			break;
		case RT_TEXTURE_2D_ARRAY:
		case RT_TEXTURE_3D:
			glTextureStorage3D(image->gl_texture, (GLsizei)image->mip_levels, image->gl_internal_format, (GLsizei)image->width, (GLsizei)image->height, (GLsizei)image->depth);
			break;
		default:
			return;
		}
		glTextureParameteri(image->gl_texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(image->gl_texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(image->gl_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(image->gl_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		if (data) {
			/* Initial uploads use the complete base level. */
			const GLenum format = rtgl_texture_upload_format(image->format);
			const GLenum type = rtgl_texture_upload_type(image->format);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			switch (image->type) {
			case RT_TEXTURE_1D:
				glTextureSubImage1D(image->gl_texture, 0, 0, (GLsizei)image->width, format, type, data);
				break;
			case RT_TEXTURE_1D_ARRAY:
				glTextureSubImage2D(image->gl_texture, 0, 0, 0, (GLsizei)image->width, (GLsizei)image->depth, format, type, data);
				break;
			case RT_TEXTURE_2D:
				glTextureSubImage2D(image->gl_texture, 0, 0, 0, (GLsizei)image->width, (GLsizei)image->height, format, type, data);
				break;
			case RT_TEXTURE_2D_ARRAY:
			case RT_TEXTURE_3D:
				glTextureSubImage3D(image->gl_texture, 0, 0, 0, 0, (GLsizei)image->width, (GLsizei)image->height, (GLsizei)image->depth, format, type, data);
				break;
			default:
				break;
			}
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		}
	});
}

static rt_texture_range rtgl_execution_texture_mip_range(rt_texture_range range, usize level) {
	rt_texture_range mip = range;
	mip.base_mip += level;
	mip.mip_count = 1;
	mip.offset.width >>= level;
	mip.offset.height >>= level;
	mip.offset.depth >>= level;
	mip.extent.width >>= level;
	mip.extent.height >>= level;
	mip.extent.depth >>= level;
	if (!mip.extent.width)
		mip.extent.width = 1;
	if (!mip.extent.height)
		mip.extent.height = 1;
	if (!mip.extent.depth)
		mip.extent.depth = 1;
	return mip;
}

static usize rtgl_execution_texture_range_bytes(const struct rtgl_image_base* image, rt_texture_range range) {
	const usize bytes = rtgl_texture_format_aspect_size(image->format, range.aspects);
	const usize layers = image->type == RT_TEXTURE_1D_ARRAY || image->type == RT_TEXTURE_2D_ARRAY ? range.layer_count : 1;
	return range.extent.width * range.extent.height * range.extent.depth * layers * bytes;
}

void rtgl_execution_texture_subdata(struct rtgl_context* ctx, struct rtgl_image_base* image, rt_texture_range range, const void* data) {
	rtgl_execution_submit_sync(ctx, [image, range, data](struct rtgl_context*) {
		glMemoryBarrier(GL_ALL_BARRIER_BITS);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		const u08* bytes = (const u08*)data;
		usize offset = 0;
		for (usize level = 0; level < range.mip_count; level++) {
			const rt_texture_range mip = rtgl_execution_texture_mip_range(range, level);
			const void* mip_data = bytes + offset;
			const GLenum format = rtgl_texture_upload_format_aspect(image->format, mip.aspects);
			const GLenum type = rtgl_texture_upload_type_aspect(image->format, mip.aspects);
			switch (image->type) {
			case RT_TEXTURE_1D:
				glTextureSubImage1D(image->gl_texture, (GLint)mip.base_mip, (GLint)mip.offset.width, (GLsizei)mip.extent.width, format, type, mip_data);
				break;
			case RT_TEXTURE_1D_ARRAY:
				glTextureSubImage2D(image->gl_texture, (GLint)mip.base_mip, (GLint)mip.offset.width, (GLint)mip.base_layer, (GLsizei)mip.extent.width, (GLsizei)mip.layer_count, format, type, mip_data);
				break;
			case RT_TEXTURE_2D:
				glTextureSubImage2D(image->gl_texture, (GLint)mip.base_mip, (GLint)mip.offset.width, (GLint)mip.offset.height, (GLsizei)mip.extent.width, (GLsizei)mip.extent.height, format, type, mip_data);
				break;
			case RT_TEXTURE_2D_ARRAY:
				glTextureSubImage3D(image->gl_texture, (GLint)mip.base_mip, (GLint)mip.offset.width, (GLint)mip.offset.height, (GLint)mip.base_layer, (GLsizei)mip.extent.width, (GLsizei)mip.extent.height, (GLsizei)mip.layer_count, format, type, mip_data);
				break;
			case RT_TEXTURE_3D:
				glTextureSubImage3D(image->gl_texture, (GLint)mip.base_mip, (GLint)mip.offset.width, (GLint)mip.offset.height, (GLint)mip.offset.depth, (GLsizei)mip.extent.width, (GLsizei)mip.extent.height, (GLsizei)mip.extent.depth, format, type, mip_data);
				break;
			default:
				break;
			}
			offset += rtgl_execution_texture_range_bytes(image, mip);
		}
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	});
}

void rtgl_execution_texture_read(struct rtgl_context* ctx, struct rtgl_image_base* image, rt_texture_range range, u08* data, usize data_size) {
	rtgl_execution_submit_sync(ctx, [image, range, data, data_size](struct rtgl_context*) {
		glMemoryBarrier(GL_ALL_BARRIER_BITS);
		usize offset = 0;
		for (usize level = 0; level < range.mip_count; level++) {
			const rt_texture_range mip = rtgl_execution_texture_mip_range(range, level);
			GLint y = (GLint)mip.offset.height;
			GLint z = (GLint)mip.offset.depth;
			GLsizei width = (GLsizei)mip.extent.width;
			GLsizei height = (GLsizei)mip.extent.height;
			GLsizei depth = (GLsizei)mip.extent.depth;
			if (image->type == RT_TEXTURE_1D_ARRAY) {
				y = (GLint)mip.base_layer;
				height = (GLsizei)mip.layer_count;
				depth = 1;
			}
			if (image->type == RT_TEXTURE_2D_ARRAY) {
				z = (GLint)mip.base_layer;
				depth = (GLsizei)mip.layer_count;
			}
			if (image->type == RT_TEXTURE_1D) {
				height = 1;
				depth = 1;
			}
			if (image->type == RT_TEXTURE_2D) {
				depth = 1;
			}
			const usize mip_bytes = rtgl_execution_texture_range_bytes(image, mip);
			glGetTextureSubImage(image->gl_texture, (GLint)mip.base_mip, (GLint)mip.offset.width, y, z, width, height, depth, rtgl_texture_upload_format_aspect(image->format, mip.aspects), rtgl_texture_upload_type_aspect(image->format, mip.aspects), (GLsizei)(data_size - offset), data + offset);
			offset += mip_bytes;
		}
	});
}

struct gl_surface* rtgl_execution_glfw_surface_create(struct rtgl_context* ctx, struct GLFWwindow* window) {
	struct gl_surface* surface = NULL;
	rtgl_execution_submit_sync(ctx, [window, &surface](struct rtgl_context* exec_ctx) {
		surface = rtgl_create_glfw_surface(exec_ctx->execution.gl_context, window);
	});
	return surface;
}

void rtgl_execution_surface_destroy(struct rtgl_context* ctx, struct gl_surface* surface) {
	rtgl_execution_submit_sync(ctx, [surface](struct rtgl_context*) {
		if (surface) {
			rtgl_destroy_glsurface(surface);
		}
	});
}

static void rtgl_execution_present_now(struct rtgl_context* ctx, struct rtgl_queue* queue, struct rtgl_swapchain* swapchain, struct rtgl_framebuffer* framebuffer, u64 value) {
	struct rtgl_texture_view* view = framebuffer->color_views[0];
	u32 width = view && view->image ? view->image->width : 0;
	u32 height = view && view->image ? view->image->height : 0;

	rtgl_make_glcontext_current(ctx->execution.gl_context, swapchain->surface);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_FRAMEBUFFER_SRGB);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer->gl_framebuffer);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glViewport(0, 0, (GLsizei)width, (GLsizei)height);
	glDrawBuffer(GL_BACK);
	glBlitFramebuffer(0, 0, (GLint)width, (GLint)height, 0, 0, (GLint)width, (GLint)height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	rtgl_swap_glsurface_buffers(swapchain->surface);
	rtgl_make_glcontext_current(ctx->execution.gl_context, NULL);

	rtgl_execution_lock(ctx);
	rtgl_execution_queue_complete_locked(queue, value);
	rtgl_execution_unlock(ctx);
	rtgl_resource_release(RTGL_RESOURCE_BASE(framebuffer));
	rtgl_resource_release(RTGL_RESOURCE_BASE(swapchain));
}

rt_timepoint rtgl_execution_present(struct rtgl_context* ctx, struct rtgl_queue* queue, struct rtgl_swapchain* swapchain, struct rtgl_framebuffer* framebuffer) {
	rt_timepoint done = rtgl_queue_signal(queue);

	rtgl_retain_resource(swapchain);
	rtgl_retain_resource(framebuffer);
	if (!rtgl_execution_submit_async(ctx, [queue, swapchain, framebuffer, value = rtgl_timepoint_queue_value(done)](struct rtgl_context* exec_ctx) {
			rtgl_execution_present_now(exec_ctx, queue, swapchain, framebuffer, value);
		})) {
		rt_timepoint failed = { ((u64)queue->identifier << 56) | queue->submitted_value };
		rtgl_release_resource(framebuffer);
		rtgl_release_resource(swapchain);
		return failed;
	}
	return done;
}

void rtgl_execution_queue_complete(struct rtgl_context* ctx, struct rtgl_queue* queue, u64 value) {
	rtgl_execution_submit_sync(ctx, [queue, value](struct rtgl_context* exec_ctx) {
		rtgl_execution_lock(exec_ctx);
		rtgl_execution_queue_complete_locked(queue, value);
		rtgl_execution_unlock(exec_ctx);
	});
}
