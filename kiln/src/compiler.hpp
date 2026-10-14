#pragma once

#include "rutile/compiler.hpp"

#include <string>
#include <string_view>
#include <cstddef>
#include <cstdio>

std::string kiln_build(struct Compiler* compiler, std::string_view request);
std::string kiln_list();
