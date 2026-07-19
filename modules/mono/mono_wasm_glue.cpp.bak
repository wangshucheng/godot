// Stubs for Mono WASM runtime JS-side functions.
// These functions are normally provided by library_mono.js in the Mono WASM SDK.
// Since we use our own Godot-based initialization without the Mono JS driver,
// we provide minimal C implementations here.
//
// This file is only compiled for the web platform.

#include <emscripten.h>
#include <cstdint>

extern "C" {

// mono_wasm_enable_gc is a data symbol (flag), not a function.
// The Mono SGEN GC checks this flag to enable WASM-specific GC behavior.
int mono_wasm_enable_gc = 1;

// --- longjmp bridge ---
// Mono libraries reference __wasm_longjmp (compiled with SUPPORT_LONGJMP=wasm),
// but we link with SUPPORT_LONGJMP=emscripten which provides emscripten_longjmp.
// Without this bridge, any longjmp call in Mono crashes with a WASM trap.
// emscripten_longjmp is not declared in headers, so we declare it ourselves.
extern void emscripten_longjmp(void *env, int val);
void __wasm_longjmp(void *env, int val) {
	emscripten_longjmp(env, val);
}

// --- Timer support ---
// mono_set_timeout is called by Mono to schedule a timer.
// In the JS driver, this uses window.setTimeout.
// We use emscripten's async call to schedule the callback.
// The actual callback mono_set_timeout_exec(id) is defined in libmini.a.
extern void mono_set_timeout_exec(int id);

static void mono_timeout_callback(void *arg) {
	mono_set_timeout_exec((int)(intptr_t)arg);
}

void mono_set_timeout(int timeout, int id) {
	emscripten_async_call(mono_timeout_callback, (void *)(intptr_t)id, timeout);
}

// --- Background job scheduling ---
// schedule_background_exec is called to schedule background job processing
// (GC finalization, etc.). mono_background_exec is defined in mono-threads-wasm.c.
extern void mono_background_exec(void);

static void background_exec_wrapper(void *arg) {
	mono_background_exec();
}

void schedule_background_exec(void) {
	emscripten_async_call(background_exec_wrapper, NULL, 0);
}

// --- Debugger stubs (no-op, debugger not used in interpreter mode) ---
// Signatures must match the extern declarations in mini-wasm-debugger.c.

void mono_wasm_fire_bp(void) {
	// No-op: debugger not used
}

void mono_wasm_add_frame(int il_offset, int method_token, const char *assembly_name) {
	// No-op: debugger not used
}

void mono_wasm_add_bool_var(signed char value) {
	// No-op: debugger not used
}

void mono_wasm_add_number_var(double value) {
	// No-op: debugger not used
}

void mono_wasm_add_string_var(const char *value) {
	// No-op: debugger not used
}

void mono_wasm_add_obj_var(const char *className, unsigned long long objectId) {
	// No-op: debugger not used
}

void mono_wasm_add_array_var(const char *className, unsigned long long objectId) {
	// No-op: debugger not used
}

void mono_wasm_add_properties_var(const char *name) {
	// No-op: debugger not used
}

void mono_wasm_add_array_item(int position) {
	// No-op: debugger not used
}

} // extern "C"
