#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace rtsl {

// One member of a builtin carrier struct (RtVertex/RtFragment). The
// carrier is passed by reference into a stage entry; the generated runtime
// copies inputs in before the call and outputs out after it.
struct StageBuiltin {
	std::string member;	 // member name as written in RTSL source (e.g. "position")
	std::string type;	 // member type
	std::string gl_name; // GLSL built-in variable it maps to (e.g. "gl_Position")
	bool is_output;		 // true: written by the shader (mutable); false: read-only input
};

// Members of a builtin carrier type, or empty if `carrier_type` is not a carrier.
[[nodiscard]] std::vector<StageBuiltin> stage_builtins(std::string_view carrier_type);

// Whether a parameter type names a builtin carrier struct.
[[nodiscard]] bool is_stage_builtin_carrier(std::string_view type);

} // namespace rtsl
