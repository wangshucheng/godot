#include "gd_mono_interop_variant.h"

#include "../../mono_runtime/gd_mono.h"
#include "../../utils/mono_logger.h"

#include "core/object/object.h"
#include "core/os/os.h"
#include "core/os/keyboard.h"
#include "core/input/input.h"
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
#include <cstring>

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
	mono_runtime_object_init(obj);
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
		ensure_native_instance_field();
		if (strcmp(class_name, "GodotObject") == 0 || (godot_object_class && mono_class_is_subclass_of(cls, godot_object_class, false))) {
			Object *native = (Object *)get_native_object(p_obj);
			if (native) return Variant(native);
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

static int64_t icall_GD_Randi() {
	return Math::rand();
}

static double icall_GD_Randf() {
	return Math::randf();
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

static mono_bool icall_Object_EmitSignal(void *p_native_ptr, MonoString *p_signal, MonoArray *p_args) {
	if (!p_native_ptr || !p_signal) return false;
	Object *obj = (Object *)p_native_ptr;
	String signal_str = String::utf8(mono_string_to_utf8(p_signal));
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

static void *icall_CreateObject(MonoString *p_class_name) {
	if (!p_class_name) return nullptr;
	char *utf8 = mono_string_to_utf8(p_class_name);
	if (!utf8) return nullptr;
	String class_name = String::utf8(utf8);
	mono_free(utf8);
	Object *obj = ClassDB::instantiate(class_name);
	if (!obj) {
		MonoLogger::log_error("Failed to create object of type: " + class_name);
		return nullptr;
	}
	return obj;
}

static void icall_Node_AddChild(void *p_parent, void *p_child) {
	if (!p_parent || !p_child) return;
	Node *parent = (Node *)p_parent;
	Node *child = (Node *)p_child;
	parent->add_child(child);
}

static void icall_Object_SetString(void *p_obj, MonoString *p_prop, MonoString *p_value) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)p_obj;
	String prop = String::utf8(mono_string_to_utf8(p_prop));
	String value = p_value ? String::utf8(mono_string_to_utf8(p_value)) : String();
	obj->set(prop, Variant(value));
}

static void icall_Object_SetInt(void *p_obj, MonoString *p_prop, int64_t p_value) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)p_obj;
	String prop = String::utf8(mono_string_to_utf8(p_prop));
	obj->set(prop, Variant(p_value));
}

static void icall_Object_SetFloat(void *p_obj, MonoString *p_prop, float p_value) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)p_obj;
	String prop = String::utf8(mono_string_to_utf8(p_prop));
	obj->set(prop, Variant(p_value));
}

static void icall_Object_SetBool(void *p_obj, MonoString *p_prop, mono_bool p_value) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)p_obj;
	String prop = String::utf8(mono_string_to_utf8(p_prop));
	obj->set(prop, Variant((bool)(p_value != 0)));
}

static void icall_Object_SetVector2(void *p_obj, MonoString *p_prop, float x, float y) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)p_obj;
	String prop = String::utf8(mono_string_to_utf8(p_prop));
	obj->set(prop, Variant(Vector2(x, y)));
}

static void icall_Object_SetColor(void *p_obj, MonoString *p_prop, float r, float g, float b, float a) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)p_obj;
	String prop = String::utf8(mono_string_to_utf8(p_prop));
	obj->set(prop, Variant(Color(r, g, b, a)));
}

static void icall_Object_SetObject(void *p_obj, MonoString *p_prop, void *p_value) {
	if (!p_obj || !p_prop) return;
	Object *obj = (Object *)p_obj;
	String prop = String::utf8(mono_string_to_utf8(p_prop));
	Object *value = (Object *)p_value;
	obj->set(prop, Variant(value));
}

static void icall_Object_CallString(void *p_obj, MonoString *p_method, MonoString *p_arg) {
	if (!p_obj || !p_method) return;
	Object *obj = (Object *)p_obj;
	String method = String::utf8(mono_string_to_utf8(p_method));
	String arg = p_arg ? String::utf8(mono_string_to_utf8(p_arg)) : String();
	obj->call(method, Variant(arg));
}

static void icall_Object_CallInt(void *p_obj, MonoString *p_method, int64_t p_arg) {
	if (!p_obj || !p_method) return;
	Object *obj = (Object *)p_obj;
	String method = String::utf8(mono_string_to_utf8(p_method));
	obj->call(method, Variant(p_arg));
}

static void icall_Object_CallStringInt(void *p_obj, MonoString *p_method, MonoString *p_arg1, int64_t p_arg2) {
	if (!p_obj || !p_method) return;
	Object *obj = (Object *)p_obj;
	String method = String::utf8(mono_string_to_utf8(p_method));
	String arg1 = p_arg1 ? String::utf8(mono_string_to_utf8(p_arg1)) : String();
	obj->call(method, Variant(arg1), Variant(p_arg2));
}

static void icall_Object_CallStringColor(void *p_obj, MonoString *p_method, MonoString *p_arg1, float r, float g, float b, float a) {
	if (!p_obj || !p_method) return;
	Object *obj = (Object *)p_obj;
	String method = String::utf8(mono_string_to_utf8(p_method));
	String arg1 = p_arg1 ? String::utf8(mono_string_to_utf8(p_arg1)) : String();
	obj->call(method, Variant(arg1), Variant(Color(r, g, b, a)));
}

static void icall_Object_CallStringObject(void *p_obj, MonoString *p_method, MonoString *p_arg1, void *p_arg2) {
	if (!p_obj || !p_method) return;
	Object *obj = (Object *)p_obj;
	String method = String::utf8(mono_string_to_utf8(p_method));
	String arg1 = p_arg1 ? String::utf8(mono_string_to_utf8(p_arg1)) : String();
	Object *arg2 = (Object *)p_arg2;
	obj->call(method, Variant(arg1), Variant(arg2));
}

static void icall_Object_CallNoArgs(void *p_obj, MonoString *p_method) {
	if (!p_obj || !p_method) return;
	Object *obj = (Object *)p_obj;
	String method = String::utf8(mono_string_to_utf8(p_method));
	obj->call(method);
}

static void *icall_Object_CallNoArgsObject(void *p_obj, MonoString *p_method) {
	if (!p_obj || !p_method) return nullptr;
	Object *obj = (Object *)p_obj;
	String method = String::utf8(mono_string_to_utf8(p_method));
	Variant ret = obj->call(method);
	Object *result = ret;
	return result;
}

static double icall_Object_GetFloat(void *p_obj, MonoString *p_prop) {
	if (!p_obj || !p_prop) return 0.0;
	Object *obj = (Object *)p_obj;
	String prop = String::utf8(mono_string_to_utf8(p_prop));
	Variant v = obj->get(prop);
	return (double)v;
}

static MonoString *icall_Object_GetString(void *p_obj, MonoString *p_prop) {
	if (!p_obj || !p_prop) return nullptr;
	Object *obj = (Object *)p_obj;
	String prop = String::utf8(mono_string_to_utf8(p_prop));
	Variant v = obj->get(prop);
	String s = v;
	return mono_string_new(mono_domain_get(), s.utf8().get_data());
}

static int64_t icall_Engine_GetFramesPerSecond() {
	return (int64_t)Engine::get_singleton()->get_frames_per_second();
}

static int64_t icall_OS_GetStaticMemoryUsage() {
	return (int64_t)OS::get_singleton()->get_static_memory_usage();
}

static MonoString *icall_Time_GetTimeStringFromSystem() {
	String time = Time::get_singleton()->get_time_string_from_system();
	return mono_string_new(mono_domain_get(), time.utf8().get_data());
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
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_CallNoArgsObject", (const void *)icall_Object_CallNoArgsObject);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_GetFloat", (const void *)icall_Object_GetFloat);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_GetString", (const void *)icall_Object_GetString);
	mono_add_internal_call("Godot.Engine::godot_icall_Engine_GetFramesPerSecond", (const void *)icall_Engine_GetFramesPerSecond);
	mono_add_internal_call("Godot.OS::godot_icall_OS_GetStaticMemoryUsage", (const void *)icall_OS_GetStaticMemoryUsage);
	mono_add_internal_call("Godot.Time::godot_icall_Time_GetTimeStringFromSystem", (const void *)icall_Time_GetTimeStringFromSystem);
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
