#ifdef WEB_ENABLED

#include "mono_platform_web.h"
#include "../utils/mono_logger.h"
#include "core/os/os.h"

#include <emscripten.h>
#include <emscripten/stack.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static volatile int stack_bounds_inited = 0;
static uint8_t *web_stack_base = nullptr;
static uint8_t *web_stack_end = nullptr;

static void ensure_stack_bounds() {
	if (stack_bounds_inited)
		return;

	volatile int local_var = 0;
	uint8_t *current_sp = (uint8_t *)&local_var;

	uint8_t *ems_base = (uint8_t *)emscripten_stack_get_base();
	uint8_t *ems_end = (uint8_t *)emscripten_stack_get_end();

	if (ems_base && ems_end && ems_base > ems_end) {
		web_stack_base = ems_base;
		web_stack_end = ems_end;
	} else {
		web_stack_base = current_sp + (1 * 1024 * 1024);
		web_stack_end = current_sp - (4 * 1024 * 1024);
	}

	stack_bounds_inited = 1;

	EM_ASM({
		console.log('[Mono-Web] Stack bounds: base(high)=' + $0 + ' end(low)=' + $1 + ' size=' + $2 + ' current_sp=' + $3);
	}, web_stack_base, web_stack_end, (size_t)(web_stack_base - web_stack_end), current_sp);
}

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

int mono_wasm_enable_gc = 1;

void schedule_background_exec(void) {}

void mono_wasm_add_array_item(int item) {
	(void)item;
}
void mono_wasm_add_properties_var(const char *name) {
	(void)name;
}
void mono_wasm_add_frame(int il_offset, int method_token, const char *assembly_name) {
	(void)il_offset;
	(void)method_token;
	(void)assembly_name;
}

int mono_thread_info_get_system_max_stack_size(void) {
	return 1 * 1024 * 1024;
}

void mono_threads_suspend_init_signals(void) {}
void mono_threads_suspend_init(void) {}
void mono_threads_suspend_register(THREAD_INFO_TYPE *info) {
	(void)info;
}
gboolean mono_threads_suspend_begin_async_resume(THREAD_INFO_TYPE *info) {
	(void)info;
	return 1;
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

gboolean mono_native_thread_id_equals(MonoNativeThreadId id1, MonoNativeThreadId id2) {
	return id1 == id2;
}

MonoNativeThreadId mono_native_thread_id_get(void) {
	return (MonoNativeThreadId)1;
}

guint64 mono_native_thread_os_id_get(void) {
	return 1;
}

gint32 mono_native_thread_processor_id_get(void) {
	return -1;
}

gboolean mono_native_thread_create(MonoNativeThreadId *tid, gpointer func, gpointer arg) {
	(void)tid;
	(void)func;
	(void)arg;
	return 0;
}

static const char *web_thread_name = nullptr;

void mono_native_thread_set_name(MonoNativeThreadId tid, const char *name) {
	(void)tid;
	web_thread_name = name;
}

gboolean mono_native_thread_join(MonoNativeThreadId tid) {
	(void)tid;
	return 1;
}

gboolean mono_threads_platform_yield(void) {
	return 1;
}

void mono_threads_platform_get_stack_bounds(guint8 **staddr, size_t *stsize) {
	ensure_stack_bounds();
	*staddr = web_stack_end;
	*stsize = (size_t)(web_stack_base - web_stack_end);
	EM_ASM({
		console.log('[Mono-Web] get_stack_bounds: staddr=' + $0 + ' stsize=' + $1 + ' end=' + $2);
	}, *staddr, *stsize, web_stack_base);
}

gboolean mono_thread_platform_create_thread(MonoThreadStart thread_fn, gpointer thread_data,
		gsize *const stack_size, MonoNativeThreadId *tid) {
	(void)thread_fn;
	(void)thread_data;
	(void)stack_size;
	(void)tid;
	return 0;
}

gboolean mono_threads_platform_is_main_thread(void) {
	return 1;
}

void mono_threads_platform_init(void) {}

void mono_threads_platform_exit(gsize exit_code) {
	(void)exit_code;
}

gboolean mono_threads_platform_in_critical_region(THREAD_INFO_TYPE *info) {
	(void)info;
	return 0;
}

static background_job_cb *bg_jobs = nullptr;
static int bg_job_count = 0;
static int bg_job_cap = 0;

void mono_threads_schedule_background_job(background_job_cb cb) {
	(void)cb;
}

void mono_background_exec(void) {}

void mono_memory_barrier_process_wide(void) {}

}

namespace MonoWeb {

static bool web_initialized = false;

void initialize() {
	if (web_initialized)
		return;

	MonoLogger::log("Initializing Mono for WebAssembly platform...");

	web_initialized = true;
	MonoLogger::log("Mono Web platform initialized");
}

void cleanup() {
	if (!web_initialized)
		return;

	MonoLogger::log("Cleaning up Mono Web platform...");
	web_initialized = false;
}

String locate_assembly(const String &p_name) {
	String exe_dir = "/";
	String assembly_path = exe_dir + String(".mono/assemblies/") + p_name;
	return assembly_path;
}

void *wasm_malloc(int p_size) {
	return malloc(p_size);
}

void wasm_free(void *p_ptr) {
	free(p_ptr);
}

void *mono_wasm_malloc(size_t size) {
	return malloc(size);
}

void *mono_wasm_calloc(size_t count, size_t size) {
	return calloc(count, size);
}

void mono_wasm_free(void *ptr) {
	free(ptr);
}

void *mono_wasm_realloc(void *ptr, size_t size) {
	return realloc(ptr, size);
}

void register_bcallbacks() {
#ifndef MONO_STUB
#endif
}

}

#endif
