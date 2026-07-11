#include "mono_host.h"
#include "mono_icalls.h"
#include "mono_variant.h"
#include "mono_bridge.h"
#include "mono_gc_bridge.h"
#include "mono_aot.h"
#include "core/os/os.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/error/error_macros.h"
#include "core/config/project_settings.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mono/utils/mono-logger.h>

#ifdef WINDOWS_ENABLED
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifndef WINDOWS_ENABLED
#include <stdlib.h>
#endif

// Mono interpreter engine initialization (for WASM/interpreter mode).
// These are declared in various Mono internal headers (interp.h, marshal.h,
// method-builder.h, sgen-bridge.h) but we avoid including them to prevent
// conflicts; simple extern "C" declarations suffice.
#ifdef MONO_INTERP_MODE
extern "C" void mono_ee_interp_init(const char *);
extern "C" void mono_marshal_ilgen_init(void);
extern "C" void mono_method_builder_ilgen_init(void);
extern "C" void mono_sgen_mono_ilgen_init(void);
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
	const char *profiles[] = {"4.5", "4.5-api", "4.5.2-api", "v4.0", nullptr};

	String dir = p_start_dir.replace("\\", "/");

	// First check GodotSharp/Mono subdirectory (official Godot layout)
	String godotsharp_mono = dir.path_join("GodotSharp").path_join("Mono");
	for (int p = 0; profiles[p] != nullptr; p++) {
		String mscorlib_path = godotsharp_mono.path_join("lib").path_join("mono").path_join(profiles[p]).path_join("mscorlib.dll");
		if (FileAccess::exists(mscorlib_path)) {
			return godotsharp_mono;
		}
	}

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
			// Also check GodotSharp/Mono in parent directories
			mscorlib_path = dir.path_join("GodotSharp").path_join("Mono").path_join("lib").path_join("mono").path_join(profiles[p]).path_join("mscorlib.dll");
			if (FileAccess::exists(mscorlib_path)) {
				return dir.path_join("GodotSharp").path_join("Mono");
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

	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();

#ifdef WEB_ENABLED
	// On Web, the PCK is mounted at res:// but Mono uses standard C file I/O
	// which goes through Emscripten's MEMFS, not Godot's FileAccess layer.
	// We need to extract BCL assemblies from the PCK to the MEMFS so Mono
	// can access them via fopen().
	String bcl_dir = "lib/mono/4.5";
	String etc_dir = "etc";
	String assemblies_dir = "lib";

	// Create directories in MEMFS
	{
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		if (da.is_valid()) {
			da->make_dir_recursive(bcl_dir);
			da->make_dir_recursive(".mono/assemblies");
		}
	}

	// Extract BCL assemblies from PCK (res://lib/mono/4.5/) to MEMFS
	{
		String res_bcl = "res://lib/mono/4.5";
		Ref<DirAccess> pck_da = DirAccess::open(res_bcl);
		if (pck_da.is_valid()) {
			pck_da->list_dir_begin();
			String file = pck_da->get_next();
			int count = 0;
			while (!file.is_empty()) {
				if (!pck_da->current_is_dir() && file.ends_with(".dll")) {
					String res_path = res_bcl + "/" + file;
					String memfs_path = bcl_dir + "/" + file;
					Ref<FileAccess> src = FileAccess::open(res_path, FileAccess::READ);
					if (src.is_valid()) {
						Vector<uint8_t> data;
						data.resize(src->get_length());
						src->get_buffer(data.ptrw(), data.size());
						Ref<FileAccess> dst = FileAccess::open(memfs_path, FileAccess::WRITE);
						if (dst.is_valid()) {
							dst->store_buffer(data.ptr(), data.size());
							count++;
						}
					}
				}
				file = pck_da->get_next();
			}
			pck_da->list_dir_end();
			printf("[Mono] Extracted %d BCL assemblies to MEMFS\n", count);
			fflush(stdout);
		} else {
			ERR_PRINT("[Mono] Cannot open res://lib/mono/4.5 in PCK");
		}
	}

	// Extract Facades subdirectory (contains netstandard.dll and other facade assemblies)
	{
		String res_facades = "res://lib/mono/4.5/Facades";
		String memfs_facades = bcl_dir + "/Facades";
		Ref<DirAccess> pck_da = DirAccess::open(res_facades);
		if (pck_da.is_valid()) {
			// Create the Facades directory in MEMFS
			Ref<DirAccess> memfs_da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
			if (memfs_da.is_valid()) {
				memfs_da->make_dir_recursive(memfs_facades);
			}
			pck_da->list_dir_begin();
			String file = pck_da->get_next();
			int count = 0;
			while (!file.is_empty()) {
				if (!pck_da->current_is_dir() && file.ends_with(".dll")) {
					String res_path = res_facades + "/" + file;
					String memfs_path = memfs_facades + "/" + file;
					Ref<FileAccess> src = FileAccess::open(res_path, FileAccess::READ);
					if (src.is_valid()) {
						Vector<uint8_t> data;
						data.resize(src->get_length());
						src->get_buffer(data.ptrw(), data.size());
						Ref<FileAccess> dst = FileAccess::open(memfs_path, FileAccess::WRITE);
						if (dst.is_valid()) {
							dst->store_buffer(data.ptr(), data.size());
							count++;
						}
					}
				}
				file = pck_da->get_next();
			}
			pck_da->list_dir_end();
			printf("[Mono] Extracted %d Facades assemblies to MEMFS\n", count);
			fflush(stdout);
		}}

	// Also extract GodotSharp.dll and project assembly to MEMFS
	{
		const char *extra_asms[] = {
			"res://.mono/assemblies/GodotSharp.dll",
			nullptr
		};
		for (int i = 0; extra_asms[i]; i++) {
			String res_path = extra_asms[i];
			String filename = res_path.get_file();
			String memfs_path = ".mono/assemblies/" + filename;
			Ref<FileAccess> src = FileAccess::open(res_path, FileAccess::READ);
			if (src.is_valid()) {
				Vector<uint8_t> data;
				data.resize(src->get_length());
				src->get_buffer(data.ptrw(), data.size());
				Ref<FileAccess> dst = FileAccess::open(memfs_path, FileAccess::WRITE);
				if (dst.is_valid()) {
					dst->store_buffer(data.ptr(), data.size());
					printf("[Mono] Extracted %s to MEMFS (%d bytes)\n", filename.utf8().get_data(), (int)data.size());
					fflush(stdout);
				}
			}
		}
	}

	// Extract project assembly
	{
		String project_name = "CSharpTest";
		if (ProjectSettings::get_singleton()) {
			project_name = ProjectSettings::get_singleton()->get_setting("dotnet/project/assembly_name", "CSharpTest");
		}
		String res_path = "res://.mono/assemblies/" + project_name + ".dll";
		String memfs_path = ".mono/assemblies/" + project_name + ".dll";
		Ref<FileAccess> src = FileAccess::open(res_path, FileAccess::READ);
		if (src.is_valid()) {
			Vector<uint8_t> data;
			data.resize(src->get_length());
			src->get_buffer(data.ptrw(), data.size());
			Ref<FileAccess> dst = FileAccess::open(memfs_path, FileAccess::WRITE);
			if (dst.is_valid()) {
				dst->store_buffer(data.ptr(), data.size());
				printf("[Mono] Extracted %s.dll to MEMFS (%d bytes)\n", project_name.utf8().get_data(), (int)data.size());
				fflush(stdout);
			}
		}
	}

	if (!FileAccess::exists(bcl_dir + "/mscorlib.dll")) {
		ERR_PRINT(String("[Mono] mscorlib.dll not found at " + bcl_dir).utf8().get_data());
	}

	printf("[Mono] Setting mono_dirs: assemblies=%s, etc=%s\n", assemblies_dir.utf8().get_data(), etc_dir.utf8().get_data());
	fflush(stdout);
	mono_set_dirs(assemblies_dir.utf8().get_data(), etc_dir.utf8().get_data());

	String search_path = bcl_dir + String(":") + String(".mono/assemblies") + String(":") + bcl_dir + "/Facades";
	printf("[Mono] Setting assemblies path: %s\n", search_path.utf8().get_data());
	fflush(stdout);
	mono_set_assemblies_path(search_path.utf8().get_data());
	setenv("MONO_PATH", search_path.utf8().get_data(), 1);
#else
	String mono_root = find_mono_root(exe_dir);

#ifdef WINDOWS_ENABLED
	if (!mono_root.is_empty()) {
		String bin_dir = mono_root + "/bin";
		if (DirAccess::exists(bin_dir)) {
			SetDllDirectoryA(bin_dir.utf8().get_data());
		}
		String bin_x64_dir = mono_root + "/bin/windows/x64";
		if (DirAccess::exists(bin_x64_dir)) {
			SetDllDirectoryA(bin_x64_dir.utf8().get_data());
		}
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

	if (!DirAccess::exists(bcl_dir) || !FileAccess::exists(bcl_dir.path_join("mscorlib.dll"))) {
		ERR_PRINT(String("[Mono] mscorlib.dll not found at " + bcl_dir).utf8().get_data());
	}

	mono_set_dirs(assemblies_dir.utf8().get_data(), etc_dir.utf8().get_data());

	String godotsharp_api_debug = exe_dir.path_join("GodotSharp").path_join("Api").path_join("Debug");
	String godotsharp_api_release = exe_dir.path_join("GodotSharp").path_join("Api").path_join("Release");
	String godotsharp_tools = exe_dir.path_join("GodotSharp").path_join("Tools");
	String search_path = bcl_dir + String(";") + exe_dir;
	if (DirAccess::exists(godotsharp_api_debug)) {
		search_path = search_path + ";" + godotsharp_api_debug;
	}
	if (DirAccess::exists(godotsharp_api_release)) {
		search_path = search_path + ";" + godotsharp_api_release;
	}
	if (DirAccess::exists(godotsharp_tools)) {
		search_path = search_path + ";" + godotsharp_tools;
	}
	mono_set_assemblies_path(search_path.utf8().get_data());

#ifdef WINDOWS_ENABLED
	SetEnvironmentVariableA("MONO_PATH", search_path.utf8().get_data());
#else
	setenv("MONO_PATH", search_path.utf8().get_data(), 1);
#endif
#endif // WEB_ENABLED / else

	printf("[Mono] Initializing C# / Mono runtime...\n");
	fflush(stdout);

	// Enable Mono trace logging for type loading to debug mono_class_init failures
	mono_trace_set_level_string("debug");
	mono_trace_set_mask_string("type");
	// Set custom log handler to capture Mono trace output
	mono_trace_set_log_handler([](const char *log_domain, const char *log_level, const char *message, mono_bool fatal, void *user_data) {
		printf("[MonoTrace][%s][%s] %s\n", log_domain ? log_domain : "?", log_level ? log_level : "?", message ? message : "?");
		fflush(stdout);
	}, nullptr);
	printf("[Mono] Trace logging enabled (level=debug, mask=type)\n");
	fflush(stdout);

#ifdef MONO_INTERP_MODE
	// WASM doesn't support JIT compilation. Use interpreter mode instead.
	// This sequence mirrors the official Mono WASM driver (sdks/wasm/src/driver.c):
	//   1. Set AOT mode to INTERP_LLVMONLY (sets mono_use_interpreter = TRUE)
	//   2. Initialize IL generators for marshal/method-builder/SGEN
	//   3. mono_jit_init_version -> mini_init will call mono_ee_interp_init internally
	//      (because mono_use_interpreter is TRUE and DISABLE_INTERPRETER is not defined)
	// NOTE: We must NOT call mono_ee_interp_init ourselves - mini_init does it,
	// and calling it twice triggers an assertion (g_assert(!interp_init_done)).
	printf("[Mono] Setting AOT mode to INTERP_LLVMONLY...\n");
	fflush(stdout);
	mono_jit_set_aot_mode(MONO_AOT_MODE_INTERP_LLVMONLY);

	printf("[Mono] Initializing IL generators...\n");
	fflush(stdout);
	mono_marshal_ilgen_init();
	mono_method_builder_ilgen_init();
	mono_sgen_mono_ilgen_init();
	printf("[Mono] IL generators initialized.\n");
	fflush(stdout);
#endif

#ifdef MONO_AOT_MODE
	printf("[Mono] Calling mono_jit_init_version (AOT mode)...\n");
	fflush(stdout);
	domain = mono_jit_init_version("GodotMonoAOT", "v4.0.30319");
	if (!domain) {
		ERR_PRINT("[Mono] Failed to initialize AOT runtime (mono_jit_init_version returned NULL)");
		return FAILED;
	}
#elif defined(MONO_INTERP_MODE)
	printf("[Mono] Calling mono_jit_init_version (interpreter mode)...\n");
	fflush(stdout);
	domain = mono_jit_init_version("GodotMonoInterp", "v4.0.30319");
	if (!domain) {
		ERR_PRINT("[Mono] Failed to initialize interpreter runtime (mono_jit_init_version returned NULL)");
		return FAILED;
	}
#else
	printf("[Mono] Calling mono_jit_init_version (JIT mode)...\n");
	fflush(stdout);
	domain = mono_jit_init_version("GodotMono", "v4.0.30319");
	if (!domain) {
		ERR_PRINT("[Mono] Failed to initialize JIT runtime (mono_jit_init_version returned NULL)");
		return FAILED;
	}
#endif
	printf("[Mono] mono_jit_init_version succeeded, domain=%p\n", (void *)domain);
	fflush(stdout);

	mono_aot_init();
	mono_aot_register_modules();

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
		printf("[Mono] Note: GodotSharp.dll not loaded (managed bindings limited).\n");
		fflush(stdout);
	}

	if (godotsharp_assembly) {
		MonoImage *img = mono_assembly_get_image(godotsharp_assembly);
		MonoClass *runtime_class = mono_class_from_name(img, "Godot", "Runtime");
		if (runtime_class) {
			MonoMethod *init_method = mono_class_get_method_from_name(runtime_class, "Initialize", 0);
			if (init_method) {
				MonoObject *exc = nullptr;
				mono_runtime_invoke(init_method, nullptr, nullptr, &exc);
				if (exc) {
					printf("[Mono] WARNING: Exception in Runtime.Initialize().\n");
					fflush(stdout);
				}
			}
		}
	}

	cache_sync_context_method();

	is_initialized = true;
	printf("[Mono] C# runtime initialized.\n");
	fflush(stdout);
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
	Vector<String> search_paths;
#ifdef WEB_ENABLED
	// On Web, GodotSharp.dll is packed in the PCK at res://.mono/assemblies/
	search_paths.push_back(".mono/assemblies/GodotSharp.dll");
	search_paths.push_back("GodotSharp.dll");
#else
	search_paths.push_back(exe_dir.path_join("GodotSharp").path_join("Api").path_join("Debug").path_join("GodotSharp.dll"));
	search_paths.push_back(exe_dir.path_join("GodotSharp.dll"));
	search_paths.push_back(exe_dir.path_join("mono").path_join("GodotSharp.dll"));
#endif

	String gs_path;
	for (int i = 0; i < search_paths.size(); i++) {
		if (FileAccess::exists(search_paths[i])) {
			gs_path = search_paths[i];
			break;
		}
	}

	if (gs_path.is_empty()) {
		printf("[Mono] GodotSharp.dll not found in search paths:\n");
		for (int i = 0; i < search_paths.size(); i++) {
			printf("[Mono]   - %s\n", search_paths[i].utf8().get_data());
		}
		fflush(stdout);
		return false;
	}

	printf("[Mono] Loading GodotSharp from: %s\n", gs_path.utf8().get_data());
	fflush(stdout);

	godotsharp_assembly = mono_domain_assembly_open(domain, gs_path.utf8().get_data());
	if (!godotsharp_assembly) {
		return false;
	}

	MonoImage *img = mono_assembly_get_image(godotsharp_assembly);
	mono_bridge::cache_godot_classes(img);
	mono_variant::cache_godot_math_classes(img);
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

void MonoHost::cache_sync_context_method() {
	sync_context_pump_method = nullptr;
	sync_context_instance = nullptr;

	if (!godotsharp_assembly) {
		return;
	}

	MonoImage *img = mono_assembly_get_image(godotsharp_assembly);
	if (!img) return;

	MonoClass *sync_ctx_class = mono_class_from_name(img, "Godot", "GodotSynchronizationContext");
	if (!sync_ctx_class) {
		printf("[Mono] GodotSynchronizationContext class not found (sync context pumping disabled).\n");
		fflush(stdout);
		return;
	}

	// Read the static _instance field using mono_field_static_get_value
	// (NOT mono_field_get_value with NULL obj, which asserts in Mono 6.12).
	// This is the lazy fallback path; the primary registration is via the
	// godot_icall_RegisterSyncContext icall from Runtime.Initialize().
	MonoClassField *instance_field = mono_class_get_field_from_name(sync_ctx_class, "_instance");
	if (!instance_field) {
		printf("[Mono] GodotSynchronizationContext._instance field not found.\n");
		fflush(stdout);
		return;
	}

	MonoVTable *vtable = mono_class_vtable(mono_domain_get(), sync_ctx_class);
	if (!vtable) {
		printf("[Mono] GodotSynchronizationContext vtable could not be created.\n");
		fflush(stdout);
		return;
	}

	MonoObject *instance = nullptr;
	mono_field_static_get_value(vtable, instance_field, &instance);
	if (!instance) {
		printf("[Mono] GodotSynchronizationContext._instance is null (Install() not called yet).\n");
		fflush(stdout);
		return;
	}

	// Cache the instance method and the instance object.
	sync_context_pump_method = mono_class_get_method_from_name(sync_ctx_class, "PumpInstance", 0);
	if (sync_context_pump_method) {
		sync_context_instance = instance;
		printf("[Mono] GodotSynchronizationContext.PumpInstance() cached for main thread pumping.\n");
		fflush(stdout);
	}
}

void MonoHost::register_sync_context(MonoObject *p_instance) {
	if (!p_instance) return;

	MonoClass *cls = mono_object_get_class(p_instance);
	if (!cls) return;

	MonoMethod *pump_method = mono_class_get_method_from_name(cls, "PumpInstance", 0);
	if (!pump_method) {
		printf("[Mono] PumpInstance method not found on sync context class.\n");
		fflush(stdout);
		return;
	}

	sync_context_instance = p_instance;
	sync_context_pump_method = pump_method;
	printf("[Mono] Sync context registered for instance-based pumping.\n");
	fflush(stdout);
}

void MonoHost::pump_sync_context() {
	// Lazy cache fallback: if the icall registration hasn't happened yet
	// (e.g., Runtime.Initialize() not called), try reading the static field
	// once. The icall path (register_sync_context) is the primary mechanism.
	if (!sync_context_pump_method || !sync_context_instance) {
		if (!sync_context_lazy_attempted) {
			sync_context_lazy_attempted = true;
			cache_sync_context_method();
		}
		if (!sync_context_pump_method || !sync_context_instance) {
			return;
		}
	}

	// Invoke the instance method PumpInstance() on the singleton.
	// Instance method dispatch works in the WASM interpreter; only static
	// method dispatch via mono_runtime_invoke triggers signature mismatch.
	MonoObject *exc = nullptr;
	mono_runtime_invoke(sync_context_pump_method, sync_context_instance, nullptr, &exc);
	if (exc) {
		MonoClass *exc_class = mono_object_get_class(exc);
		const char *exc_name = exc_class ? mono_class_get_name(exc_class) : "(unknown)";
		printf("[Mono] Exception in SyncContext.PumpInstance(): %s\n", exc_name ? exc_name : "(unknown)");
		fflush(stdout);
	}
}

void MonoHost::shutdown() {
	if (!is_initialized) {
		return;
	}

	printf("[Mono] Shutting down C# runtime...\n");
	fflush(stdout);

	mono_bridge::shutdown();
	mono_gc_bridge::shutdown();
	mono_aot_shutdown();

	sync_context_pump_method = nullptr;
	sync_context_instance = nullptr;
	sync_context_lazy_attempted = false;

	if (domain) {
		mono_jit_cleanup(domain);
		domain = nullptr;
	}

	is_initialized = false;
	printf("[Mono] C# runtime shutdown complete.\n");
	fflush(stdout);
}
