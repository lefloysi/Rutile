#include "Sema/Sema.hpp"

#include <unordered_map>

namespace rtsl {

Sema::Sema(SourceManager &sources, DiagnosticEngine &diagnostics)
    : sources_(sources), diagnostics_(diagnostics) {}

SemanticModule Sema::analyze(const TranslationUnit &unit) {
    SemanticModule module{.source_name = std::string(sources_.name(unit.file_id))};
    module.structs = unit.structs;
    module.uniforms = unit.uniforms;
    module.stage_interfaces = unit.stage_interfaces;

    // Assign sequential locations to interface fields that did not request one
    // explicitly. Built-in slots (e.g. clip position) do not consume a location.
    for (auto &interface : module.stage_interfaces) {
        u32 next_location = 0;
        for (auto &field : interface.fields) {
            if (!field.builtin.empty()) {
                continue;
            }
            if (field.has_location) {
                next_location = field.location + 1;
                continue;
            }
            field.location = next_location++;
            field.has_location = true;
        }
    }
    // Every distinct scope maps to a descriptor set. Named scopes can be
    // reopened across multiple `uniform name { ... }` blocks (matched by
    // scope_name). Anonymous blocks cannot be reopened: each one gets its own
    // unique anonymous_block_id from the parser, so two `uniform { ... }`
    // blocks end up on different sets. Within a single block, fields share a
    // set and receive sequential bindings.
    std::unordered_map<std::string, u32> named_sets;
    u32 next_set = 0;
    std::unordered_map<u32, u32> binding_counts;
    for (auto &uniform : module.uniforms) {
        const std::string set_key = uniform.is_anonymous
                                        ? "$anon$" + std::to_string(uniform.anonymous_block_id)
                                        : uniform.scope_name;
        auto [it, inserted] = named_sets.emplace(set_key, next_set);
        if (inserted) {
            uniform.set = next_set++;
        } else {
            uniform.set = it->second;
        }
        uniform.binding = binding_counts[uniform.set]++;
    }

    for (const auto &decl : unit.declarations) {
        if (decl.kind == DeclKind::namespace_decl && decl.name == "rt") {
            diagnostics_.report(3001, DiagnosticSeverity::error, decl.span.begin,
                                module.source_name, "namespace 'rt' is reserved");
            continue;
        }

        module.symbols.push_back(SemanticSymbol{
            .kind = decl.kind,
            .name = decl.name,
            .parameters = decl.parameters,
            .return_type = decl.return_type,
            .body_statements = decl.body_statements,
            .exported = decl.exported,
        });
    }

    return module;
}

} // namespace rtsl
