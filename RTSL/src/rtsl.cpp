#include "rtsl.h"

#include "compiler.hpp"
#include "uniform_lowering.hpp"
#include "linker.hpp"
#include "artifact.hpp"

#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

rtsl_stage to_c_stage(rtsl::StageKind stage) {
    switch (stage) {
    case rtsl::StageKind::vertex: return RTSL_STAGE_VERTEX;
    case rtsl::StageKind::fragment: return RTSL_STAGE_FRAGMENT;
    case rtsl::StageKind::none: return RTSL_STAGE_NONE;
    }
    return RTSL_STAGE_NONE;
}

rtsl_stage_role to_c_role(rtsl::StageRole role) {
    switch (role) {
    case rtsl::StageRole::input: return RTSL_STAGE_ROLE_INPUT;
    case rtsl::StageRole::varying: return RTSL_STAGE_ROLE_VARYING;
    case rtsl::StageRole::output: return RTSL_STAGE_ROLE_OUTPUT;
    }
    return RTSL_STAGE_ROLE_VARYING;
}

} // namespace

struct rtsl_module_t {
    rtsl::Artifact artifact;

    // Reflection views cached on first query. The backing strings live in the
    // artifact (or in owned_strings), so the const char* fields stay valid for
    // the lifetime of the module.
    std::vector<std::string> owned_strings;
    std::vector<rtsl_uniform_info> uniform_views;
    std::vector<rtsl_stage_variable> stage_views;
    std::vector<rtsl_entry_info> entry_views;
    bool reflection_built = false;

    void ensure_reflection() {
        if (reflection_built) {
            return;
        }
        reflection_built = true;

        // Computed binding names need stable storage; reserve so the vector
        // never reallocates while we capture pointers into it.
        owned_strings.reserve(artifact.uniforms.size());
        for (const auto &uniform : artifact.uniforms) {
            owned_strings.push_back(rtsl::uniform_binding_name(uniform));
        }
        uniform_views.reserve(artifact.uniforms.size());
        for (std::size_t i = 0; i < artifact.uniforms.size(); ++i) {
            const auto &uniform = artifact.uniforms[i];
            uniform_views.push_back(rtsl_uniform_info{
                .scope_name = uniform.scope_name.c_str(),
                .name = uniform.name.c_str(),
                .type = uniform.type.c_str(),
                .binding_name = owned_strings[i].c_str(),
                .set = uniform.set,
                .binding = uniform.binding,
                .is_resource = rtsl::is_resource_uniform_type(uniform.type) ? 1 : 0,
            });
        }

        for (const auto &interface : artifact.stage_interfaces) {
            for (const auto &field : interface.fields) {
                stage_views.push_back(rtsl_stage_variable{
                    .role = to_c_role(interface.role),
                    .payload_type = interface.type_name.c_str(),
                    .name = field.name.c_str(),
                    .interpolation = field.interpolation.c_str(),
                    .builtin = field.builtin.c_str(),
                    .location = field.location,
                    .has_location = field.has_location ? 1 : 0,
                });
            }
        }

        for (const auto &entry : artifact.entries) {
            entry_views.push_back(rtsl_entry_info{
                .name = entry.name.c_str(),
                .stage = to_c_stage(entry.stage),
            });
        }
    }
};

struct rtsl_context_t {
    rtsl::CompilerInstance compiler;
    rtsl_result result{RTSL_OK, "ok"};
};

struct rtsl_linker_t {
    rtsl_context_t *ctx = nullptr;
    rtsl::Linker linker;

    explicit rtsl_linker_t(rtsl_context_t *context)
        : ctx(context), linker(context->compiler.diagnostics()) {}
};

namespace {

void set_result(rtsl_context ctx, int code, const char *text) {
    if (ctx) {
        ctx->result = rtsl_result{code, text};
    }
}

rtsl_output_kind to_c_kind(rtsl::ArtifactKind kind) {
    switch (kind) {
    case rtsl::ArtifactKind::object: return RTSL_OUTPUT_OBJECT;
    case rtsl::ArtifactKind::module: return RTSL_OUTPUT_MODULE;
    case rtsl::ArtifactKind::library: return RTSL_OUTPUT_LIBRARY;
    case rtsl::ArtifactKind::program: return RTSL_OUTPUT_PROGRAM;
    }
    return RTSL_OUTPUT_OBJECT;
}

rtsl_diagnostic_severity to_c_severity(rtsl::DiagnosticSeverity severity) {
    switch (severity) {
    case rtsl::DiagnosticSeverity::ignored: return RTSL_DIAG_IGNORED;
    case rtsl::DiagnosticSeverity::note: return RTSL_DIAG_NOTE;
    case rtsl::DiagnosticSeverity::warning: return RTSL_DIAG_WARNING;
    case rtsl::DiagnosticSeverity::error: return RTSL_DIAG_ERROR;
    case rtsl::DiagnosticSeverity::fatal: return RTSL_DIAG_FATAL;
    }
    return RTSL_DIAG_ERROR;
}

} // namespace

extern "C" {

rtsl_context rtslCreateContext(void) {
    try {
        return new rtsl_context_t();
    } catch (...) {
        return nullptr;
    }
}

rtsl_result rtslCtxGetResult(rtsl_context ctx) {
    return ctx ? ctx->result : rtsl_result{RTSL_ERROR_INVALID_ARGUMENT, "null context"};
}

size_t rtslCtxGetDiagnosticCount(rtsl_context ctx) {
    if (!ctx) {
        return 0;
    }
    return ctx->compiler.diagnostics().diagnostics().size();
}

rtsl_diagnostic rtslCtxGetDiagnostic(rtsl_context ctx, size_t index) {
    if (!ctx || index >= ctx->compiler.diagnostics().diagnostics().size()) {
        return {};
    }
    const auto &diagnostic = ctx->compiler.diagnostics().diagnostics()[index];
    return rtsl_diagnostic{
        .code = diagnostic.code,
        .severity = to_c_severity(diagnostic.severity),
        .source_name = diagnostic.source_name.c_str(),
        .offset = diagnostic.location.offset,
        .line = diagnostic.location.line,
        .column = diagnostic.location.column,
        .text = diagnostic.message.c_str(),
    };
}

void rtslDestroyContext(rtsl_context ctx) {
    delete ctx;
}

rtsl_module rtslCompileSource(rtsl_context ctx, const char *source, size_t source_size, const char *source_name) {
    if (!ctx || (!source && source_size != 0)) {
        set_result(ctx, RTSL_ERROR_INVALID_ARGUMENT, "invalid compile arguments");
        return nullptr;
    }

    try {
        rtsl::CompilerInvocation invocation;
        invocation.source_name = source_name ? source_name : "<memory>";
        rtsl::Artifact artifact;
        ctx->compiler.compile_source_to(artifact, std::string_view(source ? source : "", source_size), std::move(invocation));
        if (ctx->compiler.diagnostics().has_error() || artifact.bytes.empty()) {
            set_result(ctx, RTSL_ERROR_COMPILE_FAILED, "compile failed");
            return nullptr;
        }
        set_result(ctx, RTSL_OK, "ok");
        return new rtsl_module_t{.artifact = std::move(artifact)};
    } catch (const std::bad_alloc &) {
        set_result(ctx, RTSL_ERROR_INTERNAL, "allocation failed");
        return nullptr;
    } catch (...) {
        set_result(ctx, RTSL_ERROR_INTERNAL, "internal compiler error");
        return nullptr;
    }
}

rtsl_blob rtslModuleGetBytecode(rtsl_module module) {
    if (!module) {
        return {};
    }
    return rtsl_blob{module->artifact.bytes.data(), module->artifact.bytes.size()};
}

rtsl_output_kind rtslModuleGetKind(rtsl_module module) {
    return module ? to_c_kind(module->artifact.kind) : RTSL_OUTPUT_OBJECT;
}

void rtslDestroyModule(rtsl_module module) {
    delete module;
}

rtsl_module rtslLoadModule(rtsl_context ctx, const uint8_t *data, size_t size) {
    if (!ctx || (!data && size != 0)) {
        set_result(ctx, RTSL_ERROR_INVALID_ARGUMENT, "invalid load arguments");
        return nullptr;
    }
    try {
        rtsl::Artifact artifact;
        if (!rtsl::read_artifact(std::span<const rtsl::u8>(data, size), artifact, &ctx->compiler.diagnostics())) {
            set_result(ctx, RTSL_ERROR_INVALID_ARGUMENT, "failed to load artifact");
            return nullptr;
        }
        set_result(ctx, RTSL_OK, "ok");
        return new rtsl_module_t{.artifact = std::move(artifact)};
    } catch (...) {
        set_result(ctx, RTSL_ERROR_INTERNAL, "internal error while loading artifact");
        return nullptr;
    }
}

size_t rtslModuleGetUniformCount(rtsl_module module) {
    if (!module) {
        return 0;
    }
    module->ensure_reflection();
    return module->uniform_views.size();
}

int rtslModuleGetUniform(rtsl_module module, size_t index, rtsl_uniform_info *out_info) {
    if (!module || !out_info) {
        return 0;
    }
    module->ensure_reflection();
    if (index >= module->uniform_views.size()) {
        return 0;
    }
    *out_info = module->uniform_views[index];
    return 1;
}

size_t rtslModuleGetStageVariableCount(rtsl_module module) {
    if (!module) {
        return 0;
    }
    module->ensure_reflection();
    return module->stage_views.size();
}

int rtslModuleGetStageVariable(rtsl_module module, size_t index, rtsl_stage_variable *out_var) {
    if (!module || !out_var) {
        return 0;
    }
    module->ensure_reflection();
    if (index >= module->stage_views.size()) {
        return 0;
    }
    *out_var = module->stage_views[index];
    return 1;
}

int rtslModuleGetStageLocation(rtsl_module module, const char *payload_type, const char *field_name,
                               uint32_t *out_location) {
    if (!module || !payload_type || !field_name || !out_location) {
        return 0;
    }
    module->ensure_reflection();
    for (const auto &var : module->stage_views) {
        if (var.has_location && std::strcmp(var.payload_type, payload_type) == 0 &&
            std::strcmp(var.name, field_name) == 0) {
            *out_location = var.location;
            return 1;
        }
    }
    return 0;
}

size_t rtslModuleGetEntryCount(rtsl_module module) {
    if (!module) {
        return 0;
    }
    module->ensure_reflection();
    return module->entry_views.size();
}

int rtslModuleGetEntry(rtsl_module module, size_t index, rtsl_entry_info *out_entry) {
    if (!module || !out_entry) {
        return 0;
    }
    module->ensure_reflection();
    if (index >= module->entry_views.size()) {
        return 0;
    }
    *out_entry = module->entry_views[index];
    return 1;
}

rtsl_linker rtslCreateLinker(rtsl_context ctx) {
    if (!ctx) {
        return nullptr;
    }
    try {
        return new rtsl_linker_t(ctx);
    } catch (...) {
        set_result(ctx, RTSL_ERROR_INTERNAL, "failed to create linker");
        return nullptr;
    }
}

int rtslLinkerAddModule(rtsl_linker linker, rtsl_module module) {
    if (!linker || !module) {
        return 0;
    }
    return linker->linker.add_artifact(module->artifact) ? 1 : 0;
}

int rtslLinkerAddBlob(rtsl_linker linker, const uint8_t *data, size_t size) {
    if (!linker || (!data && size != 0)) {
        return 0;
    }
    return linker->linker.add_artifact_bytes(std::span<const rtsl::u8>(data, size)) ? 1 : 0;
}

rtsl_module rtslLinkProgram(rtsl_linker linker) {
    if (!linker) {
        return nullptr;
    }
    try {
        auto artifact = linker->linker.link_program();
        if (linker->ctx->compiler.diagnostics().has_error() || artifact.bytes.empty()) {
            set_result(linker->ctx, RTSL_ERROR_LINK_FAILED, "link failed");
            return nullptr;
        }
        set_result(linker->ctx, RTSL_OK, "ok");
        return new rtsl_module_t{.artifact = std::move(artifact)};
    } catch (...) {
        set_result(linker->ctx, RTSL_ERROR_INTERNAL, "internal linker error");
        return nullptr;
    }
}

void rtslDestroyLinker(rtsl_linker linker) {
    delete linker;
}

} // extern "C"
