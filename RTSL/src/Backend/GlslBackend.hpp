#pragma once

#include "AST/AST.hpp"
#include "Serialization/Artifact.hpp"

#include <string>
#include <vector>

namespace rtsl {

// One emitted Vulkan GLSL translation unit. RTSL keeps every stage in a single
// module, but Vulkan GLSL is one source file per stage, so emission splits the
// artifact into one GlslStageFile per stage entry point.
struct GlslStageFile {
    StageKind stage = StageKind::none;
    std::string extension; // ".vert", ".frag", ".geom"
    std::string source;
};

// Lower a linked program (or any artifact carrying stage entries) to Vulkan GLSL,
// producing one file per stage.
[[nodiscard]] std::vector<GlslStageFile> emit_vulkan_glsl(const Artifact &artifact);

} // namespace rtsl
