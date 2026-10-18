#pragma once

int kiln_serve(const char* endpoint);

struct Compiler;
struct EditorSession;

EditorSession* kiln_editor_start(Compiler* compiler, const char* root);
void kiln_editor_stop(EditorSession* session);
