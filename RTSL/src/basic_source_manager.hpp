#pragma once

#include "basic_types.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace rtsl {

struct SourceLocation {
    u32 file_id = 0;
    std::size_t offset = 0;
    u32 line = 1;
    u32 column = 1;
};

struct SourceSpan {
    SourceLocation begin{};
    std::size_t length = 0;
};

class SourceManager {
public:
    u32 add_buffer(std::string name, std::string text);

    [[nodiscard]] std::string_view buffer(u32 file_id) const;
    [[nodiscard]] std::string_view name(u32 file_id) const;
    [[nodiscard]] SourceLocation location_at(u32 file_id, std::size_t offset) const;
    [[nodiscard]] std::size_t file_count() const { return files_.size(); }

private:
    struct SourceFile {
        std::string name;
        std::string text;
        std::vector<std::size_t> line_offsets;
    };

    std::vector<SourceFile> files_;
};

} // namespace rtsl
