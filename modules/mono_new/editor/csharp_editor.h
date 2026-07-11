#ifndef CSHARP_EDITOR_H
#define CSHARP_EDITOR_H

#include "core/string/ustring.h"

#ifdef TOOLS_ENABLED

void initialize_csharp_editor();
void uninitialize_csharp_editor();

bool csharp_editor_ensure_project_solution();
bool csharp_editor_compile_project();
void csharp_editor_on_script_saved(const String &p_path);
String csharp_editor_get_csproj_path();
String csharp_editor_get_sln_path();
String csharp_editor_get_assemblies_output_dir();

void register_csharp_export_plugin();
void unregister_csharp_export_plugin();

#endif

#endif // CSHARP_EDITOR_H
