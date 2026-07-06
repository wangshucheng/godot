#ifndef GD_MONO_INTEROP_VARIANT_H
#define GD_MONO_INTEROP_VARIANT_H

#include "core/variant/variant.h"
#include "core/string/ustring.h"
#include "core/string/string_name.h"
#include "core/object/object.h"

#include <mono/mono-publib.h>

namespace GDMonoInterop {

enum class VariantTypeManaged : int {
	Nil = 0,
	Bool = 1,
	Int = 2,
	Float = 3,
	String = 4,
	Vector2 = 5,
	Vector2i = 6,
	Rect2 = 7,
	Rect2i = 8,
	Vector3 = 9,
	Vector3i = 10,
	Transform2D = 11,
	Vector4 = 12,
	Vector4i = 13,
	Plane = 14,
	Quaternion = 15,
	AABB = 16,
	Basis = 17,
	Transform3D = 18,
	Projection = 19,
	Color = 20,
	StringName = 21,
	NodePath = 22,
	RID = 23,
	Object = 24,
	Callable = 25,
	Signal = 26,
	Dictionary = 27,
	Array = 28,
	PackedByteArray = 29,
	PackedInt32Array = 30,
	PackedInt64Array = 31,
	PackedFloat32Array = 32,
	PackedFloat64Array = 33,
	PackedStringArray = 34,
	PackedVector2Array = 35,
	PackedVector3Array = 36,
	PackedColorArray = 37,
	Max = 38,
};

struct MonoVector2 {
	float x;
	float y;
};

struct MonoVector2i {
	int32_t x;
	int32_t y;
};

struct MonoVector3 {
	float x;
	float y;
	float z;
};

struct MonoVector3i {
	int32_t x;
	int32_t y;
	int32_t z;
};

struct MonoVector4 {
	float x;
	float y;
	float z;
	float w;
};

struct MonoVector4i {
	int32_t x;
	int32_t y;
	int32_t z;
	int32_t w;
};

struct MonoColor {
	float r;
	float g;
	float b;
	float a;
};

struct MonoRect2 {
	MonoVector2 position;
	MonoVector2 size;
};

struct MonoRect2i {
	MonoVector2i position;
	MonoVector2i size;
};

struct MonoPlane {
	float x;
	float y;
	float z;
	float d;
};

struct MonoQuaternion {
	float x;
	float y;
	float z;
	float w;
};

struct MonoAABB {
	MonoVector3 position;
	MonoVector3 size;
};

struct MonoBasis {
	MonoVector3 rows[3];
};

struct MonoTransform2D {
	MonoVector2 columns[3];
};

struct MonoTransform3D {
	MonoBasis basis;
	MonoVector3 origin;
};

struct MonoProjection {
	MonoVector4 columns[4];
};

struct MonoRID {
	uint64_t id;
};

Variant::Type managed_type_to_variant_type(VariantTypeManaged p_managed_type);
VariantTypeManaged variant_type_to_managed(Variant::Type p_type);

MonoObject *variant_to_mono_object(MonoDomain *p_domain, const Variant &p_variant);
Variant mono_object_to_variant(MonoObject *p_obj, VariantTypeManaged p_hint_type = VariantTypeManaged::Nil);

void *get_native_object(MonoObject *p_managed);
MonoObject *get_managed_wrapper(MonoDomain *p_domain, Object *p_native);

void variant_register_icalls();

}

#endif
