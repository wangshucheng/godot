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
#include "editor/bindings_generator.h"

#ifdef TOOLS_ENABLED
#include "mono_export_plugin.h"
#include "editor/editor_node.h"
#include "editor/export/editor_export.h"
#include "editor/docks/editor_dock_manager.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/mono_build_panel.h"
#include "core/object/callable_mp.h"
#endif

#include <cstdio>

static MonoHost *mono_host = nullptr;
static CSharpLanguage *csharp_lang = nullptr;

#ifdef TOOLS_ENABLED
// P4: Owned by EditorDockManager (added via add_dock). The dock manager
// takes responsibility for parenting/destroying it on editor shutdown.
static MonoBuildPanel *mono_build_panel = nullptr;

static void _editor_init() {
	// File-based marker to confirm _editor_init is actually called.
	// printf may be swallowed by Godot's print handler at this point.
	{
		FILE *f = fopen("csharp_test_p6_editor_init.marker", "w");
		if (f) {
			fprintf(f, "_editor_init called at %llu ms\n",
					(unsigned long long)(OS::get_singleton() ? OS::get_singleton()->get_ticks_msec() : 0));
			fclose(f);
		}
	}

	Ref<MonoExportPlugin> mono_export;
	mono_export.instantiate();
	EditorExport::get_singleton()->add_export_plugin(mono_export);

	// P4: register the Mono Build bottom panel.
	// add_dock() reparents the Control to the dock tab container, so the
	// editor's lifecycle owns it from here on.
	mono_build_panel = memnew(MonoBuildPanel);
	EditorDockManager::get_singleton()->add_dock(mono_build_panel);

	// P6: Watch the project filesystem so external IDE .cs saves trigger a
	// build. _on_filesystem_changed applies a 500ms cooldown and calls
	// request_build(); the actual build runs on the next frame() tick.
	// callable_mp avoids the need for ClassDB binding of the method.
	printf("[Mono] P6: _editor_init invoked\n");
	fflush(stdout);
	CSharpLanguage *csl = CSharpLanguage::get_singleton();
	EditorFileSystem *efs = EditorFileSystem::get_singleton();
	printf("[Mono] P6: CSharpLanguage::get_singleton() = %p\n", (void *)csl);
	fflush(stdout);
	printf("[Mono] P6: EditorFileSystem::get_singleton() = %p\n", (void *)efs);
	fflush(stdout);
	if (csl && efs) {
		Callable cb = callable_mp(csl, &CSharpLanguage::_on_filesystem_changed);
		Error ce = efs->connect("filesystem_changed", cb);
		printf("[Mono] P6: connect('filesystem_changed', ...) returned %d, callable=%s, target=%p\n",
				(int)ce, cb.is_valid() ? "valid" : "invalid", (void *)cb.get_object());
		fflush(stdout);
		// Write connection result to marker file
		FILE *f2 = fopen("csharp_test_p6_connect_result.marker", "w");
		if (f2) {
			fprintf(f2, "connect returned %d, csl=%p, efs=%p\n",
					(int)ce, (void *)csl, (void *)efs);
			fclose(f2);
		}
	} else {
		printf("[Mono] P6: WARNING — skipping signal connect (singleton missing)\n");
		fflush(stdout);
	}
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
		printf("[Mono] P6: register_types TOOLS_ENABLED block, calling add_init_callback\n");
		fflush(stdout);
		EditorNode::add_init_callback(_editor_init);
#else
		printf("[Mono] P6: register_types TOOLS_ENABLED NOT defined (skipping _editor_init)\n");
		fflush(stdout);
#endif
		return;
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		// Handle BindingsGenerator command-line args (--generate-csharp-bindings)
		BindingsGenerator::handle_cmdline_args(OS::get_singleton()->get_cmdline_args());

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
#ifdef TOOLS_ENABLED
	// P4: panel is owned by the dock manager's tab container, which is freed
	// by EditorNode before module uninitialize. Clear the dangling pointer.
	mono_build_panel = nullptr;
#endif
}
