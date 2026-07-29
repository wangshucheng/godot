// Android 平台胶水层：为 Mono 6.12 静态链接运行时提供平台原语。
//
// 注意：Mono 6.12 在 Android 上是 POSIX 平台，使用 mono/utils/mono-threads-posix.c
// 的实现，已编译进 libmonosgen-2.0.a。本文件**只**提供静态库中缺失的少量符号，
// 不能重复实现 libmonosgen-2.0.a 已有的函数（如 mono_threads_suspend_*、
// mono_native_thread_*、mono_threads_platform_get_stack_bounds 等），否则链接时
// 报 duplicate symbol 错误。
//
// 实现参考：
//   - mono/utils/mono-threads-posix.c（POSIX 平台线程原语）
//   - mono/utils/mono-threads-android.c（Android 专属信号处理）
//   - mono_platform_web.cpp（项目内 WASM 平台胶水层结构）

#ifdef ANDROID_ENABLED

#include "mono_platform_android.h"
#include "../utils/mono_logger.h"
#include "core/os/os.h"
#include "core/io/file_access.h"
#include "core/string/ustring.h"
#include "core/variant/variant.h"
#include "core/config/engine.h"

#include <pthread.h>
#include <android/log.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sched.h>
#include <stdio.h>
#include <langinfo.h>

// Android log tag（与 platform/android/java 的 GodotApp 共用前缀）
#define MONO_ANDROID_LOG_TAG "Godot/Mono"

// ---------------------------------------------------------------------------
// 仅提供 libmonosgen-2.0.a 中缺失的少量符号
// ---------------------------------------------------------------------------
// 经 llvm-nm 核对（libmonosgen-2.0.a），下列函数在静态库中**未定义**，
// 必须由平台胶水层提供；其余 mono_threads_*/mono_native_thread_* 均已在
// mono-threads-posix.c / mono-threads-android.c 中实现，不可重复定义。

typedef unsigned char guint8;
typedef int gboolean;
typedef int gint;
typedef int gint32;
typedef uint64_t guint64;
typedef size_t gsize;
typedef void *gpointer;
typedef void (*background_job_cb)(void);

extern "C" {

// 主线程判断：Mono 静态库未提供，需平台实现
// Android Bionic 提供 gettid()，主线程的 tid == pid
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

// ---------------------------------------------------------------------------
// GNU _unlocked 系列函数桩实现
// ---------------------------------------------------------------------------
// Bionic 不提供 fread_unlocked/fwrite_unlocked/fgets_unlocked/fputc_unlocked
// （这些是 GNU 扩展，POSIX 之外）。Mono 的 mono-sha1.c / mono-md5.c 使用了它们。
// 直接转发到带锁版本——性能影响可忽略（SHA1/MD5 不是热路径）。

size_t fread_unlocked(void *ptr, size_t size, size_t nmemb, FILE *stream) {
	return fread(ptr, size, nmemb, stream);
}

size_t fwrite_unlocked(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
	return fwrite(ptr, size, nmemb, stream);
}

char *fgets_unlocked(char *s, int n, FILE *stream) {
	return fgets(s, n, stream);
}

int fputc_unlocked(int c, FILE *stream) {
	return fputc(c, stream);
}

int fputs_unlocked(const char *s, FILE *stream) {
	return fputs(s, stream);
}

int fflush_unlocked(FILE *stream) {
	return fflush(stream);
}

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
	android_initialized = true;
	MonoLogger::log("Mono Android platform initialized (POSIX threads, sgen GC)");
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
