#include "mono_variant.h"
#include "mono_bridge.h"
#include "mono_gc_bridge.h"
#include "core/object/object.h"
#include "core/string/ustring.h"
#include <mono/metadata/object.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/appdomain.h>
#include <mono/metadata/debug-helpers.h>
#include <cstdio>

namespace mono_variant {

static MonoClass *mono_class_boolean = nullptr;
static MonoClass *mono_class_int32 = nullptr;
static MonoClass *mono_class_int64 = nullptr;
static MonoClass *mono_class_single = nullptr;
static MonoClass *mono_class_double = nullptr;
static MonoClass *mono_class_string = nullptr;
static MonoClass *mono_class_object = nullptr;
static MonoClass *mono_class_intptr = nullptr;

void cache_mono_corlib_classes() {
	MonoImage *corlib = mono_get_corlib();
	mono_class_boolean = mono_class_from_name(corlib, "System", "Boolean");
	mono_class_int32 = mono_class_from_name(corlib, "System", "Int32");
	mono_class_int64 = mono_class_from_name(corlib, "System", "Int64");
	mono_class_single = mono_class_from_name(corlib, "System", "Single");
	mono_class_double = mono_class_from_name(corlib, "System", "Double");
	mono_class_string = mono_class_from_name(corlib, "System", "String");
	mono_class_object = mono_class_from_name(corlib, "System", "Object");
	mono_class_intptr = mono_class_from_name(corlib, "System", "IntPtr");
	printf("[Mono] Cached corlib classes.\n");
}

MonoClass *get_intptr_class() {
	return mono_class_intptr;
}

static MonoClassField *find_nativeptr_field(MonoClass *p_klass) {
	for (MonoClass *k = p_klass; k; k = mono_class_get_parent(k)) {
		MonoClassField *field = mono_class_get_field_from_name(k, "NativePtr");
		if (field) return field;
	}
	return nullptr;
}

static Object *extract_godot_object(MonoObject *p_obj) {
	if (!p_obj) return nullptr;
	MonoClass *godot_obj_class = mono_bridge::get_godot_object_class();
	if (!godot_obj_class) return nullptr;
	MonoClass *klass = mono_object_get_class(p_obj);
	if (!mono_class_is_subclass_of(klass, godot_obj_class, true)) return nullptr;
	MonoClassField *field = find_nativeptr_field(klass);
	if (!field) return nullptr;
	intptr_t ptr_val = 0;
	mono_field_get_value(p_obj, field, &ptr_val);
	if (ptr_val == 0) return nullptr;
	return (Object *)ptr_val;
}

MonoObject *variant_to_mono_object(MonoDomain *p_domain, const Variant &p_variant) {
	switch (p_variant.get_type()) {
		case Variant::NIL:
			return nullptr;
		case Variant::BOOL:
			return variant_to_mono_bool(p_domain, p_variant);
		case Variant::INT:
			return variant_to_mono_int(p_domain, (int64_t)p_variant);
		case Variant::FLOAT:
			return variant_to_mono_float(p_domain, (double)p_variant);
		case Variant::STRING:
		case Variant::STRING_NAME:
			return reinterpret_cast<MonoObject *>(variant_to_mono_string(p_domain, p_variant));
		case Variant::OBJECT: {
			Object *obj = p_variant;
			if (!obj) return nullptr;
			MonoClass *godot_node_class = mono_bridge::get_godot_node_class();
			MonoClass *target_class = godot_node_class ? godot_node_class : mono_bridge::get_godot_object_class();
			if (obj->is_class("Node") && godot_node_class) {
				target_class = godot_node_class;
			} else {
				target_class = mono_bridge::get_godot_object_class();
			}
			return mono_bridge::managed_get_or_create(obj, target_class);
		}
		default: {
			String str = p_variant.operator String();
			return reinterpret_cast<MonoObject *>(variant_to_mono_string(p_domain, str));
		}
	}
}

Variant mono_object_to_variant(MonoObject *p_obj) {
	if (!p_obj) return Variant();
	MonoClass *klass = mono_object_get_class(p_obj);
	if (klass == mono_class_boolean) return mono_object_to_bool(p_obj);
	if (klass == mono_class_int32) return (int)mono_object_to_int(p_obj);
	if (klass == mono_class_int64) return mono_object_to_int(p_obj);
	if (klass == mono_class_single) return (float)mono_object_to_float(p_obj);
	if (klass == mono_class_double) return mono_object_to_float(p_obj);
	if (klass == mono_class_intptr) return (int64_t)mono_object_to_intptr(p_obj);
	if (klass == mono_class_string) return mono_object_to_native_string(p_obj);
	Object *godot_obj = extract_godot_object(p_obj);
	if (godot_obj) return Variant(godot_obj);
	bool ok;
	int64_t i = mono_object_to_int(p_obj, &ok);
	if (ok) return i;
	intptr_t ip = mono_object_to_intptr(p_obj, &ok);
	if (ok) return (int64_t)ip;
	return Variant(mono_object_to_native_string(p_obj));
}

MonoString *variant_to_mono_string(MonoDomain *p_domain, const Variant &p_variant) {
	String str = p_variant.operator String();
	return mono_string_new(p_domain, str.utf8().get_data());
}

MonoObject *variant_to_mono_bool(MonoDomain *p_domain, bool p_val) {
	return mono_value_box(p_domain, mono_class_boolean, &p_val);
}

MonoObject *variant_to_mono_int(MonoDomain *p_domain, int64_t p_val) {
	int32_t i32 = (int32_t)p_val;
	if (p_val == (int64_t)i32) {
		return mono_value_box(p_domain, mono_class_int32, &i32);
	}
	return mono_value_box(p_domain, mono_class_int64, &p_val);
}

MonoObject *variant_to_mono_float(MonoDomain *p_domain, double p_val) {
	return mono_value_box(p_domain, mono_class_double, &p_val);
}

MonoObject *variant_to_mono_intptr(MonoDomain *p_domain, intptr_t p_val) {
	return mono_value_box(mono_domain_get(), mono_class_intptr, &p_val);
}

bool mono_object_to_bool(MonoObject *p_obj, bool *r_ok) {
	if (!p_obj || mono_object_get_class(p_obj) != mono_class_boolean) {
		if (r_ok) *r_ok = false;
		return false;
	}
	char *val = reinterpret_cast<char *>(mono_object_unbox(p_obj));
	bool result = *(bool *)val;
	if (r_ok) *r_ok = true;
	return result;
}

int64_t mono_object_to_int(MonoObject *p_obj, bool *r_ok) {
	if (!p_obj) {
		if (r_ok) *r_ok = false;
		return 0;
	}
	MonoClass *k = mono_object_get_class(p_obj);
	char *val = reinterpret_cast<char *>(mono_object_unbox(p_obj));
	if (k == mono_class_int32) {
		if (r_ok) *r_ok = true;
		return *(int32_t *)val;
	}
	if (k == mono_class_int64) {
		if (r_ok) *r_ok = true;
		return *(int64_t *)val;
	}
	if (r_ok) *r_ok = false;
	return 0;
}

double mono_object_to_float(MonoObject *p_obj, bool *r_ok) {
	if (!p_obj) {
		if (r_ok) *r_ok = false;
		return 0.0;
	}
	MonoClass *k = mono_object_get_class(p_obj);
	char *val = reinterpret_cast<char *>(mono_object_unbox(p_obj));
	if (k == mono_class_single) {
		if (r_ok) *r_ok = true;
		return *(float *)val;
	}
	if (k == mono_class_double) {
		if (r_ok) *r_ok = true;
		return *(double *)val;
	}
	if (r_ok) *r_ok = false;
	return 0.0;
}

intptr_t mono_object_to_intptr(MonoObject *p_obj, bool *r_ok) {
	if (!p_obj) {
		if (r_ok) *r_ok = false;
		return 0;
	}
	MonoClass *k = mono_object_get_class(p_obj);
	if (k != mono_class_intptr) {
		if (r_ok) *r_ok = false;
		return 0;
	}
	char *val = reinterpret_cast<char *>(mono_object_unbox(p_obj));
	if (r_ok) *r_ok = true;
	return *(intptr_t *)val;
}

String mono_object_to_native_string(MonoObject *p_obj) {
	if (!p_obj) return String();
	MonoString *str = mono_object_to_string(p_obj, nullptr);
	if (!str) return String();
	char *utf8 = mono_string_to_utf8(str);
	String result(utf8);
	mono_free(utf8);
	return result;
}

Object *mono_object_to_godot_object(MonoObject *p_obj) {
	bool ok;
	intptr_t ptr = mono_object_to_intptr(p_obj, &ok);
	if (ok) return (Object *)ptr;
	return nullptr;
}

}
