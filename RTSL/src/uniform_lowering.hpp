#pragma once

#include "ast.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace rtsl {

// Mangled member identifier for a uniform (e.g. u_6albedo_7texture_h86413326).
[[nodiscard]] std::string uniform_binding_name(const UniformBinding& uniform);

// Mangled wrapper identifier for a non-resource uniform's backing block.
[[nodiscard]] std::string uniform_block_name(const UniformBinding& uniform);

// Whether the uniform is an opaque resource bound directly rather than wrapped
// in a backing block.
[[nodiscard]] bool is_resource_uniform_type(std::string_view type);

// Rewrite source-level uniform references in an expression to their mangled
// binding names. Idempotent: re-running on already-lowered text is a no-op.
[[nodiscard]] std::string lower_uniform_references(std::string statement, const std::vector<UniformBinding>& uniforms);

} // namespace rtsl
