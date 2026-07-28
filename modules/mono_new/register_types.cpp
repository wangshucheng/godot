#include "register_types.h"

#include "mono_gd/csharp_script.h"
#include "mono_runtime/csharp_debugger.h"
#include "mono_runtime/gd_mono.h"
#include "utils/mono_logger.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/os/os.h"

// 平台胶水层初始化（与 MonoWeb::initialize 对应）
#ifdef ANDROID_ENABLED
#include "platform/mono_platform_android.h"
#endif
#ifdef IOS_ENABLED
#include "platform/mono_platform_ios.h"
#endif

#ifdef TOOLS_ENABLED
#include "editor/csharp_editor.h"
#include "editor/csharp_project_editor_plugin.h"
#include "editor/plugins/editor_plugin.h"
#endif

static GDMono *_godot_mono = nullptr;
static CSharpLanguage *_csharp_language = nullptr;
static Ref<ResourceFormatLoaderCSharpScript> resource_loader_csharp;
static Ref<ResourceFormatSaverCSharpScript> resource_saver_csharp;

void initialize_mono_new_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
		// Parse --mono-debugger=PORT very early. The SDB agent must be
		// configured before mono_jit_init_version() (which runs at SCENE
		// level when GDMono is constructed), so we capture the request now.
		List<String> cmdline_args = OS::get_singleton()->get_cmdline_args();
		CSharpDebugger::parse_command_line(cmdline_args);
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		// 平台胶水层初始化（GC 栈扫描、线程注册必须在 GDMono::initialize 之前）
#ifdef ANDROID_ENABLED
		MonoAndroid::initialize();
#endif
#ifdef IOS_ENABLED
		MonoiOS::initialize();
#endif

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

		resource_saver_csharp.instantiate();
		ResourceSaver::add_resource_format_saver(resource_saver_csharp);

		ScriptServer::register_language(_csharp_language);

		MonoLogger::log("Mono module initialized successfully (static linking)");
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		initialize_csharp_editor();
		register_csharp_export_plugin();
		EditorPlugins::add_by_type<CSharpProjectEditorPlugin>();
	}
#endif
}

void uninitialize_mono_new_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		if (resource_saver_csharp.is_valid()) {
			ResourceSaver::remove_resource_format_saver(resource_saver_csharp);
			resource_saver_csharp.unref();
		}

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

		// 平台胶水层清理
#ifdef ANDROID_ENABLED
		MonoAndroid::cleanup();
#endif
#ifdef IOS_ENABLED
		MonoiOS::cleanup();
#endif
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		unregister_csharp_export_plugin();
		uninitialize_csharp_editor();
	}
#endif
}
