#include "mono_platform_web.h"

#ifdef WEB_ENABLED

#include "../utils/mono_logger.h"
#include "core/os/os.h"

#include <emscripten.h>
#include <stdlib.h>
#include <string.h>

typedef void (*MonoWebAssemblyEntry)();

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

static void* mono_wasm_malloc(size_t size) {
	return malloc(size);
}

static void* mono_wasm_calloc(size_t count, size_t size) {
	return calloc(count, size);
}

static void mono_wasm_free(void *ptr) {
	free(ptr);
}

static void* mono_wasm_realloc(void *ptr, size_t size) {
	return realloc(ptr, size);
}

void register_bcallbacks() {
	mono_wasm_register_malloc(mono_wasm_malloc);
	mono_wasm_register_calloc(mono_wasm_calloc);
	mono_wasm_register_free(mono_wasm_free);
	mono_wasm_register_realloc(mono_wasm_realloc);
}

}

#endif
