#include "shader_compiler.h"
#include "context.h"
#include "error.h"

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <new>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

enum {
	RTVK_GLSL_VERSION = 460,
};

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

static EShLanguage rtvk_shader_stage_to_glslang(VkShaderStageFlagBits stage) {
	switch (stage) {
	case VK_SHADER_STAGE_VERTEX_BIT: return EShLangVertex;
	case VK_SHADER_STAGE_FRAGMENT_BIT: return EShLangFragment;
	case VK_SHADER_STAGE_COMPUTE_BIT: return EShLangCompute;
	default: return EShLangCount;
	}
}

static bool rtvk_char_is_identifier_first(char ch) {
	return std::isalpha((unsigned char)ch) || ch == '_';
}

static bool rtvk_char_is_identifier(char ch) {
	return std::isalnum((unsigned char)ch) || ch == '_';
}

static std::string_view rtvk_trim_left(std::string_view text) {
	while (!text.empty() && std::isspace((unsigned char)text.front())) {
		text.remove_prefix(1);
	}
	return text;
}

static std::string_view rtvk_trim_right(std::string_view text) {
	while (!text.empty() && std::isspace((unsigned char)text.back())) {
		text.remove_suffix(1);
	}
	return text;
}

static std::string_view rtvk_trim(std::string_view text) {
	return rtvk_trim_right(rtvk_trim_left(text));
}

static bool rtvk_string_view_equals(std::string_view text, const char* value) {
	return text == std::string_view(value);
}

typedef struct rtvk_glsl_token {
	std::string_view text;
	size_t start;
	size_t end;
} rtvk_glsl_token;

static std::vector<rtvk_glsl_token> rtvk_glsl_tokens(std::string_view text) {
	std::vector<rtvk_glsl_token> tokens;
	for (size_t i = 0; i < text.size();) {
		if (!rtvk_char_is_identifier_first(text[i])) {
			i++;
			continue;
		}

		size_t start = i++;
		while (i < text.size() && rtvk_char_is_identifier(text[i])) {
			i++;
		}
		tokens.push_back({ text.substr(start, i - start), start, i });
	}
	return tokens;
}

static bool rtvk_glsl_is_leading_qualifier(std::string_view token) {
	return rtvk_string_view_equals(token, "flat") ||
		   rtvk_string_view_equals(token, "smooth") ||
		   rtvk_string_view_equals(token, "noperspective") ||
		   rtvk_string_view_equals(token, "centroid") ||
		   rtvk_string_view_equals(token, "sample") ||
		   rtvk_string_view_equals(token, "patch") ||
		   rtvk_string_view_equals(token, "invariant") ||
		   rtvk_string_view_equals(token, "precise") ||
		   rtvk_string_view_equals(token, "highp") ||
		   rtvk_string_view_equals(token, "mediump") ||
		   rtvk_string_view_equals(token, "lowp");
}

static bool rtvk_glsl_is_sampler_type(std::string_view token) {
	return token.find("sampler") != std::string_view::npos ||
		   token.find("texture") != std::string_view::npos ||
		   token.find("image") != std::string_view::npos;
}

static bool rtvk_glsl_has_version_directive(const std::string& source) {
	size_t offset = 0;
	bool in_block_comment = false;
	while (offset < source.size()) {
		size_t end = source.find('\n', offset);
		if (end == std::string::npos) {
			end = source.size();
		}
		std::string_view line(source.data() + offset, end - offset);
		line = rtvk_trim(line);
		if (!line.empty() && line.back() == '\r') {
			line.remove_suffix(1);
			line = rtvk_trim_right(line);
		}

		while (true) {
			if (in_block_comment) {
				size_t close = line.find("*/");
				if (close == std::string_view::npos) {
					line = {};
					break;
				}
				line.remove_prefix(close + 2);
				line = rtvk_trim_left(line);
				in_block_comment = false;
			}
			if (line.substr(0, 2) != "/*") {
				break;
			}
			size_t close = line.find("*/", 2);
			if (close == std::string_view::npos) {
				line = {};
				in_block_comment = true;
				break;
			}
			line.remove_prefix(close + 2);
			line = rtvk_trim_left(line);
		}

		if (line.empty() || line.substr(0, 2) == "//") {
			offset = end + (end < source.size() ? 1 : 0);
			continue;
		}
		return line.substr(0, 8) == "#version";
	}
	return false;
}

static std::string_view rtvk_glsl_code_without_line_comment(std::string_view line) {
	size_t comment = line.find("//");
	if (comment != std::string_view::npos) {
		line = line.substr(0, comment);
	}
	return line;
}

static i32 rtvk_glsl_parse_layout_number(std::string_view code, const char* name) {
	size_t name_offset = code.find(name);
	if (name_offset == std::string_view::npos) {
		return -1;
	}
	size_t equals = code.find('=', name_offset + std::strlen(name));
	if (equals == std::string_view::npos) {
		return -1;
	}

	size_t value = equals + 1;
	while (value < code.size() && std::isspace((unsigned char)code[value])) {
		value++;
	}
	if (value >= code.size() || !std::isdigit((unsigned char)code[value])) {
		return -1;
	}

	i32 number = 0;
	while (value < code.size() && std::isdigit((unsigned char)code[value])) {
		number = number * 10 + (code[value] - '0');
		value++;
	}
	return number;
}

typedef enum rtvk_glsl_storage {
	RTVK_GLSL_STORAGE_NONE,
	RTVK_GLSL_STORAGE_IN,
	RTVK_GLSL_STORAGE_OUT,
	RTVK_GLSL_STORAGE_ATTRIBUTE,
	RTVK_GLSL_STORAGE_VARYING,
} rtvk_glsl_storage;

typedef struct rtvk_glsl_storage_token {
	rtvk_glsl_storage storage;
	size_t start;
	size_t end;
} rtvk_glsl_storage_token;

static rtvk_glsl_storage_token rtvk_glsl_find_storage_token(std::string_view code) {
	std::vector<rtvk_glsl_token> tokens = rtvk_glsl_tokens(code);
	for (const rtvk_glsl_token& token : tokens) {
		if (rtvk_string_view_equals(token.text, "in")) {
			return { RTVK_GLSL_STORAGE_IN, token.start, token.end };
		}
		if (rtvk_string_view_equals(token.text, "out")) {
			return { RTVK_GLSL_STORAGE_OUT, token.start, token.end };
		}
		if (rtvk_string_view_equals(token.text, "attribute")) {
			return { RTVK_GLSL_STORAGE_ATTRIBUTE, token.start, token.end };
		}
		if (rtvk_string_view_equals(token.text, "varying")) {
			return { RTVK_GLSL_STORAGE_VARYING, token.start, token.end };
		}
		if (!rtvk_glsl_is_leading_qualifier(token.text)) {
			break;
		}
	}
	return { RTVK_GLSL_STORAGE_NONE, 0, 0 };
}

static bool rtvk_glsl_storage_uses_location(EShLanguage language, rtvk_glsl_storage storage, bool* is_input) {
	switch (storage) {
	case RTVK_GLSL_STORAGE_IN:
		if (language != EShLangVertex && language != EShLangFragment) {
			return false;
		}
		*is_input = true;
		return true;
	case RTVK_GLSL_STORAGE_OUT:
		if (language != EShLangVertex && language != EShLangFragment) {
			return false;
		}
		*is_input = false;
		return true;
	case RTVK_GLSL_STORAGE_ATTRIBUTE:
		if (language != EShLangVertex) {
			return false;
		}
		*is_input = true;
		return true;
	case RTVK_GLSL_STORAGE_VARYING:
		*is_input = language != EShLangVertex;
		return language == EShLangVertex || language == EShLangFragment;
	default:
		return false;
	}
}

static void rtvk_glsl_replace_storage_token(std::string& line, const rtvk_glsl_storage_token& token, const char* replacement) {
	line.replace(token.start, token.end - token.start, replacement);
}

static std::string rtvk_glsl_declaration_name(std::string_view code) {
	code = rtvk_trim_right(code);
	size_t semicolon = code.find(';');
	if (semicolon != std::string_view::npos) {
		code = code.substr(0, semicolon);
	}
	size_t bracket = code.find('[');
	if (bracket != std::string_view::npos) {
		code = code.substr(0, bracket);
	}

	std::vector<rtvk_glsl_token> tokens = rtvk_glsl_tokens(code);
	if (tokens.empty()) {
		return {};
	}
	return std::string(tokens.back().text);
}

static std::string rtvk_glsl_declaration_type(std::string_view code) {
	std::vector<rtvk_glsl_token> tokens = rtvk_glsl_tokens(code);
	rtvk_glsl_storage_token storage = rtvk_glsl_find_storage_token(code);
	bool after_storage = false;
	for (const rtvk_glsl_token& token : tokens) {
		if (!after_storage) {
			if (token.start == storage.start && token.end == storage.end) {
				after_storage = true;
			}
			continue;
		}
		if (!rtvk_glsl_is_leading_qualifier(token.text)) {
			return std::string(token.text);
		}
	}
	return {};
}

static std::string rtvk_glsl_uniform_name(std::string_view code) {
	std::vector<rtvk_glsl_token> tokens = rtvk_glsl_tokens(code);
	if (tokens.size() < 2 || !rtvk_string_view_equals(tokens[0].text, "uniform")) {
		return {};
	}
	if (code.find('{') != std::string_view::npos || code.find(';') == std::string_view::npos) {
		return std::string(tokens[1].text);
	}
	return rtvk_glsl_declaration_name(code);
}

static u32 rtvk_glsl_auto_binding(std::string_view name) {
	u32 hash = 2166136261u;
	for (char ch : name) {
		hash ^= (u32)(unsigned char)ch;
		hash *= 16777619u;
	}
	return 128u + (hash % 16384u);
}

static u32 rtvk_glsl_resource_binding(
	const std::unordered_map<std::string, u32>* resource_bindings,
	std::string_view name) {
	if (resource_bindings) {
		auto binding = resource_bindings->find(std::string(name));
		if (binding != resource_bindings->end()) {
			return binding->second;
		}
	}
	return rtvk_glsl_auto_binding(name);
}

typedef struct rtvk_glsl_normalizer {
	EShLanguage language;
	const rt_vertex_layout* vertex_layout;
	const std::unordered_map<std::string, u32>* linked_locations;
	const std::unordered_map<std::string, u32>* resource_bindings;
	u32 input_location;
	u32 output_location;
	u32 vertex_input_index;
	i32 brace_depth;
} rtvk_glsl_normalizer;

static void rtvk_glsl_note_explicit_location(rtvk_glsl_normalizer* normalizer, std::string_view code) {
	i32 location = rtvk_glsl_parse_layout_number(code, "location");
	if (location < 0) {
		return;
	}

	size_t layout_close = code.find(')');
	if (layout_close != std::string_view::npos) {
		code.remove_prefix(layout_close + 1);
	}

	rtvk_glsl_storage_token storage = rtvk_glsl_find_storage_token(code);
	bool is_input = false;
	if (!rtvk_glsl_storage_uses_location(normalizer->language, storage.storage, &is_input)) {
		return;
	}

	u32 next_location = (u32)location + 1;
	if (is_input) {
		normalizer->input_location = std::max(normalizer->input_location, next_location);
		if (normalizer->language == EShLangVertex) {
			normalizer->vertex_input_index++;
		}
	} else {
		normalizer->output_location = std::max(normalizer->output_location, next_location);
	}
}

static u32 rtvk_glsl_next_interface_location(
	rtvk_glsl_normalizer* normalizer,
	rtvk_glsl_storage storage,
	bool is_input,
	std::string_view code) {
	std::string name = rtvk_glsl_declaration_name(code);
	if (!name.empty() && normalizer->linked_locations) {
		auto linked = normalizer->linked_locations->find(name);
		if (linked != normalizer->linked_locations->end()) {
			return linked->second;
		}
	}

	if (normalizer->language == EShLangVertex && is_input) {
		u32 index = normalizer->vertex_input_index++;
		if (normalizer->vertex_layout && index < normalizer->vertex_layout->attribute_count) {
			return normalizer->vertex_layout->attributes[index].location;
		}
		return normalizer->input_location++;
	}

	if (storage == RTVK_GLSL_STORAGE_ATTRIBUTE || is_input) {
		return normalizer->input_location++;
	}
	return normalizer->output_location++;
}

static bool rtvk_glsl_line_has_layout(std::string_view code) {
	std::vector<rtvk_glsl_token> tokens = rtvk_glsl_tokens(code);
	for (const rtvk_glsl_token& token : tokens) {
		if (rtvk_string_view_equals(token.text, "layout")) {
			return true;
		}
	}
	return false;
}

static std::string_view rtvk_glsl_first_layout_arguments(std::string_view code) {
	size_t layout = code.find("layout");
	if (layout == std::string_view::npos) {
		return {};
	}
	size_t open = code.find('(', layout);
	if (open == std::string_view::npos) {
		return {};
	}
	size_t close = code.find(')', open + 1);
	if (close == std::string_view::npos || close <= open) {
		return {};
	}
	return code.substr(open + 1, close - open - 1);
}

static std::string_view rtvk_glsl_after_first_layout(std::string_view code) {
	size_t layout = code.find("layout");
	if (layout == std::string_view::npos) {
		return code;
	}
	size_t close = code.find(')', layout);
	if (close == std::string_view::npos || close + 1 >= code.size()) {
		return {};
	}
	return code.substr(close + 1);
}

static bool rtvk_glsl_layout_has_qualifier(std::string_view code, const char* qualifier) {
	std::string_view arguments = rtvk_glsl_first_layout_arguments(code);
	std::vector<rtvk_glsl_token> tokens = rtvk_glsl_tokens(arguments);
	for (const rtvk_glsl_token& token : tokens) {
		if (rtvk_string_view_equals(token.text, qualifier)) {
			return true;
		}
	}
	return false;
}

static std::string rtvk_glsl_insert_layout_qualifier(std::string_view line, const std::string& qualifier) {
	std::string rewritten(line);
	size_t layout = rewritten.find("layout");
	if (layout == std::string::npos) {
		return rewritten;
	}
	size_t open = rewritten.find('(', layout);
	if (open == std::string::npos) {
		return rewritten;
	}
	size_t close = rewritten.find(')', open + 1);
	if (close == std::string::npos) {
		return rewritten;
	}

	bool has_existing = false;
	for (size_t i = open + 1; i < close; i++) {
		if (!std::isspace((unsigned char)rewritten[i])) {
			has_existing = true;
			break;
		}
	}

	std::string insertion = qualifier;
	if (has_existing) {
		insertion += ", ";
	}
	rewritten.insert(open + 1, insertion);
	return rewritten;
}

static bool rtvk_glsl_line_is_preprocessor(std::string_view code) {
	code = rtvk_trim_left(code);
	return !code.empty() && code.front() == '#';
}

static bool rtvk_glsl_line_is_version_directive(std::string_view code) {
	code = rtvk_trim_left(code);
	return code.substr(0, 8) == "#version";
}

static bool rtvk_glsl_line_is_comment(std::string_view code) {
	code = rtvk_trim_left(code);
	return code.substr(0, 2) == "//" || code.substr(0, 2) == "/*" || code.substr(0, 1) == "*";
}

static std::string rtvk_glsl_annotate_interface(rtvk_glsl_normalizer* normalizer, std::string_view line, std::string_view code) {
	rtvk_glsl_storage_token storage = rtvk_glsl_find_storage_token(code);
	bool is_input = false;
	if (!rtvk_glsl_storage_uses_location(normalizer->language, storage.storage, &is_input)) {
		return std::string(line);
	}
	if (code.find('{') != std::string_view::npos) {
		return std::string(line);
	}

	u32 location = rtvk_glsl_next_interface_location(normalizer, storage.storage, is_input, code);
	std::string rewritten(line);
	if (storage.storage == RTVK_GLSL_STORAGE_ATTRIBUTE) {
		rtvk_glsl_replace_storage_token(rewritten, storage, "in");
	} else if (storage.storage == RTVK_GLSL_STORAGE_VARYING) {
		rtvk_glsl_replace_storage_token(rewritten, storage, is_input ? "in" : "out");
	}

	return "layout(location = " + std::to_string(location) + ") " + rewritten;
}

static bool rtvk_glsl_line_is_uniform_block_start(std::string_view code) {
	std::vector<rtvk_glsl_token> tokens = rtvk_glsl_tokens(code);
	if (tokens.size() < 2 || !rtvk_string_view_equals(tokens[0].text, "uniform")) {
		return false;
	}
	return code.find('{') != std::string_view::npos || code.find(';') == std::string_view::npos;
}

static bool rtvk_glsl_line_is_storage_block_start(std::string_view code) {
	std::vector<rtvk_glsl_token> tokens = rtvk_glsl_tokens(code);
	bool found_buffer = false;
	for (const rtvk_glsl_token& token : tokens) {
		if (rtvk_string_view_equals(token.text, "buffer")) {
			found_buffer = true;
			break;
		}
	}
	return found_buffer && (code.find('{') != std::string_view::npos || code.find(';') == std::string_view::npos);
}

static bool rtvk_glsl_line_is_sampler_uniform(std::string_view code) {
	std::vector<rtvk_glsl_token> tokens = rtvk_glsl_tokens(code);
	if (tokens.size() < 3 || !rtvk_string_view_equals(tokens[0].text, "uniform")) {
		return false;
	}
	for (size_t i = 1; i + 1 < tokens.size(); i++) {
		if (rtvk_glsl_is_sampler_type(tokens[i].text)) {
			return true;
		}
	}
	return false;
}

static std::string rtvk_glsl_resource_name(std::string_view code) {
	std::vector<rtvk_glsl_token> tokens = rtvk_glsl_tokens(code);
	if (tokens.size() < 2) {
		return {};
	}
	for (size_t i = 0; i + 1 < tokens.size(); i++) {
		if (rtvk_string_view_equals(tokens[i].text, "buffer")) {
			return std::string(tokens[i + 1].text);
		}
	}
	if (rtvk_string_view_equals(tokens[0].text, "uniform")) {
		return rtvk_glsl_uniform_name(code);
	}
	return {};
}

static bool rtvk_glsl_line_is_resource(std::string_view code) {
	return rtvk_glsl_line_is_uniform_block_start(code) ||
		   rtvk_glsl_line_is_sampler_uniform(code) ||
		   rtvk_glsl_line_is_storage_block_start(code);
}

static std::string rtvk_glsl_annotate_resource(rtvk_glsl_normalizer* normalizer, std::string_view line, std::string_view code) {
	if (!rtvk_glsl_line_is_resource(code)) {
		return std::string(line);
	}

	std::string name = rtvk_glsl_resource_name(code);
	if (name.empty()) {
		return std::string(line);
	}

	u32 binding = rtvk_glsl_resource_binding(normalizer->resource_bindings, name);
	return "layout(set = 0, binding = " + std::to_string(binding) + ") " + std::string(line);
}

static std::string rtvk_glsl_normalize_layout_line(rtvk_glsl_normalizer* normalizer, std::string_view line, std::string_view code) {
	rtvk_glsl_note_explicit_location(normalizer, code);

	std::string_view after_layout = rtvk_trim_left(rtvk_glsl_after_first_layout(code));
	std::vector<rtvk_glsl_token> tokens = rtvk_glsl_tokens(after_layout);
	if (!tokens.empty() && rtvk_glsl_line_is_resource(after_layout)) {
		if (!rtvk_glsl_layout_has_qualifier(code, "binding")) {
			std::string name = rtvk_glsl_resource_name(after_layout);
			if (!name.empty()) {
				u32 binding = rtvk_glsl_resource_binding(normalizer->resource_bindings, name);
				std::string qualifier = rtvk_glsl_layout_has_qualifier(code, "set") ? "binding = " + std::to_string(binding) : "set = 0, binding = " + std::to_string(binding);
				return rtvk_glsl_insert_layout_qualifier(line, qualifier);
			}
		}
		if (!rtvk_glsl_layout_has_qualifier(code, "set")) {
			return rtvk_glsl_insert_layout_qualifier(line, "set = 0");
		}
	}

	if (!rtvk_glsl_layout_has_qualifier(code, "location")) {
		rtvk_glsl_storage_token storage = rtvk_glsl_find_storage_token(after_layout);
		bool is_input = false;
		if (rtvk_glsl_storage_uses_location(normalizer->language, storage.storage, &is_input) &&
			after_layout.find('{') == std::string_view::npos) {
			u32 location = rtvk_glsl_next_interface_location(normalizer, storage.storage, is_input, after_layout);
			return rtvk_glsl_insert_layout_qualifier(line, "location = " + std::to_string(location));
		}
	}

	return std::string(line);
}

static std::string rtvk_glsl_normalize_line(rtvk_glsl_normalizer* normalizer, std::string_view line) {
	std::string_view code = rtvk_glsl_code_without_line_comment(line);
	if (rtvk_glsl_line_is_version_directive(code)) {
		return "#version " + std::to_string(RTVK_GLSL_VERSION);
	}
	if (rtvk_glsl_line_is_preprocessor(code) || rtvk_glsl_line_is_comment(code)) {
		return std::string(line);
	}

	if (rtvk_glsl_line_has_layout(code)) {
		return rtvk_glsl_normalize_layout_line(normalizer, line, code);
	}

	std::string_view trimmed = rtvk_trim_left(code);
	std::vector<rtvk_glsl_token> tokens = rtvk_glsl_tokens(trimmed);
	bool has_storage_buffer_token = false;
	for (const rtvk_glsl_token& token : tokens) {
		if (rtvk_string_view_equals(token.text, "buffer")) {
			has_storage_buffer_token = true;
			break;
		}
	}
	if (!tokens.empty() && (rtvk_string_view_equals(tokens[0].text, "uniform") || has_storage_buffer_token)) {
		return rtvk_glsl_annotate_resource(normalizer, line, trimmed);
	}

	return rtvk_glsl_annotate_interface(normalizer, line, code);
}

static u32 rtvk_glsl_linked_location_count(const std::unordered_map<std::string, u32>* linked_locations) {
	u32 count = 0;
	if (!linked_locations) {
		return count;
	}
	for (const auto& entry : *linked_locations) {
		count = std::max(count, entry.second + 1);
	}
	return count;
}

typedef struct rtvk_glsl_interface_decl {
	std::string name;
	std::string type;
	rtvk_glsl_storage storage;
	bool has_location;
	u32 location;
} rtvk_glsl_interface_decl;

static bool rtvk_glsl_parse_interface_decl(EShLanguage language, std::string_view line, rtvk_glsl_interface_decl* decl) {
	std::string_view code = rtvk_glsl_code_without_line_comment(line);
	if (rtvk_glsl_line_is_preprocessor(code) || rtvk_glsl_line_is_comment(code)) {
		return false;
	}

	std::string_view declaration = code;
	if (rtvk_glsl_line_has_layout(code)) {
		declaration = rtvk_trim_left(rtvk_glsl_after_first_layout(code));
	}

	rtvk_glsl_storage_token storage = rtvk_glsl_find_storage_token(declaration);
	bool is_input = false;
	if (!rtvk_glsl_storage_uses_location(language, storage.storage, &is_input)) {
		return false;
	}
	if (declaration.find('{') != std::string_view::npos) {
		return false;
	}

	std::string name = rtvk_glsl_declaration_name(declaration);
	std::string type = rtvk_glsl_declaration_type(declaration);
	if (name.empty() || type.empty()) {
		return false;
	}

	i32 location = rtvk_glsl_parse_layout_number(code, "location");
	decl->name = std::move(name);
	decl->type = std::move(type);
	decl->storage = storage.storage;
	decl->has_location = location >= 0;
	decl->location = location >= 0 ? (u32)location : 0;
	return true;
}

static void rtvk_glsl_update_brace_depth(std::string_view code, i32* brace_depth) {
	for (char ch : code) {
		if (ch == '{') {
			(*brace_depth)++;
		} else if (ch == '}' && *brace_depth > 0) {
			(*brace_depth)--;
		}
	}
}

static std::vector<rtvk_glsl_interface_decl> rtvk_glsl_collect_interface_decls(EShLanguage language, u64 size, const void* source) {
	const char* source_text = source ? (const char*)source : "";
	std::string_view input(source_text, (size_t)size);
	std::vector<rtvk_glsl_interface_decl> decls;
	i32 brace_depth = 0;

	size_t offset = 0;
	while (offset <= input.size()) {
		size_t end = input.find('\n', offset);
		if (end == std::string_view::npos) {
			end = input.size();
		}
		std::string_view line(input.data() + offset, end - offset);
		if (!line.empty() && line.back() == '\r') {
			line.remove_suffix(1);
		}

		std::string_view code = rtvk_glsl_code_without_line_comment(line);
		if (brace_depth == 0) {
			rtvk_glsl_interface_decl decl = {};
			if (rtvk_glsl_parse_interface_decl(language, line, &decl)) {
				decls.push_back(std::move(decl));
			}
		}
		rtvk_glsl_update_brace_depth(code, &brace_depth);

		if (end == input.size()) {
			break;
		}
		offset = end + 1;
	}
	return decls;
}

static bool rtvk_glsl_decl_is_stage_input(EShLanguage language, const rtvk_glsl_interface_decl& decl) {
	if (decl.storage == RTVK_GLSL_STORAGE_ATTRIBUTE) {
		return language == EShLangVertex;
	}
	if (decl.storage == RTVK_GLSL_STORAGE_VARYING) {
		return language != EShLangVertex;
	}
	return decl.storage == RTVK_GLSL_STORAGE_IN;
}

static std::unordered_map<std::string, u32> rtvk_glsl_link_graphics_stages(u64 vertex_size, const void* vertex_source, u64 fragment_size, const void* fragment_source) {
	std::vector<rtvk_glsl_interface_decl> vertex_decls = rtvk_glsl_collect_interface_decls(EShLangVertex, vertex_size, vertex_source);
	std::vector<rtvk_glsl_interface_decl> fragment_decls = rtvk_glsl_collect_interface_decls(EShLangFragment, fragment_size, fragment_source);

	std::unordered_map<std::string, const rtvk_glsl_interface_decl*> vertex_outputs;
	std::unordered_map<u32, const rtvk_glsl_interface_decl*> vertex_outputs_by_location;
	std::set<u32> used_locations;
	for (const rtvk_glsl_interface_decl& decl : vertex_decls) {
		if (!rtvk_glsl_decl_is_stage_input(EShLangVertex, decl)) {
			vertex_outputs[decl.name] = &decl;
			if (decl.has_location) {
				used_locations.insert(decl.location);
				vertex_outputs_by_location[decl.location] = &decl;
			}
		}
	}
	for (const rtvk_glsl_interface_decl& decl : fragment_decls) {
		if (rtvk_glsl_decl_is_stage_input(EShLangFragment, decl)) {
			if (decl.has_location) {
				used_locations.insert(decl.location);
			}
		}
	}

	std::unordered_map<std::string, u32> linked_locations;
	u32 next_location = 0;
	auto next_unused_location = [&]() {
		while (used_locations.find(next_location) != used_locations.end()) {
			next_location++;
		}
		used_locations.insert(next_location);
		return next_location++;
	};

	for (const rtvk_glsl_interface_decl& fragment_decl : fragment_decls) {
		if (!rtvk_glsl_decl_is_stage_input(EShLangFragment, fragment_decl)) {
			continue;
		}
		const std::string& name = fragment_decl.name;
		const rtvk_glsl_interface_decl* fragment = &fragment_decl;
		auto vertex_entry = vertex_outputs.find(name);
		if (vertex_entry == vertex_outputs.end()) {
			if (fragment->has_location) {
				auto vertex_location = vertex_outputs_by_location.find(fragment->location);
				if (vertex_location != vertex_outputs_by_location.end()) {
					if (vertex_location->second->type != fragment->type) {
						throw std::runtime_error("type mismatch at explicit varying location " + std::to_string(fragment->location));
					}
					continue;
				}
			}
			throw std::runtime_error("fragment input '" + name + "' has no matching vertex output");
		}

		const rtvk_glsl_interface_decl* vertex = vertex_entry->second;
		if (vertex->type != fragment->type) {
			throw std::runtime_error("type mismatch for varying '" + name + "'");
		}
		if (vertex->has_location && fragment->has_location && vertex->location != fragment->location) {
			throw std::runtime_error("location mismatch for varying '" + name + "'");
		}

		u32 location = vertex->has_location ? vertex->location : fragment->has_location ? fragment->location
																						: next_unused_location();
		linked_locations[name] = location;
		used_locations.insert(location);
	}

	return linked_locations;
}

typedef struct rtvk_glsl_resource_decl {
	std::string name;
	bool has_binding;
	u32 binding;
	rtvk_shader_resource_kind kind;
} rtvk_glsl_resource_decl;

static bool rtvk_reflection_add_resource(
	rtvk_shader_reflection* reflection,
	const char* name,
	u32 binding,
	rtvk_shader_resource_kind kind);

static rtvk_shader_resource_kind rtvk_glsl_resource_kind(std::string_view declaration) {
	std::vector<rtvk_glsl_token> tokens = rtvk_glsl_tokens(declaration);
	for (const rtvk_glsl_token& token : tokens) {
		if (rtvk_string_view_equals(token.text, "buffer")) {
			return RTVK_SHADER_RESOURCE_STORAGE_BUFFER;
		}
	}
	if (!tokens.empty() && rtvk_string_view_equals(tokens[0].text, "buffer")) {
		return RTVK_SHADER_RESOURCE_STORAGE_BUFFER;
	}
	if (rtvk_glsl_line_is_sampler_uniform(declaration)) {
		for (size_t i = 1; i + 1 < tokens.size(); i++) {
			if (tokens[i].text.find("image") != std::string_view::npos) {
				return RTVK_SHADER_RESOURCE_STORAGE_TEXTURE;
			}
		}
		return RTVK_SHADER_RESOURCE_SAMPLED_TEXTURE;
	}
	return RTVK_SHADER_RESOURCE_UNIFORM_BUFFER;
}

static bool rtvk_glsl_parse_resource_decl(std::string_view line, rtvk_glsl_resource_decl* decl) {
	std::string_view code = rtvk_glsl_code_without_line_comment(line);
	if (rtvk_glsl_line_is_preprocessor(code) || rtvk_glsl_line_is_comment(code)) {
		return false;
	}

	std::string_view declaration = code;
	if (rtvk_glsl_line_has_layout(code)) {
		declaration = rtvk_trim_left(rtvk_glsl_after_first_layout(code));
	}

	std::vector<rtvk_glsl_token> tokens = rtvk_glsl_tokens(declaration);
	if (tokens.empty() ||
		(!rtvk_string_view_equals(tokens[0].text, "uniform") &&
		 !rtvk_glsl_line_is_storage_block_start(declaration))) {
		return false;
	}
	if (!rtvk_glsl_line_is_resource(declaration)) {
		return false;
	}

	std::string name = rtvk_glsl_resource_name(declaration);
	if (name.empty()) {
		return false;
	}

	i32 set = rtvk_glsl_parse_layout_number(code, "set");
	if (set > 0) {
		throw std::runtime_error("resource '" + name + "' uses descriptor set " + std::to_string(set) + ", but Rutile currently exposes one backend-managed set");
	}
	i32 binding = rtvk_glsl_parse_layout_number(code, "binding");
	decl->name = std::move(name);
	decl->has_binding = binding >= 0;
	decl->binding = binding >= 0 ? (u32)binding : 0;
	decl->kind = rtvk_glsl_resource_kind(declaration);
	return true;
}

static std::vector<rtvk_glsl_resource_decl> rtvk_glsl_collect_resource_decls(u64 size, const void* source) {
	const char* source_text = source ? (const char*)source : "";
	std::string_view input(source_text, (size_t)size);
	std::vector<rtvk_glsl_resource_decl> decls;
	i32 brace_depth = 0;

	size_t offset = 0;
	while (offset <= input.size()) {
		size_t end = input.find('\n', offset);
		if (end == std::string_view::npos) {
			end = input.size();
		}
		std::string_view line(input.data() + offset, end - offset);
		if (!line.empty() && line.back() == '\r') {
			line.remove_suffix(1);
		}

		std::string_view code = rtvk_glsl_code_without_line_comment(line);
		if (brace_depth == 0) {
			rtvk_glsl_resource_decl decl = {};
			if (rtvk_glsl_parse_resource_decl(line, &decl)) {
				decls.push_back(std::move(decl));
			}
		}
		rtvk_glsl_update_brace_depth(code, &brace_depth);

		if (end == input.size()) {
			break;
		}
		offset = end + 1;
	}
	return decls;
}

static std::unordered_map<std::string, u32> rtvk_glsl_link_resource_bindings(u64 vertex_size, const void* vertex_source, u64 fragment_size, const void* fragment_source) {
	std::vector<rtvk_glsl_resource_decl> resources = rtvk_glsl_collect_resource_decls(vertex_size, vertex_source);
	std::vector<rtvk_glsl_resource_decl> fragment_resources = rtvk_glsl_collect_resource_decls(fragment_size, fragment_source);
	resources.insert(resources.end(), fragment_resources.begin(), fragment_resources.end());

	std::unordered_map<std::string, u32> bindings;
	std::set<u32> used_bindings;
	for (const rtvk_glsl_resource_decl& resource : resources) {
		if (!resource.has_binding) {
			continue;
		}
		auto existing = bindings.find(resource.name);
		if (existing != bindings.end() && existing->second != resource.binding) {
			throw std::runtime_error("resource '" + resource.name + "' uses different bindings across shader stages");
		}
		bindings[resource.name] = resource.binding;
		used_bindings.insert(resource.binding);
	}

	for (const rtvk_glsl_resource_decl& resource : resources) {
		if (bindings.find(resource.name) != bindings.end()) {
			continue;
		}

		u32 binding = rtvk_glsl_auto_binding(resource.name);
		while (used_bindings.find(binding) != used_bindings.end()) {
			binding++;
		}
		used_bindings.insert(binding);
		bindings[resource.name] = binding;
	}
	return bindings;
}

static bool rtvk_shader_reflect_glsl_resources(u64 size, const void* source, rtvk_shader_reflection* reflection) {
	if (!reflection) {
		return true;
	}
	std::vector<rtvk_glsl_resource_decl> resources = rtvk_glsl_collect_resource_decls(size, source);
	for (const rtvk_glsl_resource_decl& resource : resources) {
		if (!resource.has_binding) {
			continue;
		}
		if (!rtvk_reflection_add_resource(reflection, resource.name.c_str(), resource.binding, resource.kind)) {
			return false;
		}
	}
	return true;
}

static std::string rtvk_glsl_normalize(
	EShLanguage language,
	const rt_vertex_layout* vertex_layout,
	const std::unordered_map<std::string, u32>* linked_locations,
	const std::unordered_map<std::string, u32>* resource_bindings,
	u64 size,
	const void* source) {
	const char* source_text = source ? (const char*)source : "";
	std::string input(source_text, (size_t)size);
	std::string output;
	output.reserve(input.size() + 256);
	if (!rtvk_glsl_has_version_directive(input)) {
		output += "#version ";
		output += std::to_string(RTVK_GLSL_VERSION);
		output += "\n";
	}

	rtvk_glsl_normalizer normalizer = {};
	normalizer.language = language;
	normalizer.vertex_layout = vertex_layout;
	normalizer.linked_locations = linked_locations;
	normalizer.resource_bindings = resource_bindings;
	u32 linked_location_count = rtvk_glsl_linked_location_count(linked_locations);
	if (language == EShLangVertex) {
		normalizer.output_location = linked_location_count;
	} else if (language == EShLangFragment) {
		normalizer.input_location = linked_location_count;
	}

	size_t offset = 0;
	while (offset <= input.size()) {
		size_t end = input.find('\n', offset);
		if (end == std::string::npos) {
			end = input.size();
		}
		std::string_view line(input.data() + offset, end - offset);
		if (!line.empty() && line.back() == '\r') {
			line.remove_suffix(1);
		}
		if (normalizer.brace_depth == 0) {
			output += rtvk_glsl_normalize_line(&normalizer, line);
		} else {
			output += line;
		}
		std::string_view code = rtvk_glsl_code_without_line_comment(line);
		rtvk_glsl_update_brace_depth(code, &normalizer.brace_depth);
		if (end < input.size()) {
			output += '\n';
		}
		if (end == input.size()) {
			break;
		}
		offset = end + 1;
	}
	return output;
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtvk_shader_compiler_init(void) {
	glslang::InitializeProcess();
}

void rtvk_shader_compiler_finish(void) {
	glslang::FinalizeProcess();
}

void rtvk_shader_reflection_clear(rtvk_shader_reflection* reflection) {
	if (!reflection) {
		return;
	}
	std::free(reflection->uniform_blocks);
	std::free(reflection->textures);
	std::free(reflection->resources);
	*reflection = {};
}

static bool rtvk_shader_source_valid(EShLanguage language, u64 size, const void* source) {
	if (language == EShLangCount) {
		rtvk_throwf(RT_IMPROPER_USAGE, "unsupported shader stage");
		return false;
	}
	if (!source && size) {
		rtvk_throwf(RT_IMPROPER_USAGE, "shader source is NULL but size is %llu", (u64)size);
		return false;
	}
	if (size > (u64)std::numeric_limits<i32>::max()) {
		rtvk_throwf(RT_IMPROPER_USAGE, "shader source is too large: %llu bytes", (u64)size);
		return false;
	}

	return true;
}

static bool rtvk_reflection_add_resource(rtvk_shader_reflection* reflection, const char* name, u32 binding, rtvk_shader_resource_kind kind) {
	for (u32 i = 0; i < reflection->resource_count; i++) {
		rtvk_shader_resource* existing = &reflection->resources[i];
		if (existing->binding == binding) {
			if (existing->kind != kind) {
				rtvk_throwf(RT_SHADER_LINK_FAILED, "resources %s and %s use binding %u with different kinds", existing->name, name, binding);
				return false;
			}
			return true;
		}
	}

	rtvk_shader_resource* resources = (rtvk_shader_resource*)std::realloc(
		reflection->resources,
		sizeof(rtvk_shader_resource) * (size_t)(reflection->resource_count + 1));
	if (!resources) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate shader resource reflection");
		return false;
	}
	reflection->resources = resources;
	rtvk_shader_resource* dst = &reflection->resources[reflection->resource_count++];
	std::snprintf(dst->name, sizeof(dst->name), "%s", name);
	dst->name[sizeof(dst->name) - 1] = '\0';
	dst->binding = binding;
	dst->kind = kind;
	return true;
}

static bool rtvk_reflect_uniform_blocks(glslang::TProgram& program, rtvk_shader_reflection* reflection) {
	if (!reflection) {
		return true;
	}

	*reflection = {};
	if (!program.buildReflection(EShReflectionDefault)) {
		rtvk_throwf(RT_SHADER_LINK_FAILED, "shader reflection failed");
		return false;
	}

	int block_count = program.getNumUniformBlocks();
	if (block_count < 0) {
		block_count = 0;
	}
	if (block_count) {
		size_t bytes = sizeof(rtvk_shader_uniform_block) * (size_t)block_count;
		reflection->uniform_blocks = (rtvk_shader_uniform_block*)std::calloc(1, bytes);
		if (!reflection->uniform_blocks) {
			rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate uniform block reflection");
			return false;
		}
	}

	for (int i = 0; i < block_count; i++) {
		const glslang::TObjectReflection& block = program.getUniformBlock(i);
		int binding = block.getBinding();
		if (binding < 0) {
			rtvk_throwf(RT_SHADER_LINK_FAILED, "uniform block %s has no binding", block.name.c_str());
			return false;
		}

		rtvk_shader_uniform_block* dst = &reflection->uniform_blocks[reflection->uniform_block_count++];
		std::snprintf(dst->name, sizeof(dst->name), "%s", block.name.c_str());
		dst->name[sizeof(dst->name) - 1] = '\0';
		dst->binding = (u32)binding;
	}

	int uniform_count = program.getNumUniformVariables();
	if (uniform_count < 0) {
		uniform_count = 0;
	}
	if (uniform_count) {
		size_t bytes = sizeof(rtvk_shader_texture) * (size_t)uniform_count;
		reflection->textures = (rtvk_shader_texture*)std::calloc(1, bytes);
		if (!reflection->textures) {
			rtvk_shader_reflection_clear(reflection);
			rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate texture reflection");
			return false;
		}
	}
	for (int i = 0; i < uniform_count; i++) {
		const glslang::TObjectReflection& uniform = program.getUniform(i);
		if (uniform.index >= 0) {
			continue;
		}

		int binding = uniform.getBinding();
		if (binding < 0) {
			continue;
		}
		rtvk_shader_texture* dst = &reflection->textures[reflection->texture_count++];
		std::snprintf(dst->name, sizeof(dst->name), "%s", uniform.name.c_str());
		dst->name[sizeof(dst->name) - 1] = '\0';
		dst->binding = (u32)binding;
	}
	return true;
}

static bool rtvk_glslang_to_spirv(EShLanguage language, u64 size, const void* source, std::vector<u32>& spirv, rtvk_shader_reflection* reflection) {
	glslang::TShader shader(language);
	const char* source_text = (const char*)source;
	i32 source_length = (i32)size;
	shader.setStringsWithLengths(&source_text, &source_length, 1);
	shader.setEnvInput(glslang::EShSourceGlsl, language, glslang::EShClientVulkan, RTVK_GLSL_VERSION);
	shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
	shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);
	if (!shader.parse(GetDefaultResources(), RTVK_GLSL_VERSION, false, EShMsgDefault)) {
		rtvk_throwf(RT_SHADER_COMPILATION_FAILED, "shader compile failed: %s", shader.getInfoLog());
		return false;
	}

	glslang::TProgram program;
	program.addShader(&shader);
	if (!program.link(EShMsgDefault)) {
		rtvk_throwf(RT_SHADER_LINK_FAILED, "shader link failed: %s", program.getInfoLog());
		return false;
	}

	glslang::TIntermediate* intermediate = program.getIntermediate(language);
	if (!intermediate) {
		rtvk_throwf(RT_SHADER_LINK_FAILED, "shader link did not produce intermediate code");
		return false;
	}
	if (!rtvk_reflect_uniform_blocks(program, reflection)) {
		return false;
	}

	glslang::GlslangToSpv(*intermediate, spirv);
	return true;
}

static bool rtvk_shader_source_to_spirv(VkShaderStageFlagBits stage, const rt_vertex_layout* vertex_layout, const std::unordered_map<std::string, u32>* linked_locations, const std::unordered_map<std::string, u32>* resource_bindings, u64 size, const void* source, std::vector<u32>& spirv, rtvk_shader_reflection* reflection) {
	EShLanguage language = rtvk_shader_stage_to_glslang(stage);
	if (!rtvk_shader_source_valid(language, size, source)) {
		return false;
	}

	std::string glsl = rtvk_glsl_normalize(language, vertex_layout, linked_locations, resource_bindings, size, source);
	if (!rtvk_glslang_to_spirv(language, glsl.size(), glsl.data(), spirv, reflection)) {
		return false;
	}
	return rtvk_shader_reflect_glsl_resources(glsl.size(), glsl.data(), reflection);
}

static bool rtvk_spirv_create_shader_module(struct rtvk_context* ctx, std::span<const u32> spirv, VkShaderModule* shader) {
	VkShaderModuleCreateInfo shader_info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	shader_info.pNext = NULL;
	shader_info.flags = 0;
	shader_info.codeSize = spirv.size_bytes();
	shader_info.pCode = spirv.data();

	VkResult result = vkCreateShaderModule(ctx->vk_device, &shader_info, VK_ALLOCATOR, shader);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}

	return true;
}

static VkShaderModule rtvk_shader_compile_failed(enum rt_error error, const char* reason) {
	rtvk_throwf(error, "shader compile failed: %s", reason ? reason : "unknown C++ exception");
	return VK_NULL_HANDLE;
}

VkShaderModule rtvk_shader_compile(struct rtvk_context* ctx, VkShaderStageFlagBits stage, u64 size, const void* source, rtvk_shader_reflection* reflection, u32** spirv_source, u64* spirv_size) {
	try {
		std::vector<u32> spirv;
		if (!rtvk_shader_source_to_spirv(stage, NULL, NULL, NULL, size, source, spirv, reflection)) {
			return VK_NULL_HANDLE;
		}
		VkShaderModule shader = VK_NULL_HANDLE;
		if (!rtvk_spirv_create_shader_module(ctx, spirv, &shader)) {
			return VK_NULL_HANDLE;
		}
		if (spirv_source) {
			size_t bytes = spirv.size() * sizeof(spirv[0]);
			u32* copy = (u32*)std::malloc(bytes);
			if (!copy) {
				rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate shader SPIR-V storage");
				vkDestroyShaderModule(ctx->vk_device, shader, VK_ALLOCATOR);
				return VK_NULL_HANDLE;
			}
			std::memcpy(copy, spirv.data(), bytes);
			*spirv_source = copy;
		}
		if (spirv_size) {
			*spirv_size = (u64)(spirv.size() * sizeof(spirv[0]));
		}
		return shader;
	} catch (const std::bad_alloc&) {
		return rtvk_shader_compile_failed(RT_OUT_OF_HOST_MEMORY, "out of host memory");
	} catch (const std::exception& error) {
		return rtvk_shader_compile_failed(RT_SHADER_COMPILATION_FAILED, error.what());
	} catch (...) {
		return rtvk_shader_compile_failed(RT_SHADER_COMPILATION_FAILED, NULL);
	}
}

rtvk_graphics_shader_compile_result rtvk_shader_compile_graphics(struct rtvk_context* ctx, const rt_vertex_layout* vertex_layout, u64 vertex_size, const void* vertex_source, u64 fragment_size, const void* fragment_source) {
	rtvk_graphics_shader_compile_result result = {};
	try {
		if (!rtvk_shader_source_valid(EShLangVertex, vertex_size, vertex_source) ||
			!rtvk_shader_source_valid(EShLangFragment, fragment_size, fragment_source)) {
			return result;
		}

		std::unordered_map<std::string, u32> linked_locations = rtvk_glsl_link_graphics_stages(vertex_size, vertex_source, fragment_size, fragment_source);
		std::unordered_map<std::string, u32> resource_bindings = rtvk_glsl_link_resource_bindings(vertex_size, vertex_source, fragment_size, fragment_source);

		std::vector<u32> vertex_spirv;
		rtvk_shader_reflection new_vertex_reflection = {};
		if (!rtvk_shader_source_to_spirv(VK_SHADER_STAGE_VERTEX_BIT, vertex_layout, &linked_locations, &resource_bindings, vertex_size, vertex_source, vertex_spirv, &new_vertex_reflection)) {
			return result;
		}

		std::vector<u32> fragment_spirv;
		rtvk_shader_reflection new_fragment_reflection = {};
		if (!rtvk_shader_source_to_spirv(
				VK_SHADER_STAGE_FRAGMENT_BIT,
				NULL,
				&linked_locations,
				&resource_bindings,
				fragment_size,
				fragment_source,
				fragment_spirv,
				&new_fragment_reflection)) {
			rtvk_shader_reflection_clear(&new_vertex_reflection);
			return result;
		}

		VkShaderModule new_vertex_shader = VK_NULL_HANDLE;
		if (!rtvk_spirv_create_shader_module(ctx, vertex_spirv, &new_vertex_shader)) {
			rtvk_shader_reflection_clear(&new_vertex_reflection);
			rtvk_shader_reflection_clear(&new_fragment_reflection);
			return result;
		}

		VkShaderModule new_fragment_shader = VK_NULL_HANDLE;
		if (!rtvk_spirv_create_shader_module(ctx, fragment_spirv, &new_fragment_shader)) {
			vkDestroyShaderModule(ctx->vk_device, new_vertex_shader, VK_ALLOCATOR);
			rtvk_shader_reflection_clear(&new_vertex_reflection);
			rtvk_shader_reflection_clear(&new_fragment_reflection);
			return result;
		}

		result.vertex_shader = new_vertex_shader;
		result.vertex_reflection = new_vertex_reflection;
		result.fragment_shader = new_fragment_shader;
		result.fragment_reflection = new_fragment_reflection;
		return result;
	} catch (const std::bad_alloc&) {
		rtvk_shader_compile_failed(RT_OUT_OF_HOST_MEMORY, "out of host memory");
		return result;
	} catch (const std::exception& error) {
		rtvk_shader_compile_failed(RT_SHADER_COMPILATION_FAILED, error.what());
		return result;
	} catch (...) {
		rtvk_shader_compile_failed(RT_SHADER_COMPILATION_FAILED, NULL);
		return result;
	}
}
