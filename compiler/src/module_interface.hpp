#pragma once

#include <nlohmann/json.hpp>
#include <string_view>

nlohmann::json rutile_interface_sources(std::string_view bytes, bool test_access);
std::string rutile_interface_encode(std::string_view module, const nlohmann::json& records);
