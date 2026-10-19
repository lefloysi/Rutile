#pragma once

#include "../project/configuration.hpp"
#include <string_view>

bool kiln_configure_project(const std::filesystem::path& root, ProjectConfiguration& configuration, std::string& diagnostics, std::string_view profile = "debug");
