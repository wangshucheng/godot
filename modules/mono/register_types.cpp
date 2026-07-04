#include "register_types.h"
#include "mono_host.h"
#include "csharp_script.h"
#include "mono_bridge.h"
#include "mono_variant.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "core/io/file_access.h"
#include "core/error/error_macros.h"

#include <cstdio>

static MonoHost *mono_host = nullptr;
static CSharpLanguage *csharp_lang = nullptr;

void initialize_mono_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	printf("[Mono] Initializing module at scene level...\n");

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
	printf("[Mono] C# language registered.\n");

	String exe_path = OS::get_singleton()->get_executable_path().get_base_dir();

	String assembly_path = exe_path.path_join("HelloWorld.dll");
	if (!FileAccess::exists(assembly_path)) {
		assembly_path = exe_path.path_join("HelloMono.dll");
	}
	if (!FileAccess::exists(assembly_path)) {
		assembly_path = exe_path.path_join("mono").path_join("HelloWorld.dll");
	}
	if (!FileAccess::exists(assembly_path)) {
		assembly_path = exe_path.path_join("mono").path_join("HelloMono.dll");
	}

	if (mono_host->load_assembly_and_run(assembly_path)) {
		printf("[Mono] Test assembly executed successfully.\n");
	} else {
		printf("[Mono] Note: No test assembly loaded (HelloWorld.dll/HelloMono.dll).\n");
		printf("[Mono] This is normal - Mono runtime itself initialized OK.\n");
	}
}

void uninitialize_mono_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	if (csharp_lang) {
		ScriptServer::unregister_language(csharp_lang);
		memdelete(csharp_lang);
		csharp_lang = nullptr;
	}
	if (mono_host) {
		mono_bridge::shutdown();
		mono_host->shutdown();
		memdelete(mono_host);
		mono_host = nullptr;
	}
}
