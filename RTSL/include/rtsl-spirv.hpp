#pragma once

#include "rtsl.h"

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace rtsl {

struct spirv_graphics_program_result {
    std::vector<std::uint32_t> vert;
    std::vector<std::uint32_t> frag;
    std::vector<std::uint32_t> geom;
};

[[nodiscard]] std::expected<spirv_graphics_program_result, std::string> emit_spirv_graphics_program(const rtsl_module module);

} // namespace rtsl
