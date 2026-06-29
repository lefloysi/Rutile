#pragma once

#include <rtslp_package.hpp>

#include <vector>

namespace rt {

bool emit_rtslp_stage_spirv(const RTArtifactModule& module, RTStageKind stage, std::vector<u32>* spirv_out, std::string* error_out = nullptr);

} // namespace rt
