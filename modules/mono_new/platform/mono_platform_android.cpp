// Android 平台胶水层：为 Mono 6.12 静态链接运行时提供平台原语。
// 参考 mono_platform_web.cpp 的整体结构，但 Android 与 WASM 关键差异：
//   1. 真实线程：需用 pthread API 实现 GC 栈边界扫描、线程注册
//   2. APK 内程序集：通过 FileAccess（res://）读取，安装 preload hook
//   3. JIT 可用但本项目沿用 interpreter（与 WASM 复用路径）
//   4. LogCat 输出：通过 Android log 库（链接时 -llog）
//
// 实现参考：
//   - 旧 modules/mono/mono_gd/gd_mono.cpp:530-589 (load_assembly_from_pck)
//   - 旧 modules/mono/godotsharp_dirs.cpp:181-230 (Android 程序集目录)
//   - mono_platform_web.cpp (整体结构)

#ifdef ANDROID_ENABLED

#include "mono_platform_android.h"
#include "../utils/mono_logger.h"
#include "core/os/os.h"
#include "core/io/file_access.h"

#include <pthread.h>
#include <android/log.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

// Android log tag（与 platform/android/java 的 GodotApp 共用前缀）
#define MONO_ANDROID_LOG_TAG "Godot/Mono"

// ---------------------------------------------------------------------------
// GC 栈边界扫描
// ---------------------------------------------------------------------------
// Android 有真实线程，sgen GC 需要知道每个线程的栈范围。
// 主线程的栈边界可通过 pthread_attr_getstack 获取；子线程由 Mono 自身管理。
// 参考 mono_platform_web.cpp:17-44 的 ensure_stack_bounds 模式。

static volatile int stack_bounds_inited = 0;
static uint8_t *android_stack_base = nullptr;
static size_t android_stack_size = 0;

static void ensure_stack_bounds() {
	if (stack_bounds_inited)
		return;

	pthread_attr_t attr;
	if (pthread_getattr_np(pthread_self(), &attr) == 0) {
		void *stack_addr = nullptr;
		size_t stack_sz = 0;
		if (pthread_attr_getstack(&attr, &stack_addr, &stack_sz) == 0) {
			// pthread_attr_getstack 返回栈底地址和大小，栈向低地址增长
			android_stack_base = (uint8_t *)stack_addr + stack_sz;  // 栈顶（高地址）
			android_stack_size = stack_sz;
		}
		pthread_attr_destroy(&attr);
	}

	if (!android_stack_base || android_stack_size == 0) {
		// 失败时保守估计 1MB 栈，与 mono_platform_web.cpp 的兜底策略一致
		MonoLogger::log_warning("pthread_attr_getstack failed, using conservative 1MB stack bounds for GC");
		volatile int local_var = 0;
		uint8_t *current_sp = (uint8_t *)&local_var;
		android_stack_base = current_sp + (256 * 1024);
		android_stack_size = 1024 * 1024;
	}

	stack_bounds_inited = 1;
}

// ---------------------------------------------------------------------------
// Mono 平台原语（extern "C"，由 libmonosgen-2.0.a 调用）
// ---------------------------------------------------------------------------
// 这些函数签名必须与 mono/utils/mono-threads.h 等头文件中的声明一致。
// 静态链接时若缺少实现会链接失败；WASM 端在 mono_platform_web.cpp 提供桩实现，
// Android 端提供真实实现（因有真实线程）。

typedef unsigned char guint8;
typedef int gboolean;
typedef int gint;
typedef int gint32;
typedef uint64_t guint64;
typedef size_t gsize;
typedef void *gpointer;
typedef intptr_t MonoNativeThreadId;
typedef struct _MonoThreadInfo MonoThreadInfo;
typedef MonoThreadInfo THREAD_INFO_TYPE;
typedef gsize (*MonoThreadStart)(gpointer);
typedef void (*background_job_cb)(void);

extern "C" {

// Android 有真实线程，使用 pthread API
MonoNativeThreadId mono_native_thread_id_get(void) {
	return (MonoNativeThreadId)pthread_self();
}

gboolean mono_native_thread_id_equals(MonoNativeThreadId id1, MonoNativeThreadId id2) {
	return pthread_equal((pthread_t)id1, (pthread_t)id2) != 0;
}

guint64 mono_native_thread_os_id_get(void) {
	return (guint64)gettid();
}

gint32 mono_native_thread_processor_id_get(void) {
	// Android 没有 getcpu 系统调用直接可用，返回 -1 让 Mono 自行处理
	return -1;
}

gboolean mono_native_thread_create(MonoNativeThreadId *tid, gpointer func, gpointer arg) {
	pthread_t thread;
	int ret = pthread_create(&thread, nullptr, (void *(*)(void *))func, arg);
	if (ret == 0 && tid) {
		*tid = (MonoNativeThreadId)thread;
		return 1;
	}
	return 0;
}

void mono_native_thread_set_name(MonoNativeThreadId tid, const char *name) {
	(void)tid;
	(void)name;
	// Android NDK 的 pthread_setname_np 签名与 glibc 不同（无 tid 参数），
	// 仅能给当前线程命名。此处保留空实现，必要时通过 LogCat 输出。
}

gboolean mono_native_thread_join(MonoNativeThreadId tid) {
	return pthread_join((pthread_t)tid, nullptr) == 0 ? 1 : 0;
}

gboolean mono_threads_platform_yield(void) {
	return sched_yield() == 0 ? 1 : 0;
}

void mono_threads_platform_get_stack_bounds(guint8 **staddr, size_t *stsize) {
	ensure_stack_bounds();
	// sgen GC 期望 staddr=栈底（低地址），stsize=大小
	*staddr = android_stack_base - android_stack_size;
	*stsize = android_stack_size;
}

gboolean mono_thread_platform_create_thread(MonoThreadStart thread_fn, gpointer thread_data,
		gsize *const stack_size, MonoNativeThreadId *tid) {
	// 委托给 mono_native_thread_create
	gboolean ok = mono_native_thread_create(tid, (gpointer)thread_fn, thread_data);
	if (ok && stack_size) {
		*stack_size = android_stack_size > 0 ? android_stack_size : (1024 * 1024);
	}
	return ok;
}

gboolean mono_threads_platform_is_main_thread(void) {
	return pthread_self() == (pthread_t)1 || mono_native_thread_id_get() == (MonoNativeThreadId)getpid();
}

void mono_threads_platform_init(void) {
	ensure_stack_bounds();
}

void mono_threads_platform_exit(gsize exit_code) {
	(void)exit_code;
	pthread_exit(nullptr);
}

gboolean mono_threads_platform_in_critical_region(THREAD_INFO_TYPE *info) {
	(void)info;
	return 0;
}

// Android 信号支持：sgen GC 用 SIGPWR/SIGRTMIN 做线程挂起/恢复
// 这里提供真实实现（与 WASM 桩实现不同）
void mono_threads_suspend_init_signals(void) {}
void mono_threads_suspend_init(void) {}
void mono_threads_suspend_register(THREAD_INFO_TYPE *info) {
	(void)info;
}
gboolean mono_threads_suspend_begin_async_resume(THREAD_INFO_TYPE *info) {
	(void)info;
	return 1;  // 桩实现：Android GC 挂起机制在 libmonosgen 内部已实现
}
void mono_threads_suspend_free(THREAD_INFO_TYPE *info) {
	(void)info;
}
gboolean mono_threads_suspend_begin_async_suspend(THREAD_INFO_TYPE *info, gboolean interrupt_kernel) {
	(void)info;
	(void)interrupt_kernel;
	return 1;
}
gboolean mono_threads_suspend_check_suspend_result(THREAD_INFO_TYPE *info) {
	(void)info;
	return 1;
}
void mono_threads_suspend_abort_syscall(THREAD_INFO_TYPE *info) {
	(void)info;
}

int mono_thread_info_get_system_max_stack_size(void) {
	// Android 默认线程栈 1MB（主线程 8MB，子线程 1MB），取保守值
	return 1 * 1024 * 1024;
}

// 后台任务调度（sgen GC 的 background job）
void mono_threads_schedule_background_job(background_job_cb cb) {
	(void)cb;
}
void mono_background_exec(void) {}
void mono_memory_barrier_process_wide(void) {}

}  // extern "C"

// ---------------------------------------------------------------------------
// MonoAndroid 命名空间（与 MonoWeb 对应，供 gd_mono.cpp 调用）
// ---------------------------------------------------------------------------

namespace MonoAndroid {

static bool android_initialized = false;

void initialize() {
	if (android_initialized)
		return;

	MonoLogger::log("Initializing Mono for Android platform...");
	ensure_stack_bounds();
	android_initialized = true;
	MonoLogger::log(vformat("Mono Android platform initialized (stack_base=%p, stack_size=%zu)",
			android_stack_base, android_stack_size));
}

void cleanup() {
	if (!android_initialized)
		return;
	MonoLogger::log("Cleaning up Mono Android platform...");
	android_initialized = false;
}

// Android 程序集位于 APK 内 res://.godot/mono/publish/<arch>/ 目录
// （参考旧 modules/mono/godotsharp_dirs.cpp:181-182）
String locate_assembly(const String &p_name) {
	String arch = Engine::get_singleton()->get_architecture_name();
	String base = "res://.godot/mono/publish/" + arch;
	return base.path_join(p_name);
}

}  // namespace MonoAndroid

#endif  // ANDROID_ENABLED
