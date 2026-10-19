#pragma once

#include <cstddef>
#include <cstdint>

struct RutileLanguageText {
	const char* data;
	std::size_t size;
};

struct RutileLanguageSource {
	RutileLanguageText path;
	RutileLanguageText text;
};

// Offsets are half-open UTF-8 byte ranges in the corresponding input source.
struct RutileLanguageRange {
	std::size_t source;
	std::size_t begin;
	std::size_t end;
};

enum RutileLanguageSymbolFlags : std::uint32_t {
	rutile_symbol_has_definition = 1,
	rutile_symbol_template_parameter = 2,
	rutile_symbol_template_instance = 4,
};

enum RutileLanguageOccurrenceRoles : std::uint32_t {
	rutile_occurrence_declaration = 1,
	rutile_occurrence_type = 2,
	rutile_occurrence_read = 4,
	rutile_occurrence_write = 8,
	rutile_occurrence_call = 16,
};

struct RutileLanguageSymbol {
	std::uint64_t id;
	std::uint64_t container;
	std::uint64_t origin;
	std::uint32_t kind; // LSP SymbolKind.
	std::uint32_t flags;
	RutileLanguageRange declaration;
	RutileLanguageRange definition;
	RutileLanguageText name;
	RutileLanguageText detail;
};

struct RutileLanguageOccurrence {
	RutileLanguageRange range;
	std::uint64_t symbol;
	std::uint64_t context;
	std::uint32_t roles;
};

struct RutileLanguageDiagnostic {
	RutileLanguageRange range;
	std::uint32_t severity;
	RutileLanguageText message;
};

// Input text is copied. Returned text is borrowed until snapshot destruction.
// Enumeration order and symbol IDs are stable within an immutable snapshot.
// A missing source location uses source == SIZE_MAX and begin == end == 0.
using RutileLanguageCancellation = int (*)(void* context);

extern "C" void* rutile_language_analyze(const RutileLanguageSource* sources, std::size_t count, std::uint32_t pointer_bits, RutileLanguageCancellation cancelled, void* context);
extern "C" void rutile_language_destroy(void* snapshot);
extern "C" std::size_t rutile_language_symbol_count(const void* snapshot);
extern "C" int rutile_language_symbol(const void* snapshot, std::size_t index, RutileLanguageSymbol* result);
extern "C" std::size_t rutile_language_occurrence_count(const void* snapshot);
extern "C" int rutile_language_occurrence(const void* snapshot, std::size_t index, RutileLanguageOccurrence* result);
extern "C" std::size_t rutile_language_diagnostic_count(const void* snapshot);
extern "C" int rutile_language_diagnostic(const void* snapshot, std::size_t index, RutileLanguageDiagnostic* result);
extern "C" std::size_t rutile_language_related_count(const void* snapshot, std::size_t diagnostic);
extern "C" int rutile_language_related(const void* snapshot, std::size_t diagnostic, std::size_t index, RutileLanguageDiagnostic* result);
