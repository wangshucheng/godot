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
#include "core/templates/hash_map.h"
#include "core/variant/variant.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"
#include "core/variant/array.h"
#include "core/error/error_list.h"
#include "core/templates/rid.h"
#include "core/string/node_path.h"
#include "core/math/math_funcs.h"
#include "scene/main/node.h"
#include "core/object/class_db.h"
#include "core/config/engine.h"
#include "core/os/time.h"
#include "scene/main/canvas_layer.h"
#include "scene/gui/control.h"

#include <mono/mono-publib.h>
#include <cstdint>
#include <cstring>
#include <cstdio>

// Forward-declare Mono API functions from <mono/metadata/object.h> that are
// not exposed in our minimal Mono header set. These are linked statically
// from libmonosgen-2.0.a.
extern "C" {
MonoObject *mono_field_get_value_object(MonoDomain *domain, MonoClassField *field, MonoObject *obj);
}

namespace {
// Helper: convert MonoString to Godot String, freeing the native UTF-8 buffer.
static String mono_string_to_godot_string(MonoString *p_str) {
	if (!p_str) return String();
	char *utf8 = mono_string_to_utf8(p_str);
	if (!utf8) return String();
	String s = String::utf8(utf8);
	mono_free(utf8);
	return s;
}
} // namespace
#include "core/io/file_access.h"
#include "core/io/dir_access.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/packed_scene.h"

namespace GDMonoInterop {

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
	MonoClass *cls = mono_class_from_name(image, "Godot", capitalize_first(class_name).utf8().get_data());
	if (!cls)
		cls = godot_object_class;
	if (!cls) return nullptr;
	MonoObject *obj = mono_object_new(p_domain, cls);
	if (!obj) return nullptr;
	// 注意: 不要调用 mono_runtime_object_init, 否则 C# 构造函数会再次
	// 执行 godot_icall_CreateObject 为"已存在的原生对象"创建一个孤儿原生对象,
	// 造成内存泄漏。这里只设置 nativeInstance 字段即可。
	if (native_instance_field) {
		void *native_ptr = p_native;
		mono_field_set_value(obj, native_instance_field, &native_ptr);
	}

	if (gdmono) {
		gdmono->cache_managed_object(oid, obj);
	}

	return obj;
}

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
			return (MonoObject *)mono_string_new(p_domain, str.utf8().get_data());
		}

		case Variant::VECTOR2: {
			Vector2 vec = p_variant;
			MonoVector2 mvec = { vec.x, vec.y };
			MonoClass *cls = mono_class_from_name(image, "Godot", "Vector2");
			if (!cls) return nullptr;
			MonoObject *obj = mono_object_new(p_domain, cls);
			if (!obj) return nullptr;
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
				MonoObject *ctor_exc = nullptr;
				mono_runtime_invoke(ctor, obj, args, &ctor_exc);
				if (ctor_exc) {
					MonoLogger::log_error("Exception in .ctor invocation");
				}
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
				MonoObject *ctor_exc = nullptr;
				mono_runtime_invoke(ctor, obj, args, &ctor_exc);
				if (ctor_exc) {
					MonoLogger::log_error("Exception in .ctor invocation");
				}
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
			if (!obj_ptr) return nullptr;
			return get_managed_wrapper(p_domain, obj_ptr);
		}

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

		case Variant::DICTIONARY: {
			Dictionary dict = p_variant;
			MonoClass *cls = mono_class_from_name(image, "Godot", "Dictionary");
			if (!cls) return nullptr;
			MonoObject *obj = mono_object_new(p_domain, cls);
			if (!obj) return nullptr;
			mono_runtime_object_init(obj);
			Array keys = dict.keys();
			for (int i = 0; i < keys.size(); i++) {
				Variant key = keys[i];
				Variant val = dict[key];
				MonoObject *m_key = variant_to_mono_object(p_domain, key);
				MonoObject *m_val = variant_to_mono_object(p_domain, val);
				if (m_key && m_val) {
					MonoMethod *add_method = mono_class_get_method_from_name(cls, "Add", 2);
					if (add_method) {
						void *args[2] = { m_key, m_val };
						MonoObject *exc = nullptr;
						mono_runtime_invoke(add_method, obj, args, &exc);
					}
				}
			}
			return obj;
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
		// StringName / NodePath: extract the inner string value.
		// These C# types are reference-type wrappers around a single private string field.
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
		// Callable: if nativeCallable != 0, return the underlying Godot Callable pointer.
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
			// Fall back to TargetDelegate if nativeCallable is null.
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
		// Signal: build a Signal Variant from Owner + Name.
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
		ensure_native_instance_field();
		if (strcmp(class_name, "GodotObject") == 0 || (godot_object_class && mono_class_is_subclass_of(cls, godot_object_class, false))) {
			Object *native = (Object *)get_native_object(p_obj);
			if (native) return Variant(native);
		}
	}

	return Variant();
}

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

// WASM workaround: double return triggers CANNOT HANDLE COOKIE D in
// do_icall. Return IEEE-754 bit pattern as int64; C# reinterprets via union.
static int64_t icall_GD_Randf() {
	double val = Math::randf();
	int64_t bits;
	memcpy(&bits, &val, sizeof(double));
	return bits;
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

// ===== UI and Node Manipulation Internal Calls =====

static int64_t icall_CreateObject(MonoString *p_class_name) {
	if (!p_class_name) {
		return 0;
	}
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

static void icall_Object_SetString(int64_t p_obj, MonoString *p_prop, MonoString *p_value) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	String prop = mono_string_to_godot_string(p_prop);
	String value = p_value ? mono_string_to_godot_string(p_value) : String();
	obj->set(prop, Variant(value));
}

static void icall_Object_SetInt(int64_t p_obj, MonoString *p_prop, int64_t p_value) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	String prop = mono_string_to_godot_string(p_prop);
	obj->set(prop, Variant(p_value));
}

// WASM workaround: accept int32 bit pattern instead of float.
static void icall_Object_SetFloat(int64_t p_obj, MonoString *p_prop, int32_t p_value_bits) {
	if (!p_obj || !p_prop) return;
	float p_value;
	memcpy(&p_value, &p_value_bits, sizeof(float));
	Object *obj = (Object *)(intptr_t)p_obj;
	String prop = mono_string_to_godot_string(p_prop);
	obj->set(prop, Variant(p_value));
}

static void icall_Object_SetBool(int64_t p_obj, MonoString *p_prop, mono_bool p_value) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	String prop = mono_string_to_godot_string(p_prop);
	obj->set(prop, Variant((bool)(p_value != 0)));
}

static void icall_Object_SetVector2(int64_t p_obj, MonoString *p_prop, int32_t x_bits, int32_t y_bits) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	String prop = mono_string_to_godot_string(p_prop);
	float x, y;
	memcpy(&x, &x_bits, sizeof(float));
	memcpy(&y, &y_bits, sizeof(float));
	obj->set(prop, Variant(Vector2(x, y)));
}

static void icall_Object_SetColor(int64_t p_obj, MonoString *p_prop, int32_t r_bits, int32_t g_bits, int32_t b_bits, int32_t a_bits) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	String prop = mono_string_to_godot_string(p_prop);
	float r, g, b, a;
	memcpy(&r, &r_bits, sizeof(float));
	memcpy(&g, &g_bits, sizeof(float));
	memcpy(&b, &b_bits, sizeof(float));
	memcpy(&a, &a_bits, sizeof(float));
	obj->set(prop, Variant(Color(r, g, b, a)));
}

static void icall_Object_SetObject(int64_t p_obj, MonoString *p_prop, int64_t p_value) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	String prop = mono_string_to_godot_string(p_prop);
	Object *value = (Object *)(intptr_t)p_value;
	obj->set(prop, Variant(value));
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
	String method = mono_string_to_godot_string(p_method);
	obj->call(method, Variant(p_arg));
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
	float r, g, b, a;
	memcpy(&r, &r_bits, sizeof(float));
	memcpy(&g, &g_bits, sizeof(float));
	memcpy(&b, &b_bits, sizeof(float));
	memcpy(&a, &a_bits, sizeof(float));
	obj->call(method, Variant(arg1), Variant(Color(r, g, b, a)));
}

static void icall_Object_CallStringObject(int64_t p_obj, MonoString *p_method, MonoString *p_arg1, int64_t p_arg2) {
	if (!p_obj || !p_method) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	String method = mono_string_to_godot_string(p_method);
	String arg1 = p_arg1 ? mono_string_to_godot_string(p_arg1) : String();
	Object *arg2 = (Object *)(intptr_t)p_arg2;
	obj->call(method, Variant(arg1), Variant(arg2));
}

// Call a method deferred (at end of current frame via MessageQueue).
// This avoids "Parent node is busy" errors when switching scenes during _Ready().
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

// Call a method deferred with no arguments.
static void icall_Object_CallDeferredNoArgs(int64_t p_obj, MonoString *p_method) {
    if (!p_obj || !p_method) return;
    Object *obj = (Object *)(intptr_t)p_obj;
    String method = mono_string_to_godot_string(p_method);
    obj->call_deferred(method);
}

// Queue free for Node (deferred). Non-Node objects are not freed here.
static void icall_Object_QueueFree(int64_t p_obj) {
    if (!p_obj) return;
    Object *obj = (Object *)(intptr_t)p_obj;
    Node *node = Object::cast_to<Node>(obj);
    if (node) {
        node->queue_free();
    }
    // Non-Node objects: do nothing (caller should use Dispose instead)
}

// Get the native class name of an object.
static MonoString *icall_Object_GetClass(int64_t p_obj) {
    if (!p_obj) return nullptr;
    Object *obj = (Object *)(intptr_t)p_obj;
    String class_name = obj->get_class();
    MonoDomain *domain = GDMono::get_singleton() ? GDMono::get_singleton()->get_scripts_domain() : mono_domain_get();
    return mono_string_new(domain, class_name.utf8().get_data());
}

// Check if object is a specific class.
static mono_bool icall_Object_IsClass(int64_t p_obj, MonoString *p_class_name) {
    if (!p_obj || !p_class_name) return false;
    Object *obj = (Object *)(intptr_t)p_obj;
    String class_name = mono_string_to_godot_string(p_class_name);
    return obj->is_class(class_name);
}

// Get parent node (for Node).
static int64_t icall_Node_GetParent(int64_t p_node) {
    if (!p_node) return 0;
    Node *node = (Node *)(intptr_t)p_node;
    return (int64_t)(intptr_t)node->get_parent();
}

static void icall_Object_CallNoArgs(int64_t p_obj, MonoString *p_method) {
	if (!p_obj || !p_method) return;
	Object *obj = (Object *)(intptr_t)p_obj;
	String method = mono_string_to_godot_string(p_method);
	obj->call(method);
}

static int64_t icall_Object_CallNoArgsObject(int64_t p_obj, MonoString *p_method) {
	if (!p_obj || !p_method) return 0;
	Object *obj = (Object *)(intptr_t)p_obj;
	String method = mono_string_to_godot_string(p_method);
	Variant ret = obj->call(method);
	Object *result = ret;
	return (int64_t)(intptr_t)result;
}

// WASM workaround: return int64 bit pattern instead of double.
static int64_t icall_Object_GetFloat(int64_t p_obj, MonoString *p_prop) {
	if (!p_obj || !p_prop) {
		double zero = 0.0;
		int64_t bits;
		memcpy(&bits, &zero, sizeof(double));
		return bits;
	}
	Object *obj = (Object *)(intptr_t)p_obj;
	String prop = mono_string_to_godot_string(p_prop);
	Variant v = obj->get(prop);
	double val = (double)v;
	int64_t bits;
	memcpy(&bits, &val, sizeof(double));
	return bits;
}

static MonoString *icall_Object_GetString(int64_t p_obj, MonoString *p_prop) {
	if (!p_obj || !p_prop) return nullptr;
	Object *obj = (Object *)(intptr_t)p_obj;
	String prop = mono_string_to_godot_string(p_prop);
	Variant v = obj->get(prop);
	String s = v;
	return mono_string_new(mono_domain_get(), s.utf8().get_data());
}

static int icall_Engine_GetFramesPerSecond() {
	return Engine::get_singleton()->get_frames_per_second();
}

static int64_t icall_OS_GetStaticMemoryUsage() {
	return (int64_t)OS::get_singleton()->get_static_memory_usage();
}

static MonoString *icall_Time_GetTimeStringFromSystem() {
	String time = Time::get_singleton()->get_time_string_from_system();
	return mono_string_new(mono_domain_get(), time.utf8().get_data());
}

// ===== WebSocket and Networking Internal Calls =====

static int64_t icall_Object_CallNoArgsInt(int64_t p_obj, MonoString *p_method) {
	if (!p_obj || !p_method) return 0;
	Object *obj = (Object *)(intptr_t)p_obj;
	String method = mono_string_to_godot_string(p_method);
	Variant ret = obj->call(method);
	return (int64_t)ret;
}

static int64_t icall_Object_CallStringReturnsInt(int64_t p_obj, MonoString *p_method, MonoString *p_arg) {
	if (!p_obj || !p_method) return 0;
	Object *obj = (Object *)(intptr_t)p_obj;
	String method = mono_string_to_godot_string(p_method);
	String arg = p_arg ? mono_string_to_godot_string(p_arg) : String();
	Variant ret = obj->call(method, Variant(arg));
	return (int64_t)ret;
}

// Get the packet data from a PacketPeer (like WebSocketPeer) as a byte array.
// Returns a MonoArray* of bytes, or null if no packet available.
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

static mono_bool icall_Object_CallNoArgsBool(int64_t p_obj, MonoString *p_method) {
	if (!p_obj || !p_method) return false;
	Object *obj = (Object *)(intptr_t)p_obj;
	String method = mono_string_to_godot_string(p_method);
	Variant ret = obj->call(method);
	return (bool)ret;
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

static mono_bool icall_Engine_IsEditorHint() {
	return Engine::get_singleton()->is_editor_hint();
}

static MonoString *icall_OS_GetName() {
	String name = OS::get_singleton()->get_name();
	return mono_string_new(mono_domain_get(), name.utf8().get_data());
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
	// Godot objects are reference-counted; just unreference.
	// RefCounted objects will be freed when refcount reaches 0.
	// Non-RefCounted objects (Nodes) are owned by the scene tree.
	if (!p_ptr) return;
	Object *obj = (Object *)(intptr_t)p_ptr;
	RefCounted *r = Object::cast_to<RefCounted>(obj);
	if (r) {
		r->unreference();
	}
}

// Delete a native object. Used by derived C# class constructors to replace
// a wrong-type native object created by a base class constructor.
// For RefCounted: unreference. For Nodes: only delete if no parent (not in scene tree).
static void icall_Object_Delete(int64_t p_ptr) {
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



// === GodotSharp extension icalls - added for test suite ===

// --- FileAccess icalls ---
static mono_bool icall_FileAccess_FileExists(MonoString *p_path) {
	// M3 修复: 补空检查
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
	return mono_string_new(mono_domain_get(), content.utf8().ptr());
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
	return mono_string_new(mono_domain_get(), dir.utf8().ptr());
}

// --- Node icalls ---
static int icall_Node_GetChildCount(int64_t p_node) {
	// M3 修复: 补空检查
	if (!p_node) return 0;
	Node *n = (Node *)(intptr_t)p_node;
	return n->get_child_count();
}

static int64_t icall_Node_GetChild(int64_t p_node, int p_index) {
	if (!p_node) return 0;
	Node *n = (Node *)(intptr_t)p_node;
	return (int64_t)(intptr_t)n->get_child(p_index);
}

static MonoString *icall_Node_GetName(int64_t p_node) {
	if (!p_node) return mono_string_new(mono_domain_get(), "");
	Node *n = (Node *)(intptr_t)p_node;
	return mono_string_new(mono_domain_get(), String(n->get_name()).utf8().ptr());
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
	return mono_string_new(mono_domain_get(), String(n->get_path()).utf8().ptr());
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
	return (int64_t)(intptr_t)child;
}

static MonoString *icall_Node_GetClassName(int64_t p_node) {
	if (!p_node) return mono_string_new(mono_domain_get(), "");
	Node *n = (Node *)(intptr_t)p_node;
	return mono_string_new(mono_domain_get(), n->get_class().utf8().ptr());
}

// --- SceneTree icalls ---
static int icall_SceneTree_ChangeSceneToFile(int64_t p_tree, MonoString *p_path) {
	if (!p_tree || !p_path) return (int)ERR_INVALID_PARAMETER;
	SceneTree *tree = (SceneTree *)(intptr_t)p_tree;
	char *path = mono_string_to_utf8(p_path);
	if (!path) return (int)ERR_INVALID_PARAMETER;
	String path_str = String::utf8(path);
	mono_free(path);
	// 延迟到当前帧末尾执行：在 _Ready 中直接调 change_scene_to_file 会触发
	// "Parent node is busy adding/removing children" + DEV_ASSERT(!current_scene) 崩溃。
	// Godot 官方同样建议切换场景走 call_deferred。
	tree->call_deferred("change_scene_to_file", path_str);
	return (int)OK;
}

static int64_t icall_SceneTree_GetCurrentScene(int64_t p_tree) {
	if (!p_tree) return 0;
	SceneTree *tree = (SceneTree *)(intptr_t)p_tree;
	return (int64_t)(intptr_t)tree->get_current_scene();
}

// --- ResourceLoader icall ---
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

// --- PackedScene icall ---
static int64_t icall_PackedScene_Instantiate(int64_t p_scene) {
	PackedScene *scene = (PackedScene *)(intptr_t)p_scene;
	if (!scene) return 0;
	return (int64_t)(intptr_t)scene->instantiate();
}


// Safe numeric ToString - avoids Double.ToString()/Int64.ToString() which
// crash in WASM interpreter mode due to complex formatting routines.
static MonoString *icall_GD_DoubleToString(int64_t p_val_bits) {
    double p_val;
    memcpy(&p_val, &p_val_bits, sizeof(double));
    char buf[64];
    // M5 修复: 处理 NaN/Infinity 特殊值，避免 (int64_t)p_val 未定义行为
    if (p_val != p_val) { // NaN check (NaN != NaN)
        return mono_string_new(mono_domain_get(), "NaN");
    }
    if (p_val == (double)INFINITY || p_val > 1.7976931348623157e+308) {
        return mono_string_new(mono_domain_get(), "Infinity");
    }
    if (p_val == (double)(-INFINITY) || p_val < -1.7976931348623157e+308) {
        return mono_string_new(mono_domain_get(), "-Infinity");
    }
    // Simple formatting: show up to 6 decimal places, strip trailing zeros.
    // P2-7 修复: 原代码 (int)p_val 会把 int64 截断为 int32，导致大整数
    // (例如 5_000_000_000) 被错误地截断显示。改用 %lld + (long long)。
    if (p_val == (int64_t)p_val) {
        snprintf(buf, sizeof(buf), "%lld", (long long)(int64_t)p_val);
    } else {
        snprintf(buf, sizeof(buf), "%.6f", p_val);
        // Strip trailing zeros
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
    float fval;
    memcpy(&fval, &p_val_bits, sizeof(float));
    double dval = (double)fval;
    int64_t bits;
    memcpy(&bits, &dval, sizeof(double));
    return icall_GD_DoubleToString(bits);
}

// Async/await: C# calls this to post a continuation delegate (Action)
// to be invoked on the next Godot frame tick.
static void icall_PostSyncCallback(MonoObject *p_delegate) {
	GDMono *gdmono = GDMono::get_singleton();
	if (gdmono) {
		gdmono->post_sync_delegate(p_delegate);
	}
}

void variant_register_icalls() {
	MonoLogger::log("Registering Mono interop icalls...");

	mono_add_internal_call("Godot.GD::godot_icall_GD_Print", (const void *)icall_GD_Print);
	mono_add_internal_call("Godot.GD::godot_icall_GD_PrintErr", (const void *)icall_GD_PrintErr);
	mono_add_internal_call("Godot.GD::godot_icall_GD_Randi", (const void *)icall_GD_Randi);
	mono_add_internal_call("Godot.GD::godot_icall_GD_Randf", (const void *)icall_GD_Randf);
	mono_add_internal_call("Godot.GD::godot_icall_GD_Load", (const void *)icall_GD_Load);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_EmitSignal", (const void *)icall_Object_EmitSignal);
	mono_add_internal_call("Godot.Input::godot_icall_Input_IsKeyPressed", (const void *)icall_Input_IsKeyPressed);
	mono_add_internal_call("Godot.Input::godot_icall_Input_IsActionPressed", (const void *)icall_Input_IsActionPressed);

	mono_add_internal_call("Godot.GodotObject::godot_icall_CreateObject", (const void *)icall_CreateObject);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Node_AddChild", (const void *)icall_Node_AddChild);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_SetString", (const void *)icall_Object_SetString);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_SetInt", (const void *)icall_Object_SetInt);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_SetFloat", (const void *)icall_Object_SetFloat);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_SetBool", (const void *)icall_Object_SetBool);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_SetVector2", (const void *)icall_Object_SetVector2);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_SetColor", (const void *)icall_Object_SetColor);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_SetObject", (const void *)icall_Object_SetObject);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_CallString", (const void *)icall_Object_CallString);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_CallInt", (const void *)icall_Object_CallInt);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_CallStringInt", (const void *)icall_Object_CallStringInt);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_CallStringColor", (const void *)icall_Object_CallStringColor);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_CallStringObject", (const void *)icall_Object_CallStringObject);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_CallNoArgs", (const void *)icall_Object_CallNoArgs);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_CallDeferred", (const void *)icall_Object_CallDeferred);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_CallDeferredNoArgs", (const void *)icall_Object_CallDeferredNoArgs);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_QueueFree", (const void *)icall_Object_QueueFree);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_GetClass", (const void *)icall_Object_GetClass);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_IsClass", (const void *)icall_Object_IsClass);
	mono_add_internal_call("Godot.Node::godot_icall_Node_GetParent", (const void *)icall_Node_GetParent);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_CallNoArgsObject", (const void *)icall_Object_CallNoArgsObject);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_GetFloat", (const void *)icall_Object_GetFloat);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_GetString", (const void *)icall_Object_GetString);
	mono_add_internal_call("Godot.Engine::godot_icall_Engine_GetFramesPerSecond", (const void *)icall_Engine_GetFramesPerSecond);
	mono_add_internal_call("Godot.OS::godot_icall_OS_GetStaticMemoryUsage", (const void *)icall_OS_GetStaticMemoryUsage);
	mono_add_internal_call("Godot.Time::godot_icall_Time_GetTimeStringFromSystem", (const void *)icall_Time_GetTimeStringFromSystem);
	mono_add_internal_call("Godot.GodotObject::godot_icall_InputEventKey_GetKeycode", (const void *)icall_InputEventKey_GetKeycode);
	mono_add_internal_call("Godot.GodotObject::godot_icall_InputEventKey_IsPressed", (const void *)icall_InputEventKey_IsPressed);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_CallNoArgsInt", (const void *)icall_Object_CallNoArgsInt);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_CallStringReturnsInt", (const void *)icall_Object_CallStringReturnsInt);
	mono_add_internal_call("Godot.GodotObject::godot_icall_PacketPeer_GetPacket", (const void *)icall_PacketPeer_GetPacket);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_CallNoArgsBool", (const void *)icall_Object_CallNoArgsBool);
	mono_add_internal_call("Godot.Input::godot_icall_Input_IsActionJustPressed", (const void *)icall_Input_IsActionJustPressed);
	mono_add_internal_call("Godot.Input::godot_icall_Input_IsActionJustReleased", (const void *)icall_Input_IsActionJustReleased);
	mono_add_internal_call("Godot.Engine::godot_icall_Engine_IsEditorHint", (const void *)icall_Engine_IsEditorHint);
	mono_add_internal_call("Godot.OS::godot_icall_OS_GetName", (const void *)icall_OS_GetName);
	mono_add_internal_call("Godot.Callable::godot_icall_Callable_CreateFromTarget", (const void *)icall_Callable_CreateFromTarget);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_Free", (const void *)icall_Object_Free);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_Delete", (const void *)icall_Object_Delete);

	mono_add_internal_call("Godot.FileAccess::godot_icall_FileAccess_FileExists", (const void *)icall_FileAccess_FileExists);
	mono_add_internal_call("Godot.FileAccess::godot_icall_FileAccess_GetFileAsString", (const void *)icall_FileAccess_GetFileAsString);
	mono_add_internal_call("Godot.FileAccess::godot_icall_FileAccess_GetFileAsBytes", (const void *)icall_FileAccess_GetFileAsBytes);
	mono_add_internal_call("Godot.FileAccess::godot_icall_FileAccess_WriteFile", (const void *)icall_FileAccess_WriteFile);
	mono_add_internal_call("Godot.FileAccess::godot_icall_FileAccess_MakeDirRecursive", (const void *)icall_FileAccess_MakeDirRecursive);
	mono_add_internal_call("Godot.FileAccess::godot_icall_FileAccess_DirExists", (const void *)icall_FileAccess_DirExists);
	mono_add_internal_call("Godot.FileAccess::godot_icall_FileAccess_Remove", (const void *)icall_FileAccess_Remove);
	mono_add_internal_call("Godot.FileAccess::godot_icall_FileAccess_GetUserDataDir", (const void *)icall_FileAccess_GetUserDataDir);
	mono_add_internal_call("Godot.Node::godot_icall_Node_GetChildCount", (const void *)icall_Node_GetChildCount);
	mono_add_internal_call("Godot.Node::godot_icall_Node_GetChild", (const void *)icall_Node_GetChild);
	mono_add_internal_call("Godot.Node::godot_icall_Node_GetName", (const void *)icall_Node_GetName);
	mono_add_internal_call("Godot.Node::godot_icall_Node_SetName", (const void *)icall_Node_SetName);
	mono_add_internal_call("Godot.Node::godot_icall_Node_RemoveChild", (const void *)icall_Node_RemoveChild);
	mono_add_internal_call("Godot.Node::godot_icall_Node_GetPath", (const void *)icall_Node_GetPath);
	mono_add_internal_call("Godot.Node::godot_icall_Node_QueueFree", (const void *)icall_Node_QueueFree);
	mono_add_internal_call("Godot.Node::godot_icall_Node_GetNode", (const void *)icall_Node_GetNode);
	mono_add_internal_call("Godot.Node::godot_icall_Node_GetClassName", (const void *)icall_Node_GetClassName);
	mono_add_internal_call("Godot.SceneTree::godot_icall_SceneTree_ChangeSceneToFile", (const void *)icall_SceneTree_ChangeSceneToFile);
	mono_add_internal_call("Godot.SceneTree::godot_icall_SceneTree_GetCurrentScene", (const void *)icall_SceneTree_GetCurrentScene);
	mono_add_internal_call("Godot.GD::godot_icall_ResourceLoader_Load", (const void *)icall_ResourceLoader_Load);
	mono_add_internal_call("Godot.PackedScene::godot_icall_PackedScene_Instantiate", (const void *)icall_PackedScene_Instantiate);

	// Safe numeric ToString icalls (avoid WASM interpreter crashes with Double.ToString)
	mono_add_internal_call("Godot.GD::godot_icall_GD_DoubleToString", (const void *)icall_GD_DoubleToString);
	mono_add_internal_call("Godot.GD::godot_icall_GD_Int64ToString", (const void *)icall_GD_Int64ToString);
	mono_add_internal_call("Godot.GD::godot_icall_GD_FloatToString", (const void *)icall_GD_FloatToString);

	// Async/await support: PostSyncCallback icall for GodotSynchronizationContext
	mono_add_internal_call("Godot.GDMonoAccess::godot_icall_PostSyncCallback", (const void *)icall_PostSyncCallback);

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
