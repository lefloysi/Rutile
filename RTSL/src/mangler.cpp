#include "mangler.hpp"

#include <cstdint>
#include <string_view>

namespace rtsl {

namespace {

std::string sanitize_identifier(std::string_view text) {
    std::string out;
    for (const char c : text) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
            out.push_back(c);
        } else if (c == ':') {
            if (!out.ends_with("__")) {
                out += "__";
            }
        } else {
            out.push_back('_');
        }
    }

    if (out.empty() || (out.front() >= '0' && out.front() <= '9')) {
        out.insert(out.begin(), '_');
    }
    return out;
}

} // namespace

std::vector<std::string_view> split_qualified_name(std::string_view name) {
    std::vector<std::string_view> parts;
    while (!name.empty()) {
        const auto scope = name.find("::");
        if (scope == std::string_view::npos) {
            parts.push_back(name);
            break;
        }
        parts.push_back(name.substr(0, scope));
        name.remove_prefix(scope + 2);
    }
    return parts;
}

std::string encode_source_name(std::string_view name) {
    std::string out;
    out += std::to_string(name.size());
    out += name;
    return out;
}

std::string encode_name_part(std::string_view name) {
    if (name.starts_with("~")) {
        return "D1";
    }
    return encode_source_name(name);
}

std::string encode_type(std::string_view type) {
    if (type == "void") return "v";
    if (type == "bool") return "b";
    if (type == "f32") return "f";
    if (type == "f64") return "d";
    if (type == "i32") return "i";
    if (type == "u32") return "j";
    return encode_source_name(type);
}

std::string Mangler::mangle_glsl_from_rtsl(std::string_view rtsl_mangled_name) const {
    std::string out = "__";
    out += sanitize_identifier(rtsl_mangled_name);
    return out;
}

std::string Mangler::mangle_rtsl(const MangleInput &input) const {
    if (input.stage != StageKind::none) {
        return std::string(input.name);
    }

    std::string out = "_Z";
    const auto parts = split_qualified_name(input.name);
    if (parts.size() > 1) {
        out += "N";
        for (const auto part : parts) {
            out += encode_name_part(part);
        }
        out += "E";
    } else {
        out += encode_name_part(input.name);
    }

    if (input.parameter_types.empty()) {
        out += "v";
    } else {
        for (const auto &parameter_type : input.parameter_types) {
            out += encode_type(parameter_type);
        }
    }
    return out;
}

std::string Mangler::mangle_for_glsl(const MangleInput &input) const {
    if (input.stage != StageKind::none) {
        return sanitize_identifier(input.name);
    }
    return mangle_glsl_from_rtsl(mangle_rtsl(input));
}

} // namespace rtsl
