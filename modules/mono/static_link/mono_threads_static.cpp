/**************************************************************************/
/*  mono_threads_static.cpp                                                */
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

#include "mono_threads_static.h"

#include <mono/metadata/threadpool.h>
#include <mono/metadata/threads.h>
#include <mono/metadata/mono-threads.h>

#include "core/os/os.h"
#include "core/string/string_name.h"

namespace {

// Thread callbacks for Godot integration
void (*s_on_thread_attach)(MonoThread *) = nullptr;
void (*s_on_thread_detach)(MonoThread *) = nullptr;
void (*s_thread_abort_callback)(void) = nullptr;

} // anonymous namespace

namespace gdmono {

void threads_init_static() {
	print_verbose(".NET: Initializing static Mono threading...");

	// Configure Mono threading for Godot integration
	// Mono will create internal threads for finalizers, etc.

	// Set thread attach/detach hooks if needed for debugging
	// mono_set_thread_abort_callback();

#ifdef DEBUG_ENABLED
	print_verbose(".NET: Thread pool initialized");
#endif
}

void threads_cleanup_static() {
	print_verbose(".NET: Cleaning up static Mono threading...");

	// Signal thread pool to finish
	// mono_threadpool_cleanup();

	print_verbose(".NET: Static Mono threading cleaned up");
}

MonoThread *attach_current_thread_static() {
	// Attach the current thread to the Mono runtime
	// This is required before the thread can use Mono objects

	MonoThread *thread = mono_thread_attach(mono_get_root_domain());

	if (thread && s_on_thread_attach) {
		s_on_thread_attach(thread);
	}

	return thread;
}

void detach_current_thread_static() {
	MonoThread *thread = mono_thread_attach(mono_get_root_domain());

	if (thread && s_on_thread_detach) {
		s_on_thread_detach(thread);
	}

	// Detach the current thread from the Mono runtime
	mono_thread_detach(thread);
}

bool is_current_thread_attached_static() {
	return mono_thread_is_attached() != 0;
}

void set_thread_abort_callback_static(void (*callback)(void)) {
	s_thread_abort_callback = callback;
	// mono_set_thread_abort_callback(callback);
}

void set_thread_callbacks_static(
		void (*on_attach)(MonoThread *),
		void (*on_detach)(MonoThread *)) {
	s_on_thread_attach = on_attach;
	s_on_thread_detach = on_detach;
}

MonoThread *get_current_thread_static() {
	return mono_thread_get_current();
}

void set_thread_name_static(const char *name) {
	// Mono doesn't support setting thread names in a portable way
	// This is mainly for debugging purposes
#ifdef DEBUG_ENABLED
	print_verbose(".NET: Setting thread name: " + String(name));
#endif
}

bool is_thread_pool_thread_static(MonoThread *thread) {
	// Check if this is a thread pool thread (internal Mono thread)
	// Mono uses special threads for finalizers, etc.
	return thread != nullptr &&
		   (mono_thread_is_excelcl || mono_thread_is_thread_pool_thread());
}

int get_thread_pool_size_static() {
	// Query Mono thread pool size
	// This returns the number of worker threads in the pool
	return mono_threadpool_get_max_threads();
}

void set_thread_pool_limits_static(int min, int max) {
	// Set thread pool size limits
	mono_threadpool_set_min_threads(min);
	mono_threadpool_set_max_threads(max);
}

void return_thread_to_pool_static(MonoThread *thread) {
	// Return a thread to the Mono thread pool
	// This is called when a thread finishes executing work items
	// Mono handles this internally in most cases
}

void thread_pool_await_static(bool finish) {
	// Wait for all thread pool work to complete
	// If finish is true, signal threads to exit
	if (finish) {
		mono_threadpool_cleanup();
	}
}

const char *get_thread_state_static(MonoThread *thread) {
	if (!thread) {
		return "null";
	}

	MonoThreadState state = mono_thread_get_state(thread);

	switch (state) {
		case MonoThreadState::TS_UNKNOWN:
			return "Unknown";
		case MonoThreadState::TS_RUNNING:
			return "Running";
		case MonoThreadState::TS_WAIT_SLEEP_JOIN:
			return "WaitSleepJoin";
		case MonoThreadState::TS_SUSPENDED:
			return "Suspended";
		case MonoThreadState::TS_ABORT:
			return "Abort";
		case MonoThreadState::TS_UNSTARTED:
			return "Unstarted";
		case MonoThreadState::TS_STOPPED:
			return "Stopped";
		case MonoThreadState::TS_ORIGINAL_ABORT:
			return "OriginalAbort";
		case MonoThreadState::TS_BACKGROUND:
			return "Background";
		case MonoThreadState::TS_UNSAFE:
			return "Unsafe";
		case MonoThreadState::TS_HELPER_THREAD:
			return "HelperThread";
		default:
			return "UnknownState";
	}
}

} // namespace gdmono
