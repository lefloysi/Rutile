#include "rtsl_spirv.h"

#include <rtsl/sdk.hpp>
#include <rtsl/spirv.hpp>

#include <cstddef>
#include <cstdio>
#include <exception>
#include <new>
#include <span>
#include <string_view>
#include <utility>

struct rtsl_spirv_translation {
	rtsl::Program program;
	rtsl::spirv::Shader vertex;
	rtsl::spirv::Shader fragment;
};

namespace {

void write_message(char* destination, u64 capacity, std::string_view context, std::string_view message) {
	if (!destination || capacity == 0) {
		return;
	}
	std::snprintf(
		destination,
		static_cast<std::size_t>(capacity),
		"%.*s: %.*s",
		static_cast<int>(context.size()),
		context.data(),
		static_cast<int>(message.size()),
		message.data()
	);
	destination[capacity - 1] = '\0';
}

rtsl_spirv_resource_kind resource_kind(rtsl::ResourceKind kind) {
	switch (kind) {
	case rtsl::ResourceKind::uniform_buffer: return RTSL_SPIRV_UNIFORM_BUFFER;
	case rtsl::ResourceKind::storage_buffer: return RTSL_SPIRV_STORAGE_BUFFER;
	case rtsl::ResourceKind::sampler: return RTSL_SPIRV_SAMPLER;
	case rtsl::ResourceKind::sampled_texture: return RTSL_SPIRV_SAMPLED_TEXTURE;
	case rtsl::ResourceKind::storage_image: return RTSL_SPIRV_STORAGE_IMAGE;
	}
	std::unreachable();
}

u32 stage_flags(rtsl::StageMask stages) {
	u32 result = 0;
	if (rtsl::contains(stages, rtsl::Stage::vertex)) {
		result |= RTSL_SPIRV_STAGE_VERTEX;
	}
	if (rtsl::contains(stages, rtsl::Stage::fragment)) {
		result |= RTSL_SPIRV_STAGE_FRAGMENT;
	}
	return result;
}

} // namespace

extern "C" rtsl_spirv_status rtsl_spirv_translate(
	u64 size,
	const void* data,
	rtsl_spirv_translation** translation,
	char* message,
	u64 message_capacity
) {
	if (!translation || !data || size == 0) {
		write_message(message, message_capacity, "rtsl_spirv_translate", "program bytes are missing");
		return RTSL_SPIRV_INVALID_PROGRAM;
	}
	*translation = nullptr;

	try {
		const auto bytes = std::span{
			reinterpret_cast<const std::byte*>(data),
			static_cast<std::size_t>(size),
		};
		auto program = rtsl::load_program(bytes);
		if (!program) {
			write_message(message, message_capacity, program.error().context, program.error().message);
			return program.error().code == rtsl::LoadErrorCode::allocation_failure
				? RTSL_SPIRV_OUT_OF_MEMORY
				: RTSL_SPIRV_INVALID_PROGRAM;
		}

		auto vertex = rtsl::spirv::transpile(*program, rtsl::Stage::vertex);
		if (!vertex) {
			write_message(message, message_capacity, vertex.error().context, vertex.error().message);
			return vertex.error().code == rtsl::spirv::ErrorCode::allocation_failure
				? RTSL_SPIRV_OUT_OF_MEMORY
				: RTSL_SPIRV_TRANSPILATION_FAILED;
		}

		auto fragment = rtsl::spirv::transpile(*program, rtsl::Stage::fragment);
		if (!fragment) {
			write_message(message, message_capacity, fragment.error().context, fragment.error().message);
			return fragment.error().code == rtsl::spirv::ErrorCode::allocation_failure
				? RTSL_SPIRV_OUT_OF_MEMORY
				: RTSL_SPIRV_TRANSPILATION_FAILED;
		}

		*translation = new rtsl_spirv_translation{
			.program = std::move(*program),
			.vertex = std::move(*vertex),
			.fragment = std::move(*fragment),
		};
		return RTSL_SPIRV_SUCCESS;
	} catch (const std::bad_alloc&) {
		write_message(message, message_capacity, "rtsl_spirv_translate", "allocation failed");
		return RTSL_SPIRV_OUT_OF_MEMORY;
	} catch (const std::exception& exception) {
		write_message(message, message_capacity, "rtsl_spirv_translate", exception.what());
		return RTSL_SPIRV_TRANSPILATION_FAILED;
	} catch (...) {
		write_message(message, message_capacity, "rtsl_spirv_translate", "unknown transpilation failure");
		return RTSL_SPIRV_TRANSPILATION_FAILED;
	}
}

extern "C" void rtsl_spirv_translation_destroy(rtsl_spirv_translation* translation) {
	delete translation;
}

extern "C" const u32* rtsl_spirv_stage_words(
	const rtsl_spirv_translation* translation,
	rtsl_spirv_stage stage,
	u64* word_count
) {
	if (!translation || !word_count) {
		return nullptr;
	}
	const rtsl::spirv::Shader& shader = stage == RTSL_SPIRV_VERTEX
		? translation->vertex
		: translation->fragment;
	*word_count = static_cast<u64>(shader.words.size());
	return shader.words.data();
}

extern "C" u32 rtsl_spirv_resource_count(const rtsl_spirv_translation* translation) {
	if (!translation) {
		return 0;
	}
	return static_cast<u32>(translation->program.resources().size());
}

extern "C" bool rtsl_spirv_resource(
	const rtsl_spirv_translation* translation,
	u32 index,
	rtsl_spirv_resource_info* resource
) {
	if (!translation || !resource || index >= translation->program.resources().size()) {
		return false;
	}
	const rtsl::Resource& source = translation->program.resources()[index];
	*resource = rtsl_spirv_resource_info{
		.name = source.name.c_str(),
		.descriptor_set = source.descriptor.set,
		.binding = source.descriptor.binding,
		.stages = stage_flags(source.stages),
		.kind = resource_kind(source.kind),
	};
	return true;
}
