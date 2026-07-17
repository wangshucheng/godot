#ifdef TOOLS_ENABLED

#include "mono_export_plugin.h"
#include "core/os/os.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/config/project_settings.h"

// M12: sanitize assembly name the same way csharp_script.cpp's
// sanitize_project_name does — keep the two in sync to avoid export-time
// surprises (e.g. user sets application/config/name with spaces/#/.).
// Allowed: [A-Za-z0-9_]; space/-/#/./(/) → '_'; drop other chars; prefix
// leading digit with '_'; fall back to "GodotProject" if empty/unsanitizable.
static String sanitize_assembly_name(const String &p_name) {
	String name = p_name;
	if (name.is_empty()) {
		return "GodotProject";
	}
	String result;
	for (int i = 0; i < name.length(); i++) {
		char32_t c = name[i];
		if (c < 128) {
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
				result += c;
			} else if (c == ' ' || c == '-' || c == '#' || c == '.' || c == '(' || c == ')') {
				result += '_';
			}
		}
	}
	if (result.is_empty()) {
		return "GodotProject";
	}
	if (result[0] >= '0' && result[0] <= '9') {
		result = "_" + result;
	}
	return result;
}

void MonoExportPlugin::_export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
	// M12: get the assembly name from project settings and sanitize it.
	// Previously this was hardcoded to "CSharpTest", which broke any project
	// whose dotnet/project/assembly_name differed from the test project.
	String raw_name;
	if (ProjectSettings::get_singleton()) {
		raw_name = ProjectSettings::get_singleton()->get_setting("dotnet/project/assembly_name", String());
	}
	String project_name = sanitize_assembly_name(raw_name);
	if (project_name != raw_name) {
		print_line("[Mono Export] Assembly name sanitized: '" + raw_name + "' → '" + project_name + "'");
	}

	// Get the project resource path
	String project_path = ProjectSettings::get_singleton()->get_resource_path();

	// Editor executable directory (where GodotSharp.dll lives)
	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();

	// 1. Add the project assembly (e.g., CSharpTest.dll)
	String project_asm_path = project_path.path_join(".mono/assemblies").path_join(project_name + ".dll");
	if (FileAccess::exists(project_asm_path)) {
		Vector<uint8_t> data = FileAccess::get_file_as_bytes(project_asm_path);
		if (data.size() > 0) {
			add_file("res://.mono/assemblies/" + project_name + ".dll", data, false);
			print_line("[Mono Export] Added project assembly: " + project_name + ".dll (" + itos(data.size()) + " bytes)");
		}
	} else {
		print_line("[Mono Export] WARNING: Project assembly not found at: " + project_asm_path);
	}

	// 2. Add GodotSharp.dll
	// Prefer the project's .mono/assemblies/ copy (always fresh from build),
	// then fall back to Release/Debug in exe_dir.
	String godotsharp_path = project_path.path_join(".mono/assemblies/GodotSharp.dll");
	if (!FileAccess::exists(godotsharp_path)) {
		godotsharp_path = exe_dir.path_join("GodotSharp/Api/Release/GodotSharp.dll");
	}
	if (!FileAccess::exists(godotsharp_path)) {
		godotsharp_path = exe_dir.path_join("GodotSharp/Api/Debug/GodotSharp.dll");
	}
	if (FileAccess::exists(godotsharp_path)) {
		Vector<uint8_t> data = FileAccess::get_file_as_bytes(godotsharp_path);
		if (data.size() > 0) {
			add_file("res://.mono/assemblies/GodotSharp.dll", data, false);
			print_line("[Mono Export] Added GodotSharp.dll (" + itos(data.size()) + " bytes)");
		}
	} else {
		print_line("[Mono Export] WARNING: GodotSharp.dll not found at: " + godotsharp_path);
	}

	// 3. BCL assemblies for Web platform are embedded in WASM via emcc
	// --embed-file (SCsub_web.py). No need to export them in PCK — would
	// be redundant. GodotSharp.dll and project assembly above remain in PCK
	// for hot updates.
}

#endif // TOOLS_ENABLED
