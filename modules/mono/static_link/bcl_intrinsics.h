/**************************************************************************/
/*  bcl_intrinsics.h                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining    */
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

#pragma once

#include <mono/metadata/object.h>
#include <mono/metadata/class.h>
#include <mono/metadata/reflection.h>

namespace gdmono {

/**
 * BCL (Base Class Library) Intrinsics for Static Mono Linking
 *
 * This header defines the intrinsic functions that need special handling
 * when Mono is statically linked. These are functions that would normally
 * be provided by the BCL but need custom implementations for embedded use.
 */
namespace BCLIntrinsics {

/**
 * String comparison internals
 */
int string_equals_static(MonoString *str1, MonoString *str2);
int string_compare_static(MonoString *str1, MonoString *str2);

/**
 * Array operations
 */
void *array_element_address_static(MonoArray *arr, int index, int element_size);
MonoArray *array_new_static(MonoClass *element_class, uintptr_t length);
void array_set_length_static(MonoArray *arr, uintptr_t length);

/**
 * Type checking
 */
bool is_assignable_from_static(MonoClass *dest, MonoClass *src);
bool is_instance_of_static(void *obj, MonoClass *klass);

/**
 * Runtime type information
 */
MonoClass *get_type_from_handle_static(void *type_handle);
void *get_runtime_type_handle_static(MonoClass *klass);

/**
 * Exception handling
 */
void throw_exception_static(void *exception);
void rethrow_exception_static();
void *get_current_exception_static();

/**
 * Monitor/Lock operations
 */
void monitor_enter_static(void *obj);
bool monitor_try_enter_static(void *obj);
void monitor_exit_static(void *obj);

/**
 *GC safe region operations
 */
void *gc_safe_region_enter_static(void *ptr, size_t size);
void gc_safe_region_exit_static(void *handle);

/**
 * Reflection helpers
 */
void *get_type_handle_static(MonoReflectionType *type);
MonoObject *invoke_member_static(void *handle, const char *name, void **params, int param_count);

/**
 * Memory copy/move for interop
 */
void memory_copy_static(void *dest, const void *src, size_t count);
void memory_move_static(void *dest, const void *src, size_t count);
int memory_compare_static(const void *buf1, const void *buf2, size_t count);

/**
 * Initialization entry point
 * Called during static linking setup to register all intrinsics
 */
void initialize_bcl_intrinsics();

} // namespace BCLIntrinsics

} // namespace gdmono
