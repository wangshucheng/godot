#include "register_types.h"

#include "mono_gd/csharp_script.h"
#include "mono_runtime/gd_mono.h"
#include "utils/mono_logger.h"
#include "core/io/resource_loader.h"

#ifdef TOOLS_ENABLED
#include "editor/csharp_editor.h"
#endif

static GDMono *_godot_mono = nullptr;
static CSharpLanguage *_csharp_language = nullptr;
static Ref<ResourceFormatLoaderCSharpScript> resource_loader_csharp;

void initialize_mono_new_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		_godot_mono = memnew(GDMono);
		_csharp_language = memnew(CSharpLanguage);

		if (!_godot_mono->initialize()) {
			ERR_PRINT("Mono runtime failed to initialize, C# support disabled");
			memdelete(_godot_mono);
			_godot_mono = nullptr;
			memdelete(_csharp_language);
			_csharp_language = nullptr;
			return;
		}

		resource_loader_csharp.instantiate();
		ResourceLoader::add_resource_format_loader(resource_loader_csharp);

		ScriptServer::register_language(_csharp_language);

		MonoLogger::log("Mono module initialized successfully (static linking)");
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		initialize_csharp_editor();
	}
#endif
}

void uninitialize_mono_new_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		if (resource_loader_csharp.is_valid()) {
			ResourceLoader::remove_resource_format_loader(resource_loader_csharp);
			resource_loader_csharp.unref();
		}

		if (_csharp_language) {
			ScriptServer::unregister_language(_csharp_language);
			memdelete(_csharp_language);
			_csharp_language = nullptr;
		}

		if (_godot_mono) {
			_godot_mono->cleanup();
			memdelete(_godot_mono);
			_godot_mono = nullptr;
		}
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		uninitialize_csharp_editor();
	}
#endif
}
