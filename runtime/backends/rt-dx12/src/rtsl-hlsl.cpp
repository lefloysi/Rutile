#include "rtsl-hlsl.hpp"

#include <cstring>
#include <sstream>

rtdx_rtsl_program rtdx_rtsl_program_load(std::span<const std::byte> rtslp_source) {
	rtdx_rtsl_program program{};
	program.module = rtslLoadModuleFromBytes(
		reinterpret_cast<const std::uint8_t*>(rtslp_source.data()),
		rtslp_source.size());
	return program;
}

static std::string draft_vertex_hlsl(const std::vector<rtsl_uniform_info>& uniforms) {
	std::ostringstream out;
	out << "// draft RTSL->HLSL bridge\n";
	for (const rtsl_uniform_info& uniform : uniforms) {
		out << "// uniform " << (uniform.qualified_name ? uniform.qualified_name : "") << " binding " << uniform.binding << "\n";
	}
	out << "float4 vert_main() : SV_Position { return float4(0.0, 0.0, 0.0, 1.0); }\n";
	return out.str();
}

static std::string draft_fragment_hlsl(const std::vector<rtsl_uniform_info>& uniforms) {
	std::ostringstream out;
	out << "// draft RTSL->HLSL bridge\n";
	for (const rtsl_uniform_info& uniform : uniforms) {
		out << "// uniform " << (uniform.qualified_name ? uniform.qualified_name : "") << " binding " << uniform.binding << "\n";
	}
	out << "float4 frag_main() : SV_Target { return float4(1.0, 0.0, 1.0, 1.0); }\n";
	return out.str();
}

rtdx_rtsl_translation rtdx_rtsl_program_translate(const rtdx_rtsl_program& program) {
	rtdx_rtsl_translation translation{};
	if (!program.module) {
		return translation;
	}

	const size_t uniform_count = rtslModuleGetUniformCount(program.module);
	translation.uniforms.reserve(uniform_count);
	for (size_t i = 0; i < uniform_count; ++i) {
		rtsl_uniform_info uniform{};
		if (rtslModuleGetUniform(program.module, i, &uniform)) {
			translation.uniforms.push_back(uniform);
		}
	}
	translation.vertex_hlsl = draft_vertex_hlsl(translation.uniforms);
	translation.fragment_hlsl = draft_fragment_hlsl(translation.uniforms);
	return translation;
}

void rtdx_rtsl_program_destroy(rtdx_rtsl_program& program) {
	if (program.module) {
		rtslDestroyModule(program.module);
		program.module = nullptr;
	}
}
