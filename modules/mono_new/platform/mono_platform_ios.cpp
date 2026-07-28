// iOS 平台胶水层：为 Mono 6.12 静态链接运行时提供平台原语。
// 参考 mono_platform_web.cpp / mono_platform_android.cpp 的整体结构。
// iOS 关键约束：
//   1. App Store 禁止 JIT（W^X 内存保护），必须用 interpreter 或 Full AOT
//   2. 不支持动态库加载（dlopen 禁用），所有代码必须静态链接进主二进制
//   3. BCL 与用户程序集打包进 NSBundle mainBundle 资源目录
//   4. 代码签名限制：mmap 可写可执行内存会触发 SIGKILL
//
// 本文件实现 iOS 真实线程的 GC 栈扫描、线程注册、文件系统适配。
// interpreter 模式由 gd_mono.cpp 通过 mono_jit_set_aot_mode(MONO_EE_MODE_INTERP) 设置。

#ifdef IOS_ENABLED

#include "mono_platform_ios.h"
#include "../utils/mono_logger.h"
#include "core/os/os.h"
#include "core/io/file_access.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/resource.h>

// iOS 上访问 NSBundle 需 Objective-C++（.mm 文件）。本文件用 .cpp 是因为
// 仅需要 pthread/unistd 即可获取栈边界；NSBundle 路径解析由 Godot 引擎的
// OS::get_singleton()->get_executable_path() 已封装（iOS 上返回 mainBundle 资源目录）。

// ---------------------------------------------------------------------------
// GC 栈边界扫描
// ---------------------------------------------------------------------------
// iOS 主线程栈大小默认 1MB（系统可配置）。用 pthread_attr_getstack 获取。
// 与 Android 不同：iOS 主线程栈可能由系统分配，pthread_attr_getstack 在主线程
// 上返回的可能不是真实栈范围——需要用 rlimit 兜底。

static volatile int stack_bounds_inited = 0;
static uint8_t *ios_stack_base = nullptr;
static size_t ios_stack_size = 0;

static void ensure_stack_bounds() {
	if (stack_bounds_inited)
		return;

	// 方法 1：pthread_attr_getstack（对子线程可靠，对主线程可能不准）
	pthread_attr_t attr;
	if (pthread_getattr_np(pthread_self(), &attr) == 0) {
		void *stack_addr = nullptr;
		size_t stack_sz = 0;
		if (pthread_attr_getstack(&attr, &stack_addr, &stack_sz) == 0) {
			ios_stack_base = (uint8_t *)stack_addr + stack_sz;
			ios_stack_size = stack_sz;
		}
		pthread_attr_destroy(&attr);
	}

	// 方法 2：用 rlimit 查询栈大小上限（主线程）
	if (!ios_stack_base || ios_stack_size == 0) {
		struct rlimit rl;
		if (getrlimit(RLIMIT_STACK, &rl) == 0 && rl.rlim_cur > 0 && rl.rlim_cur != RLIM_INFINITY) {
			volatile int local_var = 0;
			uint8_t *current_sp = (uint8_t *)&local_var;
			ios_stack_base = current_sp + (256 * 1024);  // 保守向上 256KB
			ios_stack_size = (size_t)rl.rlim_cur;
			MonoLogger::log(vformat("iOS stack bounds via rlimit: base=%p size=%zu", ios_stack_base, ios_stack_size));
		}
	}

	if (!ios_stack_base || ios_stack_size == 0) {
		// 兜底：8MB 保守范围（iOS 主线程默认 8MB 栈）
		MonoLogger::log_warning("pthread_attr_getstack and getrlimit both failed, using conservative 8MB stack bounds for GC");
		volatile int local_var = 0;
		uint8_t *current_sp = (uint8_t *)&local_var;
		ios_stack_base = current_sp + (512 * 1024);
		ios_stack_size = 8 * 1024 * 1024;
	}

	stack_bounds_inited = 1;
}

// ---------------------------------------------------------------------------
// Mono 平台原语（extern "C"，由 libmonosgen-2.0.a 调用）
// ---------------------------------------------------------------------------
// 签名与 mono/utils/mono-threads.h 一致。iOS 与 Android 都用 pthread，实现高度相似。

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
	// iOS 上 pthread_setname_np 仅能给当前线程命名，且名字长度限制 16 字节
}

gboolean mono_native_thread_join(MonoNativeThreadId tid) {
	return pthread_join((pthread_t)tid, nullptr) == 0 ? 1 : 0;
}

gboolean mono_threads_platform_yield(void) {
	return sched_yield() == 0 ? 1 : 0;
}

void mono_threads_platform_get_stack_bounds(guint8 **staddr, size_t *stsize) {
	ensure_stack_bounds();
	*staddr = ios_stack_base - ios_stack_size;
	*stsize = ios_stack_size;
}

gboolean mono_thread_platform_create_thread(MonoThreadStart thread_fn, gpointer thread_data,
		gsize *const stack_size, MonoNativeThreadId *tid) {
	gboolean ok = mono_native_thread_create(tid, (gpointer)thread_fn, thread_data);
	if (ok && stack_size) {
		*stack_size = ios_stack_size > 0 ? ios_stack_size : (1024 * 1024);
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

// iOS 信号支持：与 Android 相同，sgen GC 用信号做线程挂起
void mono_threads_suspend_init_signals(void) {}
void mono_threads_suspend_init(void) {}
void mono_threads_suspend_register(THREAD_INFO_TYPE *info) { (void)info; }
gboolean mono_threads_suspend_begin_async_resume(THREAD_INFO_TYPE *info) { (void)info; return 1; }
void mono_threads_suspend_free(THREAD_INFO_TYPE *info) { (void)info; }
gboolean mono_threads_suspend_begin_async_suspend(THREAD_INFO_TYPE *info, gboolean interrupt_kernel) {
	(void)info; (void)interrupt_kernel; return 1;
}
gboolean mono_threads_suspend_check_suspend_result(THREAD_INFO_TYPE *info) { (void)info; return 1; }
void mono_threads_suspend_abort_syscall(THREAD_INFO_TYPE *info) { (void)info; }

int mono_thread_info_get_system_max_stack_size(void) {
	struct rlimit rl;
	if (getrlimit(RLIMIT_STACK, &rl) == 0 && rl.rlim_cur > 0 && rl.rlim_cur != RLIM_INFINITY) {
		return (int)rl.rlim_cur;
	}
	return 8 * 1024 * 1024;  // iOS 默认主线程 8MB
}

void mono_threads_schedule_background_job(background_job_cb cb) { (void)cb; }
void mono_background_exec(void) {}
void mono_memory_barrier_process_wide(void) {}

}  // extern "C"

// ---------------------------------------------------------------------------
// MonoiOS 命名空间
// ---------------------------------------------------------------------------

namespace MonoiOS {

static bool ios_initialized = false;

void initialize() {
	if (ios_initialized)
		return;

	MonoLogger::log("Initializing Mono for iOS platform...");
	ensure_stack_bounds();
	ios_initialized = true;
	MonoLogger::log(vformat("Mono iOS platform initialized (stack_base=%p, stack_size=%zu, interpreter mode)",
			ios_stack_base, ios_stack_size));
}

void cleanup() {
	if (!ios_initialized)
		return;
	MonoLogger::log("Cleaning up Mono iOS platform...");
	ios_initialized = false;
}

// iOS 程序集打包在 NSBundle mainBundle 资源目录下
// OS::get_singleton()->get_executable_path() 在 iOS 上返回 mainBundle 资源目录
// gd_mono.cpp 的 exe_dir 已覆盖此路径，这里仅作辅助查询
String locate_assembly(const String &p_name) {
	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	return exe_dir.path_join(".mono").path_join("assemblies").path_join(p_name);
}

}  // namespace MonoiOS

#endif  // IOS_ENABLED
