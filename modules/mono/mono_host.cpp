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
// P1-#7 fix: use Path::get_csharp_project_name() for unified assembly name
// resolution (was hardcoded "CSharpTest" fallback, diverged from the export
// plugin's sanitized name → runtime couldn't find the PCK-embedded DLL).
#include "utils/path_utils.h"

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
	// On Web, BCL is embedded in WASM MEMFS via emcc --embed-file
	// (SCsub_web.py). GodotSharp.dll and project assembly are in PCK and
	// must be extracted to MEMFS for Mono to load them (and for hot updates).
	String bcl_dir = "lib/mono/4.5";
	String etc_dir = "etc";
	String assemblies_dir = "lib";

	// Create .mono/assemblies directory in MEMFS for project assemblies
	{
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		if (da.is_valid()) {
			da->make_dir_recursive(".mono/assemblies");
		}
	}

	// Verify BCL is available in WASM MEMFS (embedded at build time)
	if (!FileAccess::exists(bcl_dir + "/mscorlib.dll")) {
		ERR_PRINT(String("[Mono] mscorlib.dll not found at " + bcl_dir +
		                 " — BCL embedding may have failed in SCsub_web.py").utf8().get_data());
	} else {
		printf("[Mono] BCL available in WASM MEMFS at %s\n", bcl_dir.utf8().get_data());
		fflush(stdout);
	}

	// Extract GodotSharp.dll from PCK to MEMFS (NOT in WASM — for hot updates)
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

	// Extract project assembly from PCK to MEMFS (for hot updates)
	{
		// P1-#7 fix: was hardcoded "CSharpTest" fallback + unsanitized direct
		// read, which diverged from the export plugin's sanitized name when
		// dotnet/project/assembly_name was unset (default config). The export
		// plugin writes the DLL under Path::get_csharp_project_name(); the
		// runtime must read it back with the SAME resolution to find it.
		String project_name = Path::get_csharp_project_name();
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

	mono_set_dirs(assemblies_dir.utf8().get_data(), etc_dir.utf8().get_data());

	String search_path = bcl_dir + String(":") + String(".mono/assemblies") + String(":") + bcl_dir + "/Facades";
	printf("[Mono] Setting assemblies path: %s\n", search_path.utf8().get_data());
	fflush(stdout);
	mono_set_assemblies_path(search_path.utf8().get_data());
	setenv("MONO_PATH", search_path.utf8().get_data(), 1);
#else
	String mono_root = find_mono_root(exe_dir);

#ifdef WINDOWS_ENABLED
	// SetDllDirectoryA replaces (not appends) the previous setting, so only the
	// final call takes effect. We point it at the exe directory to allow Mono
	// DLLs placed alongside the executable to be found by the loader.
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
// Mono accepts both ';' and ':' as separators in mono_set_assemblies_path on
// all platforms, but the MONO_PATH environment variable is platform-specific.
#ifdef WINDOWS_ENABLED
	const char *path_sep = ";";
#else
	const char *path_sep = ":";
#endif
	String search_path = bcl_dir + String(path_sep) + exe_dir;
	if (DirAccess::exists(godotsharp_api_debug)) {
		search_path = search_path + path_sep + godotsharp_api_debug;
	}
	if (DirAccess::exists(godotsharp_api_release)) {
		search_path = search_path + path_sep + godotsharp_api_release;
	}
	if (DirAccess::exists(godotsharp_tools)) {
		search_path = search_path + path_sep + godotsharp_tools;
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

	// Enable Mono trace logging for type loading to debug mono_class_init failures.
	// Wrapped in DEBUG_ENABLED to avoid excessive logging in release builds.
#ifdef DEBUG_ENABLED
	mono_trace_set_level_string("debug");
	mono_trace_set_mask_string("type");
	// Set custom log handler to capture Mono trace output
	mono_trace_set_log_handler([](const char *log_domain, const char *log_level, const char *message, mono_bool fatal, void *user_data) {
		printf("[MonoTrace][%s][%s] %s\n", log_domain ? log_domain : "?", log_level ? log_level : "?", message ? message : "?");
		fflush(stdout);
	}, nullptr);
	printf("[Mono] Trace logging enabled (level=debug, mask=type)\n");
	fflush(stdout);
#endif

#if defined(TOOLS_ENABLED) && !defined(WEB_ENABLED)
	// P7: External IDE attach debugger (sdb agent).
	// Must be called BEFORE mono_jit_init_version. When enabled, launches a
	// dt_socket server on 127.0.0.1:<port> that Rider/VS/VSCode can attach to.
	// Default off — enable via ProjectSettings dotnet/debugger/enabled=true
	// (dotnet/debugger/port defaults to 55555). The env var
	// GODOT_MONO_DEBUGGER_PORT overrides the port for quick CLI use without
	// touching project settings. Editor process stays off by default; the
	// setting is primarily intended for the in-editor Play mode (which runs
	// in the editor process under TOOLS_ENABLED).
	{
		bool dbg_enabled = false;
		int dbg_port = 55555;
		if (ProjectSettings::get_singleton()) {
			dbg_enabled = (bool)ProjectSettings::get_singleton()->get_setting("dotnet/debugger/enabled", false);
			dbg_port = (int)ProjectSettings::get_singleton()->get_setting("dotnet/debugger/port", 55555);
		}
		// Env var override: GODOT_MONO_DEBUGGER_PORT=<port> forces enabled.
		const char *env_port = getenv("GODOT_MONO_DEBUGGER_PORT");
		if (env_port && env_port[0] != '\0') {
			int parsed = atoi(env_port);
			if (parsed > 0 && parsed < 65536) {
				dbg_enabled = true;
				dbg_port = parsed;
			}
		}
		if (dbg_enabled) {
			String opt = "--debugger-agent=transport=dt_socket,server=y,suspend=n,address=127.0.0.1:" + itos(dbg_port);
			CharString opt_utf8 = opt.utf8();
			char *opt_argv[] = { opt_utf8.ptrw() };
			printf("[Mono] P7: Enabling sdb debugger agent on 127.0.0.1:%d (suspend=n)\n", dbg_port);
			fflush(stdout);
			mono_jit_parse_options(1, opt_argv);
		}
	}
#endif // TOOLS_ENABLED && !WEB_ENABLED

#if defined(MONO_AOT_MODE) && defined(MONO_INTERP_MODE)
	// ========================================
	// Hybrid AOT + Interpreter mode (WASM)
	// AOT-compiled methods run as native code,
	// uncompiled methods fall back to Interpreter.
	//
	// Use MONO_AOT_MODE_INTERP_LLVMONLY (same as pure interpreter mode).
	// Register AOT modules AFTER jit_init to avoid "not compiled with
	// --aot=interp" check during runtime init.
	// GodotSharp.dll load failure ("dependency cannot be found") is
	// expected because System.Runtime/System.Collections/etc are not
	// AOT-compiled. The load_godotsharp() call handles this gracefully
	// (returns false, mono_host continues without managed bindings).
	// ========================================
	printf("[Mono] Hybrid AOT mode: AOT + Interpreter fallback (INTERP_LLVMONLY, deferred registration)\n");
	fflush(stdout);

	// Set AOT mode to INTERP_LLVMONLY (disables JIT, uses interpreter)
	mono_jit_set_aot_mode(MONO_AOT_MODE_INTERP_LLVMONLY);

	// Initialize IL generators (required for Interpreter fallback)
	printf("[Mono] Initializing IL generators for Interpreter fallback...\n");
	fflush(stdout);
	mono_marshal_ilgen_init();
	mono_method_builder_ilgen_init();
	mono_sgen_mono_ilgen_init();
	printf("[Mono] IL generators initialized.\n");
	fflush(stdout);

	// Initialize Mono runtime FIRST (no AOT modules registered yet,
	// so INTERP_LLVMONLY won't enforce --aot=interp flag check)
	printf("[Mono] Calling mono_jit_init_version (Hybrid AOT)...\n");
	fflush(stdout);
	domain = mono_jit_init_version("GodotMonoHybridAOT", "v4.0.30319");
	if (!domain) {
		ERR_PRINT("[Mono] Failed to initialize Hybrid AOT runtime (mono_jit_init_version returned NULL)");
		return FAILED;
	}

	// Register AOT modules AFTER runtime init.
	printf("[Mono] Registering AOT modules (post-init)...\n");
	fflush(stdout);
	mono_aot_init();
	mono_aot_register_modules();

	// H9 fix (problem 3): Probe AOT module table integrity before any
	// mono_class_init call (which triggers module loading and would abort
	// if a registered AOT module's dependency is missing). The probe loads
	// mscorlib's Object class — the most fundamental type. If this succeeds,
	// the AOT module table + MEMFS BCL layout is consistent. If it fails,
	// we log a diagnostic and continue (Hybrid AOT falls back to interpreter
	// for missing modules, rather than aborting the whole runtime).
	printf("[Mono] H9: Probing AOT module table integrity (mono_class_from_name Object)...\n");
	fflush(stdout);
	MonoClass *probe_object = mono_class_from_name(mono_get_corlib(), "System", "Object");
	if (!probe_object) {
		printf("[Mono] H9 WARNING: AOT module probe failed - Object class not found. "
		       "Falling back to interpreter-only mode for corlib types.\n");
		fflush(stdout);
		// Re-set to INTERP_LLVMONLY to relax AOT dependency checks for
		// subsequent mono_class_init calls. This is a no-op if already in
		// INTERP_LLVMONLY, but harmless and documents intent.
		mono_jit_set_aot_mode(MONO_AOT_MODE_INTERP_LLVMONLY);
	} else {
		printf("[Mono] H9: AOT module probe OK - Object class resolved (ptr=%p).\n",
		       (void *)probe_object);
		fflush(stdout);
	}

#elif defined(MONO_AOT_MODE)
	// ========================================
	// Pure Full AOT mode (no Interpreter fallback)
	// ========================================
	printf("[Mono] Pure Full AOT mode (no Interpreter fallback)\n");
	fflush(stdout);
	mono_jit_set_aot_mode(MONO_AOT_MODE_FULL);

	mono_aot_init();
	mono_aot_register_modules();

	printf("[Mono] Calling mono_jit_init_version (AOT mode)...\n");
	fflush(stdout);
	domain = mono_jit_init_version("GodotMonoAOT", "v4.0.30319");
	if (!domain) {
		ERR_PRINT("[Mono] Failed to initialize AOT runtime (mono_jit_init_version returned NULL)");
		return FAILED;
	}

#elif defined(MONO_INTERP_MODE)
	// ========================================
	// Pure Interpreter mode (existing, WASM default)
	// ========================================
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

#ifndef MONO_AOT_MODE
	// For non-AOT modes (Interpreter/JIT), register AOT modules after runtime init.
	// In AOT modes, mono_aot_init/register_modules are called before jit_init above.
	mono_aot_init();
	mono_aot_register_modules();
#endif

	mono_bridge::init(domain);
	mono_gc_bridge::init(domain);
	mono_variant::cache_mono_corlib_classes();

	if (!register_internal_calls()) {
		// M2 fix: cleanup partial init state so the process can re-attempt or
		// exit cleanly without leaking the root domain. Previously shutdown()
		// early-returned because is_initialized was still false.
		cleanup_partial_init();
		return FAILED;
	}

	if (!load_corlib()) {
		cleanup_partial_init();
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

	// GodotSharp and ProjectScripts are NOT in the AOT module table (set to nullptr).
	// Mono will load them as regular interpreted assemblies — no AOT dependency check.
	// BCL (mscorlib, System, System.Core) still runs as AOT native code.

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
		// Pin the instance with a strong GCHandle to prevent GC from
		// collecting it while C++ holds the raw pointer. The C# static
		// _instance field also holds a reference, but the C++ side must
		// not rely on C# GC root tracking alone.
		if (sync_context_gchandle != 0) {
			mono_gchandle_free(sync_context_gchandle);
		}
		sync_context_gchandle = mono_gchandle_new(instance, false);
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

	// Free any previous GCHandle before storing the new instance.
	if (sync_context_gchandle != 0) {
		mono_gchandle_free(sync_context_gchandle);
	}
	sync_context_gchandle = mono_gchandle_new(p_instance, false);
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

void MonoHost::cleanup_partial_init() {
	// M2: clean up state created during a partially-successful initialize()
	// when a later step (register_internal_calls / load_corlib) fails. The
	// regular shutdown() cannot be used because is_initialized is still false
	// (it is only set at the end of initialize()), so shutdown() would
	// early-return and leak the root domain + bridge state.
	printf("[Mono] Cleaning up partially-initialized runtime...\n");
	fflush(stdout);

	mono_bridge::shutdown();
	mono_gc_bridge::shutdown();
	mono_aot_shutdown();

	sync_context_pump_method = nullptr;
	if (sync_context_gchandle != 0) {
		mono_gchandle_free(sync_context_gchandle);
		sync_context_gchandle = 0;
	}
	sync_context_instance = nullptr;
	sync_context_lazy_attempted = false;

	if (domain) {
		mono_jit_cleanup(domain);
		domain = nullptr;
	}

	is_initialized = false;
	printf("[Mono] Partial-init cleanup complete.\n");
	fflush(stdout);
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
	if (sync_context_gchandle != 0) {
		mono_gchandle_free(sync_context_gchandle);
		sync_context_gchandle = 0;
	}
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
