#include "register_types.h"

#include "mono_gd/csharp_script.h"
#include "mono_runtime/gd_mono.h"
#include "utils/mono_logger.h"

#ifdef TOOLS_ENABLED
#include "editor/csharp_editor.h"
#endif

static GDMono *_godot_mono = nullptr;
static CSharpLanguage *_csharp_language = nullptr;

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
