#include "mono_variant.h"
#include "mono_bridge.h"
#include "mono_gc_bridge.h"
#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/math/vector2.h"
#include "core/math/vector2i.h"
#include "core/math/vector3.h"
#include "core/math/vector3i.h"
#include "core/math/vector4.h"
#include "core/math/vector4i.h"
#include "core/math/color.h"
#include "core/math/rect2.h"
#include "core/math/rect2i.h"
#include "core/math/quaternion.h"
#include "core/math/plane.h"
#include "core/math/basis.h"
#include "core/math/transform_2d.h"
#include "core/math/transform_3d.h"
#include "core/math/aabb.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/string/node_path.h"
#include "core/templates/rid.h"
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

// Extended math types
static MonoClass *mono_class_vector4 = nullptr;
static MonoClass *mono_class_vector2i = nullptr;
static MonoClass *mono_class_vector3i = nullptr;
static MonoClass *mono_class_vector4i = nullptr;
static MonoClass *mono_class_rect2i = nullptr;
static MonoClass *mono_class_quaternion = nullptr;
static MonoClass *mono_class_plane = nullptr;
static MonoClass *mono_class_aabb = nullptr;
static MonoClass *mono_class_basis = nullptr;
static MonoClass *mono_class_transform2d = nullptr;
static MonoClass *mono_class_transform3d = nullptr;

// M10: Godot.Collections.Array / Dictionary class caches
static MonoClass *mono_class_godot_array = nullptr;
static MonoClass *mono_class_godot_dictionary = nullptr;

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
	fflush(stdout);
}

void cache_godot_math_classes(MonoImage *p_godot_image) {
	mono_class_vector2 = mono_class_from_name(p_godot_image, "Godot", "Vector2");
	mono_class_vector3 = mono_class_from_name(p_godot_image, "Godot", "Vector3");
	mono_class_color = mono_class_from_name(p_godot_image, "Godot", "Color");
	mono_class_rect2 = mono_class_from_name(p_godot_image, "Godot", "Rect2");
	// Extended math types
	mono_class_vector4 = mono_class_from_name(p_godot_image, "Godot", "Vector4");
	mono_class_vector2i = mono_class_from_name(p_godot_image, "Godot", "Vector2I");
	mono_class_vector3i = mono_class_from_name(p_godot_image, "Godot", "Vector3I");
	mono_class_vector4i = mono_class_from_name(p_godot_image, "Godot", "Vector4I");
	mono_class_rect2i = mono_class_from_name(p_godot_image, "Godot", "Rect2I");
	mono_class_quaternion = mono_class_from_name(p_godot_image, "Godot", "Quaternion");
	mono_class_plane = mono_class_from_name(p_godot_image, "Godot", "Plane");
	mono_class_aabb = mono_class_from_name(p_godot_image, "Godot", "Aabb");
	mono_class_basis = mono_class_from_name(p_godot_image, "Godot", "Basis");
	mono_class_transform2d = mono_class_from_name(p_godot_image, "Godot", "Transform2D");
	mono_class_transform3d = mono_class_from_name(p_godot_image, "Godot", "Transform3D");
	// M10: Godot.Collections.Array/Dictionary (the C# wrappers own a
	// heap-allocated Array*/Dictionary* via NativePtr). These are looked up
	// by namespace "Godot.Collections" + class name.
	mono_class_godot_array = mono_class_from_name(p_godot_image, "Godot.Collections", "Array");
	mono_class_godot_dictionary = mono_class_from_name(p_godot_image, "Godot.Collections", "Dictionary");
	printf("[Mono] Cached Godot math classes (extended).\n");
	fflush(stdout);
}

MonoClass *get_intptr_class() { return mono_class_intptr; }
MonoClass *get_vector2_class() { return mono_class_vector2; }
MonoClass *get_vector3_class() { return mono_class_vector3; }
MonoClass *get_color_class() { return mono_class_color; }
MonoClass *get_rect2_class() { return mono_class_rect2; }
MonoClass *get_vector4_class() { return mono_class_vector4; }
MonoClass *get_quaternion_class() { return mono_class_quaternion; }
MonoClass *get_basis_class() { return mono_class_basis; }
MonoClass *get_transform3d_class() { return mono_class_transform3d; }
MonoClass *get_transform2d_class() { return mono_class_transform2d; }
MonoClass *get_plane_class() { return mono_class_plane; }
MonoClass *get_aabb_class() { return mono_class_aabb; }
MonoClass *get_vector2i_class() { return mono_class_vector2i; }
MonoClass *get_vector3i_class() { return mono_class_vector3i; }
MonoClass *get_vector4i_class() { return mono_class_vector4i; }
MonoClass *get_rect2i_class() { return mono_class_rect2i; }

// M10: Godot.Collections.Array/Dictionary class accessors
MonoClass *get_godot_array_class() { return mono_class_godot_array; }
MonoClass *get_godot_dictionary_class() { return mono_class_godot_dictionary; }

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
		case Variant::VECTOR2I: {
			::Vector2i v = p_variant;
			return variant_to_mono_vector2i(p_domain, v.x, v.y);
		}
		case Variant::VECTOR3: {
			::Vector3 v = p_variant;
			return variant_to_mono_vector3(p_domain, v.x, v.y, v.z);
		}
		case Variant::VECTOR3I: {
			::Vector3i v = p_variant;
			return variant_to_mono_vector3i(p_domain, v.x, v.y, v.z);
		}
		case Variant::VECTOR4: {
			::Vector4 v = p_variant;
			return variant_to_mono_vector4(p_domain, v.x, v.y, v.z, v.w);
		}
		case Variant::VECTOR4I: {
			::Vector4i v = p_variant;
			return variant_to_mono_vector4i(p_domain, v.x, v.y, v.z, v.w);
		}
		case Variant::COLOR: {
			::Color c = p_variant;
			return variant_to_mono_color(p_domain, c.r, c.g, c.b, c.a);
		}
		case Variant::RECT2: {
			::Rect2 r = p_variant;
			return variant_to_mono_rect2(p_domain, r.position.x, r.position.y, r.size.x, r.size.y);
		}
		case Variant::RECT2I: {
			::Rect2i r = p_variant;
			return variant_to_mono_rect2i(p_domain, r.position.x, r.position.y, r.size.x, r.size.y);
		}
		case Variant::QUATERNION: {
			::Quaternion q = p_variant;
			return variant_to_mono_quaternion(p_domain, q.x, q.y, q.z, q.w);
		}
		case Variant::PLANE: {
			::Plane pl = p_variant;
			::Vector3 n = pl.get_normal();
			return variant_to_mono_plane(p_domain, n.x, n.y, n.z, pl.d);
		}
		case Variant::AABB: {
			::AABB a = p_variant;
			return variant_to_mono_aabb(p_domain, a.position.x, a.position.y, a.position.z,
					a.size.x, a.size.y, a.size.z);
		}
		case Variant::BASIS: {
			::Basis b = p_variant;
			real_t elements[9];
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					elements[i * 3 + j] = b.rows[i][j];
				}
			}
			return variant_to_mono_basis(p_domain, elements);
		}
		case Variant::TRANSFORM2D: {
			::Transform2D t = p_variant;
			real_t elements[6];
			for (int i = 0; i < 3; i++) {
				elements[i * 2 + 0] = t.columns[i].x;
				elements[i * 2 + 1] = t.columns[i].y;
			}
			return variant_to_mono_transform2d(p_domain, elements);
		}
		case Variant::TRANSFORM3D: {
			::Transform3D t = p_variant;
			real_t basis_elements[9];
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					basis_elements[i * 3 + j] = t.basis.rows[i][j];
				}
			}
			return variant_to_mono_transform3d(p_domain, basis_elements, t.origin.x, t.origin.y, t.origin.z);
		}
		case Variant::NODE_PATH: {
			NodePath np = p_variant;
			String str = np.operator String();
			return reinterpret_cast<MonoObject *>(variant_to_mono_string(p_domain, str));
		}
		case Variant::RID: {
			RID rid = p_variant;
			return variant_to_mono_int(p_domain, (int64_t)rid.get_id());
		}
		case Variant::OBJECT: {
			Object *obj = p_variant;
			if (!obj) return nullptr;
			MonoClass *target_class = mono_bridge::get_mono_class_for_object(obj);
			return mono_bridge::managed_get_or_create(obj, target_class);
		}
		case Variant::ARRAY: {
			// M10: wrap the Array in a Godot.Collections.Array C# wrapper that
			// owns a heap-allocated copy of the source Array. The previous
			// behavior stringified the Array, which broke any engine API
			// returning Array (InvalidCastException on the C# cast).
			Array arr = p_variant;
			return variant_to_mono_array(p_domain, arr);
		}
		case Variant::DICTIONARY: {
			// M10: same as ARRAY above — wrap in Godot.Collections.Dictionary.
			Dictionary dict = p_variant;
			return variant_to_mono_dictionary(p_domain, dict);
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

static void set_struct_int_fields(MonoObject *p_obj, MonoClass *p_class, const char **p_names, const int32_t *p_values, int p_count) {
	if (!p_obj || !p_class) return;
	for (int i = 0; i < p_count; i++) {
		MonoClassField *field = mono_class_get_field_from_name(p_class, p_names[i]);
		if (field) {
			int32_t val = p_values[i];
			mono_field_set_value(p_obj, field, &val);
		}
	}
}

static bool get_struct_int_field(MonoObject *p_obj, MonoClass *p_class, const char *p_name, int32_t &r_val) {
	if (!p_obj || !p_class) return false;
	MonoClassField *field = mono_class_get_field_from_name(p_class, p_name);
	if (!field) return false;
	mono_field_get_value(p_obj, field, &r_val);
	return true;
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

	// M10: Godot.Collections.Array/Dictionary wrappers. They are NOT Godot.Object
	// subclasses (extract_godot_object returned nullptr), so we identify them
	// by class cache and read their NativePtr (which points to a
	// heap-allocated Array*/Dictionary* owned by the wrapper).
	if (mono_class_godot_array && klass == mono_class_godot_array) {
		MonoClassField *field = find_nativeptr_field(klass);
		if (field) {
			intptr_t ptr_val = 0;
			mono_field_get_value(p_obj, field, &ptr_val);
			if (ptr_val != 0) {
				Array *arr = reinterpret_cast<Array *>(ptr_val);
				return Variant(*arr);
			}
		}
		return Variant(Array());
	}
	if (mono_class_godot_dictionary && klass == mono_class_godot_dictionary) {
		MonoClassField *field = find_nativeptr_field(klass);
		if (field) {
			intptr_t ptr_val = 0;
			mono_field_get_value(p_obj, field, &ptr_val);
			if (ptr_val != 0) {
				Dictionary *dict = reinterpret_cast<Dictionary *>(ptr_val);
				return Variant(*dict);
			}
		}
		return Variant(Dictionary());
	}

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
	if (mono_class_vector4 && klass == mono_class_vector4) {
		real_t x = 0, y = 0, z = 0, w = 0;
		if (mono_object_to_vector4(p_obj, &x, &y, &z, &w)) return ::Vector4(x, y, z, w);
	}
	if (mono_class_vector2i && klass == mono_class_vector2i) {
		int32_t x = 0, y = 0;
		if (mono_object_to_vector2i(p_obj, &x, &y)) return ::Vector2i(x, y);
	}
	if (mono_class_vector3i && klass == mono_class_vector3i) {
		int32_t x = 0, y = 0, z = 0;
		if (mono_object_to_vector3i(p_obj, &x, &y, &z)) return ::Vector3i(x, y, z);
	}
	if (mono_class_vector4i && klass == mono_class_vector4i) {
		int32_t x = 0, y = 0, z = 0, w = 0;
		if (mono_object_to_vector4i(p_obj, &x, &y, &z, &w)) return ::Vector4i(x, y, z, w);
	}
	if (mono_class_rect2i && klass == mono_class_rect2i) {
		int32_t x = 0, y = 0, w = 0, h = 0;
		if (mono_object_to_rect2i(p_obj, &x, &y, &w, &h)) return ::Rect2i(x, y, w, h);
	}
	if (mono_class_quaternion && klass == mono_class_quaternion) {
		real_t x = 0, y = 0, z = 0, w = 0;
		if (mono_object_to_quaternion(p_obj, &x, &y, &z, &w)) return ::Quaternion(x, y, z, w);
	}
	if (mono_class_plane && klass == mono_class_plane) {
		real_t nx = 0, ny = 0, nz = 0, d = 0;
		if (mono_object_to_plane(p_obj, &nx, &ny, &nz, &d)) return ::Plane(::Vector3(nx, ny, nz), d);
	}
	if (mono_class_aabb && klass == mono_class_aabb) {
		real_t px = 0, py = 0, pz = 0, sx = 0, sy = 0, sz = 0;
		if (mono_object_to_aabb(p_obj, &px, &py, &pz, &sx, &sy, &sz)) return ::AABB(::Vector3(px, py, pz), ::Vector3(sx, sy, sz));
	}
	// Basis, Transform2D, Transform3D: read by fields (more complex, fallback to string for now)

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
	// Godot Variant::INT is always int64, and the generated C# bindings unbox
	// int getters as `(long)` (Int64). Dynamically boxing small values as
	// Int32 broke this contract: any long getter receiving an Int32 box threw
	// InvalidCastException at runtime, and int (Int32) getters also broke for
	// values outside the int32 range. Always box as Int64 to match the
	// binding contract. (H6)
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
	// Rect2.position and Rect2.size are value-type Vector2 fields.
	// mono_field_set_value copies sizeof(field) bytes from the source pointer
	// into the field. We pass a pointer to a 2-float buffer (matching the
	// memory layout of Vector2: float x, float y), so the bytes get written
	// directly into the parent's value-type field. The previous code created a
	// boxed Vector2 MonoObject and passed &box, which wrote a MonoObject*
	// pointer value (8 bytes of pointer) into a field expecting 8 bytes of
	// (x, y) float data — corrupting the Rect2 layout. (H4 / M9)
	float pos_vals[2] = { (float)p_x, (float)p_y };
	float siz_vals[2] = { (float)p_w, (float)p_h };
	MonoClassField *fp = mono_class_get_field_from_name(mono_class_rect2, "position");
	if (fp) mono_field_set_value(obj, fp, pos_vals);
	MonoClassField *fs = mono_class_get_field_from_name(mono_class_rect2, "size");
	if (fs) mono_field_set_value(obj, fs, siz_vals);
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
	// P10 encoding fix: String(const char*) appends as Latin-1, which mangles
	// any non-ASCII (UTF-8) text. Use String::utf8() to decode properly.
	String result = String::utf8(utf8);
	mono_free(utf8);
	return result;
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
	// Rect2.position and Rect2.size are value-type Vector2 fields (layout:
	// float x, y = 8 bytes). mono_field_get_value copies sizeof(field) bytes
	// into the destination buffer. We read into 2-float buffers and interpret
	// the bytes as (x, y) directly. The previous code read into a MonoObject*
	// variable (8 bytes) and then called mono_object_to_vector2 on the
	// "pointer" — which actually contained the (x, y) float bytes reinterpreted
	// as a pointer value, causing crashes / garbage. This was masked before
	// the write-side fix because the field previously held a real MonoObject*
	// pointer; the write-side fix (variant_to_mono_rect2) now stores the raw
	// (x, y) floats, so the read side must match. (M9 read side, paired with
	// H4 write side fix.)
	float pos_buf[2] = { 0, 0 };
	float size_buf[2] = { 0, 0 };
	mono_field_get_value(p_obj, pos_field, pos_buf);
	mono_field_get_value(p_obj, size_field, size_buf);
	if (r_x) *r_x = pos_buf[0];
	if (r_y) *r_y = pos_buf[1];
	if (r_w) *r_w = size_buf[0];
	if (r_h) *r_h = size_buf[1];
	return true;
}

// ============================================================
// Extended math type conversions
// ============================================================

MonoObject *variant_to_mono_vector4(MonoDomain *p_domain, real_t p_x, real_t p_y, real_t p_z, real_t p_w) {
	if (!mono_class_vector4) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, mono_class_vector4);
	if (!obj) return nullptr;
	float fx = (float)p_x, fy = (float)p_y, fz = (float)p_z, fw = (float)p_w;
	const char *names[] = {"x", "y", "z", "w"};
	float vals[] = {fx, fy, fz, fw};
	set_struct_fields(obj, mono_class_vector4, names, vals, 4);
	return obj;
}

MonoObject *variant_to_mono_vector2i(MonoDomain *p_domain, int32_t p_x, int32_t p_y) {
	if (!mono_class_vector2i) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, mono_class_vector2i);
	if (!obj) return nullptr;
	const char *names[] = {"x", "y"};
	int32_t vals[] = {p_x, p_y};
	set_struct_int_fields(obj, mono_class_vector2i, names, vals, 2);
	return obj;
}

MonoObject *variant_to_mono_vector3i(MonoDomain *p_domain, int32_t p_x, int32_t p_y, int32_t p_z) {
	if (!mono_class_vector3i) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, mono_class_vector3i);
	if (!obj) return nullptr;
	const char *names[] = {"x", "y", "z"};
	int32_t vals[] = {p_x, p_y, p_z};
	set_struct_int_fields(obj, mono_class_vector3i, names, vals, 3);
	return obj;
}

MonoObject *variant_to_mono_vector4i(MonoDomain *p_domain, int32_t p_x, int32_t p_y, int32_t p_z, int32_t p_w) {
	if (!mono_class_vector4i) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, mono_class_vector4i);
	if (!obj) return nullptr;
	const char *names[] = {"x", "y", "z", "w"};
	int32_t vals[] = {p_x, p_y, p_z, p_w};
	set_struct_int_fields(obj, mono_class_vector4i, names, vals, 4);
	return obj;
}

MonoObject *variant_to_mono_rect2i(MonoDomain *p_domain, int32_t p_x, int32_t p_y, int32_t p_w, int32_t p_h) {
	if (!mono_class_rect2i) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, mono_class_rect2i);
	if (!obj) return nullptr;
	// Rect2I.position and Rect2I.size are value-type Vector2I fields
	// (layout: int32_t x, int32_t y). mono_field_set_value copies sizeof(field)
	// bytes from the source pointer into the field. We pass a pointer to a
	// 2-int32 buffer so the bytes get written directly into the parent's
	// value-type field. The previous code created a boxed Vector2I MonoObject
	// and passed &box, which wrote a MonoObject* pointer value into a field
	// expecting 8 bytes of (x, y) int32 data — corrupting the Rect2I layout.
	// (M9, same pattern as H4 Rect2 fix)
	int32_t pos_vals[2] = { p_x, p_y };
	int32_t siz_vals[2] = { p_w, p_h };
	MonoClassField *fp = mono_class_get_field_from_name(mono_class_rect2i, "position");
	if (fp) mono_field_set_value(obj, fp, pos_vals);
	MonoClassField *fs = mono_class_get_field_from_name(mono_class_rect2i, "size");
	if (fs) mono_field_set_value(obj, fs, siz_vals);
	return obj;
}

MonoObject *variant_to_mono_quaternion(MonoDomain *p_domain, real_t p_x, real_t p_y, real_t p_z, real_t p_w) {
	if (!mono_class_quaternion) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, mono_class_quaternion);
	if (!obj) return nullptr;
	float fx = (float)p_x, fy = (float)p_y, fz = (float)p_z, fw = (float)p_w;
	const char *names[] = {"x", "y", "z", "w"};
	float vals[] = {fx, fy, fz, fw};
	set_struct_fields(obj, mono_class_quaternion, names, vals, 4);
	return obj;
}

MonoObject *variant_to_mono_plane(MonoDomain *p_domain, real_t p_nx, real_t p_ny, real_t p_nz, real_t p_d) {
	if (!mono_class_plane) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, mono_class_plane);
	if (!obj) return nullptr;
	// Plane.normal is a value-type Vector3 field (layout: float x, y, z) and
	// Plane.d is a value-type float field. mono_field_set_value copies
	// sizeof(field) bytes from the source pointer into the field. We pass a
	// 3-float buffer for the normal and a 1-float buffer for d. The previous
	// code created a boxed Vector3 MonoObject and passed &normal, which wrote a
	// MonoObject* pointer value (8 bytes of pointer) into a field expecting
	// 12 bytes of (x, y, z) float data — both corrupting the layout and
	// underwriting 4 bytes. (M9, same pattern as H4 Rect2 fix)
	float normal_vals[3] = { (float)p_nx, (float)p_ny, (float)p_nz };
	float d_val = (float)p_d;
	MonoClassField *fn = mono_class_get_field_from_name(mono_class_plane, "normal");
	if (fn) mono_field_set_value(obj, fn, normal_vals);
	MonoClassField *fd = mono_class_get_field_from_name(mono_class_plane, "d");
	if (fd) mono_field_set_value(obj, fd, &d_val);
	return obj;
}

MonoObject *variant_to_mono_aabb(MonoDomain *p_domain, real_t p_px, real_t p_py, real_t p_pz, real_t p_sx, real_t p_sy, real_t p_sz) {
	if (!mono_class_aabb) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, mono_class_aabb);
	if (!obj) return nullptr;
	// Aabb.position and Aabb.size are value-type Vector3 fields
	// (layout: float x, y, z = 12 bytes each). mono_field_set_value copies
	// sizeof(field) bytes from the source pointer into the field. We pass a
	// 3-float buffer so the bytes get written directly into the parent's
	// value-type field. The previous code created a boxed Vector3 MonoObject
	// and passed &pos, which wrote a MonoObject* pointer value (8 bytes) into
	// a field expecting 12 bytes of (x, y, z) float data — corrupting the
	// layout and leaving the last 4 bytes uninitialized. (M9, same pattern as
	// H4 Rect2 fix)
	float pos_vals[3] = { (float)p_px, (float)p_py, (float)p_pz };
	float siz_vals[3] = { (float)p_sx, (float)p_sy, (float)p_sz };
	MonoClassField *fp = mono_class_get_field_from_name(mono_class_aabb, "position");
	if (fp) mono_field_set_value(obj, fp, pos_vals);
	MonoClassField *fs = mono_class_get_field_from_name(mono_class_aabb, "size");
	if (fs) mono_field_set_value(obj, fs, siz_vals);
	return obj;
}

MonoObject *variant_to_mono_basis(MonoDomain *p_domain, const real_t *p_elements) {
	if (!mono_class_basis) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, mono_class_basis);
	if (!obj) return nullptr;
	// Basis has rows[3] (Vector3) or columns[3] (Vector3)
	// Try setting via Rows property or direct field
	const char *row_names[] = {"rows", "RowX", "RowY", "RowZ"};
	// Try to find a "rows" field (array of Vector3)
	MonoClassField *rows_field = mono_class_get_field_from_name(mono_class_basis, "rows");
	if (rows_field) {
		// rows is a fixed-size buffer of 3 Vector3 (each 12 bytes)
		// Directly write to the unboxed memory
		char *data = reinterpret_cast<char *>(mono_object_unbox(obj));
		int offset = mono_field_get_offset(rows_field) - (int)sizeof(MonoObject);
		if (offset >= 0) {
			for (int i = 0; i < 3; i++) {
				float *vec = reinterpret_cast<float *>(data + offset + i * 12);
				vec[0] = (float)p_elements[i * 3 + 0];
				vec[1] = (float)p_elements[i * 3 + 1];
				vec[2] = (float)p_elements[i * 3 + 2];
			}
		}
	}
	return obj;
}

MonoObject *variant_to_mono_transform2d(MonoDomain *p_domain, const real_t *p_elements) {
	if (!mono_class_transform2d) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, mono_class_transform2d);
	if (!obj) return nullptr;
	// Transform2D has columns[3] (Vector2)
	MonoClassField *cols_field = mono_class_get_field_from_name(mono_class_transform2d, "columns");
	if (cols_field) {
		char *data = reinterpret_cast<char *>(mono_object_unbox(obj));
		int offset = mono_field_get_offset(cols_field) - (int)sizeof(MonoObject);
		if (offset >= 0) {
			for (int i = 0; i < 3; i++) {
				float *vec = reinterpret_cast<float *>(data + offset + i * 8);
				vec[0] = (float)p_elements[i * 2 + 0];
				vec[1] = (float)p_elements[i * 2 + 1];
			}
		}
	}
	return obj;
}

MonoObject *variant_to_mono_transform3d(MonoDomain *p_domain, const real_t *p_basis, real_t p_ox, real_t p_oy, real_t p_oz) {
	if (!mono_class_transform3d) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, mono_class_transform3d);
	if (!obj) return nullptr;
	// Transform3D.basis (value-type Basis = 9 floats = 36 bytes) and
	// Transform3D.origin (value-type Vector3 = 3 floats = 12 bytes) are value
	// types. mono_field_set_value copies sizeof(field) bytes from the source
	// pointer into the field. We pass flat float buffers so the bytes get
	// written directly into the parent's value-type fields. The previous code
	// created boxed Basis/Vector3 MonoObjects and passed &basis_obj / &origin,
	// which wrote MonoObject* pointer values (8 bytes each) into fields
	// expecting 36 / 12 bytes of float data — corrupting the layout and
	// leaving most bytes uninitialized. (M9, same pattern as H4 Rect2 fix)
	float basis_vals[9];
	for (int i = 0; i < 9; i++) {
		basis_vals[i] = (float)p_basis[i];
	}
	float origin_vals[3] = { (float)p_ox, (float)p_oy, (float)p_oz };
	MonoClassField *fb = mono_class_get_field_from_name(mono_class_transform3d, "basis");
	if (fb) mono_field_set_value(obj, fb, basis_vals);
	MonoClassField *fo = mono_class_get_field_from_name(mono_class_transform3d, "origin");
	if (fo) mono_field_set_value(obj, fo, origin_vals);
	return obj;
}

bool mono_object_to_vector4(MonoObject *p_obj, real_t *r_x, real_t *r_y, real_t *r_z, real_t *r_w) {
	if (!p_obj || !mono_class_vector4 || mono_object_get_class(p_obj) != mono_class_vector4) return false;
	float x = 0, y = 0, z = 0, w = 0;
	bool ok = get_struct_float_field(p_obj, mono_class_vector4, "x", x);
	ok &= get_struct_float_field(p_obj, mono_class_vector4, "y", y);
	ok &= get_struct_float_field(p_obj, mono_class_vector4, "z", z);
	ok &= get_struct_float_field(p_obj, mono_class_vector4, "w", w);
	if (ok) {
		if (r_x) *r_x = (real_t)x;
		if (r_y) *r_y = (real_t)y;
		if (r_z) *r_z = (real_t)z;
		if (r_w) *r_w = (real_t)w;
	}
	return ok;
}

bool mono_object_to_vector2i(MonoObject *p_obj, int32_t *r_x, int32_t *r_y) {
	if (!p_obj || !mono_class_vector2i || mono_object_get_class(p_obj) != mono_class_vector2i) return false;
	int32_t x = 0, y = 0;
	bool ok = get_struct_int_field(p_obj, mono_class_vector2i, "x", x);
	ok &= get_struct_int_field(p_obj, mono_class_vector2i, "y", y);
	if (ok) {
		if (r_x) *r_x = x;
		if (r_y) *r_y = y;
	}
	return ok;
}

bool mono_object_to_vector3i(MonoObject *p_obj, int32_t *r_x, int32_t *r_y, int32_t *r_z) {
	if (!p_obj || !mono_class_vector3i || mono_object_get_class(p_obj) != mono_class_vector3i) return false;
	int32_t x = 0, y = 0, z = 0;
	bool ok = get_struct_int_field(p_obj, mono_class_vector3i, "x", x);
	ok &= get_struct_int_field(p_obj, mono_class_vector3i, "y", y);
	ok &= get_struct_int_field(p_obj, mono_class_vector3i, "z", z);
	if (ok) {
		if (r_x) *r_x = x;
		if (r_y) *r_y = y;
		if (r_z) *r_z = z;
	}
	return ok;
}

bool mono_object_to_vector4i(MonoObject *p_obj, int32_t *r_x, int32_t *r_y, int32_t *r_z, int32_t *r_w) {
	if (!p_obj || !mono_class_vector4i || mono_object_get_class(p_obj) != mono_class_vector4i) return false;
	int32_t x = 0, y = 0, z = 0, w = 0;
	bool ok = get_struct_int_field(p_obj, mono_class_vector4i, "x", x);
	ok &= get_struct_int_field(p_obj, mono_class_vector4i, "y", y);
	ok &= get_struct_int_field(p_obj, mono_class_vector4i, "z", z);
	ok &= get_struct_int_field(p_obj, mono_class_vector4i, "w", w);
	if (ok) {
		if (r_x) *r_x = x;
		if (r_y) *r_y = y;
		if (r_z) *r_z = z;
		if (r_w) *r_w = w;
	}
	return ok;
}

bool mono_object_to_rect2i(MonoObject *p_obj, int32_t *r_x, int32_t *r_y, int32_t *r_w, int32_t *r_h) {
	if (!p_obj || !mono_class_rect2i || mono_object_get_class(p_obj) != mono_class_rect2i) return false;
	MonoClassField *pos_field = mono_class_get_field_from_name(mono_class_rect2i, "position");
	MonoClassField *size_field = mono_class_get_field_from_name(mono_class_rect2i, "size");
	if (!pos_field || !size_field) return false;
	// Rect2I.position and Rect2I.size are value-type Vector2I fields (layout:
	// int32_t x, y = 8 bytes). Read into 2-int32 buffers. (M9 read side,
	// paired with the variant_to_mono_rect2i write-side fix.)
	int32_t pos_buf[2] = { 0, 0 };
	int32_t size_buf[2] = { 0, 0 };
	mono_field_get_value(p_obj, pos_field, pos_buf);
	mono_field_get_value(p_obj, size_field, size_buf);
	if (r_x) *r_x = pos_buf[0];
	if (r_y) *r_y = pos_buf[1];
	if (r_w) *r_w = size_buf[0];
	if (r_h) *r_h = size_buf[1];
	return true;
}

bool mono_object_to_quaternion(MonoObject *p_obj, real_t *r_x, real_t *r_y, real_t *r_z, real_t *r_w) {
	if (!p_obj || !mono_class_quaternion || mono_object_get_class(p_obj) != mono_class_quaternion) return false;
	float x = 0, y = 0, z = 0, w = 0;
	bool ok = get_struct_float_field(p_obj, mono_class_quaternion, "x", x);
	ok &= get_struct_float_field(p_obj, mono_class_quaternion, "y", y);
	ok &= get_struct_float_field(p_obj, mono_class_quaternion, "z", z);
	ok &= get_struct_float_field(p_obj, mono_class_quaternion, "w", w);
	if (ok) {
		if (r_x) *r_x = (real_t)x;
		if (r_y) *r_y = (real_t)y;
		if (r_z) *r_z = (real_t)z;
		if (r_w) *r_w = (real_t)w;
	}
	return ok;
}

bool mono_object_to_plane(MonoObject *p_obj, real_t *r_nx, real_t *r_ny, real_t *r_nz, real_t *r_d) {
	if (!p_obj || !mono_class_plane || mono_object_get_class(p_obj) != mono_class_plane) return false;
	MonoClassField *normal_field = mono_class_get_field_from_name(mono_class_plane, "normal");
	MonoClassField *d_field = mono_class_get_field_from_name(mono_class_plane, "d");
	if (!normal_field || !d_field) return false;
	// Plane.normal is a value-type Vector3 field (layout: float x, y, z = 12
	// bytes) and Plane.d is a value-type float field. Read into appropriately
	// sized buffers. The previous code read 12 bytes of normal-field data into
	// a MonoObject* variable (8 bytes) — a 4-byte stack overwrite — and then
	// dereferenced the garbage "pointer". (M9 read side, paired with the
	// variant_to_mono_plane write-side fix.)
	float normal_buf[3] = { 0, 0, 0 };
	float d_val = 0;
	mono_field_get_value(p_obj, normal_field, normal_buf);
	mono_field_get_value(p_obj, d_field, &d_val);
	if (r_nx) *r_nx = normal_buf[0];
	if (r_ny) *r_ny = normal_buf[1];
	if (r_nz) *r_nz = normal_buf[2];
	if (r_d) *r_d = (real_t)d_val;
	return true;
}

bool mono_object_to_aabb(MonoObject *p_obj, real_t *r_px, real_t *r_py, real_t *r_pz, real_t *r_sx, real_t *r_sy, real_t *r_sz) {
	if (!p_obj || !mono_class_aabb || mono_object_get_class(p_obj) != mono_class_aabb) return false;
	MonoClassField *pos_field = mono_class_get_field_from_name(mono_class_aabb, "position");
	MonoClassField *size_field = mono_class_get_field_from_name(mono_class_aabb, "size");
	if (!pos_field || !size_field) return false;
	// Aabb.position and Aabb.size are value-type Vector3 fields (layout:
	// float x, y, z = 12 bytes each). Read into 3-float buffers. The previous
	// code read 12 bytes into a MonoObject* variable (8 bytes) — a 4-byte
	// stack overwrite — and then dereferenced the garbage "pointer".
	// (M9 read side, paired with the variant_to_mono_aabb write-side fix.)
	float pos_buf[3] = { 0, 0, 0 };
	float size_buf[3] = { 0, 0, 0 };
	mono_field_get_value(p_obj, pos_field, pos_buf);
	mono_field_get_value(p_obj, size_field, size_buf);
	if (r_px) *r_px = pos_buf[0];
	if (r_py) *r_py = pos_buf[1];
	if (r_pz) *r_pz = pos_buf[2];
	if (r_sx) *r_sx = size_buf[0];
	if (r_sy) *r_sy = size_buf[1];
	if (r_sz) *r_sz = size_buf[2];
	return true;
}

// ============================================================
// M10: Godot.Collections.Array / Dictionary conversions
//
// The C# wrappers (Godot.Collections.Array / Dictionary) own a
// heap-allocated Array*/Dictionary* (memnew/memdelete) stored in their
// NativePtr field. variant_to_mono_* creates a fresh heap allocation and
// copies the source into it (value semantics — mutations on the C# side do
// not leak back to the original Variant). mono_object_to_variant reads the
// NativePtr and copies the pointed-to container into the returned Variant
// (again value semantics).
// ============================================================

MonoObject *variant_to_mono_array(MonoDomain *p_domain, const Array &p_array) {
	if (!mono_class_godot_array) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, mono_class_godot_array);
	if (!obj) return nullptr;
	// Allocate a heap Array and copy-construct from the source. Array is
	// internally refcounted, so this shares the underlying VVector — cheap.
	Array *heap_arr = memnew(Array(p_array));
	MonoClassField *field = find_nativeptr_field(mono_class_godot_array);
	if (!field) {
		memdelete(heap_arr);
		return nullptr;
	}
	intptr_t ptr_val = reinterpret_cast<intptr_t>(heap_arr);
	mono_field_set_value(obj, field, &ptr_val);
	return obj;
}

MonoObject *variant_to_mono_dictionary(MonoDomain *p_domain, const Dictionary &p_dictionary) {
	if (!mono_class_godot_dictionary) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, mono_class_godot_dictionary);
	if (!obj) return nullptr;
	Dictionary *heap_dict = memnew(Dictionary(p_dictionary));
	MonoClassField *field = find_nativeptr_field(mono_class_godot_dictionary);
	if (!field) {
		memdelete(heap_dict);
		return nullptr;
	}
	intptr_t ptr_val = reinterpret_cast<intptr_t>(heap_dict);
	mono_field_set_value(obj, field, &ptr_val);
	return obj;
}

}
