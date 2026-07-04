#include "mono_host.h"
#include "mono_icalls.h"
#include "mono_variant.h"
#include "mono_bridge.h"
#include "mono_gc_bridge.h"
#include "core/os/os.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/error/error_macros.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef WINDOWS_ENABLED
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifndef WINDOWS_ENABLED
#include <stdlib.h>
#endif

MonoHost *MonoHost::singleton = nullptr;

MonoHost::MonoHost() {
	singleton = this;
}

MonoHost::~MonoHost() {
	shutdown();
	if (singleton == this) {
		singleton = nullptr;
	}
}

static String find_mono_root(const String &p_start_dir) {
	const char *profiles[] = {"4.5", "4.5-api", "4.5.2-api", nullptr};

	String dir = p_start_dir.replace("\\", "/");
	for (int i = 0; i < 10; i++) {
		for (int p = 0; profiles[p] != nullptr; p++) {
			String mscorlib_path = dir.path_join("mono").path_join("lib").path_join("mono").path_join(profiles[p]).path_join("mscorlib.dll");
			if (FileAccess::exists(mscorlib_path)) {
				return dir.path_join("mono");
			}
			mscorlib_path = dir.path_join("lib").path_join("mono").path_join(profiles[p]).path_join("mscorlib.dll");
			if (FileAccess::exists(mscorlib_path)) {
				return dir;
			}
		}
		String parent = dir.get_base_dir();
		if (parent == dir || parent.is_empty()) {
			break;
		}
		dir = parent;
	}

	return String();
}

Error MonoHost::initialize() {
	if (is_initialized) {
		return OK;
	}

	printf("[Mono] Initializing Mono Runtime...\n");

	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();

	String mono_root = find_mono_root(exe_dir);

#ifdef WINDOWS_ENABLED
	if (!mono_root.is_empty()) {
		SetDllDirectoryA((mono_root + "/bin/windows/x64").utf8().get_data());
	}
	SetDllDirectoryA(exe_dir.utf8().get_data());
#endif

	String bcl_dir;
	String etc_dir;
	String assemblies_dir;

	if (!mono_root.is_empty()) {
		const char *profiles[] = {"4.5", "4.5-api", "4.5.2-api", nullptr};
		for (int p = 0; profiles[p] != nullptr; p++) {
			String try_bcl = mono_root.path_join("lib").path_join("mono").path_join(profiles[p]);
			if (DirAccess::exists(try_bcl) && FileAccess::exists(try_bcl.path_join("mscorlib.dll"))) {
				bcl_dir = try_bcl;
				break;
			}
		}
		etc_dir = mono_root.path_join("etc");
		assemblies_dir = mono_root.path_join("lib");
	} else {
		bcl_dir = exe_dir.path_join("lib").path_join("mono").path_join("4.5");
		etc_dir = exe_dir;
		assemblies_dir = exe_dir.path_join("lib");
	}

	printf("[Mono] Exe dir: %s\n", exe_dir.utf8().get_data());
	printf("[Mono] Mono root: %s\n", mono_root.is_empty() ? "(not found)" : mono_root.utf8().get_data());
	printf("[Mono] Assemblies dir: %s\n", assemblies_dir.utf8().get_data());
	printf("[Mono] BCL dir: %s\n", bcl_dir.utf8().get_data());
	printf("[Mono] Config (etc) dir: %s\n", etc_dir.utf8().get_data());

	if (!DirAccess::exists(bcl_dir) || !FileAccess::exists(bcl_dir.path_join("mscorlib.dll"))) {
		printf("[Mono] WARNING: mscorlib.dll not found at %s\n", bcl_dir.utf8().get_data());
	}

	mono_set_dirs(assemblies_dir.utf8().get_data(), etc_dir.utf8().get_data());
	mono_set_assemblies_path(bcl_dir.utf8().get_data());

#ifdef WINDOWS_ENABLED
	SetEnvironmentVariableA("MONO_PATH", bcl_dir.utf8().get_data());
#else
	setenv("MONO_PATH", bcl_dir.utf8().get_data(), 1);
#endif

	domain = mono_jit_init_version("GodotMono", "v4.0.30319");
	if (!domain) {
		ERR_PRINT("[Mono] Failed to initialize JIT runtime (mono_jit_init_version returned NULL)");
		return FAILED;
	}

	printf("[Mono] JIT domain created: %s\n", mono_domain_get_friendly_name(domain));

	mono_bridge::init(domain);
	mono_gc_bridge::init(domain);
	mono_variant::cache_mono_corlib_classes();

	if (!register_internal_calls()) {
		return FAILED;
	}

	if (!load_corlib()) {
		return FAILED;
	}

	if (!load_godotsharp()) {
		printf("[Mono] Note: GodotSharp.dll not loaded (Phase 2 bindings will be limited).\n");
	}

	is_initialized = true;
	printf("[Mono] Mono runtime initialized successfully.\n");
	return OK;
}

bool MonoHost::register_internal_calls() {
	godot_register_icalls();
	return true;
}

bool MonoHost::load_corlib() {
	MonoImage *corlib_image = mono_get_corlib();
	if (!corlib_image) {
		ERR_PRINT("[Mono] Failed to get corlib image!");
		return false;
	}

	MonoAssembly *mscorlib = mono_image_get_assembly(corlib_image);
	if (!mscorlib) {
		ERR_PRINT("[Mono] Failed to get corlib assembly from image!");
		return false;
	}

	const char *mscorlib_name = mono_image_get_name(corlib_image);
	printf("[Mono] Loaded corlib: %s\n", mscorlib_name ? mscorlib_name : "(unknown)");

	corlib_assembly = mscorlib;
	return true;
}

MonoAssembly *MonoHost::load_assembly(const String &p_path) {
	if (!FileAccess::exists(p_path)) {
		return nullptr;
	}
	return mono_domain_assembly_open(domain, p_path.utf8().get_data());
}

bool MonoHost::load_godotsharp() {
	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	String gs_path = exe_dir.path_join("GodotSharp.dll");
	if (!FileAccess::exists(gs_path)) {
		gs_path = exe_dir.path_join("mono").path_join("GodotSharp.dll");
	}
	if (!FileAccess::exists(gs_path)) {
		return false;
	}

	godotsharp_assembly = mono_domain_assembly_open(domain, gs_path.utf8().get_data());
	if (!godotsharp_assembly) {
		printf("[Mono] Failed to open GodotSharp.dll\n");
		return false;
	}

	MonoImage *img = mono_assembly_get_image(godotsharp_assembly);
	mono_bridge::cache_godot_classes(img);
	printf("[Mono] GodotSharp loaded successfully.\n");
	return true;
}

bool MonoHost::load_assembly_and_run(const String &p_assembly_path) {
	if (!is_initialized) {
		ERR_PRINT("[Mono] Cannot load assembly - runtime not initialized");
		return false;
	}

	printf("[Mono] Attempting to load assembly: %s\n", p_assembly_path.utf8().get_data());

	if (!FileAccess::exists(p_assembly_path)) {
		printf("[Mono] Assembly file not found, skipping.\n");
		return false;
	}

	MonoAssembly *assembly = mono_domain_assembly_open(domain, p_assembly_path.utf8().get_data());
	if (!assembly) {
		ERR_PRINT("[Mono] Failed to load assembly!");
		return false;
	}

	MonoImage *image = mono_assembly_get_image(assembly);
	if (!image) {
		ERR_PRINT("[Mono] Failed to get assembly image!");
		return false;
	}

	const char *image_name = mono_image_get_name(image);
	printf("[Mono] Loaded assembly image: %s\n", image_name ? image_name : "(unknown)");

	MonoClass *main_class = nullptr;

	const char *namespaces[] = {"HelloMono", "HelloWorld", ""};
	const char *class_names[] = {"Program", "MainClass"};

	for (int ni = 0; ni < 3 && !main_class; ni++) {
		for (int ci = 0; ci < 2 && !main_class; ci++) {
			main_class = mono_class_from_name(image, namespaces[ni], class_names[ci]);
		}
	}

	if (!main_class) {
		MonoMethodDesc *desc = mono_method_desc_new("*:Main()", false);
		MonoMethod *main_method = mono_method_desc_search_in_image(desc, image);
		mono_method_desc_free(desc);

		if (!main_method) {
			ERR_PRINT("[Mono] Could not find Main() method in assembly!");
			return false;
		}

		printf("[Mono] Found Main method via method desc, invoking...\n");

		MonoObject *exc = nullptr;
		mono_runtime_invoke(main_method, nullptr, nullptr, &exc);
		if (exc) {
			MonoClass *exc_class = mono_object_get_class(exc);
			const char *exc_name = mono_class_get_name(exc_class);
			printf("[Mono] Exception during Main(): %s\n", exc_name ? exc_name : "(unknown)");
			return false;
		}
		return true;
	}

	MonoMethod *main_method = mono_class_get_method_from_name(main_class, "Main", 0);
	if (!main_method) {
		main_method = mono_class_get_method_from_name(main_class, "Main", 1);
	}

	if (!main_method) {
		ERR_PRINT("[Mono] Could not find Main method in Program class!");
		return false;
	}

	printf("[Mono] Invoking Program.Main()...\n");

	MonoObject *exc = nullptr;
	void *args[1] = { nullptr };
	mono_runtime_invoke(main_method, nullptr, args, &exc);

	if (exc) {
		MonoClass *exc_class = mono_object_get_class(exc);
		const char *exc_name = mono_class_get_name(exc_class);
		printf("[Mono] Exception in Main(): %s\n", exc_name ? exc_name : "(unknown)");

		MonoMethod *to_string_method = mono_class_get_method_from_name(exc_class, "ToString", 0);
		if (to_string_method) {
			MonoObject *to_str_obj = mono_runtime_invoke(to_string_method, exc, nullptr, nullptr);
			if (to_str_obj) {
				MonoString *to_str = (MonoString *)to_str_obj;
				char *ts_utf8 = mono_string_to_utf8(to_str);
				if (ts_utf8) {
					printf("[Mono] Exception: %s\n", ts_utf8);
					mono_free(ts_utf8);
				}
			}
		}
		return false;
	}

	return true;
}

void MonoHost::shutdown() {
	if (!is_initialized) {
		return;
	}

	printf("[Mono] Shutting down Mono runtime...\n");

	mono_bridge::shutdown();
	mono_gc_bridge::shutdown();

	if (domain) {
		mono_jit_cleanup(domain);
		domain = nullptr;
	}

	is_initialized = false;
	printf("[Mono] Mono runtime shutdown complete.\n");
}
