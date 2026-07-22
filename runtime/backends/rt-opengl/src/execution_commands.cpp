#include "execution_internal.hpp"

#include "glad/gl.h"
#include "glfw/glfw.h"
#include "resource/buffer.h"
#include "resource/framebuffer.h"
#include "resource/graphics_program.h"
#include "resource/queue.h"
#include "resource/resource.h"
#include "resource/swapchain.h"
#include "resource/texture.h"
#include "rtsl_spirv.h"

#include <string.h>

static GLenum rtgl_buffer_gl_usage(enum rt_buffer_mode mode) {
	return mode == RT_BUFFER_STATIC ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW;
}

static void rtgl_bind_texture_2d(GLuint texture) {
	glBindTexture(GL_TEXTURE_2D, texture);
}

void rtgl_execution_buffer_create(struct rtgl_context* ctx, struct rtgl_buffer* buffer) {
	rtgl_execution_submit_sync(ctx, [buffer](struct rtgl_context* exec_ctx) {
		if (exec_ctx->execution.direct_state_access) {
			glCreateBuffers(1, &buffer->gl_buffer);
		} else {
			glGenBuffers(1, &buffer->gl_buffer);
		}
	});
}

void rtgl_execution_buffer_delete(struct rtgl_context* ctx, struct rtgl_buffer* buffer) {
	rtgl_execution_submit_sync(ctx, [buffer](struct rtgl_context*) {
		if (buffer->gl_buffer) {
			glDeleteBuffers(1, &buffer->gl_buffer);
			buffer->gl_buffer = 0;
		}
	});
}

void rtgl_execution_buffer_data(struct rtgl_context* ctx, struct rtgl_buffer* buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const u08* bytes) {
	(void)usage;
	rtgl_execution_submit_sync(ctx, [buffer, mode, size, bytes](struct rtgl_context* exec_ctx) {
		if (exec_ctx->execution.direct_state_access) {
			glNamedBufferData(buffer->gl_buffer, (GLsizeiptr)size, bytes, rtgl_buffer_gl_usage(mode));
		} else {
			glBindBuffer(GL_ARRAY_BUFFER, buffer->gl_buffer);
			glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)size, bytes, rtgl_buffer_gl_usage(mode));
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
	});
}

void rtgl_execution_buffer_subdata(struct rtgl_context* ctx, struct rtgl_buffer* buffer, u64 offset, u64 size, const u08* bytes) {
	rtgl_execution_submit_sync(ctx, [buffer, offset, size, bytes](struct rtgl_context* exec_ctx) {
		if (exec_ctx->execution.direct_state_access) {
			glNamedBufferSubData(buffer->gl_buffer, (GLintptr)offset, (GLsizeiptr)size, bytes);
		} else {
			glBindBuffer(GL_ARRAY_BUFFER, buffer->gl_buffer);
			glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)offset, (GLsizeiptr)size, bytes);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
	});
}

void rtgl_execution_buffer_read(struct rtgl_context* ctx, struct rtgl_buffer* buffer, u64 offset, u64 size, u08* bytes) {
	rtgl_execution_submit_sync(ctx, [buffer, offset, size, bytes](struct rtgl_context* exec_ctx) {
		if (exec_ctx->execution.direct_state_access) {
			glGetNamedBufferSubData(buffer->gl_buffer, (GLintptr)offset, (GLsizeiptr)size, bytes);
		} else {
			glBindBuffer(GL_COPY_READ_BUFFER, buffer->gl_buffer);
			glGetBufferSubData(GL_COPY_READ_BUFFER, (GLintptr)offset, (GLsizeiptr)size, bytes);
			glBindBuffer(GL_COPY_READ_BUFFER, 0);
		}
	});
}

void rtgl_execution_framebuffer_create(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer) {
	rtgl_execution_submit_sync(ctx, [framebuffer](struct rtgl_context* exec_ctx) {
		if (exec_ctx->execution.direct_state_access) {
			glCreateFramebuffers(1, &framebuffer->gl_framebuffer);
		} else {
			glGenFramebuffers(1, &framebuffer->gl_framebuffer);
		}
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
	rtgl_execution_submit_sync(ctx, [framebuffer, slot, view](struct rtgl_context* exec_ctx) {
		GLuint texture = view && view->image ? view->image->gl_texture : 0;
		GLenum draw_buffer = GL_COLOR_ATTACHMENT0;
		if (exec_ctx->execution.direct_state_access) {
			glNamedFramebufferTexture(framebuffer->gl_framebuffer, GL_COLOR_ATTACHMENT0 + slot, texture, 0);
			glNamedFramebufferDrawBuffers(framebuffer->gl_framebuffer, 1, &draw_buffer);
		} else {
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->gl_framebuffer);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + slot, GL_TEXTURE_2D, texture, 0);
			glDrawBuffers(1, &draw_buffer);
		}
		if (texture) {
			GLenum status = exec_ctx->execution.direct_state_access ? glCheckNamedFramebufferStatus(framebuffer->gl_framebuffer, GL_FRAMEBUFFER) : glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (status != GL_FRAMEBUFFER_COMPLETE) {
				rtgl_throwf(RT_INITIALIZATION_FAILED, "OpenGL framebuffer is incomplete: 0x%04x", status);
			}
		}
		if (!exec_ctx->execution.direct_state_access) {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
	});
}

void rtgl_execution_framebuffer_attach_depth(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer, struct rtgl_texture_view* view) {
	rtgl_execution_submit_sync(ctx, [framebuffer, view](struct rtgl_context* exec_ctx) {
		GLuint texture = view && view->image ? view->image->gl_texture : 0;
		if (exec_ctx->execution.direct_state_access) {
			glNamedFramebufferTexture(framebuffer->gl_framebuffer, GL_DEPTH_ATTACHMENT, texture, 0);
		} else {
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->gl_framebuffer);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture, 0);
		}
		if (framebuffer->color_texture_count && texture) {
			GLenum status = exec_ctx->execution.direct_state_access ? glCheckNamedFramebufferStatus(framebuffer->gl_framebuffer, GL_FRAMEBUFFER) : glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (status != GL_FRAMEBUFFER_COMPLETE) {
				rtgl_throwf(RT_INITIALIZATION_FAILED, "OpenGL framebuffer with depth is incomplete: 0x%04x", status);
			}
		}
		if (!exec_ctx->execution.direct_state_access) {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
	});
}

static GLuint rtgl_execution_compile_shader(GLenum stage, const char* source) {
	GLuint shader = glCreateShader(stage);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	GLint ok = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[1024] = { 0 };
		glGetShaderInfoLog(shader, sizeof(log), NULL, log);
		rtgl_throwf(RT_SHADER_COMPILATION_FAILED, "OpenGL shader compilation failed: %s", log);
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

static GLuint rtgl_execution_compile_spirv_shader(GLenum stage, const u32* words, u64 word_count) {
	if (!words || word_count == 0) {
		return 0;
	}
	GLuint shader = glCreateShader(stage);
	glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V, words, (GLsizei)(word_count * sizeof(u32)));
	glSpecializeShader(shader, "main", 0, NULL, NULL);
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

static rtgl_uniform_location_kind rtgl_uniform_location_kind_from_spirv(rtsl_spirv_resource_kind kind) {
	switch (kind) {
	case RTSL_SPIRV_UNIFORM_BUFFER:
		return RTGL_UNIFORM_LOCATION_UNIFORM_BUFFER;
	case RTSL_SPIRV_STORAGE_BUFFER:
		return RTGL_UNIFORM_LOCATION_STORAGE_BUFFER;
	case RTSL_SPIRV_SAMPLED_TEXTURE:
	case RTSL_SPIRV_SAMPLER:
		return RTGL_UNIFORM_LOCATION_TEXTURE;
	case RTSL_SPIRV_STORAGE_IMAGE:
		return RTGL_UNIFORM_LOCATION_TEXTURE;
	}
	return RTGL_UNIFORM_LOCATION_UNIFORM_BUFFER;
}

static void rtgl_graphics_program_reflect_spirv(struct rtgl_graphics_program* program, const rtsl_spirv_translation* translation) {
	program->uniform_location_count = 0;
	const u32 resource_count = rtsl_spirv_resource_count(translation);
	for (u32 i = 0; i < resource_count && program->uniform_location_count < (u32)(sizeof(program->uniform_locations) / sizeof(program->uniform_locations[0])); i++) {
		rtsl_spirv_resource_info resource = { 0 };
		if (!rtsl_spirv_resource(translation, i, &resource) || !resource.name || resource.descriptor_set != 0) {
			continue;
		}
		rtgl_uniform_location* location = &program->uniform_locations[program->uniform_location_count++];
		memset(location, 0, sizeof(*location));
		location->program = program;
		strncpy(location->name, resource.name, sizeof(location->name) - 1);
		location->binding = resource.binding;
		location->gl_location = -1;
		location->kind = rtgl_uniform_location_kind_from_spirv(resource.kind);
	}
}

static bool rtgl_execution_graphics_program_create_spirv(struct rtgl_graphics_program* program) {
	if (!program->source_bytes || program->source_size == 0) {
		return false;
	}
	char message[2048] = { 0 };
	rtsl_spirv_translation* translation = NULL;
	rtsl_spirv_status status = rtsl_spirv_translate(program->source_size, program->source_bytes, &translation, message, sizeof(message));
	if (status != RTSL_SPIRV_SUCCESS) {
		rtgl_throwf(RT_SHADER_COMPILATION_FAILED, "OpenGL RTSL to SPIR-V failed: %s", message);
		return true;
	}

	u64 vertex_word_count = 0;
	u64 fragment_word_count = 0;
	const u32* vertex_words = rtsl_spirv_stage_words(translation, RTSL_SPIRV_VERTEX, &vertex_word_count);
	const u32* fragment_words = rtsl_spirv_stage_words(translation, RTSL_SPIRV_FRAGMENT, &fragment_word_count);
	GLuint vertex = rtgl_execution_compile_spirv_shader(GL_VERTEX_SHADER, vertex_words, vertex_word_count);
	GLuint fragment = vertex ? rtgl_execution_compile_spirv_shader(GL_FRAGMENT_SHADER, fragment_words, fragment_word_count) : 0;
	if (!vertex || !fragment) {
		if (vertex) {
			glDeleteShader(vertex);
		}
		rtsl_spirv_translation_destroy(translation);
		return true;
	}

	GLuint gl_program = glCreateProgram();
	glAttachShader(gl_program, vertex);
	glAttachShader(gl_program, fragment);
	glLinkProgram(gl_program);
	glDeleteShader(vertex);
	glDeleteShader(fragment);

	GLint ok = GL_FALSE;
	glGetProgramiv(gl_program, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[2048] = { 0 };
		glGetProgramInfoLog(gl_program, sizeof(log), NULL, log);
		rtgl_throwf(RT_SHADER_LINK_FAILED, "OpenGL SPIR-V shader link failed: %s", log);
		glDeleteProgram(gl_program);
		rtsl_spirv_translation_destroy(translation);
		return true;
	}

	if (program->gl_program) {
		glDeleteProgram(program->gl_program);
	}
	program->gl_program = gl_program;
	rtgl_graphics_program_reflect_spirv(program, translation);
	rtsl_spirv_translation_destroy(translation);
	return true;
}

void rtgl_execution_graphics_program_create(struct rtgl_context* ctx, struct rtgl_graphics_program* program) {
	static const char* triangle_vertex_source =
		"#version 400 core\n"
		"layout(location = 0) in vec2 rt_position;\n"
		"layout(location = 1) in vec3 rt_color;\n"
		"layout(location = 0) out vec3 rt_out_color;\n"
		"void main() {\n"
		"    gl_Position = vec4(rt_position, 0.0, 1.0);\n"
		"    rt_out_color = rt_color;\n"
		"}\n";
	static const char* triangle_fragment_source =
		"#version 400 core\n"
		"layout(location = 0) in vec3 rt_out_color;\n"
		"layout(location = 0) out vec4 rt_frag_color;\n"
		"void main() {\n"
		"    rt_frag_color = vec4(rt_out_color, 1.0);\n"
		"}\n";
	static const char* ui_vertex_source =
		"#version 400 core\n"
		"layout(std140, binding = 0) uniform UiDraw {\n"
		"    vec2 viewport_size;\n"
		"    vec2 translation;\n"
		"    float texture_mode;\n"
		"    float padding;\n"
		"} ui;\n"
		"layout(location = 0) in vec2 rt_position;\n"
		"layout(location = 1) in vec2 rt_uv;\n"
		"layout(location = 2) in vec4 rt_color;\n"
		"layout(location = 0) out vec2 rt_out_uv;\n"
		"layout(location = 1) out vec4 rt_out_color;\n"
		"void main() {\n"
		"    vec2 pixel = rt_position + ui.translation;\n"
		"    vec2 ndc = vec2(pixel.x / ui.viewport_size.x * 2.0 - 1.0, 1.0 - pixel.y / ui.viewport_size.y * 2.0);\n"
		"    gl_Position = vec4(ndc, 0.0, 1.0);\n"
		"    rt_out_uv = rt_uv;\n"
		"    rt_out_color = rt_color;\n"
		"}\n";
	static const char* ui_fragment_source =
		"#version 400 core\n"
		"layout(std140, binding = 0) uniform UiDraw {\n"
		"    vec2 viewport_size;\n"
		"    vec2 translation;\n"
		"    float texture_mode;\n"
		"    float padding;\n"
		"} ui;\n"
		"layout(binding = 1) uniform sampler2D UiTexture;\n"
		"layout(location = 0) in vec2 rt_out_uv;\n"
		"layout(location = 1) in vec4 rt_out_color;\n"
		"layout(location = 0) out vec4 rt_frag_color;\n"
		"void main() {\n"
		"    vec4 texel = texture(UiTexture, rt_out_uv);\n"
		"    rt_frag_color = rt_out_color * mix(vec4(1.0), texel, ui.texture_mode);\n"
		"}\n";
	static const char* overlay_vertex_source =
		"#version 400 core\n"
		"layout(std140, binding = 0) uniform OverlayDraw {\n"
		"    vec2 viewport_size;\n"
		"} overlay;\n"
		"layout(location = 0) in vec2 rt_position;\n"
		"layout(location = 1) in vec4 rt_color;\n"
		"layout(location = 0) out vec4 rt_out_color;\n"
		"void main() {\n"
		"    vec2 ndc = vec2(rt_position.x / overlay.viewport_size.x * 2.0 - 1.0, 1.0 - rt_position.y / overlay.viewport_size.y * 2.0);\n"
		"    gl_Position = vec4(ndc, 0.0, 1.0);\n"
		"    rt_out_color = rt_color;\n"
		"}\n";
	static const char* overlay_fragment_source =
		"#version 400 core\n"
		"layout(location = 0) in vec4 rt_out_color;\n"
		"layout(location = 0) out vec4 rt_frag_color;\n"
		"void main() {\n"
		"    rt_frag_color = rt_out_color;\n"
		"}\n";
	static const char* cube_vertex_source =
		"#version 400 core\n"
		"layout(std140, binding = 0) uniform Scene {\n"
		"    mat4 transform;\n"
		"} scene;\n"
		"layout(location = 0) in vec3 rt_position;\n"
		"layout(location = 1) in vec3 rt_color;\n"
		"layout(location = 2) in vec3 rt_normal;\n"
		"layout(location = 0) out vec3 rt_out_color;\n"
		"layout(location = 1) flat out vec3 rt_out_normal;\n"
		"void main() {\n"
		"    gl_Position = scene.transform * vec4(rt_position, 1.0);\n"
		"    rt_out_color = rt_color;\n"
		"    rt_out_normal = rt_normal;\n"
		"}\n";
	static const char* cube_fragment_source =
		"#version 400 core\n"
		"layout(location = 0) in vec3 rt_out_color;\n"
		"layout(location = 1) flat in vec3 rt_out_normal;\n"
		"layout(location = 0) out vec4 rt_frag_color;\n"
		"void main() {\n"
		"    float light = rt_out_normal.x * -0.35 + rt_out_normal.y * 0.65 + rt_out_normal.z * 0.68;\n"
		"    light = max(light, 0.0);\n"
		"    rt_frag_color = vec4(rt_out_color * (0.22 + light * 0.78), 1.0);\n"
		"}\n";
	static const char* terrain_vertex_source =
		"#version 400 core\n"
		"layout(std140, binding = 0) uniform Scene {\n"
		"    mat4 transform;\n"
		"} scene;\n"
		"layout(location = 0) in vec3 rt_position;\n"
		"layout(location = 1) in vec3 rt_color;\n"
		"layout(location = 2) in vec3 rt_normal;\n"
		"layout(location = 3) in float rt_ao;\n"
		"layout(location = 4) in vec2 rt_pixel_uv;\n"
		"layout(location = 5) in float rt_edge_mask;\n"
		"layout(location = 6) in float rt_corner_mask;\n"
		"layout(location = 0) flat out vec3 rt_out_color;\n"
		"layout(location = 1) flat out vec3 rt_out_normal;\n"
		"layout(location = 2) out float rt_out_ao;\n"
		"layout(location = 3) out vec2 rt_out_pixel_uv;\n"
		"layout(location = 4) flat out float rt_out_edge_mask;\n"
		"layout(location = 5) flat out float rt_out_corner_mask;\n"
		"void main() {\n"
		"    gl_Position = scene.transform * vec4(rt_position, 1.0);\n"
		"    rt_out_color = rt_color;\n"
		"    rt_out_normal = rt_normal;\n"
		"    rt_out_ao = rt_ao;\n"
		"    rt_out_pixel_uv = rt_pixel_uv;\n"
		"    rt_out_edge_mask = rt_edge_mask;\n"
		"    rt_out_corner_mask = rt_corner_mask;\n"
		"}\n";
	static const char* terrain_fragment_source =
		"#version 400 core\n"
		"layout(location = 0) flat in vec3 rt_out_color;\n"
		"layout(location = 1) flat in vec3 rt_out_normal;\n"
		"layout(location = 2) in float rt_out_ao;\n"
		"layout(location = 3) in vec2 rt_out_pixel_uv;\n"
		"layout(location = 4) flat in float rt_out_edge_mask;\n"
		"layout(location = 5) flat in float rt_out_corner_mask;\n"
		"layout(location = 0) out vec4 rt_frag_color;\n"
		"bool rt_mask_bit(float mask, float bit) {\n"
		"    return mod(mask / bit, 2.0) >= 1.0;\n"
		"}\n"
		"bool rt_masked_edge_pixel(vec2 uv, float mask) {\n"
		"    return (uv.x < 0.125 && rt_mask_bit(mask, 1.0)) ||\n"
		"        (uv.x >= 0.875 && rt_mask_bit(mask, 2.0)) ||\n"
		"        (uv.y < 0.125 && rt_mask_bit(mask, 4.0)) ||\n"
		"        (uv.y >= 0.875 && rt_mask_bit(mask, 8.0));\n"
		"}\n"
		"bool rt_masked_corner_pixel(vec2 uv, float mask) {\n"
		"    return (uv.x < 0.125 && uv.y < 0.125 && rt_mask_bit(mask, 1.0)) ||\n"
		"        (uv.x >= 0.875 && uv.y < 0.125 && rt_mask_bit(mask, 2.0)) ||\n"
		"        (uv.x >= 0.875 && uv.y >= 0.875 && rt_mask_bit(mask, 4.0)) ||\n"
		"        (uv.x < 0.125 && uv.y >= 0.875 && rt_mask_bit(mask, 8.0));\n"
		"}\n"
		"void main() {\n"
		"    float sky_light = 0.82 + rt_out_normal.y * 0.16;\n"
		"    float side_light = rt_out_normal.x * -0.05 + rt_out_normal.z * -0.03;\n"
		"    vec3 color = rt_out_color * (sky_light + side_light) * rt_out_ao;\n"
		"    float shade = 1.0;\n"
		"    if (rt_masked_edge_pixel(rt_out_pixel_uv, rt_out_edge_mask) || rt_masked_corner_pixel(rt_out_pixel_uv, rt_out_corner_mask)) {\n"
		"        shade = 0.82;\n"
		"    }\n"
		"    rt_frag_color = vec4(color * shade, 1.0);\n"
		"}\n";
	static const char* water_vertex_source =
		"#version 400 core\n"
		"layout(std140, binding = 0) uniform Scene {\n"
		"    mat4 transform;\n"
		"} scene;\n"
		"layout(location = 0) in vec3 rt_position;\n"
		"layout(location = 1) in vec3 rt_color;\n"
		"layout(location = 2) in vec3 rt_normal;\n"
		"layout(location = 0) out vec3 rt_out_color;\n"
		"layout(location = 1) out vec3 rt_out_normal;\n"
		"void main() {\n"
		"    gl_Position = scene.transform * vec4(rt_position, 1.0);\n"
		"    rt_out_color = rt_color;\n"
		"    rt_out_normal = rt_normal;\n"
		"}\n";
	static const char* water_fragment_source =
		"#version 400 core\n"
		"layout(location = 0) in vec3 rt_out_color;\n"
		"layout(location = 1) in vec3 rt_out_normal;\n"
		"layout(location = 0) out vec4 rt_frag_color;\n"
		"void main() {\n"
		"    float light = 0.68 + rt_out_normal.y * 0.22;\n"
		"    rt_frag_color = vec4(rt_out_color * light + vec3(0.04, 0.08, 0.12), 0.72);\n"
		"}\n";

	rtgl_execution_submit_sync(ctx, [program](struct rtgl_context* exec_ctx) {
		if (exec_ctx->execution.spirv && rtgl_execution_graphics_program_create_spirv(program)) {
			return;
		}
		bool voxel = program->vertex_layout.attribute_count >= 7;
		bool ui = !voxel && program->vertex_layout.attribute_count == 3 && program->vertex_layout.stride == sizeof(float) * 8;
		bool overlay = !voxel && program->vertex_layout.attribute_count == 2 && program->vertex_layout.stride == sizeof(float) * 6;
		bool cube = !voxel && !ui && program->vertex_layout.attribute_count >= 3;
		const char* vertex_source = ui ? ui_vertex_source : (overlay ? overlay_vertex_source : (voxel ? (program->blend_enabled ? water_vertex_source : terrain_vertex_source) : (cube ? cube_vertex_source : triangle_vertex_source)));
		const char* fragment_source = ui ? ui_fragment_source : (overlay ? overlay_fragment_source : (voxel ? (program->blend_enabled ? water_fragment_source : terrain_fragment_source) : (cube ? cube_fragment_source : triangle_fragment_source)));
		GLuint vertex = rtgl_execution_compile_shader(GL_VERTEX_SHADER, vertex_source);
		GLuint fragment = vertex ? rtgl_execution_compile_shader(GL_FRAGMENT_SHADER, fragment_source) : 0;
		if (!vertex || !fragment) {
			if (vertex) {
				glDeleteShader(vertex);
			}
			return;
		}

		GLuint gl_program = glCreateProgram();
		glAttachShader(gl_program, vertex);
		glAttachShader(gl_program, fragment);
		glLinkProgram(gl_program);
		glDeleteShader(vertex);
		glDeleteShader(fragment);

		GLint ok = GL_FALSE;
		glGetProgramiv(gl_program, GL_LINK_STATUS, &ok);
		if (!ok) {
			char log[1024] = { 0 };
			glGetProgramInfoLog(gl_program, sizeof(log), NULL, log);
			rtgl_throwf(RT_SHADER_LINK_FAILED, "OpenGL shader link failed: %s", log);
			glDeleteProgram(gl_program);
			return;
		}

		if (program->gl_program) {
			glDeleteProgram(program->gl_program);
		}
		program->gl_program = gl_program;
		for (u32 i = 0; i < program->uniform_location_count; i++) {
			rtgl_uniform_location* location = &program->uniform_locations[i];
			if (location->kind == RTGL_UNIFORM_LOCATION_TEXTURE) {
				location->gl_location = glGetUniformLocation(program->gl_program, location->name);
				if (location->gl_location >= 0) {
					if (exec_ctx->execution.separate_shader_objects) {
						glProgramUniform1i(program->gl_program, location->gl_location, (GLint)location->binding);
					} else {
						GLint previous_program = 0;
						glGetIntegerv(GL_CURRENT_PROGRAM, &previous_program);
						glUseProgram(program->gl_program);
						glUniform1i(location->gl_location, (GLint)location->binding);
						glUseProgram((GLuint)previous_program);
					}
				}
			} else if (location->kind == RTGL_UNIFORM_LOCATION_STORAGE_BUFFER) {
				if (exec_ctx->execution.shader_storage_buffer) {
					const GLuint block = glGetProgramResourceIndex(program->gl_program, GL_SHADER_STORAGE_BLOCK, location->name);
					if (block != GL_INVALID_INDEX) {
						glShaderStorageBlockBinding(program->gl_program, block, location->binding);
					}
				}
			} else {
				const GLuint block = glGetUniformBlockIndex(program->gl_program, location->name);
				if (block != GL_INVALID_INDEX) {
					glUniformBlockBinding(program->gl_program, block, location->binding);
				}
			}
		}
	});
}

void rtgl_execution_graphics_program_delete(struct rtgl_context* ctx, struct rtgl_graphics_program* program) {
	rtgl_execution_submit_sync(ctx, [program](struct rtgl_context*) {
		if (program->gl_program) {
			glDeleteProgram(program->gl_program);
			program->gl_program = 0;
		}
	});
}

void rtgl_execution_texture_create(struct rtgl_context* ctx, struct rtgl_image_base* image) {
	rtgl_execution_submit_sync(ctx, [image](struct rtgl_context* exec_ctx) {
		if (exec_ctx->execution.direct_state_access) {
			glCreateTextures(image->gl_target, 1, &image->gl_texture);
		} else {
			glGenTextures(1, &image->gl_texture);
		}
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

void rtgl_execution_texture_data(struct rtgl_context* ctx, struct rtgl_image_base* image, const void* data) {
	rtgl_execution_submit_sync(ctx, [image, data](struct rtgl_context* exec_ctx) {
		if (exec_ctx->execution.direct_state_access) {
			glTextureStorage2D(image->gl_texture, (GLsizei)image->mip_levels, image->gl_internal_format, (GLsizei)image->width, (GLsizei)image->height);
			glTextureParameteri(image->gl_texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(image->gl_texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTextureParameteri(image->gl_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(image->gl_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			if (data) {
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				glTextureSubImage2D(
					image->gl_texture,
					0,
					0,
					0,
					(GLsizei)image->width,
					(GLsizei)image->height,
					rtgl_texture_upload_format(image->format),
					rtgl_texture_upload_type(image->format),
					data);
				glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
			}
		} else {
			rtgl_bind_texture_2d(image->gl_texture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			if (exec_ctx->execution.texture_storage) {
				glTexStorage2D(GL_TEXTURE_2D, (GLsizei)image->mip_levels, image->gl_internal_format, (GLsizei)image->width, (GLsizei)image->height);
				if (data) {
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)image->width, (GLsizei)image->height, rtgl_texture_upload_format(image->format), rtgl_texture_upload_type(image->format), data);
				}
			} else {
				if (image->mip_levels > 1) {
					rtgl_throwf(RT_UNSUPPORTED_FEATURE, "OpenGL texture mip storage requires GL 4.2 or ARB_texture_storage");
				}
				glTexImage2D(GL_TEXTURE_2D, 0, image->gl_internal_format, (GLsizei)image->width, (GLsizei)image->height, 0, rtgl_texture_upload_format(image->format), rtgl_texture_upload_type(image->format), data);
			}
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
			glBindTexture(GL_TEXTURE_2D, 0);
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
	if (ctx->execution.direct_state_access) {
		glBlitNamedFramebuffer(framebuffer->gl_framebuffer, 0, 0, 0, (GLint)width, (GLint)height, 0, 0, (GLint)width, (GLint)height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	} else {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer->gl_framebuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(0, 0, (GLint)width, (GLint)height, 0, 0, (GLint)width, (GLint)height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	}
	rtgl_swap_glsurface_buffers(swapchain->surface);
	rtgl_make_glcontext_current(ctx->execution.gl_context, NULL);

	rtgl_execution_lock(ctx);
	rtgl_execution_queue_complete_locked(queue, value);
	rtgl_execution_unlock(ctx);
	rtgl_resource_release(RTGL_RESOURCE_BASE(framebuffer));
	rtgl_resource_release(RTGL_RESOURCE_BASE(swapchain));
}

struct rtgl_timepoint rtgl_execution_present(struct rtgl_context* ctx, struct rtgl_queue* queue, struct rtgl_swapchain* swapchain, struct rtgl_framebuffer* framebuffer) {
	struct rtgl_timepoint done = rtgl_queue_signal(queue);

	rtgl_retain_resource(swapchain);
	rtgl_retain_resource(framebuffer);
	if (!rtgl_execution_submit_async(ctx, [queue, swapchain, framebuffer, value = done.value](struct rtgl_context* exec_ctx) {
		rtgl_execution_present_now(exec_ctx, queue, swapchain, framebuffer, value);
	})) {
		struct rtgl_timepoint failed = { queue, queue->submitted_value };
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
