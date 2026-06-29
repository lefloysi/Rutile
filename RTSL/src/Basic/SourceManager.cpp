#include "Basic/SourceManager.hpp"

#include <algorithm>

namespace rtsl {

u32 SourceManager::add_buffer(std::string name, std::string text) {
    SourceFile file;
    file.name = std::move(name);
    file.text = std::move(text);
    file.line_offsets.push_back(0);

    for (std::size_t i = 0; i < file.text.size(); ++i) {
        if (file.text[i] == '\n') {
            file.line_offsets.push_back(i + 1);
        }
    }

    files_.push_back(std::move(file));
    return static_cast<u32>(files_.size() - 1);
}

std::string_view SourceManager::buffer(u32 file_id) const {
    return files_.at(file_id).text;
}

std::string_view SourceManager::name(u32 file_id) const {
    return files_.at(file_id).name;
}

SourceLocation SourceManager::location_at(u32 file_id, std::size_t offset) const {
    const auto &file = files_.at(file_id);
    offset = std::min(offset, file.text.size());
    const auto it = std::upper_bound(file.line_offsets.begin(), file.line_offsets.end(), offset);
    const auto line_index = static_cast<std::size_t>(std::distance(file.line_offsets.begin(), it) - 1);
    const auto line_start = file.line_offsets[line_index];

    return SourceLocation{
        .file_id = file_id,
        .offset = offset,
        .line = static_cast<u32>(line_index + 1),
        .column = static_cast<u32>(offset - line_start + 1),
    };
}

} // namespace rtsl
