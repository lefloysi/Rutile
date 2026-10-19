#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

struct Project {
	std::filesystem::path root;
	std::string endpoint;
};

bool project_find(struct Project& project);
std::string project_path_utf8(const std::filesystem::path& path);
int project_initialize();
int project_request(std::string_view command);
bool project_exchange(std::string_view command, std::string& response);
int project_run(std::string_view target, std::string_view profile, const std::vector<std::string>& arguments);
int project_build(std::string_view target, std::string_view profile);
int project_list();
