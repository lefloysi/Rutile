#include "IR/UniformLowering.hpp"

#include "Basic/Types.hpp"

#include <string>

namespace rtsl {

namespace {

bool is_identifier_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

std::string sanitize_symbol_part(std::string_view part) {
    std::string sanitized;
    sanitized.reserve(part.size());
    for (const char c : part) {
        sanitized.push_back(is_identifier_char(c) ? c : '_');
    }
    return sanitized;
}

void append_mangled_part(std::string &name, std::string_view part) {
    const auto sanitized = sanitize_symbol_part(part);
    name += std::to_string(sanitized.size());
    name += sanitized;
}

u32 stable_uniform_hash(const UniformBinding &uniform) {
    u32 hash = 2166136261u;
    const auto add = [&](std::string_view text) {
        for (const unsigned char c : text) {
            hash ^= c;
            hash *= 16777619u;
        }
    };
    add(uniform.scope_name);
    add("\0");
    add(uniform.name);
    add("\0");
    add(uniform.type);
    add("\0");
    add(std::to_string(uniform.set));
    add("\0");
    add(std::to_string(uniform.binding));
    return hash;
}

std::string hash_suffix(u32 hash) {
    constexpr char Digits[] = "0123456789abcdef";
    std::string out = "_h";
    for (int shift = 28; shift >= 0; shift -= 4) {
        out.push_back(Digits[(hash >> shift) & 0xfu]);
    }
    return out;
}

bool is_replacement_boundary(std::string_view text, std::size_t pos) {
    if (pos >= text.size()) {
        return true;
    }
    const char c = text[pos];
    return !is_identifier_char(c) && c != ':';
}

std::string replace_symbol(std::string text, std::string_view from, std::string_view to) {
    std::size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        if ((pos != 0 && !is_replacement_boundary(text, pos - 1)) ||
            !is_replacement_boundary(text, pos + from.size())) {
            pos += from.size();
            continue;
        }
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
    return text;
}

} // namespace

bool is_resource_uniform_type(std::string_view type) {
    return type == "Sampler2D";
}

std::string uniform_binding_name(const UniformBinding &uniform) {
    std::string name = "u_";
    if (!uniform.is_anonymous && !uniform.scope_name.empty()) {
        append_mangled_part(name, uniform.scope_name);
        name += "_";
    }
    append_mangled_part(name, uniform.name);
    name += hash_suffix(stable_uniform_hash(uniform));
    return name;
}

std::string uniform_block_name(const UniformBinding &uniform) {
    std::string name = "ub_";
    if (!uniform.is_anonymous && !uniform.scope_name.empty()) {
        append_mangled_part(name, uniform.scope_name);
        name += "_";
    }
    append_mangled_part(name, uniform.name);
    name += hash_suffix(stable_uniform_hash(uniform));
    return name;
}

std::string lower_uniform_references(std::string statement, const std::vector<UniformBinding> &uniforms) {
    for (const auto &uniform : uniforms) {
        if (uniform.is_anonymous || uniform.scope_name.empty()) {
            const auto lowered_name = is_resource_uniform_type(uniform.type)
                                          ? uniform_binding_name(uniform)
                                          : uniform_binding_name(uniform) + ".value";
            statement = replace_symbol(std::move(statement), uniform.name, lowered_name);
            continue;
        }
        const auto source_name = uniform.scope_name + "::" + uniform.name;
        const auto lowered_name = is_resource_uniform_type(uniform.type)
                                      ? uniform_binding_name(uniform)
                                      : uniform_binding_name(uniform) + ".value";
        statement = replace_symbol(std::move(statement), source_name, lowered_name);
    }
    return statement;
}

} // namespace rtsl
