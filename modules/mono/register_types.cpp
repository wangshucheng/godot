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

#include <cstdio>

static MonoHost *mono_host = nullptr;
static CSharpLanguage *csharp_lang = nullptr;

void initialize_mono_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		GDREGISTER_CLASS(CSharpScript);
		return;
	}

	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

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

void uninitialize_mono_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	unregister_csharp_resource_loader();
	if (csharp_lang) {
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
