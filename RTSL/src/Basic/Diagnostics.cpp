#include "Basic/Diagnostics.hpp"

#include <ostream>

namespace rtsl {

void DiagnosticEngine::clear() {
    diagnostics_.clear();
}

void DiagnosticEngine::report(int code, DiagnosticSeverity severity, SourceLocation location,
                              std::string source_name, std::string message) {
    diagnostics_.push_back(Diagnostic{
        .code = code,
        .severity = severity,
        .source_name = std::move(source_name),
        .location = location,
        .message = std::move(message),
    });
}

void DiagnosticEngine::render(std::ostream &out, const SourceManager *sources) const {
    for (const auto &diagnostic : diagnostics_) {
        const char *severity = diagnostic.severity == DiagnosticSeverity::warning ? "warning"
                                : diagnostic.severity == DiagnosticSeverity::note   ? "note"
                                : diagnostic.severity == DiagnosticSeverity::fatal  ? "fatal error"
                                                                                     : "error";
        out << diagnostic.source_name << '(' << diagnostic.location.line << ',' << diagnostic.location.column
            << "): " << severity;
        if (diagnostic.code != 0) {
            out << " C" << diagnostic.code;
        }
        out << ": " << diagnostic.message << '\n';
        if (!sources || diagnostic.location.file_id >= sources->file_count()) {
            continue;
        }
        const auto text = sources->buffer(diagnostic.location.file_id);
        const auto line_start = diagnostic.location.offset - (diagnostic.location.column - 1);
        const auto line_end = text.find('\n', line_start);
        out << text.substr(line_start, line_end == std::string_view::npos ? text.size() - line_start
                                                                          : line_end - line_start)
            << '\n';
        for (u32 i = 1; i < diagnostic.location.column; ++i) {
            out << ' ';
        }
        out << "^\n";
    }
}

bool DiagnosticEngine::has_error() const {
    for (const auto &diagnostic : diagnostics_) {
        if (diagnostic.severity == DiagnosticSeverity::error ||
            diagnostic.severity == DiagnosticSeverity::fatal) {
            return true;
        }
    }
    return false;
}

} // namespace rtsl
