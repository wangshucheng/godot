#include "register_types.h"
#include "mono_host.h"
#include "csharp_script.h"
#include "mono_bridge.h"
#include "mono_variant.h"
#include "core/object/script_language.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/io/file_access.h"
#include "core/error/error_macros.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"

#ifdef TOOLS_ENABLED
#include "mono_export_plugin.h"
#include "editor/editor_node.h"
#include "editor/export/editor_export.h"
#endif

#include <cstdio>

static MonoHost *mono_host = nullptr;
static CSharpLanguage *csharp_lang = nullptr;

#ifdef TOOLS_ENABLED
static void _editor_init() {
	Ref<MonoExportPlugin> mono_export;
	mono_export.instantiate();
	EditorExport::get_singleton()->add_export_plugin(mono_export);
}
#endif

void initialize_mono_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		GDREGISTER_CLASS(CSharpScript);

		mono_host = memnew(MonoHost);
		Error err = mono_host->initialize();
		if (err != OK) {
			ERR_PRINT("[Mono] Failed to initialize Mono runtime!");
			memdelete(mono_host);
			mono_host = nullptr;
			return;
		}

		csharp_lang = memnew(CSharpLanguage);
		ScriptServer::register_language(csharp_lang);
		register_csharp_resource_loader();
		csharp_lang->init();

#ifdef TOOLS_ENABLED
		EditorNode::add_init_callback(_editor_init);
#endif
		return;
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		if (mono_host) {
			List<String> cmdline_args = OS::get_singleton()->get_cmdline_args();
			bool run_test = false;
			for (const String &arg : cmdline_args) {
				if (arg == "--run-mono-test") {
					run_test = true;
					break;
				}
			}
			if (run_test) {
				String exe_path = OS::get_singleton()->get_executable_path().get_base_dir();
				String assembly_path = exe_path.path_join("HelloWorld.dll");
				if (!FileAccess::exists(assembly_path)) {
					assembly_path = exe_path.path_join("HelloMono.dll");
				}

				if (FileAccess::exists(assembly_path)) {
					if (mono_host->load_assembly_and_run(assembly_path)) {
						printf("[Mono] Test assembly executed successfully.\n");
					}
				} else {
					printf("[Mono] Note: No test assembly loaded (HelloWorld.dll/HelloMono.dll).\n");
					printf("[Mono] This is normal - Mono runtime itself initialized OK.\n");
				}
			}
		}
	}
}

void uninitialize_mono_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SERVERS) {
		return;
	}

	unregister_csharp_resource_loader();
	if (csharp_lang) {
		csharp_lang->finish();
		ScriptServer::unregister_language(csharp_lang);
		memdelete(csharp_lang);
		csharp_lang = nullptr;
	}
	if (mono_host) {
		mono_host->shutdown();
		memdelete(mono_host);
		mono_host = nullptr;
	}
}
