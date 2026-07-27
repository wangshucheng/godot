#ifdef TOOLS_ENABLED

#include "mono_export_plugin.h"
#include "utils/path_utils.h"
#include "csharp_script.h"
#include "core/os/os.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/config/project_settings.h"

// A3: sanitize_assembly_name removed — duplicated Path::sanitize_project_name.
// Single source of truth in utils/path_utils.cpp (migrated from csharp_script.cpp).

void MonoExportPlugin::_export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
	// P1-#7 fix: was reading `dotnet/project/assembly_name` directly with no
	// fallback, then sanitizing. When the setting is missing, the export
	// plugin emitted the DLL under "GodotProject" (sanitize fallback) while
	// the runtime (mono_host.cpp) looked it up under a different name →
	// runtime DLL not found. Unify both sides on Path::get_csharp_project_name()
	// which: reads dotnet/project/assembly_name → falls back to
	// application/config/name → then sanitizes. The export/runtime pair must
	// agree byte-for-byte on the .dll filename.
	String project_name = Path::get_csharp_project_name();

	// P2 v2 [REV-#06]: WASM export lint — verify file_name==class_name convention.
	// On WASM, .pdb is unavailable so the runtime relies on the
	// file_name==class_name convention to map .cs files to classes. If a
	// [GlobalClass] class violates this convention, it will be invisible at
	// runtime (Add Node dialog / class database). Catch this at export time.
	// Desktop-only check; runs when exporting to Web platform.
	bool is_web_export = p_features.has("web");
	if (is_web_export) {
		CSharpLanguage *lang = CSharpLanguage::get_singleton();
		if (lang && lang->global_classes_valid) {
			int violations = 0;
			for (const auto &entry : lang->global_class_cache) {
				const String &class_name = entry.key;
				const CSharpLanguage::GlobalClassInfo &info = entry.value;
				if (info.source_path.is_empty()) {
					continue; // No .pdb source path — cannot check (shouldn't happen on desktop).
				}
				String file_basename = info.source_path.get_file().get_basename();
				if (file_basename != class_name) {
					print_line(vformat("[Mono Export] WARNING: [GlobalClass] class '%s' is declared in '%s' "
									   "(file basename '%s' != class name '%s'). On WASM/Web platform, this class "
									   "will NOT be registered at runtime. Rename the .cs file to match the class name.",
							class_name, info.source_path, file_basename, class_name));
					violations++;
				}
			}
			if (violations > 0) {
				WARN_PRINT(vformat("Mono Export: %d [GlobalClass] class(es) violate the file_name==class_name "
								   "convention required for WASM/Web platform. See warnings above.",
						violations));
			}
		}
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
