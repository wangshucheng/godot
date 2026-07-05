#include "mono_variant.h"
#include "mono_bridge.h"
#include "mono_gc_bridge.h"
#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/color.h"
#include "core/math/rect2.h"
#include <mono/metadata/object.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/appdomain.h>
#include <mono/metadata/debug-helpers.h>
#include <cstdio>
#include <cstring>

namespace mono_variant {

static MonoClass *mono_class_boolean = nullptr;
static MonoClass *mono_class_int32 = nullptr;
static MonoClass *mono_class_int64 = nullptr;
static MonoClass *mono_class_single = nullptr;
static MonoClass *mono_class_double = nullptr;
static MonoClass *mono_class_string = nullptr;
static MonoClass *mono_class_object = nullptr;
static MonoClass *mono_class_intptr = nullptr;

static MonoClass *mono_class_vector2 = nullptr;
static MonoClass *mono_class_vector3 = nullptr;
static MonoClass *mono_class_color = nullptr;
static MonoClass *mono_class_rect2 = nullptr;

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

void cache_godot_math_classes(MonoImage *p_godot_image) {
	mono_class_vector2 = mono_class_from_name(p_godot_image, "Godot", "Vector2");
	mono_class_vector3 = mono_class_from_name(p_godot_image, "Godot", "Vector3");
	mono_class_color = mono_class_from_name(p_godot_image, "Godot", "Color");
	mono_class_rect2 = mono_class_from_name(p_godot_image, "Godot", "Rect2");
	printf("[Mono] Cached Godot math classes.\n");
}

MonoClass *get_intptr_class() { return mono_class_intptr; }
MonoClass *get_vector2_class() { return mono_class_vector2; }
MonoClass *get_vector3_class() { return mono_class_vector3; }
MonoClass *get_color_class() { return mono_class_color; }
MonoClass *get_rect2_class() { return mono_class_rect2; }

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
		case Variant::VECTOR2: {
			::Vector2 v = p_variant;
			return variant_to_mono_vector2(p_domain, v.x, v.y);
		}
		case Variant::VECTOR3: {
			::Vector3 v = p_variant;
			return variant_to_mono_vector3(p_domain, v.x, v.y, v.z);
		}
		case Variant::COLOR: {
			::Color c = p_variant;
			return variant_to_mono_color(p_domain, c.r, c.g, c.b, c.a);
		}
		case Variant::RECT2: {
			::Rect2 r = p_variant;
			return variant_to_mono_rect2(p_domain, r.position.x, r.position.y, r.size.x, r.size.y);
		}
		case Variant::OBJECT: {
			Object *obj = p_variant;
			if (!obj) return nullptr;
			MonoClass *target_class = mono_bridge::get_mono_class_for_object(obj);
			return mono_bridge::managed_get_or_create(obj, target_class);
		}
		default: {
			String str = p_variant.operator String();
			return reinterpret_cast<MonoObject *>(variant_to_mono_string(p_domain, str));
		}
	}
}

static void set_struct_fields(MonoObject *p_obj, MonoClass *p_class, const char **p_names, const float *p_values, int p_count) {
	if (!p_obj || !p_class) return;
	for (int i = 0; i < p_count; i++) {
		MonoClassField *field = mono_class_get_field_from_name(p_class, p_names[i]);
		if (field) {
			float val = p_values[i];
			mono_field_set_value(p_obj, field, &val);
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

	if (mono_class_vector2 && klass == mono_class_vector2) {
		real_t x = 0, y = 0;
		if (mono_object_to_vector2(p_obj, &x, &y)) return ::Vector2(x, y);
	}
	if (mono_class_vector3 && klass == mono_class_vector3) {
		real_t x = 0, y = 0, z = 0;
		if (mono_object_to_vector3(p_obj, &x, &y, &z)) return ::Vector3(x, y, z);
	}
	if (mono_class_color && klass == mono_class_color) {
		float r = 0, g = 0, b = 0, a = 0;
		if (mono_object_to_color(p_obj, &r, &g, &b, &a)) return ::Color(r, g, b, a);
	}
	if (mono_class_rect2 && klass == mono_class_rect2) {
		real_t x = 0, y = 0, w = 0, h = 0;
		if (mono_object_to_rect2(p_obj, &x, &y, &w, &h)) return ::Rect2(x, y, w, h);
	}

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

MonoObject *variant_to_mono_vector2(MonoDomain *p_domain, real_t p_x, real_t p_y) {
	if (!mono_class_vector2) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, mono_class_vector2);
	if (!obj) return nullptr;
	float fx = (float)p_x, fy = (float)p_y;
	const char *names[] = {"x", "y"};
	float vals[] = {fx, fy};
	set_struct_fields(obj, mono_class_vector2, names, vals, 2);
	return obj;
}

MonoObject *variant_to_mono_vector3(MonoDomain *p_domain, real_t p_x, real_t p_y, real_t p_z) {
	if (!mono_class_vector3) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, mono_class_vector3);
	if (!obj) return nullptr;
	float fx = (float)p_x, fy = (float)p_y, fz = (float)p_z;
	const char *names[] = {"x", "y", "z"};
	float vals[] = {fx, fy, fz};
	set_struct_fields(obj, mono_class_vector3, names, vals, 3);
	return obj;
}

MonoObject *variant_to_mono_color(MonoDomain *p_domain, float p_r, float p_g, float p_b, float p_a) {
	if (!mono_class_color) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, mono_class_color);
	if (!obj) return nullptr;
	const char *names[] = {"r", "g", "b", "a"};
	float vals[] = {p_r, p_g, p_b, p_a};
	set_struct_fields(obj, mono_class_color, names, vals, 4);
	return obj;
}

MonoObject *variant_to_mono_rect2(MonoDomain *p_domain, real_t p_x, real_t p_y, real_t p_w, real_t p_h) {
	if (!mono_class_rect2) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, mono_class_rect2);
	if (!obj) return nullptr;
	const char *names[] = {"position", "size"};
	MonoObject *pos = variant_to_mono_vector2(p_domain, p_x, p_y);
	MonoObject *siz = variant_to_mono_vector2(p_domain, p_w, p_h);
	if (pos) {
		MonoClassField *fp = mono_class_get_field_from_name(mono_class_rect2, "position");
		if (fp) mono_field_set_value(obj, fp, &pos);
	}
	if (siz) {
		MonoClassField *fs = mono_class_get_field_from_name(mono_class_rect2, "size");
		if (fs) mono_field_set_value(obj, fs, &siz);
	}
	return obj;
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

static bool get_struct_float_field(MonoObject *p_obj, MonoClass *p_class, const char *p_name, float &r_val) {
	if (!p_obj || !p_class) return false;
	MonoClassField *field = mono_class_get_field_from_name(p_class, p_name);
	if (!field) return false;
	mono_field_get_value(p_obj, field, &r_val);
	return true;
}

bool mono_object_to_vector2(MonoObject *p_obj, real_t *r_x, real_t *r_y) {
	if (!p_obj || !mono_class_vector2 || mono_object_get_class(p_obj) != mono_class_vector2) return false;
	float x = 0, y = 0;
	bool ok = get_struct_float_field(p_obj, mono_class_vector2, "x", x);
	ok &= get_struct_float_field(p_obj, mono_class_vector2, "y", y);
	if (ok) {
		if (r_x) *r_x = (real_t)x;
		if (r_y) *r_y = (real_t)y;
	}
	return ok;
}

bool mono_object_to_vector3(MonoObject *p_obj, real_t *r_x, real_t *r_y, real_t *r_z) {
	if (!p_obj || !mono_class_vector3 || mono_object_get_class(p_obj) != mono_class_vector3) return false;
	float x = 0, y = 0, z = 0;
	bool ok = get_struct_float_field(p_obj, mono_class_vector3, "x", x);
	ok &= get_struct_float_field(p_obj, mono_class_vector3, "y", y);
	ok &= get_struct_float_field(p_obj, mono_class_vector3, "z", z);
	if (ok) {
		if (r_x) *r_x = (real_t)x;
		if (r_y) *r_y = (real_t)y;
		if (r_z) *r_z = (real_t)z;
	}
	return ok;
}

bool mono_object_to_color(MonoObject *p_obj, float *r_r, float *r_g, float *r_b, float *r_a) {
	if (!p_obj || !mono_class_color || mono_object_get_class(p_obj) != mono_class_color) return false;
	float r = 0, g = 0, b = 0, a = 0;
	bool ok = get_struct_float_field(p_obj, mono_class_color, "r", r);
	ok &= get_struct_float_field(p_obj, mono_class_color, "g", g);
	ok &= get_struct_float_field(p_obj, mono_class_color, "b", b);
	ok &= get_struct_float_field(p_obj, mono_class_color, "a", a);
	if (ok) {
		if (r_r) *r_r = r;
		if (r_g) *r_g = g;
		if (r_b) *r_b = b;
		if (r_a) *r_a = a;
	}
	return ok;
}

bool mono_object_to_rect2(MonoObject *p_obj, real_t *r_x, real_t *r_y, real_t *r_w, real_t *r_h) {
	if (!p_obj || !mono_class_rect2 || mono_object_get_class(p_obj) != mono_class_rect2) return false;
	MonoClassField *pos_field = mono_class_get_field_from_name(mono_class_rect2, "position");
	MonoClassField *size_field = mono_class_get_field_from_name(mono_class_rect2, "size");
	if (!pos_field || !size_field) return false;
	MonoObject *pos_obj = nullptr;
	MonoObject *size_obj = nullptr;
	mono_field_get_value(p_obj, pos_field, &pos_obj);
	mono_field_get_value(p_obj, size_field, &size_obj);
	real_t px = 0, py = 0, sx = 0, sy = 0;
	bool ok = mono_object_to_vector2(pos_obj, &px, &py);
	ok &= mono_object_to_vector2(size_obj, &sx, &sy);
	if (ok) {
		if (r_x) *r_x = px;
		if (r_y) *r_y = py;
		if (r_w) *r_w = sx;
		if (r_h) *r_h = sy;
	}
	return ok;
}

}
