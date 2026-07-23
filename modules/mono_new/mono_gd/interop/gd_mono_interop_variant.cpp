#include "gd_mono_interop_variant.h"

#include "gd_mono_callable.h"
#include "../../mono_runtime/gd_mono.h"
#include "../../utils/mono_logger.h"

#include "core/object/object.h"
#include "core/os/os.h"
#include "core/os/keyboard.h"
#include "core/input/input.h"
#include "core/input/input_event.h"
#include "core/math/random_pcg.h"
#include "core/io/resource_loader.h"
#include "core/io/file_access.h"
#include "core/io/dir_access.h"
#include "core/templates/hash_map.h"
#include "core/variant/variant.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"
#include "core/variant/array.h"
#include "core/error/error_list.h"
#include "core/templates/rid.h"
#include "core/string/node_path.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/config/engine.h"
#include "core/os/time.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/main/canvas_layer.h"
#include "scene/gui/control.h"
#include "scene/resources/packed_scene.h"

#include <mono/mono-publib.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>

extern "C" {
MonoObject *mono_field_get_value_object(MonoDomain *domain, MonoClassField *field, MonoObject *obj);
}

using namespace GDMonoInterop;

namespace {

// ---------------------------------------------------------------------------
// WASM float/double bit-cast workaround
// ---------------------------------------------------------------------------
// Mono WASM interpreter m2n cookie table historically lacked entries for
// icalls that take or return float/double directly.  All such icalls therefore
// marshal floats as int32 bit patterns and doubles as int64 bit patterns, with
// C# reinterpreting via [StructLayout(LayoutKind.Explicit)] unions.
//
// This is isolated here so that once the cookie table is properly extended
// these helpers can be removed in a single place.

_FORCE_INLINE_ static double bits_to_double(int64_t p_bits) {
	union { int64_t i; double d; } u;
	u.i = p_bits;
	return u.d;
}

_FORCE_INLINE_ static int64_t double_to_bits(double p_val) {
	union { double d; int64_t i; } u;
	u.d = p_val;
	return u.i;
}

_FORCE_INLINE_ static float bits_to_float(int32_t p_bits) {
	union { int32_t i; float f; } u;
	u.i = p_bits;
	return u.f;
}

_FORCE_INLINE_ static int32_t float_to_bits(float p_val) {
	union { float f; int32_t i; } u;
	u.f = p_val;
	return u.i;
}

// ---------------------------------------------------------------------------
// String helpers
// ---------------------------------------------------------------------------

static String mono_string_to_godot_string(MonoString *p_str) {
	if (!p_str) return String();
	char *utf8 = mono_string_to_utf8(p_str);
	if (!utf8) return String();
	String s = String::utf8(utf8);
	mono_free(utf8);
	return s;
}

static MonoString *godot_string_to_mono_string(MonoDomain *p_domain, const String &p_str) {
	CharString cs = p_str.utf8();
	return mono_string_new(p_domain, cs.get_data());
}

// ---------------------------------------------------------------------------
// Godot <-> Mono value-type mapping traits
// ---------------------------------------------------------------------------
// For each blittable C# struct we define a trait that maps:
//   - GodotType     : the Godot C++ type (Vector2, Rect2i, ...)
//   - MonoType      : the C# blittable layout struct (MonoVector2, ...)
//   - CSharpName    : the C# class name inside the "Godot" namespace
//   - convert_to    : GodotType -> MonoType
//   - convert_from  : MonoType -> GodotType

template<typename TMono>
struct ValueTypeTraits;

#define GD_MONO_VALUE_TYPE(GodotT, MonoT, CSharpName)                   \
template<>                                                              \
struct ValueTypeTraits<MonoT> {                                         \
	using GodotType = GodotT;                                           \
	using MonoType = MonoT;                                             \
	static constexpr const char *CSharpClassName = #CSharpName;         \
	static MonoType to_mono(const GodotType &v) { return MonoT{}; }     \
	static GodotType from_mono(const MonoType &v) { return GodotType{}; }\
};

template<> struct ValueTypeTraits<MonoVector2> {
	using GodotType = Vector2;
	using MonoType = MonoVector2;
	static constexpr const char *CSharpClassName = "Vector2";
	static MonoType to_mono(const GodotType &v) { return { v.x, v.y }; }
	static GodotType from_mono(const MonoType &v) { return Vector2(v.x, v.y); }
};

template<> struct ValueTypeTraits<MonoVector2i> {
	using GodotType = Vector2i;
	using MonoType = MonoVector2i;
	static constexpr const char *CSharpClassName = "Vector2i";
	static MonoType to_mono(const GodotType &v) { return { v.x, v.y }; }
	static GodotType from_mono(const MonoType &v) { return Vector2i(v.x, v.y); }
};

template<> struct ValueTypeTraits<MonoVector3> {
	using GodotType = Vector3;
	using MonoType = MonoVector3;
	static constexpr const char *CSharpClassName = "Vector3";
	static MonoType to_mono(const GodotType &v) { return { v.x, v.y, v.z }; }
	static GodotType from_mono(const MonoType &v) { return Vector3(v.x, v.y, v.z); }
};

template<> struct ValueTypeTraits<MonoVector3i> {
	using GodotType = Vector3i;
	using MonoType = MonoVector3i;
	static constexpr const char *CSharpClassName = "Vector3i";
	static MonoType to_mono(const GodotType &v) { return { v.x, v.y, v.z }; }
	static GodotType from_mono(const MonoType &v) { return Vector3i(v.x, v.y, v.z); }
};

template<> struct ValueTypeTraits<MonoVector4> {
	using GodotType = Vector4;
	using MonoType = MonoVector4;
	static constexpr const char *CSharpClassName = "Vector4";
	static MonoType to_mono(const GodotType &v) { return { v.x, v.y, v.z, v.w }; }
	static GodotType from_mono(const MonoType &v) { return Vector4(v.x, v.y, v.z, v.w); }
};

template<> struct ValueTypeTraits<MonoVector4i> {
	using GodotType = Vector4i;
	using MonoType = MonoVector4i;
	static constexpr const char *CSharpClassName = "Vector4i";
	static MonoType to_mono(const GodotType &v) { return { v.x, v.y, v.z, v.w }; }
	static GodotType from_mono(const MonoType &v) { return Vector4i(v.x, v.y, v.z, v.w); }
};

template<> struct ValueTypeTraits<MonoColor> {
	using GodotType = Color;
	using MonoType = MonoColor;
	static constexpr const char *CSharpClassName = "Color";
	static MonoType to_mono(const GodotType &v) { return { v.r, v.g, v.b, v.a }; }
	static GodotType from_mono(const MonoType &v) { return Color(v.r, v.g, v.b, v.a); }
};

template<> struct ValueTypeTraits<MonoRect2> {
	using GodotType = Rect2;
	using MonoType = MonoRect2;
	static constexpr const char *CSharpClassName = "Rect2";
	static MonoType to_mono(const GodotType &v) { return { { v.position.x, v.position.y }, { v.size.x, v.size.y } }; }
	static GodotType from_mono(const MonoType &v) { return Rect2(v.position.x, v.position.y, v.size.x, v.size.y); }
};

template<> struct ValueTypeTraits<MonoRect2i> {
	using GodotType = Rect2i;
	using MonoType = MonoRect2i;
	static constexpr const char *CSharpClassName = "Rect2i";
	static MonoType to_mono(const GodotType &v) { return { { v.position.x, v.position.y }, { v.size.x, v.size.y } }; }
	static GodotType from_mono(const MonoType &v) { return Rect2i(v.position.x, v.position.y, v.size.x, v.size.y); }
};

template<> struct ValueTypeTraits<MonoPlane> {
	using GodotType = Plane;
	using MonoType = MonoPlane;
	static constexpr const char *CSharpClassName = "Plane";
	static MonoType to_mono(const GodotType &v) { return { v.normal.x, v.normal.y, v.normal.z, v.d }; }
	static GodotType from_mono(const MonoType &v) { return Plane(v.x, v.y, v.z, v.d); }
};

template<> struct ValueTypeTraits<MonoQuaternion> {
	using GodotType = Quaternion;
	using MonoType = MonoQuaternion;
	static constexpr const char *CSharpClassName = "Quaternion";
	static MonoType to_mono(const GodotType &v) { return { v.x, v.y, v.z, v.w }; }
	static GodotType from_mono(const MonoType &v) { return Quaternion(v.x, v.y, v.z, v.w); }
};

template<> struct ValueTypeTraits<MonoAABB> {
	using GodotType = AABB;
	using MonoType = MonoAABB;
	static constexpr const char *CSharpClassName = "AABB";
	static MonoType to_mono(const GodotType &v) { return { { v.position.x, v.position.y, v.position.z }, { v.size.x, v.size.y, v.size.z } }; }
	static GodotType from_mono(const MonoType &v) { return AABB(Vector3(v.position.x, v.position.y, v.position.z), Vector3(v.size.x, v.size.y, v.size.z)); }
};

template<> struct ValueTypeTraits<MonoBasis> {
	using GodotType = Basis;
	using MonoType = MonoBasis;
	static constexpr const char *CSharpClassName = "Basis";
	static MonoType to_mono(const GodotType &v) {
		MonoBasis mb{};
		for (int i = 0; i < 3; i++) {
			mb.rows[i].x = v.rows[i].x;
			mb.rows[i].y = v.rows[i].y;
			mb.rows[i].z = v.rows[i].z;
		}
		return mb;
	}
	static GodotType from_mono(const MonoType &v) {
		return Basis(
			Vector3(v.rows[0].x, v.rows[0].y, v.rows[0].z),
			Vector3(v.rows[1].x, v.rows[1].y, v.rows[1].z),
			Vector3(v.rows[2].x, v.rows[2].y, v.rows[2].z));
	}
};

template<> struct ValueTypeTraits<MonoTransform2D> {
	using GodotType = Transform2D;
	using MonoType = MonoTransform2D;
	static constexpr const char *CSharpClassName = "Transform2D";
	static MonoType to_mono(const GodotType &v) {
		MonoTransform2D mt{};
		for (int i = 0; i < 3; i++) {
			mt.columns[i].x = v.columns[i].x;
			mt.columns[i].y = v.columns[i].y;
		}
		return mt;
	}
	static GodotType from_mono(const MonoType &v) {
		return Transform2D(
			Vector2(v.columns[0].x, v.columns[0].y),
			Vector2(v.columns[1].x, v.columns[1].y),
			Vector2(v.columns[2].x, v.columns[2].y));
	}
};

template<> struct ValueTypeTraits<MonoTransform3D> {
	using GodotType = Transform3D;
	using MonoType = MonoTransform3D;
	static constexpr const char *CSharpClassName = "Transform3D";
	static MonoType to_mono(const GodotType &v) {
		MonoTransform3D mt{};
		for (int i = 0; i < 3; i++) {
			mt.basis.rows[i].x = v.basis.rows[i].x;
			mt.basis.rows[i].y = v.basis.rows[i].y;
			mt.basis.rows[i].z = v.basis.rows[i].z;
		}
		mt.origin.x = v.origin.x;
		mt.origin.y = v.origin.y;
		mt.origin.z = v.origin.z;
		return mt;
	}
	static GodotType from_mono(const MonoType &v) {
		Basis basis(
			Vector3(v.basis.rows[0].x, v.basis.rows[0].y, v.basis.rows[0].z),
			Vector3(v.basis.rows[1].x, v.basis.rows[1].y, v.basis.rows[1].z),
			Vector3(v.basis.rows[2].x, v.basis.rows[2].y, v.basis.rows[2].z));
		return Transform3D(basis, Vector3(v.origin.x, v.origin.y, v.origin.z));
	}
};

template<> struct ValueTypeTraits<MonoProjection> {
	using GodotType = Projection;
	using MonoType = MonoProjection;
	static constexpr const char *CSharpClassName = "Projection";
	static MonoType to_mono(const GodotType &v) {
		MonoProjection mp{};
		for (int i = 0; i < 4; i++) {
			mp.columns[i].x = v.columns[i].x;
			mp.columns[i].y = v.columns[i].y;
			mp.columns[i].z = v.columns[i].z;
			mp.columns[i].w = v.columns[i].w;
		}
		return mp;
	}
	static GodotType from_mono(const MonoType &v) {
		Projection p;
		for (int i = 0; i < 4; i++) {
			p.columns[i] = Vector4(v.columns[i].x, v.columns[i].y, v.columns[i].z, v.columns[i].w);
		}
		return p;
	}
};

template<> struct ValueTypeTraits<MonoRID> {
	using GodotType = RID;
	using MonoType = MonoRID;
	static constexpr const char *CSharpClassName = "RID";
	static MonoType to_mono(const GodotType &v) { return { v.is_valid() ? v.get_id() : 0 }; }
	static GodotType from_mono(const MonoType &v) { return RID::from_uint64(v.id); }
};

// ---------------------------------------------------------------------------
// Generic value-type boxing / unboxing
// ---------------------------------------------------------------------------

template<typename TMono>
static MonoObject *box_mono_struct(MonoDomain *p_domain, MonoImage *p_image, const TMono *p_mono_val) {
	using Traits = ValueTypeTraits<TMono>;
	MonoClass *cls = mono_class_from_name(p_image, "Godot", Traits::CSharpClassName);
	if (!cls) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, cls);
	if (!obj) return nullptr;
	void *raw = mono_object_unbox(obj);
	memcpy(raw, p_mono_val, sizeof(TMono));
	return obj;
}

template<typename TMono>
static MonoObject *box_godot_value(MonoDomain *p_domain, MonoImage *p_image, const typename ValueTypeTraits<TMono>::GodotType &p_val) {
	TMono mval = ValueTypeTraits<TMono>::to_mono(p_val);
	return box_mono_struct<TMono>(p_domain, p_image, &mval);
}

template<typename TMono>
static typename ValueTypeTraits<TMono>::GodotType unbox_mono_struct(MonoObject *p_obj) {
	TMono *mval = (TMono *)mono_object_unbox(p_obj);
	return ValueTypeTraits<TMono>::from_mono(*mval);
}

// ---------------------------------------------------------------------------
// Reference-type wrapper creation (StringName, NodePath, ...)
// ---------------------------------------------------------------------------

static MonoObject *create_string_wrapper_type(MonoDomain *p_domain, MonoImage *p_image, const char *p_type_name, const String &p_value) {
	MonoString *mstr = godot_string_to_mono_string(p_domain, p_value);
	MonoClass *cls = mono_class_from_name(p_image, "Godot", p_type_name);
	if (!cls) return (MonoObject *)mstr;
	MonoObject *obj = mono_object_new(p_domain, cls);
	if (!obj) return (MonoObject *)mstr;
	MonoMethod *ctor = mono_class_get_method_from_name(cls, ".ctor", 1);
	if (ctor) {
		void *args[1] = { mstr };
		MonoObject *exc = nullptr;
		mono_runtime_invoke(ctor, obj, args, &exc);
		if (exc) {
			MonoLogger::log_error(vformat("Exception in %s .ctor", p_type_name));
		}
	}
	return obj;
}

static MonoObject *create_native_wrapper_type(MonoDomain *p_domain, MonoImage *p_image, const char *p_type_name, const char *p_field_name, void *p_native_ptr) {
	MonoClass *cls = mono_class_from_name(p_image, "Godot", p_type_name);
	if (!cls) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, cls);
	if (!obj) return nullptr;
	mono_runtime_object_init(obj);
	MonoClassField *field = mono_class_get_field_from_name(cls, p_field_name);
	if (!field) {
		String alt = "_" + String(p_field_name);
		CharString alt_cs = alt.utf8();
		field = mono_class_get_field_from_name(cls, alt_cs.get_data());
	}
	if (field) {
		mono_field_set_value(obj, field, &p_native_ptr);
	}
	return obj;
}

} // anonymous namespace

namespace GDMonoInterop {

// ---------------------------------------------------------------------------
// Cached GodotObject class / nativeInstance field
// ---------------------------------------------------------------------------

static MonoClass *godot_object_class = nullptr;
static MonoClassField *native_instance_field = nullptr;

static String capitalize_first(const String &s) {
	if (s.is_empty()) return s;
	String result = s;
	result = result.substr(0, 1).to_upper() + result.substr(1);
	return result;
}

static MonoImage *get_godot_sharp_image() {
	GDMono *gdmono = GDMono::get_singleton();
	if (gdmono && gdmono->get_godotsharp_image()) {
		return gdmono->get_godotsharp_image();
	}
	return mono_get_corlib();
}

static void ensure_native_instance_field() {
	if (godot_object_class && native_instance_field)
		return;
	MonoImage *image = get_godot_sharp_image();
	if (!image) return;
	godot_object_class = mono_class_from_name(image, "Godot", "GodotObject");
	if (!godot_object_class) return;
	native_instance_field = mono_class_get_field_from_name(godot_object_class, "nativeInstance");
	if (!native_instance_field)
		native_instance_field = mono_class_get_field_from_name(godot_object_class, "_nativeInstance");
}

void *get_native_object(MonoObject *p_managed) {
	if (!p_managed) return nullptr;
	ensure_native_instance_field();
	if (!native_instance_field) return nullptr;
	void *ptr = nullptr;
	mono_field_get_value(p_managed, native_instance_field, &ptr);
	return ptr;
}

MonoObject *get_managed_wrapper(MonoDomain *p_domain, Object *p_native) {
	if (!p_native) return nullptr;
	ObjectID oid = p_native->get_instance_id();

	GDMono *gdmono = GDMono::get_singleton();
	if (gdmono) {
		MonoObject *cached = gdmono->get_cached_managed_object(oid);
		if (cached) return cached;
	}

	ensure_native_instance_field();
	if (!godot_object_class) return nullptr;
	String class_name = p_native->get_class();
	MonoImage *image = get_godot_sharp_image();
	CharString class_name_utf8 = capitalize_first(class_name).utf8();
	MonoClass *cls = mono_class_from_name(image, "Godot", class_name_utf8.get_data());
	if (!cls)
		cls = godot_object_class;
	if (!cls) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, cls);
	if (!obj) return nullptr;
	// NOTE: We intentionally do NOT call mono_runtime_object_init() here.
	// If we did, C# constructors would run and potentially call
	// godot_icall_CreateObject to create a NEW native object of the same
	// type, resulting in an orphan that leaks and shadows the real native
	// instance.  Instead we directly set the nativeInstance field.
	//
	// This means parameterized C# constructors are NOT executed for objects
	// that come from the engine side (scene-loaded nodes, returned resources).
	// Users should perform initialization in _Ready() / _EnterTree() instead.
	if (native_instance_field) {
		void *native_ptr = p_native;
		mono_field_set_value(obj, native_instance_field, &native_ptr);
	}

	if (gdmono) {
		gdmono->cache_managed_object(oid, obj);
	}

	return obj;
}

// ---------------------------------------------------------------------------
// Reference-count protection for objects returned to managed land
// ---------------------------------------------------------------------------
// When we hand a RefCounted-derived object to C# we must increment its
// reference count, because the C# GCHandle / MonoObject does NOT hold a
// Godot-side Ref<>.  Without this the object can be destroyed while C# still
// holds a pointer, leading to use-after-free.

static inline void prepare_for_managed_return(Object *p_obj) {
	if (!p_obj) return;
	RefCounted *rc = Object::cast_to<RefCounted>(p_obj);
	if (rc) {
		rc->reference();
	}
}

// ---------------------------------------------------------------------------
// VariantTypeManaged helpers
// ---------------------------------------------------------------------------

namespace EnumUtil {
	const char *GetTypeName(VariantTypeManaged p_type);
}

Variant::Type managed_type_to_variant_type(VariantTypeManaged p_type) {
	return static_cast<Variant::Type>(static_cast<int>(p_type));
}

VariantTypeManaged variant_type_to_managed(Variant::Type p_type) {
	return static_cast<VariantTypeManaged>(static_cast<int>(p_type));
}

static MonoClass *get_mono_class_for_variant_type(MonoDomain *p_domain, VariantTypeManaged p_type) {
	MonoImage *image = get_godot_sharp_image();
	if (!image)
		return nullptr;

	switch (p_type) {
		case VariantTypeManaged::Bool:
			return mono_get_boolean_class();
		case VariantTypeManaged::Int:
			return mono_get_int64_class();
		case VariantTypeManaged::Float:
			return mono_get_double_class();
		case VariantTypeManaged::String:
			return mono_get_string_class();
		case VariantTypeManaged::Vector2:
		case VariantTypeManaged::Vector2i:
		case VariantTypeManaged::Vector3:
		case VariantTypeManaged::Vector3i:
		case VariantTypeManaged::Vector4:
		case VariantTypeManaged::Vector4i:
		case VariantTypeManaged::Color:
		case VariantTypeManaged::Rect2:
		case VariantTypeManaged::Rect2i:
		case VariantTypeManaged::Plane:
		case VariantTypeManaged::Quaternion:
		case VariantTypeManaged::AABB:
		case VariantTypeManaged::Basis:
		case VariantTypeManaged::Transform2D:
		case VariantTypeManaged::Transform3D:
		case VariantTypeManaged::Projection:
		case VariantTypeManaged::RID:
		case VariantTypeManaged::StringName:
		case VariantTypeManaged::NodePath:
		case VariantTypeManaged::Callable:
		case VariantTypeManaged::Signal:
		case VariantTypeManaged::Object:
		case VariantTypeManaged::Dictionary:
		case VariantTypeManaged::Array:
		case VariantTypeManaged::PackedByteArray:
		case VariantTypeManaged::PackedInt32Array:
		case VariantTypeManaged::PackedInt64Array:
		case VariantTypeManaged::PackedFloat32Array:
		case VariantTypeManaged::PackedFloat64Array:
		case VariantTypeManaged::PackedStringArray:
		case VariantTypeManaged::PackedVector2Array:
		case VariantTypeManaged::PackedVector3Array:
		case VariantTypeManaged::PackedColorArray:
			return mono_class_from_name(image, "Godot", EnumUtil::GetTypeName(p_type));
		default:
			return nullptr;
	}
}

// ---------------------------------------------------------------------------
// Variant -> MonoObject
// ---------------------------------------------------------------------------

MonoObject *variant_to_mono_object(MonoDomain *p_domain, const Variant &p_variant) {
	if (p_variant.is_null())
		return nullptr;

	Variant::Type type = p_variant.get_type();
	MonoImage *image = get_godot_sharp_image();
	if (!image)
		return nullptr;

	switch (type) {
		case Variant::NIL:
			return nullptr;

		case Variant::BOOL: {
			bool val = p_variant;
			return mono_value_box(p_domain, mono_get_boolean_class(), &val);
		}

		case Variant::INT: {
			int64_t val = p_variant;
			return mono_value_box(p_domain, mono_get_int64_class(), &val);
		}

		case Variant::FLOAT: {
			double val = p_variant;
			return mono_value_box(p_domain, mono_get_double_class(), &val);
		}

		case Variant::STRING: {
			String str = p_variant;
			CharString cs = str.utf8();
			return (MonoObject *)mono_string_new(p_domain, cs.get_data());
		}

		// --- Value types via generic template ---
		case Variant::VECTOR2:    return box_godot_value<MonoVector2>(p_domain, image, p_variant);
		case Variant::VECTOR2I:   return box_godot_value<MonoVector2i>(p_domain, image, p_variant);
		case Variant::VECTOR3:    return box_godot_value<MonoVector3>(p_domain, image, p_variant);
		case Variant::VECTOR3I:   return box_godot_value<MonoVector3i>(p_domain, image, p_variant);
		case Variant::VECTOR4:    return box_godot_value<MonoVector4>(p_domain, image, p_variant);
		case Variant::VECTOR4I:   return box_godot_value<MonoVector4i>(p_domain, image, p_variant);
		case Variant::COLOR:      return box_godot_value<MonoColor>(p_domain, image, p_variant);
		case Variant::RECT2:      return box_godot_value<MonoRect2>(p_domain, image, p_variant);
		case Variant::RECT2I:     return box_godot_value<MonoRect2i>(p_domain, image, p_variant);
		case Variant::PLANE:      return box_godot_value<MonoPlane>(p_domain, image, p_variant);
		case Variant::QUATERNION: return box_godot_value<MonoQuaternion>(p_domain, image, p_variant);
		case Variant::AABB:       return box_godot_value<MonoAABB>(p_domain, image, p_variant);
		case Variant::BASIS:      return box_godot_value<MonoBasis>(p_domain, image, p_variant);
		case Variant::TRANSFORM2D: return box_godot_value<MonoTransform2D>(p_domain, image, p_variant);
		case Variant::TRANSFORM3D: return box_godot_value<MonoTransform3D>(p_domain, image, p_variant);
		case Variant::PROJECTION: return box_godot_value<MonoProjection>(p_domain, image, p_variant);
		case Variant::RID:        return box_godot_value<MonoRID>(p_domain, image, p_variant);

		// --- String-like reference wrappers ---
		case Variant::STRING_NAME: {
			StringName sn = p_variant;
			return create_string_wrapper_type(p_domain, image, "StringName", String(sn));
		}

		case Variant::NODE_PATH: {
			NodePath np = p_variant;
			return create_string_wrapper_type(p_domain, image, "NodePath", String(np));
		}

		// --- GodotObject ---
		case Variant::OBJECT: {
			Object *obj_ptr = p_variant;
			if (!obj_ptr) return nullptr;
			prepare_for_managed_return(obj_ptr);
			return get_managed_wrapper(p_domain, obj_ptr);
		}

		// --- Callable / Signal ---
		case Variant::CALLABLE: {
			Callable callable = p_variant;
			MonoClass *cls = mono_class_from_name(image, "Godot", "Callable");
			if (!cls) return nullptr;
			MonoObject *obj = mono_object_new(p_domain, cls);
			if (!obj) return nullptr;
			mono_runtime_object_init(obj);
			MonoClassField *field = mono_class_get_field_from_name(cls, "nativeCallable");
			if (!field) field = mono_class_get_field_from_name(cls, "_nativeCallable");
			if (field) {
				void *callable_ptr = memnew(Callable(callable));
				mono_field_set_value(obj, field, &callable_ptr);
			}
			return obj;
		}

		case Variant::SIGNAL: {
			Signal signal = p_variant;
			MonoClass *cls = mono_class_from_name(image, "Godot", "Signal");
			if (!cls) return nullptr;
			MonoObject *obj = mono_object_new(p_domain, cls);
			if (!obj) return nullptr;
			mono_runtime_object_init(obj);
			MonoClassField *field = mono_class_get_field_from_name(cls, "nativeSignal");
			if (!field) field = mono_class_get_field_from_name(cls, "_nativeSignal");
			if (field) {
				void *signal_ptr = memnew(Signal(signal));
				mono_field_set_value(obj, field, &signal_ptr);
			}
			return obj;
		}

		// --- Array ---
		case Variant::ARRAY: {
			Array arr = p_variant;
			int count = arr.size();
			MonoClass *obj_cls = mono_get_object_class();
			MonoArray *mono_arr = mono_array_new(p_domain, obj_cls, count);
			if (!mono_arr) return nullptr;
			for (int i = 0; i < count; i++) {
				MonoObject *elem = variant_to_mono_object(p_domain, arr[i]);
				if (elem) {
					MonoObject **slot = (MonoObject **)mono_array_addr_with_size(mono_arr, sizeof(MonoObject *), i);
					if (slot) *slot = elem;
				}
			}
			return (MonoObject *)mono_arr;
		}

		// --- Dictionary ---
		case Variant::DICTIONARY: {
			Dictionary dict = p_variant;
			MonoClass *cls = mono_class_from_name(image, "Godot", "Dictionary");
			if (!cls) return nullptr;
			MonoObject *obj = mono_object_new(p_domain, cls);
			if (!obj) return nullptr;
			mono_runtime_object_init(obj);
			Array keys = dict.keys();
			MonoMethod *add_method = mono_class_get_method_from_name(cls, "Add", 2);
			for (int i = 0; i < keys.size(); i++) {
				Variant key = keys[i];
				Variant val = dict[key];
				MonoObject *m_key = variant_to_mono_object(p_domain, key);
				MonoObject *m_val = variant_to_mono_object(p_domain, val);
				if (m_key && m_val && add_method) {
					void *args[2] = { m_key, m_val };
					MonoObject *exc = nullptr;
					mono_runtime_invoke(add_method, obj, args, &exc);
				}
			}
			return obj;
		}

		default:
			return nullptr;
	}
}

// ---------------------------------------------------------------------------
// MonoObject -> Variant
// ---------------------------------------------------------------------------

Variant mono_object_to_variant(MonoObject *p_obj, VariantTypeManaged p_hint_type) {
	if (!p_obj)
		return Variant();

	MonoClass *cls = mono_object_get_class(p_obj);
	if (!cls)
		return Variant();

	// --- Primitive types ---
	if (mono_class_is_subclass_of(cls, mono_get_boolean_class(), false)) {
		bool val = *(bool *)mono_object_unbox(p_obj);
		return Variant(val);
	}
	if (mono_class_is_subclass_of(cls, mono_get_int32_class(), false)) {
		int32_t val = *(int32_t *)mono_object_unbox(p_obj);
		return Variant((int64_t)val);
	}
	if (mono_class_is_subclass_of(cls, mono_get_int64_class(), false)) {
		int64_t val = *(int64_t *)mono_object_unbox(p_obj);
		return Variant(val);
	}
	if (mono_class_is_subclass_of(cls, mono_get_single_class(), false)) {
		float val = *(float *)mono_object_unbox(p_obj);
		return Variant((double)val);
	}
	if (mono_class_is_subclass_of(cls, mono_get_double_class(), false)) {
		double val = *(double *)mono_object_unbox(p_obj);
		return Variant(val);
	}
	if (mono_class_is_subclass_of(cls, mono_get_string_class(), false)) {
		MonoString *str = (MonoString *)p_obj;
		char *utf8 = mono_string_to_utf8(str);
		if (utf8) {
			String result = String::utf8(utf8);
			mono_free(utf8);
			return result;
		}
		return String();
	}

	const char *class_name = mono_class_get_name(cls);
	MonoImage *image = mono_class_get_image(cls);
	const char *namespace_name = mono_class_get_namespace(cls);

	if (namespace_name && strcmp(namespace_name, "Godot") == 0) {
		// --- Value types via class name ---
		#define GD_TRY_UNBOX(MonoT)                                                         \
			if (strcmp(class_name, ValueTypeTraits<MonoT>::CSharpClassName) == 0) {         \
				return Variant(unbox_mono_struct<MonoT>(p_obj));                            \
			}

		GD_TRY_UNBOX(MonoVector2)
		GD_TRY_UNBOX(MonoVector2i)
		GD_TRY_UNBOX(MonoVector3)
		GD_TRY_UNBOX(MonoVector3i)
		GD_TRY_UNBOX(MonoVector4)
		GD_TRY_UNBOX(MonoVector4i)
		GD_TRY_UNBOX(MonoColor)
		GD_TRY_UNBOX(MonoRect2)
		GD_TRY_UNBOX(MonoRect2i)
		GD_TRY_UNBOX(MonoPlane)
		GD_TRY_UNBOX(MonoQuaternion)
		GD_TRY_UNBOX(MonoAABB)
		GD_TRY_UNBOX(MonoBasis)
		GD_TRY_UNBOX(MonoTransform2D)
		GD_TRY_UNBOX(MonoTransform3D)
		GD_TRY_UNBOX(MonoProjection)
		GD_TRY_UNBOX(MonoRID)
		#undef GD_TRY_UNBOX

		// StringName / NodePath: extract inner string field
		if (strcmp(class_name, "StringName") == 0 || strcmp(class_name, "NodePath") == 0) {
			MonoClassField *val_field = mono_class_get_field_from_name(cls, "value");
			if (!val_field) val_field = mono_class_get_field_from_name(cls, "path");
			if (val_field) {
				MonoString *mstr = (MonoString *)mono_field_get_value_object(mono_domain_get(), val_field, p_obj);
				if (mstr) {
					char *utf8 = mono_string_to_utf8(mstr);
					if (utf8) {
						String s = String::utf8(utf8);
						mono_free(utf8);
						if (strcmp(class_name, "StringName") == 0) {
							return Variant(StringName(s));
						} else {
							return Variant(NodePath(s));
						}
					}
				}
			}
			return Variant(StringName());
		}

		// Callable: nativeCallable pointer -> Godot Callable
		if (strcmp(class_name, "Callable") == 0) {
			MonoClassField *nc_field = mono_class_get_field_from_name(cls, "nativeCallable");
			if (nc_field) {
				int64_t nc = 0;
				mono_field_get_value(p_obj, nc_field, &nc);
				if (nc != 0) {
					Callable *callable = (Callable *)(intptr_t)nc;
					return Variant(*callable);
				}
			}
			MonoClassField *td_field = mono_class_get_field_from_name(cls, "TargetDelegate");
			if (td_field) {
				MonoObject *delegate_obj = mono_field_get_value_object(mono_domain_get(), td_field, p_obj);
				if (delegate_obj) {
					Callable callable = GDMonoCallable::create_callable_from_mono_delegate(delegate_obj);
					if (callable.is_valid()) return Variant(callable);
				}
			}
			return Variant(Callable());
		}

		// Signal: Owner + Name -> Godot Signal
		if (strcmp(class_name, "Signal") == 0) {
			MonoClassField *owner_field = mono_class_get_field_from_name(cls, "Owner");
			MonoClassField *name_field = mono_class_get_field_from_name(cls, "Name");
			if (owner_field && name_field) {
				MonoObject *owner_obj = mono_field_get_value_object(mono_domain_get(), owner_field, p_obj);
				MonoObject *name_obj = mono_field_get_value_object(mono_domain_get(), name_field, p_obj);
				Object *owner = owner_obj ? (Object *)get_native_object(owner_obj) : nullptr;
				StringName signal_name;
				if (name_obj) {
					MonoClass *name_cls = mono_object_get_class(name_obj);
					MonoClassField *val_field = mono_class_get_field_from_name(name_cls, "value");
					if (val_field) {
						MonoString *mstr = (MonoString *)mono_field_get_value_object(mono_domain_get(), val_field, name_obj);
						if (mstr) {
							char *utf8 = mono_string_to_utf8(mstr);
							if (utf8) {
								signal_name = StringName(String::utf8(utf8));
								mono_free(utf8);
							}
						}
					}
				}
				if (owner && !signal_name.is_empty()) {
					return Variant(Signal(owner, signal_name));
				}
			}
			return Variant(Signal());
		}

		// GodotObject and subclasses: extract nativeInstance
		ensure_native_instance_field();
		if (strcmp(class_name, "GodotObject") == 0 || (godot_object_class && mono_class_is_subclass_of(cls, godot_object_class, false))) {
			Object *native = (Object *)get_native_object(p_obj);
			if (native) return Variant(native);
		}
	}

	return Variant();
}

// ---------------------------------------------------------------------------
// Float/double parameter helpers (WASM bit-cast)
// ---------------------------------------------------------------------------
// These helpers centralise the bit-cast workaround for icalls that need to
// pass float parameters or receive float/double return values across the
// Mono WASM interpreter boundary.  C# callers must use the corresponding
// FloatIntUnion / DoubleLongUnion unions to reinterpret.

static float bits_to_float(int32_t bits) {
	union { int32_t i; float f; } u;
	u.i = bits;
	return u.f;
}

static Vector2 bits_to_vector2(int32_t x_bits, int32_t y_bits) {
	return Vector2(bits_to_float(x_bits), bits_to_float(y_bits));
}

static Color bits_to_color(int32_t r_bits, int32_t g_bits, int32_t b_bits, int32_t a_bits) {
	return Color(bits_to_float(r_bits), bits_to_float(g_bits), bits_to_float(b_bits), bits_to_float(a_bits));
}

// ---------------------------------------------------------------------------
// ICall implementations
// ---------------------------------------------------------------------------

static void icall_GD_Print(MonoString *p_msg) {
	if (!p_msg) return;
	char *utf8 = mono_string_to_utf8(p_msg);
	if (utf8) {
		String msg = String::utf8(utf8);
		print_line(vformat("[C#] %s", msg));
		mono_free(utf8);
	}
}

static void icall_GD_PrintErr(MonoString *p_msg) {
	if (!p_msg) return;
	char *utf8 = mono_string_to_utf8(p_msg);
	if (utf8) {
		String msg = String::utf8(utf8);
		ERR_PRINT(vformat("[C#] %s", msg));
		mono_free(utf8);
	}
}

static int64_t icall_GD_Randi() {
	return Math::rand();
}

// WASM: return IEEE-754 bit pattern of double (see top-of-file comment)
static int64_t icall_GD_Randf() {
	return double_to_bits(Math::randf());
}

static MonoObject *icall_GD_Load(MonoString *p_path) {
	if (!p_path) return nullptr;
	char *utf8 = mono_string_to_utf8(p_path);
	if (!utf8) return nullptr;
	String path = String::utf8(utf8);
	mono_free(utf8);
	Ref<Resource> res = ResourceLoader::load(path);
	GDMono *gdmono = GDMono::get_singleton();
	if (!gdmono) return nullptr;
	if (res.is_null()) return nullptr;
	prepare_for_managed_return(res.ptr());
	return get_managed_wrapper(gdmono->get_scripts_domain(), res.ptr());
}

static mono_bool icall_Object_EmitSignal(int64_t p_native_ptr, MonoString *p_signal, MonoArray *p_args) {
	if (!p_native_ptr || !p_signal) return false;
	Object *obj = (Object *)(intptr_t)p_native_ptr;
	String signal_str = mono_string_to_godot_string(p_signal);
	StringName signal_name(signal_str);
	GDMono *gdmono = GDMono::get_singleton();
	if (!gdmono) return false;
	MonoDomain *domain = gdmono->get_scripts_domain();
	int argcount = p_args ? mono_array_length(p_args) : 0;
	Vector<Variant> args;
	args.resize(argcount);
	Vector<const Variant *> argptrs;
	argptrs.resize(argcount);
	for (int i = 0; i < argcount; i++) {
		MonoObject *arg = mono_array_get(p_args, MonoObject *, i);
		args.write[i] = arg ? mono_object_to_variant(arg) : Variant();
		argptrs.write[i] = &args.write[i];
	}
	Error err = obj->emit_signalp(signal_name, (const Variant **)argptrs.ptr(), argcount);
	return err == OK;
}

static int64_t icall_InputEventKey_GetKeycode(int64_t p_native) {
	if (!p_native) return 0;
	InputEventKey *key_event = Object::cast_to<InputEventKey>((Object *)(intptr_t)p_native);
	if (!key_event) return 0;
	return (int64_t)key_event->get_keycode();
}

static mono_bool icall_InputEventKey_IsPressed(int64_t p_native) {
	if (!p_native) return false;
	InputEventKey *key_event = Object::cast_to<InputEventKey>((Object *)(intptr_t)p_native);
	if (!key_event) return false;
	return key_event->is_pressed();
}

static mono_bool icall_Input_IsKeyPressed(int64_t p_key) {
	return Input::get_singleton()->is_key_pressed((Key)p_key);
}

static mono_bool icall_Input_IsActionPressed(MonoString *p_action) {
	if (!p_action) return false;
	char *utf8 = mono_string_to_utf8(p_action);
	if (!utf8) return false;
	StringName action(String::utf8(utf8));
	mono_free(utf8);
	return Input::get_singleton()->is_action_pressed(action);
}

static mono_bool icall_Input_IsActionJustPressed(MonoString *p_action) {
	if (!p_action) return false;
	char *utf8 = mono_string_to_utf8(p_action);
	if (!utf8) return false;
	StringName action(String::utf8(utf8));
	mono_free(utf8);
	return Input::get_singleton()->is_action_just_pressed(action);
}

static mono_bool icall_Input_IsActionJustReleased(MonoString *p_action) {
	if (!p_action) return false;
	char *utf8 = mono_string_to_utf8(p_action);
	if (!utf8) return false;
	StringName action(String::utf8(utf8));
	mono_free(utf8);
	return Input::get_singleton()->is_action_just_released(action);
}

// ===== Object / Node icalls =====

static int64_t icall_CreateObject(MonoString *p_class_name) {
	if (!p_class_name) return 0;
	char *utf8 = mono_string_to_utf8(p_class_name);
	if (!utf8) return 0;
	String class_name = String::utf8(utf8);
	mono_free(utf8);
	Object *obj = ClassDB::instantiate(class_name);
	if (!obj) {
		MonoLogger::log_error("Failed to create object of type: " + class_name);
		return 0;
	}
	return (int64_t)(intptr_t)obj;
}

static void icall_Node_AddChild(int64_t p_parent, int64_t p_child) {
	if (!p_parent || !p_child) return;
	Node *parent = (Node *)(intptr_t)p_parent;
	Node *child = (Node *)(intptr_t)p_child;
	parent->add_child(child);
}

// Helper: extract a property name from MonoString safely
static String get_prop_name(MonoString *p_prop) {
	return mono_string_to_godot_string(p_prop);
}

static void icall_Object_SetString(int64_t p_obj, MonoString *p_prop, MonoString *p_value) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	String prop = get_prop_name(p_prop);
	String value = p_value ? mono_string_to_godot_string(p_value) : String();
	obj->set(prop, Variant(value));
}

static void icall_Object_SetInt(int64_t p_obj, MonoString *p_prop, int64_t p_value) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	obj->set(get_prop_name(p_prop), Variant(p_value));
}

// WASM: float passed as int32 bit pattern
static void icall_Object_SetFloat(int64_t p_obj, MonoString *p_prop, int32_t p_value_bits) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	obj->set(get_prop_name(p_prop), Variant(bits_to_float(p_value_bits)));
}

static void icall_Object_SetBool(int64_t p_obj, MonoString *p_prop, mono_bool p_value) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	obj->set(get_prop_name(p_prop), Variant((bool)(p_value != 0)));
}

static void icall_Object_SetVector2(int64_t p_obj, MonoString *p_prop, int32_t x_bits, int32_t y_bits) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	obj->set(get_prop_name(p_prop), Variant(bits_to_vector2(x_bits, y_bits)));
}

static void icall_Object_SetColor(int64_t p_obj, MonoString *p_prop, int32_t r_bits, int32_t g_bits, int32_t b_bits, int32_t a_bits) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	obj->set(get_prop_name(p_prop), Variant(bits_to_color(r_bits, g_bits, b_bits, a_bits)));
}

static void icall_Object_SetObject(int64_t p_obj, MonoString *p_prop, int64_t p_value) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	Object *value = (Object *)(intptr_t)p_value;
	obj->set(get_prop_name(p_prop), Variant(value));
}

// --- Object::Call variants ---

static void icall_Object_CallNoArgs(int64_t p_obj, MonoString *p_method) {
	if (!p_obj || !p_method) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	obj->call(mono_string_to_godot_string(p_method));
}

static void icall_Object_CallString(int64_t p_obj, MonoString *p_method, MonoString *p_arg) {
	if (!p_obj || !p_method) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	String method = mono_string_to_godot_string(p_method);
	String arg = p_arg ? mono_string_to_godot_string(p_arg) : String();
	obj->call(method, Variant(arg));
}

static void icall_Object_CallInt(int64_t p_obj, MonoString *p_method, int64_t p_arg) {
	if (!p_obj || !p_method) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	obj->call(mono_string_to_godot_string(p_method), Variant(p_arg));
}

static void icall_Object_CallStringInt(int64_t p_obj, MonoString *p_method, MonoString *p_arg1, int64_t p_arg2) {
	if (!p_obj || !p_method) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	String method = mono_string_to_godot_string(p_method);
	String arg1 = p_arg1 ? mono_string_to_godot_string(p_arg1) : String();
	obj->call(method, Variant(arg1), Variant(p_arg2));
}

static void icall_Object_CallStringColor(int64_t p_obj, MonoString *p_method, MonoString *p_arg1, int32_t r_bits, int32_t g_bits, int32_t b_bits, int32_t a_bits) {
	if (!p_obj || !p_method) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	String method = mono_string_to_godot_string(p_method);
	String arg1 = p_arg1 ? mono_string_to_godot_string(p_arg1) : String();
	obj->call(method, Variant(arg1), Variant(bits_to_color(r_bits, g_bits, b_bits, a_bits)));
}

static void icall_Object_CallStringObject(int64_t p_obj, MonoString *p_method, MonoString *p_arg1, int64_t p_arg2) {
	if (!p_obj || !p_method) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	String method = mono_string_to_godot_string(p_method);
	String arg1 = p_arg1 ? mono_string_to_godot_string(p_arg1) : String();
	Object *arg2 = (Object *)(intptr_t)p_arg2;
	obj->call(method, Variant(arg1), Variant(arg2));
}

static int64_t icall_Object_CallNoArgsObject(int64_t p_obj, MonoString *p_method) {
	if (!p_obj || !p_method) return 0;
	Object *obj = (Object *)(intptr_t)p_obj;
	Variant ret = obj->call(mono_string_to_godot_string(p_method));
	Object *result = ret;
	if (result) prepare_for_managed_return(result);
	return (int64_t)(intptr_t)result;
}

static int64_t icall_Object_CallNoArgsInt(int64_t p_obj, MonoString *p_method) {
	if (!p_obj || !p_method) return 0;
	Object *obj = (Object *)(intptr_t)p_obj;
	Variant ret = obj->call(mono_string_to_godot_string(p_method));
	return (int64_t)ret;
}

static mono_bool icall_Object_CallNoArgsBool(int64_t p_obj, MonoString *p_method) {
	if (!p_obj || !p_method) return false;
	Object *obj = (Object *)(intptr_t)p_obj;
	Variant ret = obj->call(mono_string_to_godot_string(p_method));
	return (bool)ret;
}

static int64_t icall_Object_CallStringReturnsInt(int64_t p_obj, MonoString *p_method, MonoString *p_arg) {
	if (!p_obj || !p_method) return 0;
	Object *obj = (Object *)(intptr_t)p_obj;
	String method = mono_string_to_godot_string(p_method);
	String arg = p_arg ? mono_string_to_godot_string(p_arg) : String();
	Variant ret = obj->call(method, Variant(arg));
	return (int64_t)ret;
}

static void icall_Object_CallDeferred(int64_t p_obj, MonoString *p_method, MonoString *p_arg) {
	if (!p_obj || !p_method) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	String method = mono_string_to_godot_string(p_method);
	if (p_arg) {
		String arg = mono_string_to_godot_string(p_arg);
		obj->call_deferred(method, Variant(arg));
	} else {
		obj->call_deferred(method);
	}
}

static MonoObject *icall_Object_GetVariant(int64_t p_obj, MonoString *p_prop) {
	if (!p_obj || !p_prop) return nullptr;
	Object *obj = (Object *)(intptr_t)p_obj;
	Variant v = obj->get(mono_string_to_godot_string(p_prop));
	GDMono *gdmono = GDMono::get_singleton();
	if (!gdmono) return nullptr;
	MonoDomain *domain = gdmono->get_scripts_domain();
	MonoObject *result = variant_to_mono_object(domain, v);
	if (result) {
		Object *ref_result = v;
		if (ref_result) prepare_for_managed_return(ref_result);
	}
	return result;
}

static void icall_Object_SetVariant(int64_t p_obj, MonoString *p_prop, MonoObject *p_value) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	Variant v = mono_object_to_variant(p_value);
	obj->set(mono_string_to_godot_string(p_prop), v);
}

static MonoObject *icall_Object_CallVariant(int64_t p_obj, MonoString *p_method, MonoArray *p_args) {
	if (!p_obj || !p_method) return nullptr;
	Object *obj = (Object *)(intptr_t)p_obj;
	StringName method = mono_string_to_godot_string(p_method);
	int argcount = p_args ? mono_array_length(p_args) : 0;
	Vector<Variant> args;
	Vector<const Variant *> argptrs;
	args.resize(argcount);
	argptrs.resize(argcount);
	for (int i = 0; i < argcount; i++) {
		MonoObject *arg = mono_array_get(p_args, MonoObject *, i);
		args.write[i] = arg ? mono_object_to_variant(arg) : Variant();
		argptrs.write[i] = &args.write[i];
	}
	Variant ret;
	Callable::CallError ce;
	if (argcount == 0) {
		ret = obj->call(method);
	} else {
		ret = obj->callp(method, (const Variant **)argptrs.ptr(), argcount, ce);
	}
	GDMono *gdmono = GDMono::get_singleton();
	if (!gdmono) return nullptr;
	MonoDomain *domain = gdmono->get_scripts_domain();
	MonoObject *result = variant_to_mono_object(domain, ret);
	if (result) {
		Object *ref_result = ret;
		if (ref_result) prepare_for_managed_return(ref_result);
	}
	return result;
}

static void icall_Object_CallDeferredNoArgs(int64_t p_obj, MonoString *p_method) {
	if (!p_obj || !p_method) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	obj->call_deferred(mono_string_to_godot_string(p_method));
}

static void icall_Object_QueueFree(int64_t p_obj) {
	if (!p_obj) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	Node *node = Object::cast_to<Node>(obj);
	if (node) {
		node->queue_free();
	}
}

static MonoString *icall_Object_GetClass(int64_t p_obj) {
	if (!p_obj) return nullptr;
	Object *obj = (Object *)(intptr_t)p_obj;
	String class_name = obj->get_class();
	MonoDomain *domain = GDMono::get_singleton() ? GDMono::get_singleton()->get_scripts_domain() : mono_domain_get();
	CharString cs = class_name.utf8();
	return mono_string_new(domain, cs.get_data());
}

static mono_bool icall_Object_IsClass(int64_t p_obj, MonoString *p_class_name) {
	if (!p_obj || !p_class_name) return false;
	Object *obj = (Object *)(intptr_t)p_obj;
	return obj->is_class(mono_string_to_godot_string(p_class_name));
}

static int64_t icall_Node_GetParent(int64_t p_node) {
	if (!p_node) return 0;
	Node *node = (Node *)(intptr_t)p_node;
	Node *parent = node->get_parent();
	if (parent) prepare_for_managed_return(parent);
	return (int64_t)(intptr_t)parent;
}

// WASM: returns int64 bit pattern of double
static int64_t icall_Object_GetFloat(int64_t p_obj, MonoString *p_prop) {
	if (!p_obj || !p_prop) {
		return double_to_bits(0.0);
	}
	Object *obj = (Object *)(intptr_t)p_obj;
	Variant v = obj->get(get_prop_name(p_prop));
	return double_to_bits((double)v);
}

static MonoString *icall_Object_GetString(int64_t p_obj, MonoString *p_prop) {
	if (!p_obj || !p_prop) return nullptr;
	Object *obj = (Object *)(intptr_t)p_obj;
	Variant v = obj->get(get_prop_name(p_prop));
	String s = v;
	CharString cs = s.utf8();
	return mono_string_new(mono_domain_get(), cs.get_data());
}

static int64_t icall_Callable_CreateFromTarget(int64_t p_target, MonoString *p_method) {
	if (!p_target || !p_method) return 0;
	Object *target = (Object *)(intptr_t)p_target;
	char *utf8 = mono_string_to_utf8(p_method);
	if (!utf8) return 0;
	StringName method(String::utf8(utf8));
	mono_free(utf8);
	Callable *callable = memnew(Callable(target, method));
	return (int64_t)(intptr_t)callable;
}

static void icall_Object_Free(int64_t p_ptr) {
	// Godot objects are reference-counted; unreference RefCounted objects.
	// Non-RefCounted objects (Nodes) are owned by the scene tree.
	if (!p_ptr) return;
	Object *obj = (Object *)(intptr_t)p_ptr;
	RefCounted *r = Object::cast_to<RefCounted>(obj);
	if (r) {
		r->unreference();
	}
}

static void icall_Object_Delete(int64_t p_ptr) {
	// Used by derived C# class constructors to replace a wrong-type native
	// object created by a base class constructor.
	if (!p_ptr) return;
	Object *obj = (Object *)(intptr_t)p_ptr;
	RefCounted *r = Object::cast_to<RefCounted>(obj);
	if (r) {
		r->unreference();
		return;
	}
	Node *n = Object::cast_to<Node>(obj);
	if (n && !n->get_parent()) {
		memdelete(n);
	}
}

// ===== Engine / OS / Time =====

static int icall_Engine_GetFramesPerSecond() {
	return Engine::get_singleton()->get_frames_per_second();
}

static mono_bool icall_Engine_IsEditorHint() {
	return Engine::get_singleton()->is_editor_hint();
}

static int64_t icall_OS_GetStaticMemoryUsage() {
	return (int64_t)OS::get_singleton()->get_static_memory_usage();
}

static MonoString *icall_OS_GetName() {
	String name = OS::get_singleton()->get_name();
	CharString cs = name.utf8();
	return mono_string_new(mono_domain_get(), cs.get_data());
}

static MonoString *icall_Time_GetTimeStringFromSystem() {
	String time = Time::get_singleton()->get_time_string_from_system();
	CharString cs = time.utf8();
	return mono_string_new(mono_domain_get(), cs.get_data());
}

// ===== Networking =====

static MonoArray *icall_PacketPeer_GetPacket(int64_t p_obj) {
	if (!p_obj) return nullptr;
	Object *obj = (Object *)(intptr_t)p_obj;
	Variant ret = obj->call("get_packet");
	PackedByteArray pba = ret;
	int size = pba.size();
	MonoDomain *domain = mono_domain_get();
	MonoClass *byteClass = mono_get_byte_class();
	MonoArray *arr = mono_array_new(domain, byteClass, size);
	if (size > 0) {
		memcpy(mono_array_addr_with_size(arr, 1, 0), pba.ptr(), size);
	}
	return arr;
}

// ===== FileAccess / DirAccess icalls =====

static mono_bool icall_FileAccess_FileExists(MonoString *p_path) {
	if (!p_path) return false;
	char *path = mono_string_to_utf8(p_path);
	if (!path) return false;
	String s = String::utf8(path);
	mono_free(path);
	return FileAccess::exists(s);
}

static MonoString *icall_FileAccess_GetFileAsString(MonoString *p_path) {
	if (!p_path) return mono_string_new(mono_domain_get(), "");
	char *path = mono_string_to_utf8(p_path);
	if (!path) return mono_string_new(mono_domain_get(), "");
	String s = String::utf8(path);
	mono_free(path);
	String content = FileAccess::get_file_as_string(s);
	CharString cs = content.utf8();
	return mono_string_new(mono_domain_get(), cs.ptr());
}

static MonoArray *icall_FileAccess_GetFileAsBytes(MonoString *p_path) {
	if (!p_path) return mono_array_new(mono_domain_get(), mono_get_byte_class(), 0);
	char *path = mono_string_to_utf8(p_path);
	if (!path) return mono_array_new(mono_domain_get(), mono_get_byte_class(), 0);
	String s = String::utf8(path);
	mono_free(path);
	Ref<FileAccess> f = FileAccess::open(s, FileAccess::READ);
	if (f.is_null()) return mono_array_new(mono_domain_get(), mono_get_byte_class(), 0);
	uint64_t len = f->get_length();
	MonoArray *arr = mono_array_new(mono_domain_get(), mono_get_byte_class(), (uintptr_t)len);
	uint8_t *buf = (uint8_t *)mono_array_addr_with_size(arr, 1, 0);
	if (len > 0) f->get_buffer(buf, len);
	return arr;
}

static int icall_FileAccess_WriteFile(MonoString *p_path, MonoArray *p_data) {
	if (!p_path || !p_data) return (int)Error::ERR_INVALID_PARAMETER;
	char *path = mono_string_to_utf8(p_path);
	if (!path) return (int)Error::ERR_INVALID_PARAMETER;
	String s = String::utf8(path);
	mono_free(path);
	int len = mono_array_length(p_data);
	Ref<FileAccess> f = FileAccess::open(s, FileAccess::WRITE);
	if (f.is_null()) return (int)Error::ERR_FILE_CANT_OPEN;
	if (len > 0) {
		uint8_t *buf = (uint8_t *)mono_array_addr_with_size(p_data, 1, 0);
		f->store_buffer(buf, len);
	}
	f->close();
	return (int)Error::OK;
}

static int icall_FileAccess_MakeDirRecursive(MonoString *p_path) {
	if (!p_path) return (int)Error::ERR_INVALID_PARAMETER;
	char *path = mono_string_to_utf8(p_path);
	if (!path) return (int)Error::ERR_INVALID_PARAMETER;
	String s = String::utf8(path);
	mono_free(path);
	Error err = DirAccess::make_dir_recursive_absolute(s);
	return (int)err;
}

static mono_bool icall_FileAccess_DirExists(MonoString *p_path) {
	if (!p_path) return false;
	char *path = mono_string_to_utf8(p_path);
	if (!path) return false;
	String s = String::utf8(path);
	mono_free(path);
	return DirAccess::dir_exists_absolute(s);
}

static int icall_FileAccess_Remove(MonoString *p_path) {
	if (!p_path) return (int)Error::ERR_INVALID_PARAMETER;
	char *path = mono_string_to_utf8(p_path);
	if (!path) return (int)Error::ERR_INVALID_PARAMETER;
	String s = String::utf8(path);
	mono_free(path);
	Error err = DirAccess::remove_absolute(s);
	return (int)err;
}

static MonoString *icall_FileAccess_GetUserDataDir() {
	String dir = OS::get_singleton()->get_user_data_dir();
	CharString cs = dir.utf8();
	return mono_string_new(mono_domain_get(), cs.ptr());
}

// ===== Node icalls =====

static int icall_Node_GetChildCount(int64_t p_node) {
	if (!p_node) return 0;
	Node *n = (Node *)(intptr_t)p_node;
	return n->get_child_count();
}

static int64_t icall_Node_GetChild(int64_t p_node, int p_index) {
	if (!p_node) return 0;
	Node *n = (Node *)(intptr_t)p_node;
	Node *child = n->get_child(p_index);
	if (child) prepare_for_managed_return(child);
	return (int64_t)(intptr_t)child;
}

static MonoString *icall_Node_GetName(int64_t p_node) {
	if (!p_node) return mono_string_new(mono_domain_get(), "");
	Node *n = (Node *)(intptr_t)p_node;
	String name = n->get_name();
	CharString cs = name.utf8();
	return mono_string_new(mono_domain_get(), cs.ptr());
}

static void icall_Node_SetName(int64_t p_node, MonoString *p_name) {
	if (!p_node || !p_name) return;
	Node *n = (Node *)(intptr_t)p_node;
	char *name = mono_string_to_utf8(p_name);
	if (!name) return;
	n->set_name(String::utf8(name));
	mono_free(name);
}

static void icall_Node_RemoveChild(int64_t p_node, int64_t p_child) {
	if (!p_node || !p_child) return;
	Node *n = (Node *)(intptr_t)p_node;
	Node *c = (Node *)(intptr_t)p_child;
	n->remove_child(c);
}

static MonoString *icall_Node_GetPath(int64_t p_node) {
	if (!p_node) return mono_string_new(mono_domain_get(), "");
	Node *n = (Node *)(intptr_t)p_node;
	String path = String(n->get_path());
	CharString cs = path.utf8();
	return mono_string_new(mono_domain_get(), cs.ptr());
}

static void icall_Node_QueueFree(int64_t p_node) {
	if (!p_node) return;
	Node *n = (Node *)(intptr_t)p_node;
	n->queue_free();
}

static int64_t icall_Node_GetNode(int64_t p_node, MonoString *p_path) {
	if (!p_node || !p_path) return 0;
	Node *n = (Node *)(intptr_t)p_node;
	char *path = mono_string_to_utf8(p_path);
	if (!path) return 0;
	Node *child = n->get_node(NodePath(String::utf8(path)));
	mono_free(path);
	if (child) prepare_for_managed_return(child);
	return (int64_t)(intptr_t)child;
}

static MonoString *icall_Node_GetClassName(int64_t p_node) {
	if (!p_node) return mono_string_new(mono_domain_get(), "");
	Node *n = (Node *)(intptr_t)p_node;
	String cn = n->get_class();
	CharString cs = cn.utf8();
	return mono_string_new(mono_domain_get(), cs.ptr());
}

// ===== SceneTree icalls =====

static int icall_SceneTree_ChangeSceneToFile(int64_t p_tree, MonoString *p_path) {
	if (!p_tree || !p_path) return (int)ERR_INVALID_PARAMETER;
	SceneTree *tree = (SceneTree *)(intptr_t)p_tree;
	char *path = mono_string_to_utf8(p_path);
	if (!path) return (int)ERR_INVALID_PARAMETER;
	String path_str = String::utf8(path);
	mono_free(path);
	// Use call_deferred to avoid "Parent node is busy" errors when changing
	// scenes during _Ready().  Godot officially recommends this pattern.
	tree->call_deferred("change_scene_to_file", path_str);
	return (int)OK;
}

static int64_t icall_SceneTree_GetCurrentScene(int64_t p_tree) {
	if (!p_tree) return 0;
	SceneTree *tree = (SceneTree *)(intptr_t)p_tree;
	Node *scene = tree->get_current_scene();
	if (scene) prepare_for_managed_return(scene);
	return (int64_t)(intptr_t)scene;
}

// ===== ResourceLoader / PackedScene =====

static int64_t icall_ResourceLoader_Load(MonoString *p_path) {
	if (!p_path) return 0;
	char *path = mono_string_to_utf8(p_path);
	if (!path) return 0;
	String s = String::utf8(path);
	mono_free(path);
	Ref<Resource> res = ResourceLoader::load(s);
	if (res.is_null()) return 0;
	res->reference();
	return (int64_t)(intptr_t)res.ptr();
}

static int64_t icall_PackedScene_Instantiate(int64_t p_scene) {
	if (!p_scene) return 0;
	PackedScene *scene = (PackedScene *)(intptr_t)p_scene;
	Node *node = scene->instantiate();
	if (node) prepare_for_managed_return(node);
	return (int64_t)(intptr_t)node;
}

// ===== Safe numeric ToString =====
// These avoid mscorlib's Double.ToString() / Single.ToString() which use
// complex formatting routines that crash the Mono WASM interpreter.

static MonoString *icall_GD_DoubleToString(int64_t p_val_bits) {
	double p_val = bits_to_double(p_val_bits);
	char buf[64];
	if (p_val != p_val) {
		return mono_string_new(mono_domain_get(), "NaN");
	}
	if (p_val == (double)INFINITY || p_val > 1.7976931348623157e+308) {
		return mono_string_new(mono_domain_get(), "Infinity");
	}
	if (p_val == (double)(-INFINITY) || p_val < -1.7976931348623157e+308) {
		return mono_string_new(mono_domain_get(), "-Infinity");
	}
	if (p_val == (int64_t)p_val) {
		snprintf(buf, sizeof(buf), "%lld", (long long)(int64_t)p_val);
	} else {
		snprintf(buf, sizeof(buf), "%.6f", p_val);
		char *dot = strchr(buf, '.');
		if (dot) {
			char *end = buf + strlen(buf) - 1;
			while (end > dot && *end == '0') { *end = '\0'; end--; }
			if (*end == '.') *end = '\0';
		}
	}
	return mono_string_new(mono_domain_get(), buf);
}

static MonoString *icall_GD_Int64ToString(int64_t p_val) {
	char buf[32];
	snprintf(buf, sizeof(buf), "%lld", (long long)p_val);
	return mono_string_new(mono_domain_get(), buf);
}

static MonoString *icall_GD_FloatToString(int32_t p_val_bits) {
	float fval = bits_to_float(p_val_bits);
	return icall_GD_DoubleToString(double_to_bits((double)fval));
}

// ===== Async/await =====

static void icall_PostSyncCallback(MonoObject *p_delegate) {
	GDMono *gdmono = GDMono::get_singleton();
	if (gdmono) {
		gdmono->post_sync_delegate(p_delegate);
	}
}

// ---------------------------------------------------------------------------
// Table-driven icall registration
// ---------------------------------------------------------------------------

struct ICallEntry {
	const char *name;
	const void *func;
};

static const ICallEntry icall_entries[] = {
	// GD
	{ "Godot.GD::godot_icall_GD_Print",                (const void *)icall_GD_Print },
	{ "Godot.GD::godot_icall_GD_PrintErr",             (const void *)icall_GD_PrintErr },
	{ "Godot.GD::godot_icall_GD_Randi",                (const void *)icall_GD_Randi },
	{ "Godot.GD::godot_icall_GD_Randf",                (const void *)icall_GD_Randf },
	{ "Godot.GD::godot_icall_GD_Load",                 (const void *)icall_GD_Load },
	{ "Godot.GD::godot_icall_GD_DoubleToString",       (const void *)icall_GD_DoubleToString },
	{ "Godot.GD::godot_icall_GD_Int64ToString",        (const void *)icall_GD_Int64ToString },
	{ "Godot.GD::godot_icall_GD_FloatToString",        (const void *)icall_GD_FloatToString },
	{ "Godot.GD::godot_icall_ResourceLoader_Load",     (const void *)icall_ResourceLoader_Load },

	// GodotObject / Object
	{ "Godot.GodotObject::godot_icall_CreateObject",         (const void *)icall_CreateObject },
	{ "Godot.GodotObject::godot_icall_Object_EmitSignal",    (const void *)icall_Object_EmitSignal },
	{ "Godot.GodotObject::godot_icall_Object_SetString",     (const void *)icall_Object_SetString },
	{ "Godot.GodotObject::godot_icall_Object_SetInt",        (const void *)icall_Object_SetInt },
	{ "Godot.GodotObject::godot_icall_Object_SetFloat",      (const void *)icall_Object_SetFloat },
	{ "Godot.GodotObject::godot_icall_Object_SetBool",       (const void *)icall_Object_SetBool },
	{ "Godot.GodotObject::godot_icall_Object_SetVector2",    (const void *)icall_Object_SetVector2 },
	{ "Godot.GodotObject::godot_icall_Object_SetColor",      (const void *)icall_Object_SetColor },
	{ "Godot.GodotObject::godot_icall_Object_SetObject",     (const void *)icall_Object_SetObject },
	{ "Godot.GodotObject::godot_icall_Object_CallString",    (const void *)icall_Object_CallString },
	{ "Godot.GodotObject::godot_icall_Object_CallInt",       (const void *)icall_Object_CallInt },
	{ "Godot.GodotObject::godot_icall_Object_CallStringInt", (const void *)icall_Object_CallStringInt },
	{ "Godot.GodotObject::godot_icall_Object_CallStringColor",(const void *)icall_Object_CallStringColor },
	{ "Godot.GodotObject::godot_icall_Object_CallStringObject",(const void *)icall_Object_CallStringObject },
	{ "Godot.GodotObject::godot_icall_Object_CallNoArgs",    (const void *)icall_Object_CallNoArgs },
	{ "Godot.GodotObject::godot_icall_Object_CallNoArgsObject",(const void *)icall_Object_CallNoArgsObject },
	{ "Godot.GodotObject::godot_icall_Object_CallNoArgsInt", (const void *)icall_Object_CallNoArgsInt },
	{ "Godot.GodotObject::godot_icall_Object_CallNoArgsBool",(const void *)icall_Object_CallNoArgsBool },
	{ "Godot.GodotObject::godot_icall_Object_CallStringReturnsInt",(const void *)icall_Object_CallStringReturnsInt },
	{ "Godot.GodotObject::godot_icall_Object_CallDeferred", (const void *)icall_Object_CallDeferred },
	{ "Godot.GodotObject::godot_icall_Object_CallDeferredNoArgs",(const void *)icall_Object_CallDeferredNoArgs },
	{ "Godot.GodotObject::godot_icall_Object_GetVariant",   (const void *)icall_Object_GetVariant },
	{ "Godot.GodotObject::godot_icall_Object_SetVariant",   (const void *)icall_Object_SetVariant },
	{ "Godot.GodotObject::godot_icall_Object_CallVariant",  (const void *)icall_Object_CallVariant },
	{ "Godot.GodotObject::godot_icall_Object_QueueFree",    (const void *)icall_Object_QueueFree },
	{ "Godot.GodotObject::godot_icall_Object_GetClass",     (const void *)icall_Object_GetClass },
	{ "Godot.GodotObject::godot_icall_Object_IsClass",      (const void *)icall_Object_IsClass },
	{ "Godot.GodotObject::godot_icall_Object_GetFloat",     (const void *)icall_Object_GetFloat },
	{ "Godot.GodotObject::godot_icall_Object_GetString",    (const void *)icall_Object_GetString },
	{ "Godot.GodotObject::godot_icall_Object_Free",         (const void *)icall_Object_Free },
	{ "Godot.GodotObject::godot_icall_Object_Delete",       (const void *)icall_Object_Delete },
	{ "Godot.GodotObject::godot_icall_InputEventKey_GetKeycode",(const void *)icall_InputEventKey_GetKeycode },
	{ "Godot.GodotObject::godot_icall_InputEventKey_IsPressed",(const void *)icall_InputEventKey_IsPressed },
	{ "Godot.GodotObject::godot_icall_PacketPeer_GetPacket", (const void *)icall_PacketPeer_GetPacket },

	// Node
	{ "Godot.GodotObject::godot_icall_Node_AddChild",  (const void *)icall_Node_AddChild },
	{ "Godot.Node::godot_icall_Node_GetParent",     (const void *)icall_Node_GetParent },
	{ "Godot.Node::godot_icall_Node_GetChildCount", (const void *)icall_Node_GetChildCount },
	{ "Godot.Node::godot_icall_Node_GetChild",      (const void *)icall_Node_GetChild },
	{ "Godot.Node::godot_icall_Node_GetName",       (const void *)icall_Node_GetName },
	{ "Godot.Node::godot_icall_Node_SetName",       (const void *)icall_Node_SetName },
	{ "Godot.Node::godot_icall_Node_RemoveChild",   (const void *)icall_Node_RemoveChild },
	{ "Godot.Node::godot_icall_Node_GetPath",       (const void *)icall_Node_GetPath },
	{ "Godot.Node::godot_icall_Node_QueueFree",     (const void *)icall_Node_QueueFree },
	{ "Godot.Node::godot_icall_Node_GetNode",       (const void *)icall_Node_GetNode },
	{ "Godot.Node::godot_icall_Node_GetClassName",  (const void *)icall_Node_GetClassName },

	// Input
	{ "Godot.Input::godot_icall_Input_IsKeyPressed",       (const void *)icall_Input_IsKeyPressed },
	{ "Godot.Input::godot_icall_Input_IsActionPressed",    (const void *)icall_Input_IsActionPressed },
	{ "Godot.Input::godot_icall_Input_IsActionJustPressed",(const void *)icall_Input_IsActionJustPressed },
	{ "Godot.Input::godot_icall_Input_IsActionJustReleased",(const void *)icall_Input_IsActionJustReleased },

	// Engine
	{ "Godot.Engine::godot_icall_Engine_GetFramesPerSecond",(const void *)icall_Engine_GetFramesPerSecond },
	{ "Godot.Engine::godot_icall_Engine_IsEditorHint",     (const void *)icall_Engine_IsEditorHint },

	// OS
	{ "Godot.OS::godot_icall_OS_GetStaticMemoryUsage",(const void *)icall_OS_GetStaticMemoryUsage },
	{ "Godot.OS::godot_icall_OS_GetName",             (const void *)icall_OS_GetName },

	// Time
	{ "Godot.Time::godot_icall_Time_GetTimeStringFromSystem",(const void *)icall_Time_GetTimeStringFromSystem },

	// Callable
	{ "Godot.Callable::godot_icall_Callable_CreateFromTarget",(const void *)icall_Callable_CreateFromTarget },

	// FileAccess
	{ "Godot.FileAccess::godot_icall_FileAccess_FileExists",      (const void *)icall_FileAccess_FileExists },
	{ "Godot.FileAccess::godot_icall_FileAccess_GetFileAsString", (const void *)icall_FileAccess_GetFileAsString },
	{ "Godot.FileAccess::godot_icall_FileAccess_GetFileAsBytes",  (const void *)icall_FileAccess_GetFileAsBytes },
	{ "Godot.FileAccess::godot_icall_FileAccess_WriteFile",       (const void *)icall_FileAccess_WriteFile },
	{ "Godot.FileAccess::godot_icall_FileAccess_MakeDirRecursive",(const void *)icall_FileAccess_MakeDirRecursive },
	{ "Godot.FileAccess::godot_icall_FileAccess_DirExists",       (const void *)icall_FileAccess_DirExists },
	{ "Godot.FileAccess::godot_icall_FileAccess_Remove",          (const void *)icall_FileAccess_Remove },
	{ "Godot.FileAccess::godot_icall_FileAccess_GetUserDataDir",  (const void *)icall_FileAccess_GetUserDataDir },

	// SceneTree
	{ "Godot.SceneTree::godot_icall_SceneTree_ChangeSceneToFile",(const void *)icall_SceneTree_ChangeSceneToFile },
	{ "Godot.SceneTree::godot_icall_SceneTree_GetCurrentScene",  (const void *)icall_SceneTree_GetCurrentScene },

	// PackedScene
	{ "Godot.PackedScene::godot_icall_PackedScene_Instantiate",(const void *)icall_PackedScene_Instantiate },

	// Async/await
	{ "Godot.GDMonoAccess::godot_icall_PostSyncCallback",(const void *)icall_PostSyncCallback },
};

void variant_register_icalls() {
	MonoLogger::log("Registering Mono interop icalls...");

	const int count = sizeof(icall_entries) / sizeof(icall_entries[0]);
	for (int i = 0; i < count; i++) {
		mono_add_internal_call(icall_entries[i].name, icall_entries[i].func);
	}

	MonoLogger::log(vformat("Mono interop icalls registered (%d entries)", count));
}

// ---------------------------------------------------------------------------
// Enum type name table
// ---------------------------------------------------------------------------

namespace EnumUtil {
	const char *GetTypeName(VariantTypeManaged p_type) {
		switch (p_type) {
			case VariantTypeManaged::Vector2: return "Vector2";
			case VariantTypeManaged::Vector2i: return "Vector2i";
			case VariantTypeManaged::Vector3: return "Vector3";
			case VariantTypeManaged::Vector3i: return "Vector3i";
			case VariantTypeManaged::Vector4: return "Vector4";
			case VariantTypeManaged::Vector4i: return "Vector4i";
			case VariantTypeManaged::Color: return "Color";
			case VariantTypeManaged::Rect2: return "Rect2";
			case VariantTypeManaged::Rect2i: return "Rect2i";
			case VariantTypeManaged::Plane: return "Plane";
			case VariantTypeManaged::Quaternion: return "Quaternion";
			case VariantTypeManaged::AABB: return "AABB";
			case VariantTypeManaged::Basis: return "Basis";
			case VariantTypeManaged::Transform2D: return "Transform2D";
			case VariantTypeManaged::Transform3D: return "Transform3D";
			case VariantTypeManaged::Projection: return "Projection";
			case VariantTypeManaged::RID: return "RID";
			case VariantTypeManaged::StringName: return "StringName";
			case VariantTypeManaged::NodePath: return "NodePath";
			case VariantTypeManaged::Callable: return "Callable";
			case VariantTypeManaged::Signal: return "Signal";
			default: return "Variant";
		}
	}
}

} // namespace GDMonoInterop
