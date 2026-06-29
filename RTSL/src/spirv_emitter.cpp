#include "spirv_emitter.h"

namespace rtsl {

bool emit_rtslp_stage_spirv(const artifact_module&, stage_kind, std::vector<u32> *spirv_out, std::string *error_out) {
    if (spirv_out) {
        spirv_out->clear();
    }
    if (error_out) {
        *error_out = "SPIR-V emission will resume once the RTSL-IR type-table refactor lands";
    }
    return false;
}

} // namespace rtsl
