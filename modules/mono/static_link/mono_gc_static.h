/**************************************************************************/
/*  mono_gc_static.h                                                     */
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
/* permit persons to whom the Software is furnished to do so, subject to */
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
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "../../mono_gc_handle.h"

#include <mono/metadata/gc.h>
#include <mono/metadata/metadata.h>
#include <mono/metadata/object.h>

namespace gdmono {

/**
 * GC/Heap Management Interface for Static Mono Linking
 *
 * This interface provides GC management functions when Mono is statically linked.
 * The static linker includes Mono's GC (sgen) directly into the binary, so we
 * need custom wrappers around the embedded GC functions.
 */

/**
 * Initialize the static Mono GC subsystem.
 * Called during Godot's memory subsystem initialization.
 * @return true if initialization succeeded
 */
bool gc_init_static();

/**
 * Finalize the static Mono GC subsystem.
 * Called during Godot shutdown.
 */
void gc_finalize_static();

/**
 * Collect garbage for the specified generation.
 * @param generation -1 for full collection, 0-2 for specific generations
 */
void gc_collect_static(int generation);

/**
 * Get the maximum GC generation.
 * @return Maximum generation number (typically 0)
 */
int gc_get_max_generation_static();

/**
 * Wait for pending finalizers to complete.
 */
void gc_wait_for_pending_finalizers_static();

/**
 * Allocate an object in the Mono heap.
 * @param size Size of object in bytes
 * @param vtable Mono vtable for the type
 * @return Pointer to allocated object, or nullptr on failure
 */
void *gc_alloc_obj_static(size_t size, MonoVTable *vtable);

/**
 * Register a root for GC tracking.
 * @param start Start address of the root
 * @param size Size of the root region
 * @param cookie User-provided cookie for the root
 */
void gc_register_root_static(void *start, size_t size, void *cookie);

/**
 * Unregister a root from GC tracking.
 * @param start Start address of the root
 */
void gc_unregister_root_static(void *start);

/**
 * Register a finalizer callback for an object.
 * @param object The object to register
 * @param callback The finalizer callback function
 */
void gc_register_finalizer_static(void *object, void *callback);

/**
 * Check if an object is collectible.
 * @param object The object to check
 * @return true if the object is collectible
 */
bool gc_is_collectible_static(void *object);

/**
 * Suppress finalization for an object.
 * @param object The object to suppress finalization for
 */
void gcSuppressFinalizeStatic(void *object);

/**
 * Enumerate reachable objects using a callback.
 * @param callback The enumeration callback
 * @param user_data User data passed to callback
 */
void gc_enumerate_reachable_objects_static(void *callback, void *user_data);

/**
 * Get GC statistics.
 * @param key The statistic key to retrieve
 * @return The statistic value
 */
int64_t gc_get_stats_static(const char *key);

/**
 * Set GC parameters.
 * @param key The parameter key
 * @param value The parameter value
 */
void gc_set_param_static(const char *key, int64_t value);

} // namespace gdmono
