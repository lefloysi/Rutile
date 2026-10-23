#pragma once

#include <nlohmann/json_fwd.hpp>
#include <cstdio>
#include <string>
#include <vector>

struct Compiler;
int rutile_compile_module(Compiler* compiler, const nlohmann::json& request, const std::vector<std::string>& names, const std::vector<std::string>& contents, std::FILE* diagnostics);

void rutile_project_sources_update(const nlohmann::json& sources);
bool rutile_project_sources_ready();

int rutile_service_serve(const char* endpoint);
int rutile_service_control(const char* operation);
