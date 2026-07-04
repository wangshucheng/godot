/**************************************************************************/
/*  bcl_intrinsics.cpp                                                   */
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
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "bcl_intrinsics.h"

#include <mono/metadata/object.h>
#include <mono/metadata/class.h>
#include <mono/metadata/reflection.h>
#include <mono/metadata/mono-config.h>
#include <mono/metadata/mono-debug.h>
#include <mono/utils/mono-logger.h>

#include <string.h>

namespace gdmono {

namespace BCLIntrinsics {

int string_equals_static(MonoString *str1, MonoString *str2) {
	if (!str1 || !str2) {
		return str1 == str2;
	}

	if (mono_string_length(str1) != mono_string_length(str2)) {
		return 0;
	}

	// Compare string contents
	MonoChar *chars1 = mono_string_chars(str1);
	MonoChar *chars2 = mono_string_chars(str2);

	int len = mono_string_length(str1);
	for (int i = 0; i < len; i++) {
		if (chars1[i] != chars2[i]) {
			return 0;
		}
	}
	return 1;
}

int string_compare_static(MonoString *str1, MonoString *str2) {
	if (!str1 || !str2) {
		if (str1 == str2) return 0;
		return str1 ? 1 : -1;
	}

	MonoChar *chars1 = mono_string_chars(str1);
	MonoChar *chars2 = mono_string_chars(str2);

	int len1 = mono_string_length(str1);
	int len2 = mono_string_length(str2);
	int min_len = len1 < len2 ? len1 : len2;

	for (int i = 0; i < min_len; i++) {
		if (chars1[i] != chars2[i]) {
			return chars1[i] < chars2[i] ? -1 : 1;
		}
	}

	return len1 < len2 ? -1 : (len1 > len2 ? 1 : 0);
}

void *array_element_address_static(MonoArray *arr, int index, int element_size) {
	if (!arr) {
		return nullptr;
	}

	// Array data starts after the MonoArray header
	// The header size varies, but we can compute the offset
	char *arr_start = (char *)arr;
	uintptr_t num_elements = mono_array_length(arr);

	if (index < 0 || index >= (int)num_elements) {
		return nullptr;
	}

	// Calculate element address
	char *elements = arr_start + sizeof(MonoArray);
	return elements + (index * element_size);
}

MonoArray *array_new_static(MonoClass *element_class, uintptr_t length) {
	if (!element_class) {
		return nullptr;
	}

	return mono_array_new(mono_domain_get(), element_class, length);
}

void array_set_length_static(MonoArray *arr, uintptr_t length) {
	// Note: mono_array_set_length is not available in all Mono versions
	// This is a placeholder - actual implementation depends on Mono version
	// In practice, arrays in Mono are fixed size once created
}

bool is_assignable_from_static(MonoClass *dest, MonoClass *src) {
	if (!dest || !src) {
		return false;
	}

	return mono_class_is_assignable_from(dest, src);
}

bool is_instance_of_static(void *obj, MonoClass *klass) {
	if (!obj || !klass) {
		return false;
	}

	return mono_object_isinst(obj, klass) != nullptr;
}

void *get_type_from_handle_static(void *type_handle) {
	if (!type_handle) {
		return nullptr;
	}

	MonoReflectionType *ref_type = (MonoReflectionType *)type_handle;
	return (void *)mono_reflection_type_get_type(ref_type);
}

void *get_runtime_type_handle_static(MonoClass *klass) {
	if (!klass) {
		return nullptr;
	}

	// Get the Type handle for a class via reflection
	return mono_type_get_object(mono_domain_get(), &klass->byval_arg);
}

void throw_exception_static(void *exception) {
	if (!exception) {
		return;
	}

	mono_raise_exception((MonoException *)exception);
}

void rethrow_exception_static() {
	mono_rethrow_if_executing_exception();
}

void *get_current_exception_static() {
	return mono_get_current_exception();
}

void monitor_enter_static(void *obj) {
	if (!obj) {
		return;
	}

	// Note: Monitor.Enter is not thread-safe without additional coordination
	// This is a simplified implementation
	mono_monitor_enter(obj);
}

bool monitor_try_enter_static(void *obj) {
	if (!obj) {
		return false;
	}

	// Try to enter the monitor without blocking
	// Returns true if successful, false if already held
	return mono_monitor_try_enter(obj, 0);
}

void monitor_exit_static(void *obj) {
	if (!obj) {
		return;
	}

	mono_monitor_exit(obj);
}

void *gc_safe_region_enter_static(void *ptr, size_t size) {
	// Enter a GC-safe region - disable GC collection temporarily
	// This is a no-op in our implementation as Mono handles this internally
	return ptr;
}

void gc_safe_region_exit_static(void *handle) {
	// Exit a GC-safe region - re-enable GC collection
	// This is a no-op in our implementation
}

void *get_type_handle_static(MonoReflectionType *type) {
	if (!type) {
		return nullptr;
	}

	return mono_reflection_type_get_type(type);
}

MonoObject *invoke_member_static(void *handle, const char *name, void **params, int param_count) {
	// This would need to look up the member by name and invoke it
	// Implementation depends on whether it's a method, property, or field
	return nullptr;
}

void memory_copy_static(void *dest, const void *src, size_t count) {
	if (dest && src && count > 0) {
		memcpy(dest, src, count);
	}
}

void memory_move_static(void *dest, const void *src, size_t count) {
	if (dest && src && count > 0) {
		memmove(dest, src, count);
	}
}

int memory_compare_static(const void *buf1, const void *buf2, size_t count) {
	if (!buf1 || !buf2) {
		if (buf1 == buf2) return 0;
		return buf1 ? 1 : -1;
	}
	return memcmp(buf1, buf2, count);
}

void initialize_bcl_intrinsics() {
	print_verbose(".NET: Initializing BCL intrinsics for static linking...");

	// Set up any custom BCL function pointers or configurations
	// This is called during the HostBridge::initialize_host() sequence

	print_verbose(".NET: BCL intrinsics initialized");
}

} // namespace BCLIntrinsics

} // namespace gdmono
