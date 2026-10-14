#pragma once

#include <filesystem>

int kiln_link(const std::filesystem::path& object, const std::filesystem::path& output, const std::filesystem::path& diagnostics);
