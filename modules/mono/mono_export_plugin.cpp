#ifdef TOOLS_ENABLED

#include "mono_export_plugin.h"
#include "core/os/os.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/config/project_settings.h"

void MonoExportPlugin::_export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
	// Get the project name (assembly name)
	String project_name = "CSharpTest";
	if (ProjectSettings::get_singleton()) {
		project_name = ProjectSettings::get_singleton()->get_setting("dotnet/project/assembly_name", "CSharpTest");
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
	// Prefer Release build for exports (Debug is a stub on this setup)
	String godotsharp_path = exe_dir.path_join("GodotSharp/Api/Release/GodotSharp.dll");
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

	// 3. Add mscorlib.dll and other BCL assemblies for Web platform
	// On Web (interpreter mode), BCL assemblies must be available at runtime
	// The Mono runtime looks for them at lib/mono/4.5/ (relative to exe_dir /
	// virtual filesystem root on Web).
	bool is_web = p_features.has("web") || p_features.has("Web");
	if (is_web) {
		String bcl_dir = exe_dir.path_join("GodotSharp/Mono/lib/mono/4.5");
		if (!DirAccess::exists(bcl_dir)) {
			// Try alternative path
			bcl_dir = exe_dir.path_join("Mono/lib/mono/4.5");
		}

		if (DirAccess::exists(bcl_dir)) {
			// Add ALL .dll files from the BCL directory (not just a subset)
			// so that any assembly referenced at runtime can be resolved.
			Ref<DirAccess> da = DirAccess::open(bcl_dir);
			if (da.is_valid()) {
				da->list_dir_begin();
				String file = da->get_next();
				while (!file.is_empty()) {
					if (!da->current_is_dir() && file.ends_with(".dll")) {
						String asm_path = bcl_dir.path_join(file);
						Vector<uint8_t> data = FileAccess::get_file_as_bytes(asm_path);
						if (data.size() > 0) {
							// Add at lib/mono/4.5/ path where Mono runtime expects them
							add_file("res://lib/mono/4.5/" + file, data, false);
						}
					}
					file = da->get_next();
				}
				da->list_dir_end();
				print_line("[Mono Export] Added BCL assemblies from: " + bcl_dir);
			}
		} else {
			print_line("[Mono Export] WARNING: BCL directory not found at: " + bcl_dir);
		}

		// Also add Facades assemblies
		String facades_dir = exe_dir.path_join("GodotSharp/Mono/lib/mono/4.5/Facades");
		if (!DirAccess::exists(facades_dir)) {
			facades_dir = exe_dir.path_join("Mono/lib/mono/4.5/Facades");
		}

		if (DirAccess::exists(facades_dir)) {
			Ref<DirAccess> da = DirAccess::open(facades_dir);
			if (da.is_valid()) {
				da->list_dir_begin();
				String file = da->get_next();
				while (!file.is_empty()) {
					if (!da->current_is_dir() && file.ends_with(".dll")) {
						String asm_path = facades_dir.path_join(file);
						Vector<uint8_t> data = FileAccess::get_file_as_bytes(asm_path);
						if (data.size() > 0) {
							add_file("res://lib/mono/4.5/Facades/" + file, data, false);
						}
					}
					file = da->get_next();
				}
				da->list_dir_end();
				print_line("[Mono Export] Added Facades assemblies");
			}
		}
	}
}

#endif // TOOLS_ENABLED
