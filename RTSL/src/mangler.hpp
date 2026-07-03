#pragma once

#include "ast.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace rtsl {

// Shape-agnostic mangling input. The Mangler used to take IRFunction
// directly, which tied it to the old (statement-string) IR. Decoupling it
// lets the SSA IR own its own mangled-name slot (StringId) while the
// frontend / IR-gen pass derives the value through this small adapter.
struct MangleInput {
	std::string_view name;			   // authored display name
	StageKind stage = StageKind::none; // none for ordinary functions
	std::vector<std::string_view> parameter_types;
};

class Mangler {
  public:
	[[nodiscard]] std::string mangle_rtsl(const MangleInput& input) const;
	[[nodiscard]] std::string mangle_glsl_from_rtsl(std::string_view rtsl_mangled_name) const;
	[[nodiscard]] std::string mangle_for_glsl(const MangleInput& input) const;
};

} // namespace rtsl
