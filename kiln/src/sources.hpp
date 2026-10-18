#pragma once

extern "C" int kiln_project_sources(const char* root, int (*add)(void*, const char*, const char*, int), void* context, void (*diagnostic)(void*, const char*));
