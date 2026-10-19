#pragma once

struct CompilerRegistry;

int kiln_serve(CompilerRegistry& compilers, const char* endpoint);

struct EditorSession;

EditorSession* kiln_editor_start(CompilerRegistry& compilers, const char* root);
void kiln_editor_stop(EditorSession* session);
