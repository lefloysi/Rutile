#pragma once

#include "rtsl.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

struct rtdx_rtsl_program {
	rtsl_module module = nullptr;
};

struct rtdx_rtsl_translation {
	std::string vertex_hlsl;
	std::string fragment_hlsl;
	std::vector<rtsl_uniform_info> uniforms;
};

[[nodiscard]] rtdx_rtsl_program rtdx_rtsl_program_load(std::span<const std::byte> rtslp_source);
[[nodiscard]] rtdx_rtsl_translation rtdx_rtsl_program_translate(const rtdx_rtsl_program& program);
void rtdx_rtsl_program_destroy(rtdx_rtsl_program& program);
