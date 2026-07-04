/**************************************************************************/
/*  mono_reflection_static.h                                              */
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

#pragma once

#include <mono/metadata/object.h>
#include <mono/metadata/class.h>
#include <mono/metadata/reflection.h>
#include <mono/metadata/method.h>
#include <mono/metadata/property.h>
#include <mono/metadata/field.h>
#include <mono/metadata/assembly.h>

namespace gdmono {

/**
 * Reflection System for Static Mono Linking
 *
 * This module provides enhanced reflection capabilities for statically linked Mono.
 * It handles:
 * - Type discovery and caching
 * - Method invocation via reflection
 * - Property and field access
 * - Custom attribute querying
 * - Dynamic type loading
 */
namespace ReflectionStatic {

/**
 * Cached type information
 */
struct CachedTypeInfo {
	MonoClass *klass = nullptr;
	MonoReflectionType *ref_type = nullptr;
	char *assembly_name = nullptr;
	char *namespace_name = nullptr;
	char *type_name = nullptr;
	bool is_initialized = false;
};

/**
 * Cached method information
 */
struct CachedMethodInfo {
	MonoMethod *method = nullptr;
	char *name = nullptr;
	int param_count = 0;
	bool is_static = false;
	void *invoker_func = nullptr;
};

/**
 * Cached property information
 */
struct CachedPropertyInfo {
	MonoProperty *property = nullptr;
	char *name = nullptr;
	MonoClass *property_class = nullptr;
	MonoType *property_type = nullptr;
};

/**
 * Cached field information
 */
struct CachedFieldInfo {
	MonoField *field = nullptr;
	char *name = nullptr;
	MonoClass *field_class = nullptr;
	MonoType *field_type = nullptr;
};

/**
 * Initialize the reflection system.
 * Must be called after Mono runtime is initialized.
 * @return true if initialization succeeded
 */
bool initialize();

/**
 * Shutdown the reflection system.
 * Frees all cached type information.
 */
void shutdown();

/**
 * Find a type by name from a loaded assembly.
 *
 * @param assembly_name Name of the assembly (e.g., "mscorlib")
 * @param namespace_name Namespace of the type (e.g., "System")
 * @param type_name Name of the type (e.g., "String")
 * @return The MonoClass, or nullptr if not found
 */
MonoClass *find_type(const char *assembly_name, const char *namespace_name, const char *type_name);

/**
 * Find a type by name from any loaded assembly.
 * Searches through all loaded assemblies.
 *
 * @param full_type_name Full type name (e.g., "System.String" or "Namespace.Type")
 * @return The MonoClass, or nullptr if not found
 */
MonoClass *find_type_anywhere(const char *full_type_name);

/**
 * Get or create a cached type info structure.
 *
 * @param klass The MonoClass to cache
 * @return Cached type info, or nullptr on failure
 */
CachedTypeInfo *get_cached_type_info(MonoClass *klass);

/**
 * Get all methods of a class.
 *
 * @param klass The class to get methods from
 * @param include_inherited If true, include inherited methods
 * @return A newly allocated array of CachedMethodInfo, or nullptr
 *         Caller must free the returned array and each element's strings
 */
CachedMethodInfo *get_class_methods(MonoClass *klass, bool include_inherited, int *out_count);

/**
 * Get all properties of a class.
 *
 * @param klass The class to get properties from
 * @param include_inherited If true, include inherited properties
 * @return A newly allocated array of CachedPropertyInfo, or nullptr
 */
CachedPropertyInfo *get_class_properties(MonoClass *klass, bool include_inherited, int *out_count);

/**
 * Get all fields of a class.
 *
 * @param klass The class to get fields from
 * @param include_inherited If true, include inherited fields
 * @return A newly allocated array of CachedFieldInfo, or nullptr
 */
CachedFieldInfo *get_class_fields(MonoClass *klass, bool include_inherited, int *out_count);

/**
 * Invoke a method via reflection.
 *
 * @param instance The object instance, or nullptr for static methods
 * @param method The method to invoke
 * @param params Array of parameter values
 * @param param_count Number of parameters
 * @param exc Exception object (output)
 * @return The return value, or nullptr for void methods
 */
MonoObject *invoke_method(void *instance, MonoMethod *method, void **params, MonoObject **exc);

/**
 * Get a property value via reflection.
 *
 * @param instance The object instance, or nullptr for static properties
 * @param property The property to get
 * @param exc Exception object (output)
 * @return The property value
 */
MonoObject *get_property_value(void *instance, MonoProperty *property, MonoObject **exc);

/**
 * Set a property value via reflection.
 *
 * @param instance The object instance, or nullptr for static properties
 * @param property The property to set
 * @param value The value to set
 * @param exc Exception object (output)
 */
void set_property_value(void *instance, MonoProperty *property, MonoObject *value, MonoObject **exc);

/**
 * Get a field value via reflection.
 *
 * @param instance The object instance, or nullptr for static fields
 * @param field The field to get
 * @param exc Exception object (output)
 * @return The field value
 */
MonoObject *get_field_value(void *instance, MonoField *field, MonoObject **exc);

/**
 * Set a field value via reflection.
 *
 * @param instance The object instance, or nullptr for static fields
 * @param field The field to set
 * @param value The value to set
 * @param exc Exception object (output)
 */
void set_field_value(void *instance, MonoField *field, MonoObject *value, MonoObject **exc);

/**
 * Check if a type has a specific custom attribute.
 *
 * @param klass The class to check
 * @param attribute_namespace Namespace of the attribute (e.g., "System")
 * @param attribute_name Name of the attribute (e.g., "SerializableAttribute")
 * @return true if the type has the attribute
 */
bool has_attribute(MonoClass *klass, const char *attribute_namespace, const char *attribute_name);

/**
 * Get the custom attributes of a type.
 *
 * @param klass The class to get attributes from
 * @param attribute_class Optional: filter by specific attribute class
 * @param out_count Number of attributes returned (output)
 * @return Array of attribute objects, or nullptr
 */
MonoObject **get_attributes(MonoClass *klass, MonoClass *attribute_class, int *out_count);

/**
 * Create an instance of a type via reflection.
 *
 * @param klass The class to instantiate
 * @param exc Exception object (output)
 * @return The new object instance, or nullptr on failure
 */
MonoObject *create_instance(MonoClass *klass, MonoObject **exc);

/**
 * Get the base class of a type.
 *
 * @param klass The class
 * @return The base class, or nullptr if no base class (for interfaces or object)
 */
MonoClass *get_base_class(MonoClass *klass);

/**
 * Get all interfaces implemented by a type.
 *
 * @param klass The class
 * @param out_count Number of interfaces returned (output)
 * @return Array of interface classes, or nullptr
 */
MonoClass **get_interfaces(MonoClass *klass, int *out_count);

/**
 * Check if a type implements an interface.
 *
 * @param klass The class to check
 * @param interface_klass The interface class
 * @return true if the type implements the interface
 */
bool implements_interface(MonoClass *klass, MonoClass *interface_klass);

/**
 * Get the assembly qualified name of a type.
 *
 * @param klass The class
 * @return Newly allocated string with the full assembly qualified name
 *         Caller must free with mono_free()
 */
char *get_assembly_qualified_name(MonoClass *klass);

/**
 * Format a MonoClass as a string for debugging.
 *
 * @param klass The class
 * @return Newly allocated string with type name
 *         Caller must free with mono_free()
 */
char *class_to_string(MonoClass *klass);

} // namespace ReflectionStatic

} // namespace gdmono
