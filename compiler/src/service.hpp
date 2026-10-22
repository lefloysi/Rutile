#pragma once

#include <nlohmann/json_fwd.hpp>

void rutile_project_sources_update(const nlohmann::json& sources);
bool rutile_project_sources_ready();

int rutile_service_serve(const char* endpoint);
int rutile_service_control(const char* operation);
