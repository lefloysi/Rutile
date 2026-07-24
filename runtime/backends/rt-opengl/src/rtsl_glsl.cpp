#include "rtsl_glsl.h"

#include <rtsl/glsl.hpp>
#include <rtsl/sdk.hpp>

#include <cstddef>
#include <cstdio>
#include <exception>
#include <new>
#include <span>
#include <string_view>
#include <utility>

struct rtsl_glsl_translation {
	rtsl::Program program;
	rtsl::glsl::Shader vertex;
	rtsl::glsl::Shader fragment;
};

namespace {

void write_message(char* destination, u64 capacity, std::string_view context, std::string_view message) {
	if (!destination || capacity == 0) {
		return;
	}
	std::snprintf(destination, static_cast<std::size_t>(capacity), "%.*s: %.*s",
		static_cast<int>(context.size()), context.data(),
		static_cast<int>(message.size()), message.data());
	destination[capacity - 1] = '\0';
}

rtsl_glsl_resource_kind resource_kind(rtsl::ResourceKind kind) {
	switch (kind) {
	case rtsl::ResourceKind::uniform_buffer: return RTSL_GLSL_UNIFORM_BUFFER;
	case rtsl::ResourceKind::storage_buffer: return RTSL_GLSL_STORAGE_BUFFER;
	case rtsl::ResourceKind::sampler: return RTSL_GLSL_SAMPLER;
	case rtsl::ResourceKind::sampled_texture: return RTSL_GLSL_SAMPLED_TEXTURE;
	case rtsl::ResourceKind::storage_image: return RTSL_GLSL_STORAGE_IMAGE;
	}
	std::unreachable();
}

u32 stage_flags(rtsl::StageMask stages) {
	u32 result = 0;
	if (rtsl::contains(stages, rtsl::Stage::vertex)) {
		result |= 1u << 0;
	}
	if (rtsl::contains(stages, rtsl::Stage::fragment)) {
		result |= 1u << 1;
	}
	return result;
}

rtsl_glsl_storage_texture_format storage_texture_format(const rtsl::Program& program, const rtsl::Resource& resource) {
	if (resource.kind != rtsl::ResourceKind::storage_buffer) {
		return RTSL_GLSL_STORAGE_TEXTURE_FORMAT_NONE;
	}
	const rtsl::ir::Type* block = program.find_type(resource.value_type);
	const rtsl::ir::Type* array = block && block->kind == rtsl::ir::TypeKind::structure && block->members.size() == 1
		? program.find_type(block->members.front()) : nullptr;
	const rtsl::ir::Type* vector = array && array->kind == rtsl::ir::TypeKind::runtime_array ? program.find_type(array->element_type) : nullptr;
	const rtsl::ir::Type* scalar = vector && vector->kind == rtsl::ir::TypeKind::vector && vector->element_count == 4
		? program.find_type(vector->element_type) : nullptr;
	if (!scalar) {
		return RTSL_GLSL_STORAGE_TEXTURE_FORMAT_NONE;
	}
	switch (scalar->kind) {
	case rtsl::ir::TypeKind::floating: return RTSL_GLSL_STORAGE_TEXTURE_FORMAT_RGBA32F;
	case rtsl::ir::TypeKind::signed_integer: return RTSL_GLSL_STORAGE_TEXTURE_FORMAT_RGBA32I;
	case rtsl::ir::TypeKind::unsigned_integer: return RTSL_GLSL_STORAGE_TEXTURE_FORMAT_RGBA32UI;
	default: return RTSL_GLSL_STORAGE_TEXTURE_FORMAT_NONE;
	}
}

rtsl::glsl::StorageBufferLowering storage_buffer_lowering(rtsl_glsl_storage_buffer_lowering lowering) {
	switch (lowering) {
	case RTSL_GLSL_STORAGE_BUFFER_AUTO: return rtsl::glsl::StorageBufferLowering::auto_;
	case RTSL_GLSL_STORAGE_BUFFER_NATIVE_SSBO: return rtsl::glsl::StorageBufferLowering::native_ssbo;
	case RTSL_GLSL_STORAGE_BUFFER_TEXTURE_BUFFER_READONLY_VEC4: return rtsl::glsl::StorageBufferLowering::texture_buffer_readonly_vec4;
	case RTSL_GLSL_STORAGE_BUFFER_UNSUPPORTED: return rtsl::glsl::StorageBufferLowering::unsupported;
	}
	return rtsl::glsl::StorageBufferLowering::auto_;
}

} // namespace

extern "C" rtsl_glsl_status rtsl_glsl_translate(u64 size, const void* data, const rtsl_glsl_options* options, rtsl_glsl_translation** translation, char* message, u64 message_capacity) {
	if (!translation || !data || size == 0) {
		write_message(message, message_capacity, "rtsl_glsl_translate", "program bytes are missing");
		return RTSL_GLSL_INVALID_PROGRAM;
	}
	*translation = nullptr;

	try {
		const auto bytes = std::span{ reinterpret_cast<const std::byte*>(data), static_cast<std::size_t>(size) };
		auto program = rtsl::load_program(bytes);
		if (!program) {
			write_message(message, message_capacity, program.error().context, program.error().message);
			return program.error().code == rtsl::LoadErrorCode::allocation_failure ? RTSL_GLSL_OUT_OF_MEMORY : RTSL_GLSL_INVALID_PROGRAM;
		}

		const rtsl::glsl::Options opts{
			.version = options && options->version ? options->version : 460,
			.separate_shader_objects = options ? options->separate_shader_objects : true,
			.shader_storage_buffer = options ? options->shader_storage_buffer : true,
			.texture_buffer = options ? options->texture_buffer : true,
			.storage_buffer_lowering = options ? storage_buffer_lowering(options->storage_buffer_lowering) : rtsl::glsl::StorageBufferLowering::auto_,
		};
		auto vertex = rtsl::glsl::transpile(*program, rtsl::Stage::vertex, opts);
		if (!vertex) {
			write_message(message, message_capacity, vertex.error().context, vertex.error().message);
			return vertex.error().code == rtsl::glsl::ErrorCode::allocation_failure ? RTSL_GLSL_OUT_OF_MEMORY : RTSL_GLSL_TRANSPILATION_FAILED;
		}
		auto fragment = rtsl::glsl::transpile(*program, rtsl::Stage::fragment, opts);
		if (!fragment) {
			write_message(message, message_capacity, fragment.error().context, fragment.error().message);
			return fragment.error().code == rtsl::glsl::ErrorCode::allocation_failure ? RTSL_GLSL_OUT_OF_MEMORY : RTSL_GLSL_TRANSPILATION_FAILED;
		}

		*translation = new rtsl_glsl_translation{ .program = std::move(*program), .vertex = std::move(*vertex), .fragment = std::move(*fragment) };
		return RTSL_GLSL_SUCCESS;
	} catch (const std::bad_alloc&) {
		write_message(message, message_capacity, "rtsl_glsl_translate", "allocation failed");
		return RTSL_GLSL_OUT_OF_MEMORY;
	} catch (const std::exception& exception) {
		write_message(message, message_capacity, "rtsl_glsl_translate", exception.what());
		return RTSL_GLSL_TRANSPILATION_FAILED;
	} catch (...) {
		write_message(message, message_capacity, "rtsl_glsl_translate", "unknown transpilation failure");
		return RTSL_GLSL_TRANSPILATION_FAILED;
	}
}

extern "C" void rtsl_glsl_translation_destroy(rtsl_glsl_translation* translation) {
	delete translation;
}

extern "C" const char* rtsl_glsl_stage_source(const rtsl_glsl_translation* translation, rtsl_glsl_stage stage) {
	if (!translation) {
		return nullptr;
	}
	switch (stage) {
	case RTSL_GLSL_VERTEX: return translation->vertex.source.c_str();
	case RTSL_GLSL_FRAGMENT: return translation->fragment.source.c_str();
	}
	return nullptr;
}

extern "C" u32 rtsl_glsl_resource_count(const rtsl_glsl_translation* translation) {
	return translation ? static_cast<u32>(translation->program.resources().size()) : 0;
}

extern "C" bool rtsl_glsl_resource(const rtsl_glsl_translation* translation, u32 index, rtsl_glsl_resource_info* resource) {
	if (!translation || !resource || index >= translation->program.resources().size()) {
		return false;
	}
	const rtsl::Resource& source = translation->program.resources()[index];
	*resource = rtsl_glsl_resource_info{
		.name = source.name.c_str(),
		.descriptor_set = source.descriptor.set,
		.binding = source.descriptor.binding,
		.stages = stage_flags(source.stages),
		.kind = resource_kind(source.kind),
		.storage_texture_format = storage_texture_format(translation->program, source),
	};
	return true;
}
