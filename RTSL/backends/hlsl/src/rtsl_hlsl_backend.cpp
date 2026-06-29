#include "rtsl_hlsl_backend.h"

#include <rtslp_package.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <sstream>
#include <string>
#include <string_view>

namespace rtsl {
namespace {
bool is_resource_uniform_type(std::string_view type) { return type == "Sampler2D"; }
std::string sanitize(std::string_view text) { std::string out; for (char c : text) out.push_back((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'?c:'_'); return out; }
std::string uniform_binding_name(const uniform &u) { return "u_" + sanitize(u.scope_name + "_" + u.name); }
std::string uniform_block_name(const uniform &u) { return "cb_" + sanitize(u.scope_name + "_" + u.name); }
const function* find_stage_fn(const artifact_module &m, stage_kind stage) { for (const auto &fn : m.functions) if (fn.stage == stage) return &fn; return nullptr; }
std::string hlsl_type(std::string_view type) { if (type=="f32"||type=="float") return "float"; if (type=="i32"||type=="int") return "int"; if (type=="u32"||type=="uint") return "uint"; if (type=="vec2") return "float2"; if (type=="vec3") return "float3"; if (type=="vec4") return "float4"; if (type=="mat4") return "float4x4"; if (type=="Sampler2D") return "Texture2D<float4>"; return std::string(type); }

std::string emit_stage(const artifact_module &module, stage_kind stage) {
	const function *fn = find_stage_fn(module, stage);
	if (!fn) throw std::runtime_error(stage == stage_kind::vertex ? "missing vertex stage" : "missing fragment stage");
	std::ostringstream out;
	out << "#pragma pack_matrix(row_major)\n\n";
	for (const auto &s : module.structs) { out << "struct " << s.name << " {\n"; for (const auto &f : s.fields) out << "    " << hlsl_type(f.type) << " " << f.name << ";\n"; out << "};\n\n"; }
	for (const auto &u : module.uniforms) {
		if (is_resource_uniform_type(u.type)) out << hlsl_type(u.type) << " " << uniform_binding_name(u) << " : register(t" << u.binding << ");\n";
		else { out << "cbuffer " << uniform_block_name(u) << " : register(b" << u.binding << ") {\n    " << hlsl_type(u.type) << " value;\n};\n"; }
	}
	if (!module.uniforms.empty()) out << "\n";
	if (stage == stage_kind::vertex) {
		out << "struct VSInput {\n    float3 position : position;\n    float3 color : color;\n};\n\n";
		out << "struct VSOutput {\n    float4 position : SV_Position;\n    float3 color : color;\n};\n\n";
		out << "VSOutput main(VSInput input) {\n    VSOutput outv;\n    outv.position = float4(input.position, 1.0);\n    outv.color = input.color;\n    return outv;\n}\n";
	} else {
		out << "struct PSInput {\n    float4 position : SV_Position;\n    float3 color : color;\n};\n\n";
		out << "struct PSOutput {\n    float4 color : SV_Target0;\n};\n\n";
		out << "PSOutput main(PSInput input) {\n    PSOutput outv;\n    outv.color = float4(input.color, 1.0);\n    return outv;\n}\n";
	}
	return out.str();
}
} // namespace
} // namespace rtsl

extern "C" void rtsl_graphics_hlsl_result_clear(rtsl_graphics_hlsl_compile_result *result) {
	if (!result) return; std::free(result->vertex_hlsl); std::free(result->fragment_hlsl); std::free(result->uniforms); *result = {};
}

extern "C" rtsl_graphics_hlsl_compile_result rtsl_compile_graphics_rtslp_hlsl(u64 program_size, const void *program_source) {
	rtsl_graphics_hlsl_compile_result result{};
	try {
		const auto module = rtsl::read_rtslp_module(program_size, program_source);
		const std::string vertex = rtsl::emit_stage(module, rtsl::stage_kind::vertex);
		const std::string fragment = rtsl::emit_stage(module, rtsl::stage_kind::fragment);
		result.vertex_hlsl_size = vertex.size(); result.fragment_hlsl_size = fragment.size();
		result.vertex_hlsl = static_cast<char*>(std::malloc(result.vertex_hlsl_size + 1));
		result.fragment_hlsl = static_cast<char*>(std::malloc(result.fragment_hlsl_size + 1));
		if (!result.vertex_hlsl || !result.fragment_hlsl) { rtsl_graphics_hlsl_result_clear(&result); return {}; }
		std::memcpy(result.vertex_hlsl, vertex.c_str(), result.vertex_hlsl_size + 1);
		std::memcpy(result.fragment_hlsl, fragment.c_str(), result.fragment_hlsl_size + 1);
		if (!module.uniforms.empty()) {
			result.uniforms = static_cast<rtsl_hlsl_uniform_info*>(std::calloc(module.uniforms.size(), sizeof(rtsl_hlsl_uniform_info)));
			if (!result.uniforms) { rtsl_graphics_hlsl_result_clear(&result); return {}; }
			for (size_t i = 0; i < module.uniforms.size(); ++i) {
				const auto &src = module.uniforms[i]; auto &dst = result.uniforms[i];
				std::snprintf(dst.name, sizeof(dst.name), "%s", src.name.c_str());
				dst.binding = src.binding; dst.kind = rtsl::is_resource_uniform_type(src.type) ? RTSL_HLSL_UNIFORM_TEXTURE_SAMPLED : RTSL_HLSL_UNIFORM_BUFFER;
			}
			result.uniform_count = static_cast<u32>(module.uniforms.size());
		}
		return result;
	} catch (...) { rtsl_graphics_hlsl_result_clear(&result); return {}; }
}
