#ifdef WEB_ENABLED

#include "mono_platform_web.h"
#include "../utils/mono_logger.h"
#include "core/os/os.h"

#include <emscripten.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

extern "C" {

// Runtime support functions
int mono_wasm_enable_gc = 1;

// Timer function - signature must match Mono libmini import:
//   void mono_set_timeout(int timeout, int id);
// Mono's libmini.a declares this as an external JS import (param i32 i32).
// We provide this stub so the linker resolves mono_set_timeout locally
// rather than as an unresolved import.
void mono_set_timeout(int timeout, int id) {
    (void)timeout;
    (void)id;
}

// Override Mono's mono_wasm_set_timeout to avoid the buggy generated code
// path that leaves "found 1 elements on stack for fallthru". Our version
// simply consumes both arguments and returns, without calling mono_set_timeout.
void mono_wasm_set_timeout(int timeout, int id) {
    (void)timeout;
    (void)id;
}

// Background execution
void schedule_background_exec() {}

// Debugger helper stubs - signatures must match Mono libmini library:
//   mono_wasm_add_frame(int il_offset, int method_token, const char *assembly_name)
//   mono_wasm_add_properties_var(const char*)
//   mono_wasm_add_array_item(int)
void mono_wasm_add_array_item(int item) {}
void mono_wasm_add_properties_var(const char *name) {}
void mono_wasm_add_frame(int il_offset, int method_token, const char *assembly_name) {}

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
	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	String assembly_path = exe_dir.path_join(".mono/assemblies").path_join(p_name);
	return assembly_path;
}

void* wasm_malloc(int p_size) {
	return malloc(p_size);
}

void wasm_free(void *p_ptr) {
	free(p_ptr);
}

void* mono_wasm_malloc(size_t size) {
	return malloc(size);
}

void* mono_wasm_calloc(size_t count, size_t size) {
	return calloc(count, size);
}

void mono_wasm_free(void *ptr) {
	free(ptr);
}

void* mono_wasm_realloc(void *ptr, size_t size) {
	return realloc(ptr, size);
}

void register_bcallbacks() {
#ifndef MONO_STUB
#endif
}

}

#endif
