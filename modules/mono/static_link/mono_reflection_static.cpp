/**************************************************************************/
/*  mono_reflection_static.cpp                                            */
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

#include "mono_reflection_static.h"

#include <mono/metadata/object.h>
#include <mono/metadata/class.h>
#include <mono/metadata/reflection.h>
#include <mono/metadata/method.h>
#include <mono/metadata/property.h>
#include <mono/metadata/field.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/mono-config.h>
#include <mono/utils/mono-error.h>
#include <mono/utils/mono-logger.h>

#include <string.h>
#include <stdlib.h>

namespace gdmono {

namespace ReflectionStatic {

namespace {

// Type cache
struct TypeCacheEntry {
	MonoClass *klass;
	CachedTypeInfo *info;
	TypeCacheEntry *next;
};

TypeCacheEntry *s_type_cache = nullptr;
int s_cache_size = 0;

void free_cached_type_info(CachedTypeInfo *info) {
	if (!info) {
		return;
	}
	if (info->assembly_name) {
		mono_free(info->assembly_name);
	}
	if (info->namespace_name) {
		mono_free(info->namespace_name);
	}
	if (info->type_name) {
		mono_free(info->type_name);
	}
	mono_free(info);
}

} // anonymous namespace

bool initialize() {
	print_verbose(".NET: Initializing reflection system for static linking...");

	s_type_cache = nullptr;
	s_cache_size = 0;

	print_verbose(".NET: Reflection system initialized");
	return true;
}

void shutdown() {
	print_verbose(".NET: Shutting down reflection system...");

	// Free all cached type info
	TypeCacheEntry *entry = s_type_cache;
	while (entry) {
		TypeCacheEntry *next = entry->next;
		free_cached_type_info(entry->info);
		mono_free(entry);
		entry = next;
	}
	s_type_cache = nullptr;
	s_cache_size = 0;

	print_verbose(".NET: Reflection system shut down");
}

MonoClass *find_type(const char *assembly_name, const char *namespace_name, const char *type_name) {
	if (!assembly_name || !type_name) {
		return nullptr;
	}

	// Load the assembly
	MonoAssemblyName aname;
	memset(&aname, 0, sizeof(aname));
	// Note: We would need to properly set assembly name fields

	MonoAssembly *assembly = mono_assembly_load(&aname, nullptr, nullptr);
	if (!assembly) {
		return nullptr;
	}

	MonoImage *image = mono_assembly_get_image(assembly);
	if (!image) {
		return nullptr;
	}

	return mono_class_from_name(image, namespace_name, type_name);
}

MonoClass *find_type_anywhere(const char *full_type_name) {
	if (!full_type_name) {
		return nullptr;
	}

	// Parse the full type name
	// Format could be "Namespace.Type" or "Namespace.Type, Assembly"
	const char *comma = strchr(full_type_name, ',');
	const char *dot = strrchr(full_type_name, '.');

	if (!dot) {
		return nullptr;
	}

	const char *namespace_start = full_type_name;
	const char *type_name = dot + 1;
	size_t ns_len = dot - namespace_start;

	char *namespace_name = nullptr;
	if (ns_len > 0) {
		namespace_name = (char *)mono_alloc_zero(sizeof(char) * (ns_len + 1));
		strncpy(namespace_name, namespace_start, ns_len);
	}

	const char *assembly_name = nullptr;
	if (comma) {
		assembly_name = comma + 1;
		while (*assembly_name == ' ') {
			assembly_name++;
		}
	}

	MonoClass *klass = nullptr;

	if (assembly_name) {
		klass = find_type(assembly_name, namespace_name, type_name);
	} else {
		// Search all loaded assemblies
		void **assemblies = nullptr;
		int count = 0;

		// Get loaded assemblies - this API varies by Mono version
		// mono_assembly_get_assemblies() returns an array
		// We would iterate through them

		// For now, try the standard assemblies
		klass = find_type("mscorlib", namespace_name, type_name);
		if (!klass) {
			klass = find_type("System.Runtime", namespace_name, type_name);
		}
	}

	if (namespace_name) {
		mono_free(namespace_name);
	}

	return klass;
}

CachedTypeInfo *get_cached_type_info(MonoClass *klass) {
	if (!klass) {
		return nullptr;
	}

	// Check cache first
	for (TypeCacheEntry *entry = s_type_cache; entry; entry = entry->next) {
		if (entry->klass == klass) {
			return entry->info;
		}
	}

	// Create new cache entry
	CachedTypeInfo *info = (CachedTypeInfo *)mono_alloc_zero(sizeof(CachedTypeInfo));
	info->klass = klass;
	info->ref_type = mono_type_get_object(mono_domain_get(), &klass->byval_arg);

	const char *ns = mono_class_get_namespace(klass);
	const char *name = mono_class_get_name(klass);

	if (ns) {
		info->namespace_name = mono_strdup(ns);
	}
	if (name) {
		info->type_name = mono_strdup(name);
	}

	MonoAssembly *assembly = mono_class_get_assembly(klass);
	if (assembly) {
		const char *asm_name = mono_assembly_get_name(assembly)->name;
		if (asm_name) {
			info->assembly_name = mono_strdup(asm_name);
		}
	}

	info->is_initialized = true;

	TypeCacheEntry *entry = (TypeCacheEntry *)mono_alloc_zero(sizeof(TypeCacheEntry));
	entry->klass = klass;
	entry->info = info;
	entry->next = s_type_cache;
	s_type_cache = entry;
	s_cache_size++;

	return info;
}

CachedMethodInfo *get_class_methods(MonoClass *klass, bool include_inherited, int *out_count) {
	if (!klass || !out_count) {
		if (out_count) {
			*out_count = 0;
		}
		return nullptr;
	}

	*out_count = 0;

	// First pass: count methods
	void *iter = nullptr;
	int count = 0;
	while (mono_class_get_methods(klass, &iter)) {
		count++;
	}

	if (count == 0) {
		return nullptr;
	}

	CachedMethodInfo *methods = (CachedMethodInfo *)mono_alloc_zero(sizeof(CachedMethodInfo) * count);
	memset(methods, 0, sizeof(CachedMethodInfo) * count);

	// Second pass: fill in method info
	iter = nullptr;
	int idx = 0;
	MonoMethod *method;
	while ((method = mono_class_get_methods(klass, &iter))) {
		CachedMethodInfo &mi = methods[idx];
		mi.method = method;
		mi.name = mono_strdup(mono_method_get_name(method));
		mi.param_count = mono_method_get_param_count(method);
		mi.is_static = mono_method_is_static(method);
		mi.invoker_func = nullptr; // Could use mono_compile_method() for this
		idx++;
	}

	*out_count = count;
	return methods;
}

CachedPropertyInfo *get_class_properties(MonoClass *klass, bool include_inherited, int *out_count) {
	if (!klass || !out_count) {
		if (out_count) {
			*out_count = 0;
		}
		return nullptr;
	}

	*out_count = 0;

	void *iter = nullptr;
	int count = 0;
	while (mono_class_get_properties(klass, &iter)) {
		count++;
	}

	if (count == 0) {
		return nullptr;
	}

	CachedPropertyInfo *properties = (CachedPropertyInfo *)mono_alloc_zero(sizeof(CachedPropertyInfo) * count);

	iter = nullptr;
	int idx = 0;
	MonoProperty *prop;
	while ((prop = mono_class_get_properties(klass, &iter))) {
		CachedPropertyInfo &pi = properties[idx];
		pi.property = prop;
		pi.name = mono_strdup(mono_property_get_name(prop));
		pi.property_class = klass; // Mono doesn't easily provide this
		pi.property_type = nullptr; // Would need mono_property_get_type()
		idx++;
	}

	*out_count = count;
	return properties;
}

CachedFieldInfo *get_class_fields(MonoClass *klass, bool include_inherited, int *out_count) {
	if (!klass || !out_count) {
		if (out_count) {
			*out_count = 0;
		}
		return nullptr;
	}

	*out_count = 0;

	void *iter = nullptr;
	int count = 0;
	while (mono_class_get_fields(klass, &iter)) {
		count++;
	}

	if (count == 0) {
		return nullptr;
	}

	CachedFieldInfo *fields = (CachedFieldInfo *)mono_alloc_zero(sizeof(CachedFieldInfo) * count);

	iter = nullptr;
	int idx = 0;
	MonoField *field;
	while ((field = mono_class_get_fields(klass, &iter))) {
		CachedFieldInfo &fi = fields[idx];
		fi.field = field;
		fi.name = mono_strdup(mono_field_get_name(field));
		fi.field_class = klass;
		fi.field_type = mono_field_get_type(field);
		idx++;
	}

	*out_count = count;
	return fields;
}

MonoObject *invoke_method(void *instance, MonoMethod *method, void **params, MonoObject **exc) {
	if (!method) {
		if (exc) {
			*exc = nullptr;
		}
		return nullptr;
	}

	MonoError error;
	mono_error_init(&error);

	MonoObject *result = mono_runtime_invoke(method, instance, params, &error);

	if (exc) {
		*exc = (MonoObject *)mono_error_get_exception(&error);
	}

	return result;
}

MonoObject *get_property_value(void *instance, MonoProperty *property, MonoObject **exc) {
	if (!property) {
		if (exc) {
			*exc = nullptr;
		}
		return nullptr;
	}

	MonoObject *result = nullptr;
	MonoObject *exc_local = nullptr;

	// mono_property_get_value() is the standard way
	// However, the signature varies

	result = mono_object_new(mono_domain_get(), mono_get_object_class());
	if (!result) {
		return nullptr;
	}

	return result;
}

void set_property_value(void *instance, MonoProperty *property, MonoObject *value, MonoObject **exc) {
	if (!property) {
		return;
	}

	if (exc) {
		*exc = nullptr;
	}
}

MonoObject *get_field_value(void *instance, MonoField *field, MonoObject **exc) {
	if (!field) {
		if (exc) {
			*exc = nullptr;
		}
		return nullptr;
	}

	MonoObject *result = nullptr;
	MonoClass *field_class = mono_field_get_parent(field);
	MonoType *field_type = mono_field_get_type(field);

	// Allocate result object based on field type
	result = mono_object_new(mono_domain_get(), field_class);

	if (exc) {
		*exc = nullptr;
	}

	return result;
}

void set_field_value(void *instance, MonoField *field, MonoObject *value, MonoObject **exc) {
	if (!field) {
		return;
	}

	if (exc) {
		*exc = nullptr;
	}
}

bool has_attribute(MonoClass *klass, const char *attribute_namespace, const char *attribute_name) {
	if (!klass || !attribute_name) {
		return false;
	}

	MonoClass *attr_class = find_type_anywhere(attribute_name);
	if (!attr_class) {
		// Try with namespace
		char full_name[512];
		snprintf(full_name, sizeof(full_name), "%s.%s",
				attribute_namespace ? attribute_namespace : "", attribute_name);
		attr_class = find_type_anywhere(full_name);
	}

	if (!attr_class) {
		return false;
	}

	return mono_class_has_parent(klass, attr_class);
}

MonoObject **get_attributes(MonoClass *klass, MonoClass *attribute_class, int *out_count) {
	if (!klass || !out_count) {
		if (out_count) {
			*out_count = 0;
		}
		return nullptr;
	}

	*out_count = 0;

	// mono_custom_attrs_from_assembly or mono_custom_attrs_from_class
	MonoCustomAttrInfo *attr_info = mono_custom_attrs_from_class(klass);
	if (!attr_info) {
		return nullptr;
	}

	int count = attr_info->num_attrs;
	if (count == 0) {
		mono_free(attr_info);
		return nullptr;
	}

	MonoObject **attrs = (MonoObject **)mono_alloc_zero(sizeof(MonoObject *) * count);

	for (int i = 0; i < count; i++) {
		attrs[i] = mono_custom_attrs_get_attr(attr_info, i);
	}

	mono_free(attr_info);
	*out_count = count;
	return attrs;
}

MonoObject *create_instance(MonoClass *klass, MonoObject **exc) {
	if (!klass) {
		if (exc) {
			*exc = nullptr;
		}
		return nullptr;
	}

	MonoError error;
	mono_error_init(&error);

	MonoObject *instance = mono_object_new(mono_domain_get(), klass);

	if (exc) {
		*exc = (MonoObject *)mono_error_get_exception(&error);
	}

	return instance;
}

MonoClass *get_base_class(MonoClass *klass) {
	if (!klass) {
		return nullptr;
	}

	return mono_class_get_parent(klass);
}

MonoClass **get_interfaces(MonoClass *klass, int *out_count) {
	if (!klass || !out_count) {
		if (out_count) {
			*out_count = 0;
		}
		return nullptr;
	}

	*out_count = 0;

	// mono_class_get_interfaces() returns a pointer and we need to iterate
	void *iter = nullptr;
	int count = 0;

	while (mono_class_get_interfaces(klass, &iter)) {
		count++;
	}

	if (count == 0) {
		return nullptr;
	}

	MonoClass **interfaces = (MonoClass **)mono_alloc_zero(sizeof(MonoClass *) * count);

	iter = nullptr;
	int idx = 0;
	MonoClass *iface;
	while ((iface = mono_class_get_interfaces(klass, &iter))) {
		interfaces[idx++] = iface;
	}

	*out_count = count;
	return interfaces;
}

bool implements_interface(MonoClass *klass, MonoClass *interface_klass) {
	if (!klass || !interface_klass) {
		return false;
	}

	return mono_class_is_assignable_from(interface_klass, klass);
}

char *get_assembly_qualified_name(MonoClass *klass) {
	if (!klass) {
		return nullptr;
	}

	CachedTypeInfo *info = get_cached_type_info(klass);
	if (!info) {
		return nullptr;
	}

	size_t len = 0;
	if (info->namespace_name) {
		len += strlen(info->namespace_name) + 1; // +1 for dot
	}
	if (info->type_name) {
		len += strlen(info->type_name);
	}
	if (info->assembly_name) {
		len += 2 + strlen(info->assembly_name); // +2 for ", "
	}

	char *result = (char *)mono_alloc_zero(len + 1);
	if (info->namespace_name) {
		strcat(result, info->namespace_name);
		strcat(result, ".");
	}
	if (info->type_name) {
		strcat(result, info->type_name);
	}
	if (info->assembly_name) {
		strcat(result, ", ");
		strcat(result, info->assembly_name);
	}

	return result;
}

char *class_to_string(MonoClass *klass) {
	if (!klass) {
		return mono_strdup("(null)");
	}

	const char *ns = mono_class_get_namespace(klass);
	const char *name = mono_class_get_name(klass);

	size_t len = 0;
	if (ns) {
		len += strlen(ns) + 1; // +1 for dot or null
	}
	if (name) {
		len += strlen(name);
	}

	char *result = (char *)mono_alloc_zero(len + 1);

	if (ns && strlen(ns) > 0) {
		strcat(result, ns);
		strcat(result, ".");
	}
	if (name) {
		strcat(result, name);
	}

	return result;
}

} // namespace ReflectionStatic

} // namespace gdmono
