#include "gd_mono.h"
// mono_image_open_from_data, mono_assembly_load_from, MonoImageOpenStatus
// are already declared in Mono headers (mono/metadata/image.h, assembly.h).

#ifdef WEB_ENABLED
#include <emscripten.h>
#include <cstdio>
#include <cstring>
#endif

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/object.h"
#include "core/os/os.h"
#include "core/os/main_loop.h"
#include "utils/mono_logger.h"
#include "csharp_debugger.h"
#include "../mono_gd/interop/gd_mono_interop_variant.h"
#include "../mono_gd/interop/gd_mono_callable.h"
#include "../mono_gd/interop/signal_awaiter_utils.h"
#include "../glue/mono_glue.h"
#include <mono/mono-publib.h>
#include <cstring>
#include <cstdlib>
#include <algorithm>

#define MONO_AOT_MODE_INTERP 5
#define MONO_EE_MODE_INTERP 1000
#define MONO_TABLE_TYPEDEF 2

// Mono debug format enum (from mono/metadata/mono-debug.h)
#define MONO_DEBUG_FORMAT_MONO 1

extern "C" {
void mono_jit_set_aot_mode(int mode);
const char *mono_check_corlib_version(void);
MonoImage *mono_get_corlib(void);
const char *mono_image_get_name(MonoImage *image);
MonoImage *mono_image_open_full(const char *fname, MonoImageOpenStatus *status, mono_bool refonly);
MonoImage *mono_image_open_from_data(char *data, uint32_t data_len, mono_bool need_copy, MonoImageOpenStatus *status);
const char *mono_image_strerror(MonoImageOpenStatus status);
void mono_image_close(MonoImage *image);
void mono_trace_set_level_string(const char *value);
void mono_trace_set_mask_string(const char *value);
typedef void (*MonoLogCallback)(const char *log_domain, const char *log_level, const char *message, mono_bool fatal, void *user_data);
typedef void (*MonoPrintCallback)(const char *string, mono_bool is_stdout);
void mono_trace_set_log_handler(MonoLogCallback callback, void *user_data);
void mono_trace_set_print_handler(MonoPrintCallback callback);
void mono_trace_set_printerr_handler(MonoPrintCallback callback);
const void *mono_image_get_table_info(MonoImage *image, int table_id);
int mono_table_info_get_rows(const void *table);
MonoClass *mono_class_get(MonoImage *image, uint32_t type_token);
MonoAssembly *mono_assembly_load_from(MonoImage *image, const char *fname, MonoImageOpenStatus *status);
// Android assembly preload hook APIs（精简头文件缺少声明，见 mono/metadata/assembly.h）
typedef MonoAssembly *(*MonoAssemblyPreloadFunc)(MonoAssemblyName *aname, char **assemblies_path, void *user_data);
const char *mono_assembly_name_get_name(MonoAssemblyName *aname);
const char *mono_assembly_name_get_culture(MonoAssemblyName *aname);
MonoImage *mono_image_open_from_data_with_name(char *data, uint32_t data_len, mono_bool need_copy, MonoImageOpenStatus *status, mono_bool refonly, const char *name);
MonoAssembly *mono_assembly_load_from_full(MonoImage *image, const char *fname, MonoImageOpenStatus *status, mono_bool refonly);
void mono_install_assembly_preload_hook(MonoAssemblyPreloadFunc func, void *user_data);
// Debug APIs
void mono_debug_init(int format);
void mono_debug_cleanup(void);
mono_bool mono_debug_enabled(void);
}

#ifdef WEB_ENABLED
static void web_mono_log_callback(const char *log_domain, const char *log_level, const char *message, mono_bool fatal, void *user_data) {
	if (message) {
		printf("[Mono-Trace] %s: %s\n", log_domain ? log_domain : "?", message);
		fflush(stdout);
	}
}

static void web_mono_print_callback(const char *string, mono_bool is_stdout) {
	if (string) {
		printf("%s", string);
		fflush(stdout);
	}
}
#endif

static GDMono *singleton = nullptr;

GDMono *GDMono::get_singleton() {
	return singleton;
}

GDMono::GDMono() {
	singleton = this;
}

GDMono::~GDMono() {
	cleanup();
	if (singleton == this)
		singleton = nullptr;
}

void GDMono::cache_managed_object(ObjectID p_native_id, MonoObject *p_mono_obj) {
	if (!p_mono_obj) return;
	uint32_t gchandle = mono_gchandle_new(p_mono_obj, true);
	if (object_gchandles.has(p_native_id)) {
		mono_gchandle_free(object_gchandles[p_native_id]);
	}
	object_gchandles[p_native_id] = gchandle;
}

MonoObject *GDMono::get_cached_managed_object(ObjectID p_native_id) const {
	const uint32_t *gchandle = object_gchandles.getptr(p_native_id);
	if (!gchandle || *gchandle == 0) return nullptr;
	return mono_gchandle_get_target(*gchandle);
}

void GDMono::remove_cached_managed_object(ObjectID p_native_id) {
	uint32_t *gchandle = object_gchandles.getptr(p_native_id);
	if (gchandle && *gchandle != 0) {
		mono_gchandle_free(*gchandle);
		*gchandle = 0;
	}
	object_gchandles.erase(p_native_id);
}

// P1.1: 属性系统引用类型字段支持
MonoObject *GDMono::get_mono_object_for_godot_object(Object *p_obj) const {
	if (!p_obj) return nullptr;
	return get_cached_managed_object(p_obj->get_instance_id());
}

Object *GDMono::get_godot_object_for_mono_object(MonoObject *p_mono_obj) const {
	if (!p_mono_obj) return nullptr;
	// 反向查找：遍历 object_gchandles 找匹配的 MonoObject
	for (const KeyValue<ObjectID, uint32_t> &E : object_gchandles) {
		MonoObject *cached = mono_gchandle_get_target(E.value);
		if (cached == p_mono_obj) {
			return ObjectDB::get_instance(E.key);
		}
	}
	return nullptr;
}

void GDMono::post_sync_callback(void (*p_callback)()) {
	if (p_callback) {
		pending_sync_callbacks.push_back(p_callback);
	}
}

void GDMono::post_sync_delegate(MonoObject *p_delegate) {
	if (!p_delegate) return;
	// Pin the delegate with a GC handle so it survives until processed.
	uint32_t handle = mono_gchandle_new(p_delegate, true);
	pending_delegate_handles.push_back(handle);
}

void GDMono::process_sync_callbacks() {
	// Process C function-pointer callbacks
	while (!pending_sync_callbacks.is_empty()) {
		void (*cb)() = pending_sync_callbacks.front()->get();
		pending_sync_callbacks.pop_front();
		if (cb) cb();
	}
	// Process C# delegate callbacks (async/await continuations)
	while (!pending_delegate_handles.is_empty()) {
		uint32_t handle = pending_delegate_handles.front()->get();
		pending_delegate_handles.pop_front();
		if (handle != 0) {
			MonoObject *delegate_obj = mono_gchandle_get_target(handle);
			if (delegate_obj) {
				// Invoke the Action delegate: Find Invoke method on delegate type
				MonoClass *delegate_class = mono_object_get_class(delegate_obj);
				if (delegate_class) {
					MonoMethod *invoke_method = mono_class_get_method_from_name(delegate_class, "Invoke", 0);
					if (invoke_method) {
						MonoObject *exc = nullptr;
						mono_runtime_invoke(invoke_method, delegate_obj, nullptr, &exc);
						if (exc) {
							MonoLogger::log_warning("Exception in async continuation");
						}
					}
				}
			}
			mono_gchandle_free(handle);
		}
	}
}

void GDMono::install_synchronization_context() {
	if (!scripts_domain) return;
	if (!godotsharp_image) {
		MonoLogger::log_warning("Cannot install sync context: GodotSharp image not loaded");
		return;
	}
	MonoLogger::log("Installing Godot synchronization context...");

	// Find GodotSynchronizationContext.Install() and invoke it
	MonoClass *sync_ctx_class = mono_class_from_name(godotsharp_image, "Godot", "GodotSynchronizationContext");
	if (!sync_ctx_class) {
		MonoLogger::log_warning("GodotSynchronizationContext class not found in GodotSharp");
		return;
	}
	MonoMethod *install_method = mono_class_get_method_from_name(sync_ctx_class, "Install", 0);
	if (!install_method) {
		MonoLogger::log_warning("GodotSynchronizationContext.Install method not found");
		return;
	}
	MonoObject *exc = nullptr;
	mono_runtime_invoke(install_method, nullptr, nullptr, &exc);
	if (exc) {
		MonoLogger::log_error("Exception while installing GodotSynchronizationContext");
	} else {
		MonoLogger::log("GodotSynchronizationContext installed successfully");
	}
}

void GDMono::on_frame_tick() {
	process_sync_callbacks();
}

bool GDMono::initialize() {
	if (initialized)
		return true;

	MonoLogger::log("Initializing Mono runtime (static linkage mode)...");

	// Configure the SDB agent BEFORE mono_jit_init_version(). The agent is
	// configured via mono_jit_parse_options() (NOT the MONO_DEBUG env var,
	// which Mono 6.12 does not honor for --debugger-agent). The agent starts
	// listening on the requested port once the JIT is initialized.
	// No-op if --mono-debugger was not passed.
	CSharpDebugger::configure_before_jit_init();

	// Initialize Mono debug support (enables source location / stack frame info)
	// Skip in WASM (interpreter mode doesn't support full debug, and it's stubbed)
#ifndef WEB_ENABLED
	mono_debug_init(MONO_DEBUG_FORMAT_MONO);
	MonoLogger::log("Mono debug symbols initialized");
#endif

#ifdef WEB_ENABLED
	// In WebAssembly, get_executable_path() returns the module name (e.g. "godot.js"),
	// not a real filesystem path. Use "/" as base directory since BCL and assemblies
	// are embedded into MEMFS via Emscripten --preload-file at build time.
	String exe_dir = "/";
#elif defined(ANDROID_ENABLED)
	// Android: BCL 与用户程序集打包在 APK 内的 res:// 路径下。
	// OS::get_executable_path() 在 Android 上返回 APK 内部路径，不可直接 fopen。
	// 用 Godot 的 user data 目录（FileAccess 会自动处理 APK 读取）。
	// 程序集加载走 preload hook（load_assembly_from_pck，见下方）。
	String exe_dir = OS::get_singleton()->get_user_data_dir();  // 通常为 /data/data/<pkg>/files
#elif defined(IOS_ENABLED)
	// iOS: BCL 与用户程序集打包在 NSBundle mainBundle 资源目录下。
	// OS::get_executable_path() 在 iOS 上返回 mainBundle 可执行文件路径，
	// 其 base_dir 即资源目录（如 .../MyApp.app/）。
	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
#else
	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
#endif

	String mono_lib_dir = exe_dir.path_join("mono").path_join("lib");
	String mono_etc_dir = exe_dir.path_join("mono").path_join("etc");
	String mono_bcl_dir = mono_lib_dir.path_join("mono").path_join("4.5");

	CharString mono_lib_utf8 = mono_lib_dir.utf8();
	CharString mono_etc_utf8 = mono_etc_dir.utf8();

	MonoLogger::log(vformat("Mono lib dir (assembly_dir): %s", mono_lib_dir));
	MonoLogger::log(vformat("Mono etc dir (config_dir): %s", mono_etc_dir));
	MonoLogger::log(vformat("BCL candidate 1 (DISABLE_DESKTOP_LOADER): %s/mscorlib.dll", mono_lib_dir));
	MonoLogger::log(vformat("BCL candidate 2 (desktop layout): %s/mscorlib.dll", mono_bcl_dir));

	mono_set_dirs(mono_lib_utf8.get_data(), mono_etc_utf8.get_data());

#ifdef WEB_ENABLED
	// M5 修复: WASM 日志收口 - release 默认关闭，避免性能与隐私问题
	// 通过环境变量 GDMONO_WASM_TRACE=1 可重新开启调试
	{
		CharString trace_env_utf8 = OS::get_singleton()->get_environment("GDMONO_WASM_TRACE").utf8();
		const char *trace_env = trace_env_utf8.get_data();
		bool enable_trace = (trace_env && trace_env[0] == '1');
		if (enable_trace) {
			MonoLogger::log("Installing Mono trace log handlers for diagnostics...");
			mono_trace_set_level_string("debug");
			mono_trace_set_mask_string("all");
			mono_trace_set_log_handler(web_mono_log_callback, nullptr);
			mono_trace_set_print_handler(web_mono_print_callback);
			mono_trace_set_printerr_handler(web_mono_print_callback);
			MonoLogger::log("Mono trace log handlers installed");
		} else {
			// 默认只显示 error 级别，关闭 debug/info
			mono_trace_set_level_string("error");
			mono_trace_set_mask_string("all");
		}
	}
#endif

	mono_config_parse(nullptr);

	assemblies_path = exe_dir.path_join(".mono").path_join("assemblies");
#ifdef WEB_ENABLED
	DirAccess::make_dir_recursive_absolute(assemblies_path);
#else
	if (!DirAccess::exists(assemblies_path)) {
		DirAccess::make_dir_recursive_absolute(assemblies_path);
	}
#endif

#ifdef WEB_ENABLED
	{
		String test_paths[] = {
			mono_lib_dir.path_join("mscorlib.dll"),
			mono_bcl_dir.path_join("mscorlib.dll"),
		};
		for (const String &p : test_paths) {
			CharString p_utf8 = p.utf8();
			FILE *f = fopen(p_utf8.get_data(), "rb");
			if (f) {
				fseek(f, 0, SEEK_END);
				long sz = ftell(f);
				fseek(f, 0, SEEK_SET);
				char sig[4] = {0};
				fread(sig, 1, 4, f);
				fclose(f);
				bool valid_mz = (sig[0] == 'M' && sig[1] == 'Z');
				MonoLogger::log(vformat("BCL found: %s size=%d MZ=%s", p, (int)sz, valid_mz ? "yes" : "no"));
			} else {
				MonoLogger::log(vformat("BCL NOT FOUND: %s", p));
			}
		}
	}
#endif

	const char *runtime_version = "v4.0.30319";

#ifdef WEB_ENABLED
	MonoLogger::log("Setting up interpreter mode (EE_MODE_INTERP, no AOT trampolines)...");
	mono_jit_set_aot_mode(MONO_EE_MODE_INTERP);
	// P2.5: MONO_NO_VERIFY 安全让步——WASM 解释器无法通过严格 CIL 验证（BCL 程序集
	// 也会被拒），故跳过全部验证。这是已知安全让步：恶意构造的 CIL 字节码可绕过类型
	// 安全检查。缓解措施：
	//   1. 仅加载构建管线产出的 BCL 与编辑器编译的用户 DLL，不加载不可信第三方 DLL
	//   2. 用户 DLL 应先在桌面平台（MONO_NO_VERIFY 未设置）跑过完整验证
	//   3. 生产环境用 monolinker/mono-cil-strip 预处理（见 scripts/trim_assemblies.py）
	// 详见 AGENTS.md 第八节"安全注意事项"。
	setenv("MONO_NO_VERIFY", "1", 1);
	MonoLogger::log_warning("MONO_NO_VERIFY=1 (WASM only) — CIL verification skipped, "
	                        "load only trusted assemblies (BCL + editor-built user DLL)");
#elif defined(IOS_ENABLED)
	// iOS: App Store 禁止 JIT（W^X 内存保护），必须用 interpreter 模式。
	// 与 WASM 复用 MONO_EE_MODE_INTERP，避免 Full AOT 的复杂工具链。
	// interpreter 加载 BCL 同样会触发严格 CIL 验证失败，故沿用 MONO_NO_VERIFY 让步。
	MonoLogger::log("Setting up interpreter mode for iOS (EE_MODE_INTERP, no JIT per App Store policy)...");
	mono_jit_set_aot_mode(MONO_EE_MODE_INTERP);
	setenv("MONO_NO_VERIFY", "1", 1);
	MonoLogger::log_warning("MONO_NO_VERIFY=1 (iOS) — CIL verification skipped, "
	                        "load only trusted assemblies (BCL + editor-built user DLL)");
#elif defined(ANDROID_ENABLED)
	// Android: 允许 JIT，但本项目沿用 interpreter 模式以复用 mono_new 的 WASM 路径
	// （icall ABI、Variant 封送等已在 interpreter 下验证）。
	// Android NDK 的 BCL 也无法通过严格 CIL 验证，沿用 MONO_NO_VERIFY 让步。
	MonoLogger::log("Setting up interpreter mode for Android (EE_MODE_INTERP)...");
	mono_jit_set_aot_mode(MONO_EE_MODE_INTERP);
	setenv("MONO_NO_VERIFY", "1", 1);
	MonoLogger::log_warning("MONO_NO_VERIFY=1 (Android) — CIL verification skipped, "
	                        "load only trusted assemblies (BCL + editor-built user DLL)");
	// 线程挂起策略：切换到协作式（cooperative），避免 SGen GC 通过信号挂起
	// Godot 的 VkThread 等非 Mono 线程。ARM64 上信号挂起会读取 FPSIMD 上下文，
	// 若线程未使用浮点指令则 magic 不匹配，触发 mono-context.c 断言 SIGABRT。
	// 协作式模式下，只有注册到 Mono 的线程（运行 C# 代码）会在安全点被挂起，
	// 未注册线程（VkThread 等）不被挂起也不被扫描——正好是我们想要的行为。
	setenv("MONO_THREADS_SUSPEND_POLICY", "cooperative", 1);
	MonoLogger::log("MONO_THREADS_SUSPEND_POLICY=cooperative (Android) — avoids FPSIMD "
	                "assertion in VkThread during GC signal-based suspend");
#endif

#if (defined(ANDROID_ENABLED) || (!defined(WEB_ENABLED) && !defined(IOS_ENABLED))) && !defined(MONO_STUB)
	// 通用（Desktop + Android）preload hook 必须在 mono_jit_init_version() 之前装：
	// mono_jit_init_version() 内部就会加载 mscorlib.dll，而无论 Desktop .pck 还是
	// Android APK，BCL/GodotSharp/用户 DLL 全部是通过 Godot FileAccess 挂在 res://
	// 下的 VFS 文件，Mono 自身的 fopen() 查找路径根本看不见 → 不装 hook 的话，
	// JIT init 阶段直接因为找不到 mscorlib 调用 exit() 闪退。
	install_universal_assembly_preload_hook();
#endif

	MonoLogger::log(vformat("Calling mono_jit_init_version with runtime: %s", runtime_version));


	root_domain = mono_jit_init_version("GodotEngine", runtime_version);

	// Mark JIT as initialized so any later call to configure_before_jit_init()
	// is rejected (calling mono_jit_parse_options after JIT init is undefined
	// behavior and typically aborts the process).
	CSharpDebugger::mark_jit_initialized();

	MonoLogger::log(vformat("mono_jit_init_version returned, root_domain=%s", root_domain ? "non-null" : "null"));

	// Install the unhandled exception hook now that the JIT is initialized.
	// The hook captures C# exceptions for display in the Godot debugger panel.
	if (root_domain) {
		CSharpDebugger::install_exception_hook();
	}

#ifdef WEB_ENABLED
	const char *corlib_version_err = mono_check_corlib_version();
	if (corlib_version_err) {
		MonoLogger::log_error(vformat("mono_check_corlib_version FAILED: %s", corlib_version_err));
	} else {
		MonoLogger::log("mono_check_corlib_version PASSED");
	}

	if (root_domain) {
		MonoImage *corlib = mono_get_corlib();
		if (corlib) {
			const char *corlib_name = mono_image_get_name(corlib);
			MonoLogger::log(vformat("corlib loaded successfully: %s", corlib_name ? corlib_name : "(null)"));
		} else {
			MonoLogger::log_error("mono_get_corlib() returned NULL - mscorlib invalid!");
		}
	}
#endif
	if (!root_domain) {
		MonoLogger::log_warning(vformat("Mono JIT init reported issues (BCL not found at %s or %s). Managed code execution will be unavailable until BCL assemblies are deployed.", mono_lib_dir, mono_bcl_dir));
		MonoLogger::log_warning("Place mscorlib.dll and BCL assemblies in: <exe_dir>/mono/lib/ (for DISABLE_DESKTOP_LOADER) or <exe_dir>/mono/lib/mono/4.5/");
	} else {
		MonoLogger::log(vformat("Mono BCL path: %s", mono_bcl_dir));
	}

	if (root_domain) {
#ifdef DISABLE_APPDOMAINS
		scripts_domain = root_domain;
		MonoLogger::log("Using root domain (multi-appdomain support disabled in WASM build)");
#else
		scripts_domain = mono_domain_create_appdomain(const_cast<char *>("GodotScripts"), nullptr);
		if (!scripts_domain) {
			MonoLogger::log_error("Failed to create scripts app domain");
		} else {
			mono_domain_set(scripts_domain, true);
		}
#endif
	}

	GDMonoInterop::variant_register_icalls();
	GDMonoCallable::register_icalls();
	GDSignalAwaiter::register_icalls();

	// Note: Android preload hook 已在 mono_jit_init_version() 之前安装（见上方）。
	// desktop/WASM/iOS 不需要此 hook（文件系统可直接 fopen）。

	// Register ClassDB-generated Node/Node2D/Node3D/Control/Resource/Timer icalls (see glue/glue_cpp/).
	GDMonoInterop::register_node_icalls();
	GDMonoInterop::register_node2d_icalls();
	GDMonoInterop::register_node3d_icalls();
	GDMonoInterop::register_control_icalls();
	GDMonoInterop::register_resource_icalls();
	GDMonoInterop::register_timer_icalls();

	// mono_glue_init() removed - was overriding real icalls with stubs

	if (scripts_domain) {
		Vector<String> search_paths;
		search_paths.push_back(exe_dir.path_join("GodotSharp.dll"));
		search_paths.push_back(exe_dir.path_join(".mono").path_join("assemblies").path_join("GodotSharp.dll"));
		search_paths.push_back(assemblies_path.path_join("GodotSharp.dll"));
		// Web: PCK assemblies live under the res:// VFS (e.g. res://.mono/assemblies/),
		// which is NOT reachable via the bare /-rooted filesystem paths above.
		// Keep the res:// prefix so FileAccess can read the PCK entry (matching the
		// user-assembly scanning logic at line ~324).
		search_paths.push_back(String("res://.mono/assemblies/GodotSharp.dll"));

		for (const String &path : search_paths) {
			if (FileAccess::exists(path)) {
				MonoLogger::log(vformat("Loading GodotSharp from: %s", path));
#if defined(WEB_ENABLED) || defined(ANDROID_ENABLED)
				// In WASM, mono_pe_file_map fails because the library's internal
				// mono_file_map_size returns 0 for MEMFS files. Load from buffer instead.
				// Android 同样无法直接 fopen APK 内的 res:// 路径，需用 FileAccess 读取字节流。
				PackedByteArray gs_data = FileAccess::get_file_as_bytes(path);
				if (gs_data.size() > 0) {
					MonoLogger::log(vformat("Reading GodotSharp.dll into buffer: %d bytes", gs_data.size()));
					MonoImageOpenStatus status = MONO_IMAGE_OK;
					MonoImage *gs_image = mono_image_open_from_data(
						(char *)gs_data.ptrw(), (unsigned int)gs_data.size(), 1, &status);
					CharString path_utf8 = path.utf8();
					if (gs_image && status == 0) {
						godotsharp_assembly = mono_assembly_load_from(gs_image, path_utf8.get_data(), &status);
						if (godotsharp_assembly) {
							godotsharp_image = mono_assembly_get_image(godotsharp_assembly);
							if (godotsharp_image) {
								MonoLogger::log("GodotSharp loaded successfully (from buffer)");
								break;
							}
						}
					}
					if (!godotsharp_assembly) {
						MonoLogger::log_error(vformat("mono_image_open_from_data/load_from failed (status=%d), falling back to mono_domain_assembly_open", status));
						godotsharp_assembly = mono_domain_assembly_open(scripts_domain, path_utf8.get_data());
						if (godotsharp_assembly) {
							godotsharp_image = mono_assembly_get_image(godotsharp_assembly);
							if (godotsharp_image) {
								MonoLogger::log("GodotSharp loaded successfully (fallback)");
								break;
							}
						}
					}
				} else {
					MonoLogger::log_error(vformat("Failed to read GodotSharp.dll: %s", path));
				}
#else
				// Desktop (Windows/macOS/Linux):
				//   - 前 3 个候选：exe_dir/GodotSharp.dll、exe_dir/.mono/assemblies/GodotSharp.dll、assemblies_path/GodotSharp.dll
				//     → 都是真实磁盘路径，直接 mono_domain_assembly_open 就行。
				//   - 第 4 个候选：res://.mono/assemblies/GodotSharp.dll
				//     → 这是 Godot Export 把 DLL 打进 .pck 时的路径。mono_domain_assembly_open
				//       只接受真实磁盘路径，它不认识 Godot VFS 的 res://。必须走 FileAccess
				//       读字节 + mono_image_open_from_data + mono_assembly_load_from。
				bool use_buffer = path.begins_with("res://") || path.begins_with("user://");
				if (use_buffer) {
					PackedByteArray gs_data = FileAccess::get_file_as_bytes(path);
					if (gs_data.size() > 0) {
						MonoLogger::log(vformat("Reading GodotSharp.dll from VFS into buffer: %d bytes (%s)", gs_data.size(), path));
						MonoImageOpenStatus status = MONO_IMAGE_OK;
						MonoImage *gs_image = mono_image_open_from_data(
							(char *)gs_data.ptrw(), (unsigned int)gs_data.size(), 1, &status);
						CharString path_utf8 = path.utf8();
						if (gs_image && status == 0) {
							godotsharp_assembly = mono_assembly_load_from(gs_image, path_utf8.get_data(), &status);
							if (godotsharp_assembly) {
								godotsharp_image = mono_assembly_get_image(godotsharp_assembly);
								if (godotsharp_image) {
									MonoLogger::log("GodotSharp loaded successfully (from PCK buffer, desktop)");
									break;
								}
							}
						}
						if (!godotsharp_assembly) {
							MonoLogger::log_warning(vformat("Desktop VFS mono_image_open_from_data/load_from failed (status=%d) for %s", status, path));
						}
					}
				} else {
					CharString path_utf8 = path.utf8();
					godotsharp_assembly = mono_domain_assembly_open(scripts_domain, path_utf8.get_data());
					if (godotsharp_assembly) {
						godotsharp_image = mono_assembly_get_image(godotsharp_assembly);
						if (godotsharp_image) {
							MonoLogger::log("GodotSharp loaded successfully");
							break;
						}
					}
				}
#endif
			}
		}
	}

	if (scripts_domain) {
		// 当前项目名（project.godot 里的 application/config/name），用于过滤
		// "从 editor bin 里带过来的其它项目 DLL" 这种跨项目污染。
		// 如果取不到（例如未加载 project_settings），留空表示不做过滤。
		String current_project_name;
		{
			ProjectSettings *ps = ProjectSettings::get_singleton();
			if (ps) {
				current_project_name = ps->get_setting("application/config/name", String());
				current_project_name = current_project_name.strip_edges();
			}
		}

		Vector<String> search_dirs;
		// ① 项目级：PCK / 源码树内的 res://.mono/assemblies（优先级最高，必须是当前项目自己的）
		search_dirs.push_back(String("res://.mono/assemblies"));
		// ② 项目级：res:// 翻译成物理绝对路径后的 .mono/assemblies
		//   （有些 dev 场景下 DirAccess 对 res:// 只读 PCK、看不到物理新增的 DLL）
		{
			ProjectSettings *ps = ProjectSettings::get_singleton();
			if (ps) {
				String globalized = ps->globalize_path("res://.mono/assemblies");
				if (!globalized.is_empty() && !search_dirs.has(globalized)) {
					search_dirs.push_back(globalized);
				}
			}
		}
		// ③ 用户级：userdata 下的 .mono/assemblies（热更新等使用）
		search_dirs.push_back(OS::get_singleton()->get_user_data_dir().path_join(".mono").path_join("assemblies"));
		// ④ 最后才是 exe_dir/.mono/assemblies（fallback：editor 自己以前残留的 DLL、
		//   或导出模板部署目录；这个目录里的 DLL 如果不是当前项目名，极可能是跨项目污染）

		Vector<String> user_dll_paths;

		{
			Vector<String> with_fallback = search_dirs;
			with_fallback.push_back(assemblies_path);  // ④ 末尾追加

			for (int si = 0; si < with_fallback.size(); si++) {
				const String &search_dir = with_fallback[si];
				String abs_dir;
#if defined(WEB_ENABLED) || defined(ANDROID_ENABLED) || defined(IOS_ENABLED)
				// In WASM/Android/iOS, keep res:// prefix so DirAccess can scan PCK/APK directories.
				// globalize_path strips res:// prefix which breaks PCK/APK directory access.
				if (search_dir.begins_with("res://")) {
					abs_dir = search_dir;
				} else {
					abs_dir = ProjectSettings::get_singleton() ? ProjectSettings::get_singleton()->globalize_path(search_dir) : search_dir;
				}
#else
				// Desktop: keep res:// prefix for PCK entries too (exported .pck is mounted at res://).
				if (search_dir.begins_with("res://")) {
					abs_dir = search_dir;
				} else {
					abs_dir = ProjectSettings::get_singleton() ? ProjectSettings::get_singleton()->globalize_path(search_dir) : search_dir;
				}
#endif
				bool is_exe_dir_fallback = (si == with_fallback.size() - 1);

				MonoLogger::log(vformat("Scanning for user assemblies in: %s", abs_dir));

				if (FileAccess::exists(abs_dir) || DirAccess::exists(abs_dir)) {
					Ref<DirAccess> dir = DirAccess::open(abs_dir);
					if (dir.is_valid()) {
						dir->list_dir_begin();
						String fname = dir->get_next();
						while (!fname.is_empty()) {
							if (!dir->current_is_dir() && fname.ends_with(".dll") && fname != "GodotSharp.dll") {
								String full_path = abs_dir.path_join(fname);
								// 跨项目污染检测（仅当我们明确知道当前项目名时才启用）：
								// ④ exe_dir fallback 目录里的 DLL，如果 base name 不是
								// 当前项目名，那就是 editor 之前编译过别的项目残留的
								// 历史垃圾，继续加载会让 "C# class not found" 排查更难。
								// 这里不阻止加载（可能是 NuGet 依赖 DLL），但给一个醒目 WARNING。
								if (is_exe_dir_fallback && !current_project_name.is_empty()) {
									String base = fname.get_basename();
									// 去掉版本化 Name_<timestamp>.dll 的时间戳后缀
									int us = base.rfind("_");
									String cks = (us > 0) ? base.substr(us + 1) : String();
									bool all_digit = !cks.is_empty();
									for (int ci = 0; all_digit && ci < cks.length(); ci++) {
										if (cks[ci] < '0' || cks[ci] > '9') { all_digit = false; break; }
									}
									String bare_name = (us > 0 && all_digit) ? base.substr(0, us) : base;

									if (bare_name != current_project_name) {
										MonoLogger::log_warning(vformat(
											"Cross-project assembly candidate found in exe_dir fallback: %s"
											" (base '%s' != current project '%s'). "
											"Keeping it only as fallback/nuget dependency; if this is your project DLL, "
											"move/copy it to <project>/.mono/assemblies/ and delete this stale copy.",
											full_path, bare_name, current_project_name));
									}
								}

								if (!user_dll_paths.has(full_path)) {
									user_dll_paths.push_back(full_path);
								}
							}
							fname = dir->get_next();
						}
						dir->list_dir_end();
					} else {
						MonoLogger::log_warning(vformat("DirAccess::open failed for: %s", abs_dir));
					}
				}
			}
		}

		MonoLogger::log(vformat("Found %d candidate user assemblies", user_dll_paths.size()));

		// M12 修复: 版本化 DLL（形如 Name_<timestamp>.dll）的去重 + 排序。
		// 背景: csharp_editor_compile_project 每次产出 Name_<时间戳>.dll，并尽力
		//   删除旧版本；但 Mono 锁定的 DLL 删除失败会留在 .mono/assemblies/ 中。
		//   若不处理，下次启动会同时加载多个版本，mono_class_from_name 在多份
		//   同名类中行为不可预期，热重载后可能命中陈旧类。
		// 策略:
		//   1) 解析每个 DLL 文件名，提取 base name（去掉 _<时间戳> 后缀）和时间戳。
		//   2) 同一 base name 下仅保留时间戳最大的版本（无时间戳视为非版本化，全保留）。
		//   3) 剩余项按时间戳升序排序，配合 get_class/find_class 的反向遍历确保
		//      最新版本最后加载、最先被查询。
		// 注: NuGet 依赖 DLL（无 _<纯数字> 时间戳后缀）不受影响，全部保留。
		{
			struct DllEntry {
				String path;
				String base_name;  // 去掉 _<timestamp> 后的 base
				int64_t timestamp; // -1 = 无时间戳（非版本化）
			};
			Vector<DllEntry> entries;
			for (const String &p : user_dll_paths) {
				DllEntry e;
				e.path = p;
				e.timestamp = -1;
				// 提取文件名中最后一个 '_' 后的数字时间戳
				String base = p.get_file().get_basename(); // 去掉 .dll
				int underscore_pos = base.rfind("_");
				if (underscore_pos >= 0 && underscore_pos < base.length() - 1) {
					String suffix = base.substr(underscore_pos + 1);
					bool all_digits = !suffix.is_empty();
					for (int ci = 0; ci < suffix.length(); ci++) {
						if (suffix[ci] < '0' || suffix[ci] > '9') { all_digits = false; break; }
					}
					if (all_digits) {
						e.timestamp = suffix.to_int();
						e.base_name = base.substr(0, underscore_pos);
					} else {
						e.base_name = base;
					}
				} else {
					e.base_name = base;
				}
				entries.push_back(e);
			}

			// 按 base_name 分组，每组仅保留时间戳最大的版本化 DLL
			// （非版本化 timestamp=-1 的 DLL 彼此独立，不参与去重）
			HashMap<String, int64_t> latest_ts_by_base;
			for (const DllEntry &e : entries) {
				if (e.timestamp < 0) continue; // 非版本化，跳过
				int64_t *existing = latest_ts_by_base.getptr(e.base_name);
				if (!existing || *existing < e.timestamp) {
					latest_ts_by_base[e.base_name] = e.timestamp;
				}
			}

			Vector<DllEntry> deduped;
			int dropped_count = 0;
			for (const DllEntry &e : entries) {
				if (e.timestamp >= 0) {
					int64_t *latest = latest_ts_by_base.getptr(e.base_name);
					if (latest && *latest != e.timestamp) {
						// 旧版本化 DLL，跳过加载（不删除磁盘文件，留给编辑器清理）
						MonoLogger::log(vformat("Skipping stale versioned assembly: %s (kept ts=%d)",
								e.path, *latest));
						dropped_count++;
						continue;
					}
				}
				deduped.push_back(e);
			}
			if (dropped_count > 0) {
				MonoLogger::log(vformat("M12 dedup: dropped %d stale versioned DLL(s)", dropped_count));
			}

			// 升序排序：无时间戳(-1)在前，有时间戳按值升序（最新在最后）
			std::stable_sort(deduped.ptrw(), deduped.ptrw() + deduped.size(),
					[](const DllEntry &a, const DllEntry &b) {
						return a.timestamp < b.timestamp;
					});

			user_dll_paths.clear();
			for (const DllEntry &e : deduped) {
				user_dll_paths.push_back(e.path);
			}
		}

		for (const String &path : user_dll_paths) {
			MonoLogger::log(vformat("Loading user assembly from: %s", path));
			MonoAssembly *assy = nullptr;
			CharString path_utf8 = path.utf8();
			// Desktop 导出时：扫描到的是 res://.mono/assemblies/<name>.dll（在 .pck 里），
			// mono_domain_assembly_open 只接受真实磁盘路径。所以统一：
			//   - 任何 res:// / user:// 开头的路径 → FileAccess 读字节 + mono_image_open_from_data + mono_assembly_load_from + 必要时落盘 fallback
			//   - 其它路径 → 直接 mono_domain_assembly_open
			bool is_vfs_path = path.begins_with("res://") || path.begins_with("user://");
			if (is_vfs_path) {
				PackedByteArray data = FileAccess::get_file_as_bytes(path);
				if (data.size() > 0) {
					MonoLogger::log(vformat("Loading assembly from buffer (VFS): %s (%d bytes)", path, data.size()));
					MonoImageOpenStatus status = MONO_IMAGE_OK;
					MonoImage *img = mono_image_open_from_data(
						(char *)data.ptrw(), (unsigned int)data.size(), 1, &status);
					if (img && status == 0) {
						assy = mono_assembly_load_from(img, path_utf8.get_data(), &status);
					}
					if (!assy) {
						MonoLogger::log_warning(vformat("Buffer load failed (status=%d), falling back to MEMFS copy: %s", status, path));
						// 落盘到 assemblies_path（exe_dir/.mono/assemblies 或 userdata/.mono/assemblies），
						// mono_domain_assembly_open 才能吃到真实路径。
						String memfs_path = assemblies_path.path_join(path.get_file());
						DirAccess::make_dir_recursive_absolute(memfs_path.get_base_dir());
						{
							Ref<FileAccess> f = FileAccess::open(memfs_path, FileAccess::WRITE);
							if (f.is_valid()) {
								f->store_buffer(data.ptr(), data.size());
								f->close();
							}
						}
						CharString memfs_path_utf8 = memfs_path.utf8();
						assy = mono_domain_assembly_open(scripts_domain, memfs_path_utf8.get_data());
					}
				} else {
					MonoLogger::log_warning(vformat("Failed to read user DLL from VFS: %s", path));
				}
			} else {
				assy = mono_domain_assembly_open(scripts_domain, path_utf8.get_data());
			}
			if (assy) {
				MonoImage *img = mono_assembly_get_image(assy);
				if (img) {
					UserAssembly ua;
					ua.assembly = assy;
					ua.image = img;
					ua.name = path.get_file().get_basename();
					user_assemblies.push_back(ua);
					MonoLogger::log(vformat("User assembly loaded: %s", ua.name));
				}
			} else {
				MonoLogger::log_warning(vformat("Failed to load assembly: %s", path));
			}
		}
	}

	initialized = true;

	if (root_domain && scripts_domain) {
#ifndef WEB_ENABLED
		install_synchronization_context();
#else
		MonoLogger::log("Skipping GodotSynchronizationContext.Install() in WASM (triggers mscorlib internal icall signature mismatch)");
#endif
		MonoLogger::log("Mono runtime initialized successfully");
		MonoLogger::log(vformat("Runtime build info: %s", mono_get_runtime_build_info()));
	} else {
		MonoLogger::log("Mono runtime initialized in limited mode (no BCL/assemblies)");
	}
	MonoLogger::log(vformat("User assemblies path: %s", assemblies_path));

	return true;
}

#if (defined(ANDROID_ENABLED) || (!defined(WEB_ENABLED) && !defined(IOS_ENABLED))) && !defined(MONO_STUB)
// 通用的程序集 preload hook（Desktop / macOS / Linux / Android 共享一份实现）。
//
// 为什么 Desktop 也需要这个 hook？
//   - 我们的 C# 导出插件 _deploy_mono_desktop / _deploy_mono_mobile 会把
//     BCL / GodotSharp / 用户 DLL 全部打进 .pck，路径形如：
//         res://mono/lib/mono/4.5/mscorlib.dll
//         res://.mono/assemblies/GodotSharp.dll
//         res://.mono/assemblies/<ProjectName>.dll
//   - 但 mono_set_dirs(mono_lib_dir, mono_etc_dir) 只会让 Mono 在
//     **真实文件系统** 里搜索 fopen() 能打开的路径；.pck 里的 res:// 是
//     Godot 虚拟文件系统，fopen() 根本找不到。
//   - 之前 Desktop 导出能跑全靠"导出插件顺手把 DLL 也复制到了 EXE 旁边的物理副本"，
//     一旦 Custom template 没选对 / 复制分支漏写 / 用户把 EXE 单独拷走，
//     直接就在 mono_jit_init_version() 里因为找不到 mscorlib.dll 崩溃。
//   - 解决方案：给 Desktop 也装一份 preload hook（和 Android 同款）。
//     Mono 在 JIT 初始化加载 mscorlib.dll / 解析任何程序集引用时先回调这个 hook，
//     hook 走 Godot 的 FileAccess 去读 .pck → 只要 add_file 打进了 .pck，就一定能
//     被找到；即使没有物理副本，依然能正常启动。
static MonoAssembly *universal_load_assembly_from_pck(MonoAssemblyName *p_name, char **p_assemblies_path, void *p_user_data) {
	(void)p_assemblies_path;
	(void)p_user_data;

	const char *name = mono_assembly_name_get_name(p_name);
	const char *culture = mono_assembly_name_get_culture(p_name);
	if (!name) return nullptr;

	String assembly_name;
	if (culture && culture[0]) {
		assembly_name += String(culture) + "/";
	}
	assembly_name += String(name);
	if (!assembly_name.ends_with(".dll")) {
		assembly_name += ".dll";
	}

	String arch = Engine::get_singleton() ? Engine::get_singleton()->get_architecture_name() : String();
	Vector<String> candidates;
#ifdef ANDROID_ENABLED
	candidates.push_back("res://.godot/mono/publish/" + arch + "/" + assembly_name);
#endif
	candidates.push_back("res://.mono/assemblies/" + assembly_name);
	candidates.push_back("res://mono/lib/mono/4.5/" + assembly_name);

	for (const String &path : candidates) {
		if (!FileAccess::exists(path)) continue;
		PackedByteArray data = FileAccess::get_file_as_bytes(path);
		if (data.is_empty()) continue;

		MonoImageOpenStatus status = MONO_IMAGE_OK;
		MonoImage *image = mono_image_open_from_data_with_name(
				reinterpret_cast<char *>(data.ptrw()), data.size(),
				/*need_copy*/ true, &status, /*ref_only*/ false,
				assembly_name.utf8().get_data());
		if (status != MONO_IMAGE_OK || !image) continue;

		status = MONO_IMAGE_OK;
		MonoAssembly *assembly = mono_assembly_load_from_full(
				image, assembly_name.utf8().get_data(), &status, /*ref_only*/ false);
		if (status == MONO_IMAGE_OK && assembly) {
#ifdef ANDROID_ENABLED
			MonoLogger::log(vformat("Android preload hook loaded: %s from %s", assembly_name, path));
#else
			MonoLogger::log(vformat("Desktop preload hook loaded: %s from %s", assembly_name, path));
#endif
			return assembly;
		}
	}
	return nullptr;  // 让 Mono 继续走默认查找路径
}
#endif  // (ANDROID || !WEB && !IOS) && !MONO_STUB

#if (defined(ANDROID_ENABLED) || (!defined(WEB_ENABLED) && !defined(IOS_ENABLED))) && !defined(MONO_STUB)
void GDMono::install_universal_assembly_preload_hook() {
	mono_install_assembly_preload_hook(&universal_load_assembly_from_pck, nullptr);
#ifdef ANDROID_ENABLED
	MonoLogger::log("Universal assembly preload hook installed (Android, loads from res://.mono/assemblies/ + res://mono/lib/mono/4.5/)");
#else
	MonoLogger::log("Universal assembly preload hook installed (Desktop, loads from PCK .mono/assemblies + mono/lib/mono/4.5 even when no physical side-by-side copy exists)");
#endif
}
#endif

void GDMono::cleanup() {
	if (!initialized)
		return;

	// 1. 释放所有 GC handle（安全，不涉及线程）
	for (KeyValue<ObjectID, uint32_t> &E : object_gchandles) {
		if (E.value != 0) {
			mono_gchandle_free(E.value);
		}
	}
	object_gchandles.clear();

	if (sync_context_gchandle != 0) {
		mono_gchandle_free(sync_context_gchandle);
		sync_context_gchandle = 0;
	}

	user_assemblies.clear();
	godotsharp_assembly = nullptr;
	godotsharp_image = nullptr;

	// 2. 卸载 scripts domain（DISABLE_APPDOMAINS 模式下 scripts_domain == root_domain，跳过）
	if (scripts_domain) {
#ifndef DISABLE_APPDOMAINS
		mono_domain_set(root_domain, true);
		mono_domain_unload(scripts_domain);
#endif
		scripts_domain = nullptr;
	}

	// 3. JIT cleanup
	// Android 平台跳过 mono_jit_cleanup()：该函数会销毁 Mono 内部 mutex/hash table，
	// 但 Godot 的 VkThread 等非 Mono 线程可能仍在运行，线程退出时触发 Mono 的
	// thread detach 回调，访问已销毁的 mutex 导致
	// "pthread_mutex_lock called on a destroyed mutex" 崩溃。
	// Android 进程退出时 OS 会回收所有资源（包括 mutex/内存），跳过 jit_cleanup
	// 不会造成资源泄漏。桌面/WASM/iOS 仍正常调用以保持干净的关闭语义。
#if !defined(ANDROID_ENABLED)
	if (root_domain) {
		mono_jit_cleanup(root_domain);
		root_domain = nullptr;
	}
#else
	// Android: 不调用 mono_jit_cleanup()，仅清空指针避免悬垂引用
	root_domain = nullptr;
#endif

	initialized = false;
	MonoLogger::log("Mono runtime cleaned up");
}

bool GDMono::load_assembly(const String &p_path, bool p_is_proj_assembly) {
	if (!scripts_domain) {
		MonoLogger::log_error(vformat("Cannot load assembly (scripts domain not initialized): %s", p_path));
		return false;
	}

	MonoAssembly *assembly = nullptr;
	CharString p_path_utf8 = p_path.utf8();
// Android 与 WASM 一样，无法直接 fopen APK 内的 res:// 路径，
// 必须用 FileAccess 读取字节流后通过 mono_image_open_from_data 加载。
// 桌面平台（windows/linux/macos）可直接 fopen，走 mono_domain_assembly_open。
#if defined(WEB_ENABLED) || defined(ANDROID_ENABLED)
	{
		PackedByteArray data = FileAccess::get_file_as_bytes(p_path);
		MonoLogger::log(vformat("Loading assembly from bytes: %s (size=%d)", p_path, data.size()));
		if (data.size() > 0) {
			MonoImageOpenStatus status = MONO_IMAGE_OK;
			MonoImage *img = mono_image_open_from_data(
				(char *)data.ptrw(), (unsigned int)data.size(), 1, &status);
			if (img && status == 0) {
				assembly = mono_assembly_load_from(img, p_path_utf8.get_data(), &status);
				if (!assembly) {
					MonoLogger::log_error(vformat("mono_assembly_load_from failed: status=%d (%s)",
							status, mono_image_strerror(status)));
				}
			} else {
				MonoLogger::log_error(vformat("mono_image_open_from_data failed: status=%d (%s), img=%s",
						status, mono_image_strerror(status), img ? "non-null" : "null"));
			}
		} else {
			MonoLogger::log_error(vformat("FileAccess returned empty data for: %s", p_path));
		}
		if (!assembly) {
			assembly = mono_domain_assembly_open(scripts_domain, p_path_utf8.get_data());
		}
	}
#else
	assembly = mono_domain_assembly_open(scripts_domain, p_path_utf8.get_data());
#endif
	if (!assembly) {
		MonoLogger::log_error(vformat("Failed to load assembly: %s", p_path));
		return false;
	}

	MonoImage *image = mono_assembly_get_image(assembly);
	if (!image) {
		MonoLogger::log_error(vformat("Failed to get image for assembly: %s", p_path));
		return false;
	}

	const char *name = mono_image_get_name(image);
	MonoLogger::log(vformat("Loaded assembly: %s (added to user_assemblies)", name));

	// Add to user_assemblies so find_class can search it.
	UserAssembly ua;
	ua.assembly = assembly;
	ua.image = image;
	ua.name = p_path.get_file().get_basename();
	user_assemblies.push_back(ua);
	loaded_assembly_paths.push_back(p_path);

	return true;
}

bool GDMono::reload_domain() {
	if (!initialized) {
		MonoLogger::log_warning("Cannot reload domain: Mono not initialized");
		return false;
	}

#ifndef DISABLE_APPDOMAINS
	MonoLogger::log("Reloading scripts AppDomain (full hot reload)...");

	// 1. Free all managed object GC handles
	for (KeyValue<ObjectID, uint32_t> &E : object_gchandles) {
		if (E.value != 0) {
			mono_gchandle_free(E.value);
		}
	}
	object_gchandles.clear();

	// 2. Free sync context handle
	if (sync_context_gchandle != 0) {
		mono_gchandle_free(sync_context_gchandle);
		sync_context_gchandle = 0;
	}

	// 3. Clear user assemblies
	user_assemblies.clear();

	// 4. Save loaded assembly paths for reloading
	List<String> saved_paths(loaded_assembly_paths);
	loaded_assembly_paths.clear();

	// 5. Unload scripts domain
	if (scripts_domain && scripts_domain != root_domain) {
		mono_domain_set(root_domain, true);
		mono_domain_unload(scripts_domain);
		scripts_domain = nullptr;
	}

	// 6. Create new scripts domain
	scripts_domain = mono_domain_create_appdomain(const_cast<char *>("GodotScripts"), nullptr);
	if (!scripts_domain) {
		MonoLogger::log_error("Failed to create new scripts domain during reload");
		scripts_domain = root_domain;
		return false;
	}
	mono_domain_set(scripts_domain, true);

	// 7. Reload GodotSharp assembly
	godotsharp_assembly = nullptr;
	godotsharp_image = nullptr;
	if (godotsharp_assembly == nullptr) {
		// Try to re-open GodotSharp from known paths
		Vector<String> search_paths;
		search_paths.push_back(OS::get_singleton()->get_executable_path().get_base_dir().path_join("GodotSharp.dll"));
		search_paths.push_back(assemblies_path.path_join("GodotSharp.dll"));
		for (const String &path : search_paths) {
			if (FileAccess::exists(path)) {
				CharString path_utf8 = path.utf8();
				MonoAssembly *gs_assembly = mono_domain_assembly_open(scripts_domain, path_utf8.get_data());
				if (gs_assembly) {
					godotsharp_assembly = gs_assembly;
					godotsharp_image = mono_assembly_get_image(gs_assembly);
					if (godotsharp_image) {
						MonoLogger::log("GodotSharp reloaded successfully");
						break;
					}
				}
			}
		}
	}

	// 8. Reload user assemblies
	for (const String &path : saved_paths) {
		load_assembly(path, true);
	}

	// 9. Reinstall sync context
	install_synchronization_context();

	MonoLogger::log("Scripts AppDomain reloaded successfully");
	return true;
#else
	// WASM (DISABLE_APPDOMAINS): cannot unload AppDomain, use pseudo-reload.
	// M11 修复: 删除从未被读取的 saved_paths 死代码（caller reload_assembly 会
	// 调用 clear_user_assemblies + load_assembly 重新装载，无需在此构建列表）。
	MonoLogger::log("Pseudo hot reload (WASM mode): clearing assemblies and reloading...");
	clear_user_assemblies();
	loaded_assembly_paths.clear();
	// The caller (csharp_editor_on_script_saved / reload_assembly) will call load_assembly
	return false; // indicates domain reload not used
#endif
}

bool GDMono::reload_assembly(const String &p_path) {
	// Try full domain reload first (desktop)
	if (reload_domain()) {
		// Domain reload reloaded all assemblies; if the specific path wasn't in the list, load it
		bool found = false;
		for (const String &path : loaded_assembly_paths) {
			if (path == p_path) { found = true; break; }
		}
		if (!found) {
			return load_assembly(p_path, true);
		}
		return true;
	}
	// WASM fallback: just clear and load
	clear_user_assemblies();
	return load_assembly(p_path, true);
}

void GDMono::clear_user_assemblies() {
	user_assemblies.clear();
}

// start_debugger()/stop_debugger() removed: SDB agent must be configured
// before mono_jit_init_version(), so hot-toggling at runtime does not work.
// Use the --mono-debugger=PORT command-line argument instead, or call
// CSharpDebugger::configure_before_jit_init() / install_exception_hook()
// directly (see csharp_debugger.h).

bool GDMono::is_debugger_active() const {
	// Delegate to CSharpDebugger. Returns true if the SDB agent was requested
	// via --mono-debugger (regardless of whether an IDE is currently attached).
	return CSharpDebugger::is_requested();
}

MonoClass *GDMono::get_class(const String &p_namespace, const String &p_class_name) {
	if (!root_domain) {
		return nullptr;
	}

	MonoClass *klass = nullptr;

	// S4 修复: p_namespace.utf8() 返回临时 CharString，直接存 .get_data() 会悬垂；
	// 统一先保存到局部变量延长生命周期（整个函数内有效）。
	CharString ns_utf8 = p_namespace.utf8();
	CharString class_utf8 = p_class_name.utf8();

	// M12 修复: 反向遍历 user_assemblies，最新加载的程序集优先命中，
	// 避免版本化 DLL 并存时找到旧版类。
	for (int i = user_assemblies.size() - 1; i >= 0; i--) {
		const UserAssembly &ua = user_assemblies[i];
		if (ua.image) {
			klass = mono_class_from_name(ua.image,
					ns_utf8.get_data(),
					class_utf8.get_data());
			if (klass) return klass;
		}
	}

	if (godotsharp_image) {
		klass = mono_class_from_name(godotsharp_image,
				ns_utf8.get_data(),
				class_utf8.get_data());
		if (klass) return klass;
	}

	const char *namespaces[] = { ns_utf8.get_data(), "Godot", "System", nullptr };
	for (int i = 0; namespaces[i] != nullptr; i++) {
		klass = mono_class_from_name(mono_get_corlib(), namespaces[i], class_utf8.get_data());
		if (klass) return klass;
	}

	if (p_namespace.is_empty()) {
		klass = mono_class_from_name(mono_get_corlib(), "", class_utf8.get_data());
	}

	return klass;
}

MonoClass *GDMono::find_class(const String &p_class_name) {
	if (!root_domain) {
		return nullptr;
	}

	MonoClass *klass = nullptr;

	// Try common namespaces first
	const char *namespaces_to_try[] = { "", "Godot", nullptr };
	for (int i = 0; namespaces_to_try[i] != nullptr; i++) {
		klass = get_class(namespaces_to_try[i], p_class_name);
		if (klass) return klass;
	}

	// Try project name as namespace (sanitized)
	ProjectSettings *ps = ProjectSettings::get_singleton();
	if (ps) {
		String project_name = ps->get_setting("application/config/name", String());
		if (!project_name.is_empty()) {
			// Sanitize: remove non-alphanumeric, capitalize each word
			String sanitized;
			bool capitalize_next = true;
			for (int i = 0; i < project_name.length(); i++) {
				char32_t c = project_name[i];
				if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
					if (capitalize_next && c >= 'a' && c <= 'z') {
						sanitized += String::chr(c - 32);
					} else {
						sanitized += String::chr(c);
					}
					capitalize_next = false;
				} else {
					capitalize_next = true;
				}
			}
			if (!sanitized.is_empty()) {
				klass = get_class(sanitized, p_class_name);
				if (klass) return klass;
			}
		}

		// Try custom assembly name setting
		String assembly_name = ps->get_setting("mono/project/assembly_name", String());
		if (!assembly_name.is_empty()) {
			klass = get_class(assembly_name, p_class_name);
			if (klass) return klass;
		}
	}

	// Last resort: search all user assemblies by iterating images
	// M12 修复: 反向遍历，最新版本优先
	CharString p_class_name_utf8 = p_class_name.utf8();
	for (int idx = user_assemblies.size() - 1; idx >= 0; idx--) {
		const UserAssembly &ua = user_assemblies[idx];
		if (!ua.image) continue;
		const void *table = mono_image_get_table_info(ua.image, MONO_TABLE_TYPEDEF);
		if (!table) continue;
		int rows = mono_table_info_get_rows(table);
		for (int i = 0; i < rows; i++) {
			MonoClass *cls = mono_class_get(ua.image, (i + 1) | (MONO_TABLE_TYPEDEF << 24));
			if (cls) {
				const char *name = mono_class_get_name(cls);
				if (name && strcmp(name, p_class_name_utf8.get_data()) == 0) {
					return cls;
				}
			}
		}
	}

	return nullptr;
}

MonoMethod *GDMono::get_method(MonoClass *p_class, const String &p_name, int p_param_count) {
	CharString p_name_utf8 = p_name.utf8();
	return mono_class_get_method_from_name(p_class, p_name_utf8.get_data(), p_param_count);
}
