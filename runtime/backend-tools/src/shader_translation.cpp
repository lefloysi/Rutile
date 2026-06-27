#include "rutile/backend_tools/shader_translation.hpp"

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_hlsl.hpp>

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace rutile::backend_tools {
namespace {

constexpr i32 kGlslVersion = 460;

EShLanguage to_glslang(ShaderStage stage) {
	switch (stage) {
	case ShaderStage::Vertex: return EShLangVertex;
	case ShaderStage::Fragment: return EShLangFragment;
	}
	return EShLangCount;
}

std::string_view trim_left(std::string_view text) {
	while (!text.empty() && std::isspace((unsigned char)text.front())) {
		text.remove_prefix(1);
	}
	return text;
}

std::string_view trim_right(std::string_view text) {
	while (!text.empty() && std::isspace((unsigned char)text.back())) {
		text.remove_suffix(1);
	}
	return text;
}

std::string_view trim(std::string_view text) {
	return trim_right(trim_left(text));
}

bool has_version_directive(const std::string& source) {
	size_t offset = 0;
	while (offset < source.size()) {
		size_t end = source.find('\n', offset);
		if (end == std::string::npos) {
			end = source.size();
		}
		std::string_view line(source.data() + offset, end - offset);
		line = trim(line);
		if (!line.empty() && line.back() == '\r') {
			line.remove_suffix(1);
			line = trim_right(line);
		}
		if (!line.empty() && line.substr(0, 2) != "//") {
			return line.substr(0, 8) == "#version";
		}
		offset = end + (end < source.size() ? 1 : 0);
	}
	return false;
}

std::string source_with_version(u64 size, const void* source) {
	const char* text = source ? static_cast<const char*>(source) : "";
	std::string result(text, static_cast<size_t>(size));
	if (!has_version_directive(result)) {
		result.insert(0, "#version " + std::to_string(kGlslVersion) + "\n");
	}
	return result;
}

void validate_source(EShLanguage language, u64 size, const void* source) {
	if (language == EShLangCount) {
		throw std::runtime_error("unsupported shader stage");
	}
	if (!source && size) {
		throw std::runtime_error("shader source is NULL but size is non-zero");
	}
	if (size > static_cast<u64>(std::numeric_limits<i32>::max())) {
		throw std::runtime_error("shader source is too large");
	}
}

void append_spirv_resources(const std::vector<u32>& spirv, std::vector<ShaderResource>& resources) {
	spirv_cross::Compiler compiler(spirv);
	spirv_cross::ShaderResources reflected = compiler.get_shader_resources();
	auto binding_of = [&](const spirv_cross::Resource& resource) {
		return compiler.get_decoration(resource.id, spv::DecorationBinding);
	};
	for (const spirv_cross::Resource& resource : reflected.uniform_buffers) {
		resources.push_back({ resource.name, ShaderResourceKind::UniformBuffer, binding_of(resource) });
	}
	for (const spirv_cross::Resource& resource : reflected.storage_buffers) {
		resources.push_back({ resource.name, ShaderResourceKind::StorageBuffer, binding_of(resource) });
	}
	for (const spirv_cross::Resource& resource : reflected.sampled_images) {
		resources.push_back({ resource.name, ShaderResourceKind::Texture, binding_of(resource) });
	}
}

StageTranslation compile_stage_to_spirv(ShaderStage stage, u64 size, const void* source) {
	EShLanguage language = to_glslang(stage);
	validate_source(language, size, source);

	std::string glsl = source_with_version(size, source);
	const char* source_text = glsl.data();
	i32 source_length = static_cast<i32>(glsl.size());

	glslang::TShader shader(language);
	shader.setStringsWithLengths(&source_text, &source_length, 1);
	shader.setEnvInput(glslang::EShSourceGlsl, language, glslang::EShClientVulkan, kGlslVersion);
	shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
	shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);
	if (!shader.parse(GetDefaultResources(), kGlslVersion, false, EShMsgDefault)) {
		throw std::runtime_error(std::string("shader compile failed: ") + shader.getInfoLog());
	}

	glslang::TProgram program;
	program.addShader(&shader);
	if (!program.link(EShMsgDefault)) {
		throw std::runtime_error(std::string("shader link failed: ") + program.getInfoLog());
	}

	glslang::TIntermediate* intermediate = program.getIntermediate(language);
	if (!intermediate) {
		throw std::runtime_error("shader link did not produce intermediate code");
	}

	StageTranslation output;
	glslang::GlslangToSpv(*intermediate, output.spirv);
	append_spirv_resources(output.spirv, output.resources);
	return output;
}

std::string spirv_to_hlsl(const std::vector<u32>& spirv) {
	spirv_cross::CompilerHLSL compiler(spirv);
	spirv_cross::CompilerHLSL::Options options;
	options.shader_model = 50;
	compiler.set_hlsl_options(options);
	return compiler.compile();
}

void merge_resources(std::vector<ShaderResource>& resources, const std::vector<ShaderResource>& extra) {
	for (const ShaderResource& resource : extra) {
		auto existing = std::find_if(resources.begin(), resources.end(), [&](const ShaderResource& other) {
			return other.name == resource.name && other.kind == resource.kind;
		});
		if (existing == resources.end()) {
			resources.push_back(resource);
			continue;
		}
		if (existing->binding != resource.binding) {
			throw std::runtime_error("resource '" + resource.name + "' uses different bindings across shader stages");
		}
	}
}

} // namespace

void initialize_shader_tools() {
	glslang::InitializeProcess();
}

void shutdown_shader_tools() {
	glslang::FinalizeProcess();
}

GraphicsTranslation translate_graphics_glsl_to_hlsl(
	const rt_vertex_layout*,
	u64 vertex_size,
	const void* vertex_source,
	u64 fragment_size,
	const void* fragment_source) {
	GraphicsTranslation output;
	output.vertex = compile_stage_to_spirv(ShaderStage::Vertex, vertex_size, vertex_source);
	output.fragment = compile_stage_to_spirv(ShaderStage::Fragment, fragment_size, fragment_source);
	output.vertex.hlsl = spirv_to_hlsl(output.vertex.spirv);
	output.fragment.hlsl = spirv_to_hlsl(output.fragment.spirv);

	std::vector<ShaderResource> merged;
	merge_resources(merged, output.vertex.resources);
	merge_resources(merged, output.fragment.resources);
	output.vertex.resources = merged;
	output.fragment.resources.clear();
	return output;
}

} // namespace rutile::backend_tools
