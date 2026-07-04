#include "mono_bridge.h"
#include "mono_variant.h"
#include "core/object/ref_counted.h"
#include "core/error/error_macros.h"
#include <cstdio>

namespace mono_bridge {

static MonoDomain *domain = nullptr;
static MonoClass *godot_object_class = nullptr;
static MonoClass *godot_node_class = nullptr;
static MonoClass *system_intptr_class = nullptr;

struct ObjectBinding {
	MonoGCHandle gc_handle;
	Object *native_ptr = nullptr;
};

static HashMap<Object *, ObjectBinding> native_to_managed;
static HashMap<MonoGCHandle, Object *> managed_to_native;

void init(MonoDomain *p_domain) {
	domain = p_domain;

	MonoImage *corlib = mono_get_corlib();
	system_intptr_class = mono_class_from_name(corlib, "System", "IntPtr");

	printf("[Mono] Bridge initialized.\n");
}

void shutdown() {
	for (auto &pair : native_to_managed) {
		mono_gchandle_free(pair.value.gc_handle);
	}
	native_to_managed.clear();
	managed_to_native.clear();

	domain = nullptr;
	godot_object_class = nullptr;
	godot_node_class = nullptr;
	system_intptr_class = nullptr;
	printf("[Mono] Bridge shut down.\n");
}

Object *unmanaged_get_from_ptr(intptr_t p_native_ptr) {
	if (p_native_ptr == 0) return nullptr;
	return (Object *)p_native_ptr;
}

Object *unmanaged_get(MonoObject *p_cs_obj) {
	if (!p_cs_obj) return nullptr;
	for (auto &pair : native_to_managed) {
		MonoObject *target = mono_gchandle_get_target(pair.value.gc_handle);
		if (target == p_cs_obj) return pair.key;
	}
	return nullptr;
}

void unregister_object(Object *p_obj) {
	if (!p_obj || !native_to_managed.has(p_obj)) return;
	ObjectBinding &binding = native_to_managed[p_obj];
	managed_to_native.erase(binding.gc_handle);
	mono_gchandle_free(binding.gc_handle);
	native_to_managed.erase(p_obj);
}

void tie_native_ptr(MonoObject *p_cs_obj, Object *p_obj) {
	if (!p_cs_obj || !p_obj) return;

	if (native_to_managed.has(p_obj)) {
		mono_gchandle_free(native_to_managed[p_obj].gc_handle);
	}

	MonoGCHandle gch = mono_gchandle_new(p_cs_obj, false);
	ObjectBinding binding;
	binding.gc_handle = gch;
	binding.native_ptr = p_obj;
	native_to_managed[p_obj] = binding;
	managed_to_native[gch] = p_obj;

	MonoClass *klass = mono_object_get_class(p_cs_obj);

	MonoProperty *prop = mono_class_get_property_from_name(klass, "NativePtr");
	if (!prop) {
		for (MonoClass *base = klass; base; base = mono_class_get_parent(base)) {
			prop = mono_class_get_property_from_name(base, "NativePtr");
			if (prop) break;
		}
	}
	if (!prop) return;

	MonoMethod *setter = mono_property_get_set_method(prop);
	if (!setter) return;

	MonoObject *exc = nullptr;
	intptr_t ptr_val = (intptr_t)p_obj;
	void *params[1];
	MonoObject *box = mono_value_box(domain, system_intptr_class, &ptr_val);
	params[0] = box;
	mono_runtime_invoke(setter, p_cs_obj, params, &exc);
	if (exc) {
		printf("[Mono] WARNING: Failed to set NativePtr on C# object.\n");
	}
}

MonoObject *managed_get_or_create(Object *p_obj, MonoClass *p_class) {
	if (!p_obj) return nullptr;

	if (native_to_managed.has(p_obj)) {
		return mono_gchandle_get_target(native_to_managed[p_obj].gc_handle);
	}

	if (!p_class) p_class = godot_object_class;
	if (!p_class) {
		intptr_t ptr_val = (intptr_t)p_obj;
		return mono_value_box(domain, system_intptr_class, &ptr_val);
	}

	MonoObject *cs_obj = mono_object_new(domain, p_class);
	if (!cs_obj) return nullptr;

	MonoObject *exc = nullptr;
	mono_runtime_object_init(cs_obj);
	if (exc) {
		MonoClass *exc_cls = mono_object_get_class(exc);
		char *msg = mono_string_to_utf8(mono_object_to_string(exc, nullptr));
		printf("[Mono] Exception in Godot.Object ctor: %s\n", msg ? msg : "unknown");
		mono_free(msg);
		intptr_t ptr_val = (intptr_t)p_obj;
		return mono_value_box(domain, system_intptr_class, &ptr_val);
	}

	tie_native_ptr(cs_obj, p_obj);
	return cs_obj;
}

MonoDomain *get_domain() { return domain; }
MonoClass *get_godot_object_class() { return godot_object_class; }
MonoClass *get_godot_node_class() { return godot_node_class; }

void cache_godot_classes(MonoImage *p_godot_image) {
	godot_object_class = mono_class_from_name(p_godot_image, "Godot", "Object");
	godot_node_class = mono_class_from_name(p_godot_image, "Godot", "Node");
	if (!godot_object_class) {
		printf("[Mono] WARNING: Godot.Object class not found in GodotSharp.\n");
	} else {
		printf("[Mono] Godot classes cached (Object: %p, Node: %p).\n", (void *)godot_object_class, (void *)godot_node_class);
	}
}

}
