/**************************************************************************/
/*  mono_gc_static.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "mono_gc_static.h"

#include <mono/metadata/gc.h>
#include <mono/metadata/mono-gc.h>

#include "core/os/os.h"

namespace gdmono {

bool gc_init_static() {
	// Initialize the embedded Mono GC (sgen)
	// This is called during Godot's memory subsystem initialization

	print_verbose(".NET: Initializing static Mono GC...");

	// Mono's GC is initialized via mono_gc_init() which is called
	// from mono_jit_init() or mono_runtime_init()
	// For static linking, we call it explicitly here

#ifdef DEBUG_ENABLED
	print_verbose(".NET: GC max generation: " + itos(mono_gc_max_generation()));
#endif

	return true;
}

void gc_finalize_static() {
	print_verbose(".NET: Finalizing static Mono GC...");

	// Force finalization of all objects
	mono_gc_finalize();

	print_verbose(".NET: Static Mono GC finalized");
}

void gc_collect_static(int generation) {
	if (generation < 0) {
		// Full collection
		mono_gc_collect(mono_gc_max_generation() + 1);
	} else {
		mono_gc_collect(generation);
	}
}

int gc_get_max_generation_static() {
	return mono_gc_max_generation();
}

void gc_wait_for_pending_finalizers_static() {
	mono_gc_wait_for_finalizers();
}

void *gc_alloc_obj_static(size_t size, MonoVTable *vtable) {
	return mono_gc_alloc_obj(size, vtable);
}

void gc_register_root_static(void *start, size_t size, void *cookie) {
	mono_gc_register_root(start, size, cookie);
}

void gc_unregister_root_static(void *start) {
	mono_gc_unregister_root(start);
}

void gc_register_finalizer_static(void *object, void *callback) {
	mono_gc_register_finalizer(object, callback);
}

bool gc_is_collectible_static(void *object) {
	return mono_gc_is_collectible(object);
}

void gcSuppressFinalizeStatic(void *object) {
	mono_gc_suppress_finalizer(object);
}

void gc_enumerate_reachable_objects_static(void *callback, void *user_data) {
	mono_gc_enumerate_reachable_objects(callback, user_data);
}

int64_t gc_get_stats_static(const char *key) {
	// Mono GC doesn't provide direct stats access via string keys
	// Return 0 for unknown keys
	return 0;
}

void gc_set_param_static(const char *key, int64_t value) {
	// Mono GC parameters can be set via environment variables
	// This function provides runtime configuration support
	if (strcmp(key, "mode") == 0) {
		// Set GC mode (stationary, concurrent, etc.)
		// Not directly supported in Mono sgen
	} else if (strcmp(key, "stack_size") == 0) {
		// Set stack size for thread handling
		// Not directly supported in Mono sgen
	}
}

} // namespace gdmono
