/**************************************************************************/
/*  mono_threads_static.h                                                 */
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

#include <mono/metadata/threadpool.h>
#include <mono/metadata/threads.h>

namespace gdmono {

/**
 * Thread Management Interface for Static Mono Linking
 *
 * This interface provides thread management functions when Mono is statically linked.
 * Each thread that wants to use Mono objects must be attached to the Mono runtime.
 */

/**
 * Initialize the static Mono threading subsystem.
 * Called during Mono runtime initialization.
 */
void threads_init_static();

/**
 * Cleanup the static Mono threading subsystem.
 * Called during Mono runtime shutdown.
 */
void threads_cleanup_static();

/**
 * Attach the current thread to the Mono runtime.
 * @return The MonoThread object for the current thread, or nullptr on failure
 */
MonoThread *attach_current_thread_static();

/**
 * Detach the current thread from the Mono runtime.
 */
void detach_current_thread_static();

/**
 * Check if the current thread is attached to Mono.
 * @return true if the thread is attached
 */
bool is_current_thread_attached_static();

/**
 * Set the abort-exception thread callback.
 * @param callback The callback function
 */
void set_thread_abort_callback_static(void (*callback)(void));

/**
 * Set callbacks for thread attach/detach events.
 * These callbacks allow Godot to track Mono thread lifecycle.
 * @param on_attach Callback when a thread is attached
 * @param on_detach Callback when a thread is detached
 */
void set_thread_callbacks_static(
		void (*on_attach)(MonoThread *),
		void (*on_detach)(MonoThread *));

/**
 * Get the current thread's Mono thread object.
 * @return The current MonoThread, or nullptr if not attached
 */
MonoThread *get_current_thread_static();

/**
 * Set the current thread name (for debugging purposes).
 * @param name The thread name
 */
void set_thread_name_static(const char *name);

/**
 * Check if a thread is an internal Mono thread (GC finalizer, etc.)
 * @param thread The thread to check
 * @return true if it's an internal Mono thread
 */
bool is_thread_pool_thread_static(MonoThread *thread);

/**
 * Get the number of threads in the thread pool.
 * @return Number of threads in the Mono thread pool
 */
int get_thread_pool_size_static();

/**
 * Set the thread pool size limits.
 * @param min Minimum number of threads
 * @param max Maximum number of threads
 */
void set_thread_pool_limits_static(int min, int max);

/**
 * Return a thread to the thread pool.
 * @param thread The thread to return to the pool
 */
void return_thread_to_pool_static(MonoThread *thread);

/**
 * Process pending thread pool work items.
 * @param finish If true, signal all threads to exit
 */
void thread_pool_await_static(bool finish);

/**
 * Get detailed information about the current thread's state.
 * @return A string describing the thread state
 */
const char *get_thread_state_static(MonoThread *thread);

} // namespace gdmono
