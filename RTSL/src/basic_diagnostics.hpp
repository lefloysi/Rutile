#pragma once

#include "basic_source_manager.hpp"

#include <ostream>
#include <string>
#include <vector>

namespace rtsl {

enum class DiagnosticSeverity : u8 {
    ignored,
    note,
    warning,
    error,
    fatal,
};

struct Diagnostic {
    int code = 0;
    DiagnosticSeverity severity = DiagnosticSeverity::error;
    std::string source_name;
    SourceLocation location{};
    std::string message;
};

class DiagnosticEngine {
public:
    void clear();
    void report(int code, DiagnosticSeverity severity, SourceLocation location,
                std::string source_name, std::string message);
    void render(std::ostream &out, const SourceManager *sources = nullptr) const;

    [[nodiscard]] bool has_error() const;
    [[nodiscard]] const std::vector<Diagnostic> &diagnostics() const { return diagnostics_; }

private:
    std::vector<Diagnostic> diagnostics_;
};

} // namespace rtsl
