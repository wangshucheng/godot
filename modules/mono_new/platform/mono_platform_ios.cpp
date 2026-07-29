// iOS 平台胶水层：为 Mono 6.12 静态链接运行时提供平台原语。
//
// 注意：Mono 6.12 在 iOS 上是 POSIX 平台，使用 mono/utils/mono-threads-posix.c
// 的实现，已编译进 libmonosgen-2.0.a。本文件**只**提供静态库中缺失的少量符号，
// 不能重复实现 libmonosgen-2.0.a 已有的函数（如 mono_threads_suspend_*、
// mono_native_thread_*、mono_threads_platform_get_stack_bounds 等），否则链接时
// 报 duplicate symbol 错误。
//
// iOS 关键约束：
//   1. App Store 禁止 JIT（W^X 内存保护），必须用 interpreter 或 Full AOT
//   2. 不支持动态库加载（dlopen 禁用），所有代码必须静态链接进主二进制
//   3. BCL 与用户程序集打包进 NSBundle mainBundle 资源目录
//   4. 代码签名限制：mmap 可写可执行内存会触发 SIGKILL
//
// interpreter 模式由 gd_mono.cpp 通过 mono_jit_set_aot_mode(MONO_EE_MODE_INTERP) 设置。

#ifdef IOS_ENABLED

#include "mono_platform_ios.h"
#include "../utils/mono_logger.h"
#include "core/os/os.h"
#include "core/io/file_access.h"
#include "core/string/ustring.h"
#include "core/variant/variant.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sched.h>

// ---------------------------------------------------------------------------
// 仅提供 libmonosgen-2.0.a 中缺失的少量符号
// ---------------------------------------------------------------------------
// 经 llvm-nm 核对（libmonosgen-2.0.a），下列函数在静态库中**未定义**，
// 必须由平台胶水层提供；其余 mono_threads_*/mono_native_thread_* 均已在
// mono-threads-posix.c 中实现，不可重复定义。

typedef int gboolean;
typedef size_t gsize;
typedef void (*background_job_cb)(void);

extern "C" {

// 主线程判断：Mono 静态库未提供，需平台实现
// iOS 同样：主线程的 tid == pid
gboolean mono_threads_platform_is_main_thread(void) {
	return gettid() == getpid();
}

// 后台任务调度：sgen GC 的 background job，静态库留作平台钩子
void mono_threads_schedule_background_job(background_job_cb cb) {
	if (cb) {
		cb();
	}
}

// 内存屏障：静态库未提供 process-wide 实现
void mono_background_exec(void) {}

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
	ios_initialized = true;
	MonoLogger::log("Mono iOS platform initialized (POSIX threads, sgen GC, interpreter mode)");
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
