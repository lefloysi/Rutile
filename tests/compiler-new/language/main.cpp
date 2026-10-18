#include "rutile/language.hpp"

#include <cassert>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

extern "C" int shared_analysis_tests();

using Snapshot = std::unique_ptr<void, decltype(&rutile_language_destroy)>;

RutileLanguageText text(std::string_view value) {
	return {value.data(), value.size()};
}

std::vector<RutileLanguageOccurrence> at(const void* snapshot, std::size_t source,
	std::size_t offset) {
	std::vector<RutileLanguageOccurrence> result;
	for (std::size_t index = 0; index < rutile_language_occurrence_count(snapshot); ++index) {
		RutileLanguageOccurrence occurrence{};
		assert(rutile_language_occurrence(snapshot, index, &occurrence));
		if (occurrence.range.source == source && occurrence.range.begin <= offset
			&& offset < occurrence.range.end) {
			result.push_back(occurrence);
		}
	}
	return result;
}

RutileLanguageSymbol symbol(const void* snapshot, std::uint64_t id) {
	for (std::size_t index = 0; index < rutile_language_symbol_count(snapshot); ++index) {
		RutileLanguageSymbol result{};
		assert(rutile_language_symbol(snapshot, index, &result));
		if (result.id == id) {
			return result;
		}
	}
	assert(false);
	return {};
}

int cancelled(void*) {
	return 1;
}

int main() {
	std::string declarations = R"rt(
struct Foo { i32 data; };
struct Bar { i32 data; };
template<typename T> struct Box {
	T value;
	fn read(const Box& this) -> i32;
};
template<typename T> fn Box<T>::read(const Box& this) -> i32 {
	return this.value.data;
};
fn select(const Foo& value) -> i32 { return value.data; };
fn select(const Bar& value) -> i32 { return value.data; };
)rt";
	std::string uses = R"rt(
fn use() -> i32 {
	Foo outer;
	{
		Bar outer;
		@outer.data = 1;
		select(outer);
	};
	Box<Foo> first;
	Box<Bar> second;
	@first.value.data = 20;
	@second.value.data = 22;
	return first.read() + second.read();
};
)rt";
	RutileLanguageSource input[] = {
		{text("declarations.rt"), text(declarations)},
		{text("uses.rt"), text(uses)},
	};
	Snapshot snapshot(rutile_language_analyze(input, 2, 64, nullptr, nullptr),
		rutile_language_destroy);
	assert(snapshot);
	assert(rutile_language_diagnostic_count(snapshot.get()) == 0);

	// A type argument is a bound type occurrence in the other source, not a textual match.
	auto foo = at(snapshot.get(), 1, uses.find("Box<Foo>") + 4);
	assert(!foo.empty());
	bool cross_file = false;
	for (const auto& occurrence : foo) {
		auto target = symbol(snapshot.get(), occurrence.symbol);
		cross_file |= target.definition.source == 0
			&& target.definition.begin == declarations.find("Foo");
	}
	assert(cross_file);

	// Final overload selection uses the shadowing Bar variable, not the outer Foo variable.
	auto selected = at(snapshot.get(), 1, uses.find("select(outer)"));
	assert(!selected.empty());
	bool bar_overload = false;
	for (const auto& occurrence : selected) {
		if (occurrence.roles & rutile_occurrence_call) {
			auto target = symbol(snapshot.get(), occurrence.symbol);
			bar_overload |= target.definition.source == 0
				&& target.definition.begin == declarations.find("select(const Bar");
		}
	}
	assert(bar_overload);

	// The same dependent member source occurrence retains both instantiation bindings.
	auto dependent = at(snapshot.get(), 0, declarations.find("this.value.data") + 11);
	std::set<std::uint64_t> targets;
	std::set<std::uint64_t> contexts;
	for (const auto& occurrence : dependent) {
		targets.insert(occurrence.symbol);
		if (occurrence.context) {
			contexts.insert(occurrence.context);
		}
	}
	assert(targets.size() == 2);
	assert(contexts.size() == 2);

	// A method call selects the out-of-line definition without conflating its origin with the enclosing record.
	auto method = at(snapshot.get(), 1, uses.find("first.read") + 6);
	bool definition = false;
	for (const auto& occurrence : method) {
		if (occurrence.roles & rutile_occurrence_call) {
			auto target = symbol(snapshot.get(), occurrence.symbol);
			definition |= target.origin == 0 && target.definition.source == 0
				&& target.definition.begin == declarations.find("::read") + 2;
		}
	}
	assert(definition);

	// Input storage can be changed or destroyed after analysis; the snapshot owns its text.
	declarations.assign("changed");
	uses.clear();
	assert(rutile_language_symbol_count(snapshot.get()) > 0);
	assert(!rutile_language_analyze(input, 2, 64, cancelled, nullptr));
	assert(shared_analysis_tests() == 42);
	return 42;
}
