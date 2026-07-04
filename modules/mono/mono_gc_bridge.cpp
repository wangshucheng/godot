#include "mono_gc_bridge.h"
#include "core/object/ref_counted.h"
#include "core/error/error_macros.h"
#include <cstdio>
#include <cstdint>

namespace mono_gc_bridge {

static MonoDomain *domain = nullptr;

struct ObjectBinding {
	uint32_t weak_gchandle;
	Object *native_ptr;
};

static HashMap<Object *, ObjectBinding> native_to_managed;
static HashMap<uint32_t, Object *> managed_to_native;

static MonoClassField *find_nativeptr_field(MonoClass *p_klass) {
	for (MonoClass *k = p_klass; k; k = mono_class_get_parent(k)) {
		MonoClassField *field = mono_class_get_field_from_name(k, "NativePtr");
		if (field) return field;
	}
	return nullptr;
}

void init(MonoDomain *p_domain) {
	domain = p_domain;
	printf("[Mono] GC bridge initialized.\n");
}

void shutdown() {
	for (auto &pair : native_to_managed) {
		mono_gchandle_free(pair.value.weak_gchandle);
	}
	native_to_managed.clear();
	managed_to_native.clear();
	domain = nullptr;
	printf("[Mono] GC bridge shut down.\n");
}

uint32_t tie_managed_to_native(MonoObject *p_cs_obj, Object *p_native_obj, bool p_weak) {
	if (!p_cs_obj || !p_native_obj) return 0;

	if (native_to_managed.has(p_native_obj)) {
		uint32_t old = native_to_managed[p_native_obj].weak_gchandle;
		mono_gchandle_free(old);
		managed_to_native.erase(old);
	}

	uint32_t gch;
	if (p_weak) {
		gch = mono_gchandle_new_weakref(p_cs_obj, true);
	} else {
		gch = mono_gchandle_new(p_cs_obj, false);
	}

	ObjectBinding binding;
	binding.weak_gchandle = gch;
	binding.native_ptr = p_native_obj;
	native_to_managed[p_native_obj] = binding;
	managed_to_native[gch] = p_native_obj;

	return gch;
}

void notify_native_destroyed(Object *p_obj) {
	if (!p_obj || !native_to_managed.has(p_obj)) return;

	ObjectBinding &binding = native_to_managed[p_obj];

	MonoObject *cs_target = mono_gchandle_get_target(binding.weak_gchandle);
	if (cs_target && domain) {
		MonoClass *klass = mono_object_get_class(cs_target);
		MonoClassField *field = find_nativeptr_field(klass);
		if (field) {
			intptr_t zero = 0;
			mono_field_set_value(cs_target, field, &zero);
		}
	}

	mono_gchandle_free(binding.weak_gchandle);
	managed_to_native.erase(binding.weak_gchandle);
	native_to_managed.erase(p_obj);
}

MonoObject *get_managed(Object *p_native) {
	if (!p_native || !native_to_managed.has(p_native)) return nullptr;
	return mono_gchandle_get_target(native_to_managed[p_native].weak_gchandle);
}

Object *get_native(MonoObject *p_managed) {
	if (!p_managed) return nullptr;
	for (auto &pair : native_to_managed) {
		MonoObject *t = mono_gchandle_get_target(pair.value.weak_gchandle);
		if (t == p_managed) return pair.key;
	}
	return nullptr;
}

bool is_native_alive(Object *p_native) {
	return p_native && native_to_managed.has(p_native);
}

void object_predelete_notification(Object *p_obj) {
	notify_native_destroyed(p_obj);
}

}
