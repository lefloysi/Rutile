#include "compiler.hpp"

#include "ir.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "sema.hpp"

#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <span>
#include <unordered_set>

namespace rtsl {

std::string read_text_file(const std::filesystem::path& path) {
	std::ifstream input(path, std::ios::binary);
	if (!input) {
		return {};
	}
	return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

static void skip_ws(std::string_view& text) {
	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
}

// Consume the leading `[A-Za-z0-9_]+` run — a preprocessor identifier.
// Empty result means the head wasn't an identifier character.
static std::string_view take_identifier(std::string_view& text) {
	std::size_t n = 0;
	while (n < text.size() && (std::isalnum(static_cast<unsigned char>(text[n])) || text[n] == '_')) ++n;
	const auto out = text.substr(0, n);
	text.remove_prefix(n);
	return out;
}

std::string preprocess_source(std::string_view source, std::span<const std::string> defines) {
	std::unordered_set<std::string> defined(defines.begin(), defines.end());
	// active_stack.back() is the "should we emit lines right now" flag; each
	// #if{def,ndef} pushes and #endif pops. #else toggles under its parent's
	// active state so nested-and-both-inactive branches stay inactive.
	std::vector<bool> active_stack{ true };

	// Directive handlers keyed by the identifier that follows `#`. Each is
	// given the rest of the trimmed line (after the identifier) plus a
	// reference to `defined` and `active_stack`.
	struct DirectiveContext {
		std::string_view args;
		std::unordered_set<std::string>& defined;
		std::vector<bool>& active_stack;
		bool parent_active;
	};
	using Handler = void (*)(DirectiveContext&);
	static constexpr std::pair<std::string_view, Handler> directives[] = {
		{ "define", [](DirectiveContext& c) {
			 if (!c.parent_active) return;
			 skip_ws(c.args);
			 const auto name = take_identifier(c.args);
			 if (!name.empty()) c.defined.emplace(name);
		 } },
		{ "ifdef", [](DirectiveContext& c) {
			 skip_ws(c.args);
			 const auto name = take_identifier(c.args);
			 c.active_stack.push_back(c.parent_active && c.defined.contains(std::string(name)));
		 } },
		{ "ifndef", [](DirectiveContext& c) {
			 skip_ws(c.args);
			 const auto name = take_identifier(c.args);
			 c.active_stack.push_back(c.parent_active && !c.defined.contains(std::string(name)));
		 } },
		{ "else", [](DirectiveContext& c) {
			 if (c.active_stack.size() > 1) {
				 const bool parent = c.active_stack[c.active_stack.size() - 2];
				 c.active_stack.back() = parent && !c.active_stack.back();
			 }
		 } },
		{ "endif", [](DirectiveContext& c) {
			 if (c.active_stack.size() > 1) c.active_stack.pop_back();
		 } },
	};

	std::string out;
	out.reserve(source.size());
	std::size_t cursor = 0;
	while (cursor < source.size()) {
		const auto line_end = source.find('\n', cursor);
		const auto line_len = line_end == std::string_view::npos ? source.size() - cursor : line_end - cursor;
		const std::string_view line = source.substr(cursor, line_len);
		std::string_view trimmed = line;
		skip_ws(trimmed);
		while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) trimmed.remove_suffix(1);

		const bool active = active_stack.back();
		if (trimmed.starts_with('#')) {
			trimmed.remove_prefix(1);
			skip_ws(trimmed);
			const auto name = take_identifier(trimmed);
			for (const auto& [directive, handler] : directives) {
				if (name == directive) {
					DirectiveContext ctx{ trimmed, defined, active_stack, active };
					handler(ctx);
					break;
				}
			}
		} else if (active) {
			out.append(line.begin(), line.end());
			if (line_end != std::string_view::npos) out.push_back('\n');
		}
		if (line_end == std::string_view::npos) break;
		cursor = line_end + 1;
	}
	return out;
}

std::vector<std::filesystem::path> import_search_roots(const CompilerInvocation& invocation) {
	std::vector<std::filesystem::path> roots;
	if (!invocation.source_name.empty()) {
		roots.push_back(std::filesystem::path(invocation.source_name).parent_path());
	}
	for (const auto& path : invocation.import_paths) {
		roots.push_back(path);
	}
	return roots;
}

std::filesystem::path resolve_import(const CompilerInvocation& invocation, std::string_view name) {
	const auto roots = import_search_roots(invocation);
	for (const auto& root : roots) {
		const auto candidate = root / std::filesystem::path(std::string(name)).replace_extension(".rtslm");
		if (std::filesystem::exists(candidate)) {
			return candidate;
		}
	}
	return {};
}

Artifact CompilerInstance::compile_source(std::string_view source, CompilerInvocation invocation) {
	Artifact artifact;
	compile_source_to(artifact, source, std::move(invocation));
	return artifact;
}

void CompilerInstance::compile_source_to(Artifact& artifact, std::string_view source, CompilerInvocation invocation) {
	diagnostics_.clear();
	artifact = Artifact{ .kind = ArtifactKind::object };
	const auto preprocessed = preprocess_source(source, invocation.defines);
	const auto file_id = sources_.add_buffer(std::move(invocation.source_name), std::move(preprocessed));

	Lexer lexer(sources_, diagnostics_, file_id);
	const auto tokens = lexer.lex();

	Parser parser(sources_, diagnostics_, file_id, tokens);
	auto ast = parser.parse_translation_unit();

	std::vector<Artifact> imported_modules;
	for (const auto& import_name : ast.imports) {
		const auto import_path = resolve_import(invocation, import_name);
		if (import_path.empty()) {
			diagnostics_.report(2002, DiagnosticSeverity::error, sources_.location_at(file_id, 0), invocation.source_name,
								std::format("failed to resolve import '{}'", import_name));
			continue;
		}
		Artifact imported;
		std::ifstream input(import_path, std::ios::binary);
		std::vector<u8> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
		if (!read_artifact(bytes, imported, &diagnostics_)) {
			diagnostics_.report(2003, DiagnosticSeverity::error, sources_.location_at(file_id, 0), invocation.source_name,
								std::format("failed to load import '{}'", import_name));
			continue;
		}
		imported_modules.push_back(std::move(imported));
	}

	Sema sema(sources_, diagnostics_);
	auto semantic_module = sema.analyze(ast);
	for (const auto& imported : imported_modules) {
		semantic_module.imported_exports.insert(semantic_module.imported_exports.end(), imported.exports.begin(), imported.exports.end());
	}
	auto ir = lower_to_ir(semantic_module, &diagnostics_);

	if (!diagnostics_.has_error() && verify_ir(ir)) {
		artifact.bytes = write_artifact(ArtifactKind::object, ir);
		artifact.module = ir;
		artifact.strings.clear();
		artifact.structs = ir.structs;
		artifact.imports = ir.imports;
		artifact.imported_exports = ir.imported_exports;
		artifact.uniforms = ir.uniforms;
		artifact.stage_interfaces = ir.stage_interfaces;
		artifact.entries.clear();
		for (const auto& function : ir.functions) {
			if (function.stage == StageKind::none) {
				continue;
			}
			artifact.entries.push_back(Artifact::EntryPoint{
				.name = std::string(stage_entry_name(function.stage)),
				.mangled_name = std::string(stage_entry_name(function.stage)),
				.stage = function.stage,
				.function_id = function.result_id,
			});
		}
		artifact.debug_bytes = write_debug_artifact(ir);
	}
}

} // namespace rtsl
