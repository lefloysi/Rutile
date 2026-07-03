#include "basic_diagnostics.hpp"

#include <ostream>

namespace rtsl {

void DiagnosticEngine::clear() {
	diagnostics_.clear();
}

void DiagnosticEngine::report(int code, DiagnosticSeverity severity, SourceLocation location, std::string_view source_name, std::string_view message) {
	diagnostics_.emplace_back(Diagnostic{
		.code = code,
		.severity = severity,
		.source_name = std::string(source_name),
		.location = location,
		.message = std::string(message),
	});
}

static std::string_view severity_name(DiagnosticSeverity severity) {
	switch (severity) {
	case DiagnosticSeverity::note: return "note";
	case DiagnosticSeverity::warning: return "warning";
	case DiagnosticSeverity::error: return "error";
	case DiagnosticSeverity::fatal: return "error";
	case DiagnosticSeverity::ignored: return "ignored";
	}
	return "error";
}

void DiagnosticEngine::render(std::ostream& out, const SourceManager* sources) const {
	for (const auto& diagnostic : diagnostics_) {
		const auto severity = severity_name(diagnostic.severity);
		out << diagnostic.source_name << '(' << diagnostic.location.line << ',' << diagnostic.location.column
			<< "): " << severity;
		if (diagnostic.code != 0) {
			out << " RTSL" << diagnostic.code;
		}
		out << ": " << diagnostic.message << '\n';
		if (!sources || diagnostic.location.file_id >= sources->file_count()) {
			continue;
		}
		const auto text = sources->buffer(diagnostic.location.file_id);
		const auto line_start = diagnostic.location.offset - (diagnostic.location.column - 1);
		const auto line_end = text.find('\n', line_start);
		out << text.substr(line_start, line_end == std::string_view::npos ? text.size() - line_start : line_end - line_start)
			<< '\n';
		for (u32 i = 1; i < diagnostic.location.column; ++i) {
			out << ' ';
		}
		out << "^\n";
	}
}

bool DiagnosticEngine::has_error() const {
	for (const auto& diagnostic : diagnostics_) {
		if (diagnostic.severity == DiagnosticSeverity::error ||
			diagnostic.severity == DiagnosticSeverity::fatal) {
			return true;
		}
	}
	return false;
}

} // namespace rtsl
