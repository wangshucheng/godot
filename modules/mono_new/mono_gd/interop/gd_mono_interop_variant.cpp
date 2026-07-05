#include "gd_mono_interop_variant.h"

#include "../../utils/mono_logger.h"

#include <mono/mono-publib.h>
#include <cstring>

namespace GDMonoInterop {

namespace EnumUtil {
	const char *GetTypeName(VariantTypeManaged p_type);
}

Variant::Type managed_type_to_variant_type(VariantTypeManaged p_managed_type) {
	return static_cast<Variant::Type>(static_cast<int>(p_managed_type));
}

VariantTypeManaged variant_type_to_managed(Variant::Type p_type) {
	return static_cast<VariantTypeManaged>(static_cast<int>(p_type));
}

static MonoClass *get_mono_class_for_variant_type(MonoDomain *p_domain, VariantTypeManaged p_type) {
	MonoImage *image = mono_get_corlib();
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

MonoObject *variant_to_mono_object(MonoDomain *p_domain, const Variant &p_variant) {
	if (p_variant.is_null())
		return nullptr;

	Variant::Type type = p_variant.get_type();
	MonoImage *image = mono_get_corlib();
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
			return mono_string_new(p_domain, str.utf8().get_data());
		}

		case Variant::VECTOR2: {
			Vector2 vec = p_variant;
			MonoVector2 mvec = { vec.x, vec.y };
			MonoClass *cls = mono_class_from_name(image, "Godot", "Vector2");
			if (!cls) return nullptr;
			MonoObject *obj = mono_object_new(p_domain, cls);
			if (!obj) return nullptr;
			MonoVTable *vtable = mono_class_vtable(p_domain, cls);
			void *raw = mono_object_unbox(obj);
			memcpy(raw, &mvec, sizeof(MonoVector2));
			return obj;
		}

		case Variant::VECTOR2I: {
			Vector2i vec = p_variant;
			MonoVector2i mvec = { vec.x, vec.y };
			MonoClass *cls = mono_class_from_name(image, "Godot", "Vector2i");
			if (!cls) return nullptr;
			MonoObject *obj = mono_object_new(p_domain, cls);
			if (!obj) return nullptr;
			void *raw = mono_object_unbox(obj);
			memcpy(raw, &mvec, sizeof(MonoVector2i));
			return obj;
		}

		case Variant::VECTOR3: {
			Vector3 vec = p_variant;
			MonoVector3 mvec = { vec.x, vec.y, vec.z };
			MonoClass *cls = mono_class_from_name(image, "Godot", "Vector3");
			if (!cls) return nullptr;
			MonoObject *obj = mono_object_new(p_domain, cls);
			if (!obj) return nullptr;
			void *raw = mono_object_unbox(obj);
			memcpy(raw, &mvec, sizeof(MonoVector3));
			return obj;
		}

		case Variant::VECTOR3I: {
			Vector3i vec = p_variant;
			MonoVector3i mvec = { vec.x, vec.y, vec.z };
			MonoClass *cls = mono_class_from_name(image, "Godot", "Vector3i");
			if (!cls) return nullptr;
			MonoObject *obj = mono_object_new(p_domain, cls);
			if (!obj) return nullptr;
			void *raw = mono_object_unbox(obj);
			memcpy(raw, &mvec, sizeof(MonoVector3i));
			return obj;
		}

		case Variant::COLOR: {
			Color col = p_variant;
			MonoColor mcol = { col.r, col.g, col.b, col.a };
			MonoClass *cls = mono_class_from_name(image, "Godot", "Color");
			if (!cls) return nullptr;
			MonoObject *obj = mono_object_new(p_domain, cls);
			if (!obj) return nullptr;
			void *raw = mono_object_unbox(obj);
			memcpy(raw, &mcol, sizeof(MonoColor));
			return obj;
		}

		case Variant::RECT2: {
			Rect2 rect = p_variant;
			MonoRect2 mrect = { { rect.position.x, rect.position.y }, { rect.size.x, rect.size.y } };
			MonoClass *cls = mono_class_from_name(image, "Godot", "Rect2");
			if (!cls) return nullptr;
			MonoObject *obj = mono_object_new(p_domain, cls);
			if (!obj) return nullptr;
			void *raw = mono_object_unbox(obj);
			memcpy(raw, &mrect, sizeof(MonoRect2));
			return obj;
		}

		case Variant::QUATERNION: {
			Quaternion q = p_variant;
			MonoQuaternion mq = { q.x, q.y, q.z, q.w };
			MonoClass *cls = mono_class_from_name(image, "Godot", "Quaternion");
			if (!cls) return nullptr;
			MonoObject *obj = mono_object_new(p_domain, cls);
			if (!obj) return nullptr;
			void *raw = mono_object_unbox(obj);
			memcpy(raw, &mq, sizeof(MonoQuaternion));
			return obj;
		}

		case Variant::PLANE: {
			Plane p = p_variant;
			MonoPlane mp = { p.normal.x, p.normal.y, p.normal.z, p.d };
			MonoClass *cls = mono_class_from_name(image, "Godot", "Plane");
			if (!cls) return nullptr;
			MonoObject *obj = mono_object_new(p_domain, cls);
			if (!obj) return nullptr;
			void *raw = mono_object_unbox(obj);
			memcpy(raw, &mp, sizeof(MonoPlane));
			return obj;
		}

		case Variant::AABB: {
			AABB aabb = p_variant;
			MonoAABB maabb = { { aabb.position.x, aabb.position.y, aabb.position.z }, { aabb.size.x, aabb.size.y, aabb.size.z } };
			MonoClass *cls = mono_class_from_name(image, "Godot", "AABB");
			if (!cls) return nullptr;
			MonoObject *obj = mono_object_new(p_domain, cls);
			if (!obj) return nullptr;
			void *raw = mono_object_unbox(obj);
			memcpy(raw, &maabb, sizeof(MonoAABB));
			return obj;
		}

		case Variant::BASIS: {
			Basis basis = p_variant;
			MonoBasis mbasis;
			for (int i = 0; i < 3; i++) {
				mbasis.rows[i].x = basis.rows[i].x;
				mbasis.rows[i].y = basis.rows[i].y;
				mbasis.rows[i].z = basis.rows[i].z;
			}
			MonoClass *cls = mono_class_from_name(image, "Godot", "Basis");
			if (!cls) return nullptr;
			MonoObject *obj = mono_object_new(p_domain, cls);
			if (!obj) return nullptr;
			void *raw = mono_object_unbox(obj);
			memcpy(raw, &mbasis, sizeof(MonoBasis));
			return obj;
		}

		case Variant::TRANSFORM3D: {
			Transform3D tr = p_variant;
			MonoTransform3D mtr;
			for (int i = 0; i < 3; i++) {
				mtr.basis.rows[i].x = tr.basis.rows[i].x;
				mtr.basis.rows[i].y = tr.basis.rows[i].y;
				mtr.basis.rows[i].z = tr.basis.rows[i].z;
			}
			mtr.origin.x = tr.origin.x;
			mtr.origin.y = tr.origin.y;
			mtr.origin.z = tr.origin.z;
			MonoClass *cls = mono_class_from_name(image, "Godot", "Transform3D");
			if (!cls) return nullptr;
			MonoObject *obj = mono_object_new(p_domain, cls);
			if (!obj) return nullptr;
			void *raw = mono_object_unbox(obj);
			memcpy(raw, &mtr, sizeof(MonoTransform3D));
			return obj;
		}

		case Variant::TRANSFORM2D: {
			Transform2D tr = p_variant;
			MonoTransform2D mtr;
			for (int i = 0; i < 3; i++) {
				mtr.columns[i].x = tr.columns[i].x;
				mtr.columns[i].y = tr.columns[i].y;
			}
			MonoClass *cls = mono_class_from_name(image, "Godot", "Transform2D");
			if (!cls) return nullptr;
			MonoObject *obj = mono_object_new(p_domain, cls);
			if (!obj) return nullptr;
			void *raw = mono_object_unbox(obj);
			memcpy(raw, &mtr, sizeof(MonoTransform2D));
			return obj;
		}

		case Variant::STRING_NAME: {
			StringName sn = p_variant;
			MonoString *str = mono_string_new(p_domain, String(sn).utf8().get_data());
			MonoClass *cls = mono_class_from_name(image, "Godot", "StringName");
			if (!cls) return (MonoObject *)str;
			MonoObject *obj = mono_object_new(p_domain, cls);
			if (!obj) return (MonoObject *)str;
			MonoMethod *ctor = mono_class_get_method_from_name(cls, ".ctor", 1);
			if (ctor) {
				void *args[1] = { str };
				mono_runtime_invoke(ctor, obj, args, nullptr);
			}
			return obj;
		}

		case Variant::NODE_PATH: {
			NodePath np = p_variant;
			MonoString *str = mono_string_new(p_domain, String(np).utf8().get_data());
			MonoClass *cls = mono_class_from_name(image, "Godot", "NodePath");
			if (!cls) return (MonoObject *)str;
			MonoObject *obj = mono_object_new(p_domain, cls);
			if (!obj) return (MonoObject *)str;
			MonoMethod *ctor = mono_class_get_method_from_name(cls, ".ctor", 1);
			if (ctor) {
				void *args[1] = { str };
				mono_runtime_invoke(ctor, obj, args, nullptr);
			}
			return obj;
		}

		case Variant::RID: {
			RID rid = p_variant;
			MonoRID mrid = { rid.is_valid() ? rid.get_id() : 0 };
			MonoClass *cls = mono_class_from_name(image, "Godot", "RID");
			if (!cls) return nullptr;
			MonoObject *obj = mono_object_new(p_domain, cls);
			if (!obj) return nullptr;
			void *raw = mono_object_unbox(obj);
			memcpy(raw, &mrid, sizeof(MonoRID));
			return obj;
		}

		case Variant::OBJECT: {
			Object *obj_ptr = p_variant;
			if (!obj_ptr)
				return nullptr;
			return nullptr;
		}

		default:
			return nullptr;
	}
}

Variant mono_object_to_variant(MonoObject *p_obj, VariantTypeManaged p_hint_type) {
	if (!p_obj)
		return Variant();

	MonoClass *cls = mono_object_get_class(p_obj);
	if (!cls)
		return Variant();

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
		if (strcmp(class_name, "Vector2") == 0) {
			MonoVector2 *mvec = (MonoVector2 *)mono_object_unbox(p_obj);
			return Vector2(mvec->x, mvec->y);
		}
		if (strcmp(class_name, "Vector2i") == 0) {
			MonoVector2i *mvec = (MonoVector2i *)mono_object_unbox(p_obj);
			return Vector2i(mvec->x, mvec->y);
		}
		if (strcmp(class_name, "Vector3") == 0) {
			MonoVector3 *mvec = (MonoVector3 *)mono_object_unbox(p_obj);
			return Vector3(mvec->x, mvec->y, mvec->z);
		}
		if (strcmp(class_name, "Vector3i") == 0) {
			MonoVector3i *mvec = (MonoVector3i *)mono_object_unbox(p_obj);
			return Vector3i(mvec->x, mvec->y, mvec->z);
		}
		if (strcmp(class_name, "Color") == 0) {
			MonoColor *mcol = (MonoColor *)mono_object_unbox(p_obj);
			return Color(mcol->r, mcol->g, mcol->b, mcol->a);
		}
		if (strcmp(class_name, "Rect2") == 0) {
			MonoRect2 *mrect = (MonoRect2 *)mono_object_unbox(p_obj);
			return Rect2(mrect->position.x, mrect->position.y, mrect->size.x, mrect->size.y);
		}
		if (strcmp(class_name, "Quaternion") == 0) {
			MonoQuaternion *mq = (MonoQuaternion *)mono_object_unbox(p_obj);
			return Quaternion(mq->x, mq->y, mq->z, mq->w);
		}
		if (strcmp(class_name, "Plane") == 0) {
			MonoPlane *mp = (MonoPlane *)mono_object_unbox(p_obj);
			return Plane(mp->x, mp->y, mp->z, mp->d);
		}
	}

	return Variant();
}

static MonoObject *icall_GD_Print(MonoString *p_msg) {
	if (!p_msg) return nullptr;
	char *utf8 = mono_string_to_utf8(p_msg);
	if (utf8) {
		String msg = String::utf8(utf8);
		print_line(vformat("[C#] %s", msg));
		mono_free(utf8);
	}
	return nullptr;
}

static MonoObject *icall_GD_PrintErr(MonoString *p_msg) {
	if (!p_msg) return nullptr;
	char *utf8 = mono_string_to_utf8(p_msg);
	if (utf8) {
		String msg = String::utf8(utf8);
		ERR_PRINT(vformat("[C#] %s", msg));
		mono_free(utf8);
	}
	return nullptr;
}

static MonoString *icall_GodotString_ToMonoString(const String *p_str) {
	if (!p_str) return mono_string_empty(mono_domain_get());
	return mono_string_new(mono_domain_get(), p_str->utf8().get_data());
}

void variant_register_icalls() {
	MonoLogger::log("Registering Mono interop icalls...");

	mono_add_internal_call("Godot.GD::godot_icall_GD_Print", (const void *)icall_GD_Print);
	mono_add_internal_call("Godot.GD::godot_icall_GD_PrintErr", (const void *)icall_GD_PrintErr);
	mono_add_internal_call("Godot.GD::godot_string_new", (const void *)icall_GodotString_ToMonoString);

	MonoLogger::log("Mono interop icalls registered");
}

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

}
