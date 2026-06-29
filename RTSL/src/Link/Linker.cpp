#include "Link/Linker.hpp"

#include <unordered_map>

namespace rtsl {

namespace {

// Apply a flat id offset to every IRId reference in an instruction so a
// freshly-loaded module's id space stops overlapping with the merged module's.
void shift_instruction(IRInstruction &inst, IRId offset) {
    if (inst.result_id != IRId_None) inst.result_id += offset;
    if (inst.type_id != IRId_None) inst.type_id += offset;
    for (auto &operand : inst.operands) {
        if (operand != IRId_None) operand += offset;
    }
}

// Merge `src` into `dst`. After this, `dst.next_id` is the new high-water
// mark and every id originally living in `src` has been shifted by
// (dst.next_id - 1). The shift relies on the convention that id 0 is the
// reserved "no id" sentinel.
void merge_module(IRModule &dst, IRModule src) {
    const IRId offset = dst.next_id - 1;
    dst.next_id += src.next_id - 1;

    for (auto inst : src.type_constant_pool) {
        shift_instruction(inst, offset);
        dst.type_constant_pool.push_back(std::move(inst));
    }
    for (auto inst : src.global_variables) {
        shift_instruction(inst, offset);
        dst.global_variables.push_back(std::move(inst));
    }
    for (auto decoration : src.decorations) {
        if (decoration.target != IRId_None) decoration.target += offset;
        dst.decorations.push_back(std::move(decoration));
    }

    // FunctionCall.literals[0] is an index into call_target_names; shift it
    // by the existing target count so cross-module call resolution still
    // works after the merge.
    const u32 name_offset = static_cast<u32>(dst.call_target_names.size());
    for (auto &name : src.call_target_names) {
        dst.call_target_names.push_back(std::move(name));
    }

    for (auto fn : src.functions) {
        if (fn.result_id != IRId_None) fn.result_id += offset;
        if (fn.function_type_id != IRId_None) fn.function_type_id += offset;
        if (fn.return_type_id != IRId_None) fn.return_type_id += offset;
        for (auto &pid : fn.parameter_ids) {
            if (pid != IRId_None) pid += offset;
        }
        for (auto &inst : fn.body) {
            shift_instruction(inst, offset);
            if (inst.op == IROp::FunctionCall && !inst.literals.empty()) {
                inst.literals[0] += name_offset;
            }
        }
        dst.functions.push_back(std::move(fn));
    }

    for (auto debug : src.function_debug) {
        dst.function_debug.push_back(std::move(debug));
    }
    for (auto &decl : src.structs) {
        dst.structs.push_back(std::move(decl));
    }
    for (auto &uniform : src.uniforms) {
        dst.uniforms.push_back(std::move(uniform));
    }
    for (auto &interface : src.stage_interfaces) {
        dst.stage_interfaces.push_back(std::move(interface));
    }
}

// One inline pass over the merged module: each user-function FunctionCall
// whose target now lives in this module gets its callee body spliced in. Run
// to a fixed point so cross-module call chains expand fully.
bool inline_one_module_pass(IRModule &ir) {
    bool progress = false;
    for (auto &fn : ir.functions) {
        std::vector<IRInstruction> new_body;
        new_body.reserve(fn.body.size());
        std::unordered_map<IRId, IRId> call_result_remap;
        const auto apply_remap = [&](IRInstruction &inst) {
            for (auto &op : inst.operands) {
                if (auto it = call_result_remap.find(op); it != call_result_remap.end()) op = it->second;
            }
        };

        for (const auto &inst : fn.body) {
            if (inst.op != IROp::FunctionCall || inst.operands.empty() || inst.literals.empty()) {
                IRInstruction copy = inst;
                apply_remap(copy);
                new_body.push_back(std::move(copy));
                continue;
            }
            const u32 name_index = inst.literals[0];
            if (name_index >= ir.call_target_names.size()) {
                IRInstruction copy = inst;
                apply_remap(copy);
                new_body.push_back(std::move(copy));
                continue;
            }
            const std::string &callee_name = ir.call_target_names[name_index];
            const std::size_t arg_count = inst.operands.size() - 1;

            const IRFunction *target = nullptr;
            for (const auto &candidate : ir.functions) {
                if (&candidate == &fn) continue;
                if (candidate.source_name != callee_name) continue;
                if (candidate.parameter_ids.size() != arg_count) continue;
                // A forward declaration with no body can't be inlined; keep
                // looking for the real definition (which is the whole point
                // of cross-module linking - the def may live in another input).
                if (candidate.body.empty()) continue;
                target = &candidate;
                break;
            }
            if (!target) {
                IRInstruction copy = inst;
                apply_remap(copy);
                new_body.push_back(std::move(copy));
                continue;
            }

            std::unordered_map<IRId, IRId> local_remap;
            for (std::size_t i = 0; i < target->parameter_ids.size(); ++i) {
                IRId arg_id = inst.operands[i + 1];
                if (auto it = call_result_remap.find(arg_id); it != call_result_remap.end()) arg_id = it->second;
                local_remap[target->parameter_ids[i]] = arg_id;
            }

            IRId returned_id = IRId_None;
            for (const auto &body_inst : target->body) {
                if (body_inst.op == IROp::Label) continue;
                if (body_inst.op == IROp::FunctionParameter) continue;
                if (body_inst.op == IROp::Return) continue;
                if (body_inst.op == IROp::ReturnValue) {
                    if (!body_inst.operands.empty()) {
                        IRId ret = body_inst.operands[0];
                        if (auto it = local_remap.find(ret); it != local_remap.end()) ret = it->second;
                        returned_id = ret;
                    }
                    continue;
                }
                IRInstruction copy = body_inst;
                if (copy.result_id != IRId_None) {
                    const IRId fresh = ir.next_id++;
                    local_remap[copy.result_id] = fresh;
                    copy.result_id = fresh;
                }
                for (auto &operand : copy.operands) {
                    if (auto it = local_remap.find(operand); it != local_remap.end()) operand = it->second;
                }
                new_body.push_back(std::move(copy));
            }
            if (inst.result_id != IRId_None && returned_id != IRId_None) {
                call_result_remap[inst.result_id] = returned_id;
            }
            progress = true;
        }

        fn.body = std::move(new_body);
    }
    return progress;
}

void inline_cross_module_calls(IRModule &ir) {
    for (int iteration = 0; iteration < 64; ++iteration) {
        if (!inline_one_module_pass(ir)) break;
    }
}

} // namespace

Linker::Linker(DiagnosticEngine &diagnostics) : diagnostics_(diagnostics) {}

bool Linker::add_artifact_bytes(std::span<const u8> bytes) {
    Artifact artifact;
    if (!read_artifact(bytes, artifact, &diagnostics_)) {
        return false;
    }
    inputs_.push_back(std::move(artifact));
    return true;
}

bool Linker::add_artifact(Artifact artifact) {
    if (artifact.bytes.empty()) {
        diagnostics_.report(6001, DiagnosticSeverity::error, {}, "<link>", "cannot link an empty artifact");
        return false;
    }
    inputs_.push_back(std::move(artifact));
    return true;
}

Artifact Linker::link_program() {
    Artifact program{.kind = ArtifactKind::program};
    if (inputs_.empty()) {
        diagnostics_.report(6002, DiagnosticSeverity::error, {}, "<link>", "no input artifacts provided");
        return program;
    }

    if (inputs_.size() == 1) {
        program = inputs_.front();
        program.kind = ArtifactKind::program;
        program.bytes = write_artifact(ArtifactKind::program, program.module);
        validate_program_stages(program);
        return program;
    }

    // Multi-input link: take the first input as the base, then merge each
    // subsequent input's IR module into it with id remapping. After every
    // input is merged, run the cross-module inliner so user-function calls
    // that crossed module boundaries get their bodies spliced in (the same
    // way the single-module inliner handled in-module calls at IR-gen time).
    program = inputs_.front();
    program.kind = ArtifactKind::program;
    for (std::size_t i = 1; i < inputs_.size(); ++i) {
        merge_module(program.module, std::move(inputs_[i].module));
        for (const auto &decl : inputs_[i].structs) program.structs.push_back(decl);
        for (const auto &uniform : inputs_[i].uniforms) program.uniforms.push_back(uniform);
        for (const auto &interface : inputs_[i].stage_interfaces) program.stage_interfaces.push_back(interface);
        for (const auto &entry : inputs_[i].entries) program.entries.push_back(entry);
    }
    inline_cross_module_calls(program.module);

    program.bytes = write_artifact(ArtifactKind::program, program.module);
    validate_program_stages(program);
    return program;
}

Artifact Linker::link_library() {
    Artifact library{.kind = ArtifactKind::library};
    if (inputs_.empty()) {
        diagnostics_.report(6002, DiagnosticSeverity::error, {}, "<link>", "no input artifacts provided");
        return library;
    }

    library = inputs_.front();
    library.kind = ArtifactKind::library;
    for (std::size_t i = 1; i < inputs_.size(); ++i) {
        merge_module(library.module, std::move(inputs_[i].module));
        for (const auto &decl : inputs_[i].structs) library.structs.push_back(decl);
        for (const auto &uniform : inputs_[i].uniforms) library.uniforms.push_back(uniform);
        for (const auto &interface : inputs_[i].stage_interfaces) library.stage_interfaces.push_back(interface);
        for (const auto &entry : inputs_[i].entries) library.entries.push_back(entry);
    }
    // Try to resolve any cross-input calls now. Calls that still target
    // symbols this library doesn't define survive as unresolved
    // FunctionCalls - that's fine for a library; the program linker will
    // resolve them later when more inputs are available.
    inline_cross_module_calls(library.module);

    library.bytes = write_artifact(ArtifactKind::library, library.module);
    return library;
}

Artifact extract_module_interface(const Artifact &source) {
    Artifact module_artifact{.kind = ArtifactKind::module};
    // Carry over the source name so importer-side diagnostics can point
    // back to the .rtsl this interface came from.
    module_artifact.module.source_name = source.module.source_name;
    for (const auto &fn : source.module.functions) {
        if (!fn.exported) continue;
        // Public interface entry: signature only, no body. Importers see
        // these as forward declarations and the linker resolves them
        // against an actual definition in an .rtslo / .rtsll input.
        IRFunction declaration;
        declaration.result_id = fn.result_id;
        declaration.return_type_id = fn.return_type_id;
        declaration.stage = fn.stage;
        declaration.exported = true;
        declaration.source_name = fn.source_name;
        declaration.parameter_type_names = fn.parameter_type_names;
        declaration.parameter_ids = fn.parameter_ids;
        module_artifact.module.functions.push_back(std::move(declaration));
    }
    // No exports means there's nothing to publish through .rtslm. Skip
    // sidecar emission entirely so users don't get phantom module files
    // sitting next to their compiled objects.
    if (module_artifact.module.functions.empty()) {
        return module_artifact;
    }
    // Carry over the full struct table so the exported signatures resolve
    // to real struct definitions. Pruning down to just the structs that
    // are actually transitively referenced is a later optimization.
    module_artifact.module.structs = source.module.structs;
    module_artifact.structs = source.structs;
    module_artifact.bytes = write_artifact(ArtifactKind::module, module_artifact.module);
    return module_artifact;
}

void Linker::validate_program_stages(const Artifact &program) {
    bool has_vertex = false;
    bool has_fragment = false;
    for (const auto &entry : program.entries) {
        if (entry.stage == StageKind::none) {
            continue;
        }
        switch (entry.stage) {
        case StageKind::vertex: has_vertex = true; break;
        case StageKind::fragment: has_fragment = true; break;
        case StageKind::none: break;
        }
    }

    const auto error = [&](std::string message) {
        diagnostics_.report(6003, DiagnosticSeverity::error, {}, "<link>", std::move(message));
    };

    // A program that declares any graphics stage is a graphics program and must
    // provide at least a vertex and a fragment stage.
    if (has_vertex || has_fragment) {
        if (!has_vertex) {
            error("graphics program is missing a vertex stage (vert)");
        }
        if (!has_fragment) {
            error("graphics program is missing a fragment stage (frag)");
        }
    }
}

} // namespace rtsl
