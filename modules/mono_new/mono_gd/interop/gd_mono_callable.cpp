#include "gd_mono_callable.h"
#include "gd_mono_interop_variant.h"

#include "../../mono_runtime/gd_mono.h"
#include "../../utils/mono_logger.h"

#include "core/object/object.h"
#include "core/variant/variant.h"
#include "core/variant/callable.h"
#include "core/os/memory.h"
#include "core/templates/hashfuncs.h"
#include "core/string/print_string.h"

#include <mono/mono-publib.h>
#include <cstring>

bool MonoCallableCustom::compare_equal(const CallableCustom *p_a, const CallableCustom *p_b) {
	const MonoCallableCustom *a = static_cast<const MonoCallableCustom *>(p_a);
	const MonoCallableCustom *b = static_cast<const MonoCallableCustom *>(p_b);
	return a->object_id == b->object_id && a->gchandle == b->gchandle;
}

bool MonoCallableCustom::compare_less(const CallableCustom *p_a, const CallableCustom *p_b) {
	const MonoCallableCustom *a = static_cast<const MonoCallableCustom *>(p_a);
	const MonoCallableCustom *b = static_cast<const MonoCallableCustom *>(p_b);
	if (a->object_id != b->object_id) return a->object_id < b->object_id;
	return a->gchandle < b->gchandle;
}

uint32_t MonoCallableCustom::hash() const {
	uint32_t hash = object_id.hash();
	hash = hash_murmur3_one_32(gchandle, hash);
	return hash;
}

String MonoCallableCustom::get_as_text() const {
	Object *obj = ObjectDB::get_instance(object_id);
	String obj_str = obj ? obj->to_string() : "null";
	return obj_str + "::[MonoDelegate]";
}

CallableCustom::CompareEqualFunc MonoCallableCustom::get_compare_equal_func() const {
	return compare_equal;
}

CallableCustom::CompareLessFunc MonoCallableCustom::get_compare_less_func() const {
	return compare_less;
}

ObjectID MonoCallableCustom::get_object() const {
	return object_id;
}

int MonoCallableCustom::get_argument_count(bool &r_is_valid) const {
	r_is_valid = delegate_handle != nullptr;
	return 0;
}

void MonoCallableCustom::call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_call_error) const {
	if (!delegate_handle) {
		r_call_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return;
	}

	GDMono *gdmono = GDMono::get_singleton();
	if (!gdmono) {
		r_call_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return;
	}

	MonoDomain *domain = gdmono->get_scripts_domain();
	if (!domain) {
		r_call_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return;
	}

	MonoObject *exc = nullptr;
	MonoArray *args_arr = mono_array_new(domain, mono_get_object_class(), p_argcount);
	if (!args_arr) {
		r_call_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return;
	}

	for (int i = 0; i < p_argcount; i++) {
		MonoObject *arg_obj = GDMonoInterop::variant_to_mono_object(domain, *p_arguments[i]);
		if (arg_obj) {
			MonoObject **slot = (MonoObject **)mono_array_addr_with_size(args_arr, sizeof(MonoObject *), i);
			if (slot) *slot = arg_obj;
		}
	}

	MonoClass *delegate_class = mono_object_get_class(delegate_handle);
	MonoMethod *invoke_method = mono_class_get_method_from_name(delegate_class, "Invoke", p_argcount);
	if (!invoke_method) {
		r_call_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return;
	}

	MonoObject *ret = mono_runtime_invoke(invoke_method, delegate_handle, (void **)mono_array_addr_with_size(args_arr, sizeof(void *), 0), &exc);

	if (exc) {
		mono_print_unhandled_exception(exc);
		r_call_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return;
	}

	if (ret) {
		r_return_value = GDMonoInterop::mono_object_to_variant(ret);
	} else {
		r_return_value = Variant();
	}

	r_call_error.error = Callable::CallError::CALL_OK;
}

void MonoCallableCustom::release_delegate() {
	if (gchandle != 0) {
		mono_gchandle_free(gchandle);
		gchandle = 0;
	}
	delegate_handle = nullptr;
}

bool MonoCallableCustom::is_valid() const {
	return delegate_handle != nullptr && ObjectDB::get_instance(object_id) != nullptr;
}

MonoCallableCustom::MonoCallableCustom(Object *p_object, MonoObject *p_delegate, const StringName &p_method) {
	object_id = p_object ? p_object->get_instance_id() : ObjectID();
	delegate_handle = p_delegate;
	method_name = p_method;
	if (p_delegate) {
		gchandle = mono_gchandle_new(p_delegate, false);
	} else {
		gchandle = 0;
	}
}

MonoCallableCustom::~MonoCallableCustom() {
	release_delegate();
}

static MonoObject *icall_Callable_CreateFromDelegate(MonoObject *p_delegate) {
	if (!p_delegate) return nullptr;
	Callable callable = GDMonoCallable::create_callable_from_mono_delegate(p_delegate);
	GDMono *gdmono = GDMono::get_singleton();
	if (!gdmono) return nullptr;
	MonoDomain *domain = gdmono->get_scripts_domain();
	MonoClass *cls = mono_class_from_name(mono_get_corlib(), "Godot", "Callable");
	if (!cls) return nullptr;
	MonoObject *obj = mono_object_new(domain, cls);
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

static void icall_Callable_Call(MonoObject *p_native_callable, MonoArray *p_args, MonoObject **r_ret, MonoObject **r_exc) {
	if (!p_native_callable) return;
	Callable *callable = nullptr;
	void *ptr = nullptr;
	GDMono *gdmono = GDMono::get_singleton();
	if (!gdmono) return;
	MonoDomain *domain = gdmono->get_scripts_domain();
	MonoClass *cls = mono_object_get_class(p_native_callable);
	MonoClassField *field = mono_class_get_field_from_name(cls, "nativeCallable");
	if (!field) field = mono_class_get_field_from_name(cls, "_nativeCallable");
	if (field) {
		mono_field_get_value(p_native_callable, field, &ptr);
		callable = (Callable *)ptr;
	}
	if (!callable) return;

	int argcount = mono_array_length(p_args);
	Vector<Variant> args;
	args.resize(argcount);
	Vector<const Variant *> argptrs;
	argptrs.resize(argcount);
	for (int i = 0; i < argcount; i++) {
		MonoObject *arg = mono_array_get(p_args, MonoObject *, i);
		args.write[i] = arg ? GDMonoInterop::mono_object_to_variant(arg) : Variant();
		argptrs.write[i] = &args.write[i];
	}

	Variant ret;
	Callable::CallError err;
	callable->callp((const Variant **)argptrs.ptr(), argcount, ret, err);

	if (r_ret && err.error == Callable::CallError::CALL_OK) {
		*r_ret = GDMonoInterop::variant_to_mono_object(domain, ret);
	}
}

static mono_bool icall_Object_ConnectSignal(void *p_native_ptr, MonoString *p_signal, MonoObject *p_callable, mono_bool p_oneshot) {
	if (!p_native_ptr || !p_signal || !p_callable) return false;
	Object *obj = (Object *)p_native_ptr;
	String signal_str = String::utf8(mono_string_to_utf8(p_signal));
	StringName signal_name(signal_str);
	if (!obj->has_signal(signal_name)) return false;

	Callable target = GDMonoCallable::create_callable_from_mono_delegate(p_callable);
	if (p_oneshot) {
		return obj->connect(signal_name, target, Object::CONNECT_ONE_SHOT) == OK;
	} else {
		return obj->connect(signal_name, target) == OK;
	}
}

static mono_bool icall_Object_DisconnectSignal(void *p_native_ptr, MonoString *p_signal, MonoObject *p_callable) {
	if (!p_native_ptr || !p_signal || !p_callable) return false;
	Object *obj = (Object *)p_native_ptr;
	String signal_str = String::utf8(mono_string_to_utf8(p_signal));
	StringName signal_name(signal_str);
	if (!obj->has_signal(signal_name)) return false;

	Callable target = GDMonoCallable::create_callable_from_mono_delegate(p_callable);
	obj->disconnect(signal_name, target);
	return true;
}

void GDMonoCallable::register_icalls() {
	mono_add_internal_call("Godot.Callable::godot_icall_Callable_CreateFromDelegate", (const void *)icall_Callable_CreateFromDelegate);
	mono_add_internal_call("Godot.Callable::godot_icall_Callable_Call", (const void *)icall_Callable_Call);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_ConnectSignal", (const void *)icall_Object_ConnectSignal);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_DisconnectSignal", (const void *)icall_Object_DisconnectSignal);
}

Callable GDMonoCallable::create_callable_from_mono_delegate(MonoObject *p_delegate) {
	if (!p_delegate) return Callable();
	MonoClass *del_class = mono_object_get_class(p_delegate);
	MonoClassField *target_field = mono_class_get_field_from_name(del_class, "_target");
	if (!target_field) target_field = mono_class_get_field_from_name(del_class, "target");
	MonoClassField *method_field = mono_class_get_field_from_name(del_class, "_methodPtr");
	if (!method_field) method_field = mono_class_get_field_from_name(del_class, "method_ptr");
	Object *target_obj = nullptr;
	StringName method_name;
	if (target_field) {
		MonoObject *target = nullptr;
		mono_field_get_value(p_delegate, target_field, &target);
		if (target) {
			target_obj = (Object *)GDMonoInterop::get_native_object(target);
		}
	}
	Callable custom_callable;
	MonoCallableCustom *custom = memnew(MonoCallableCustom(target_obj, p_delegate, method_name));
	custom_callable = Callable(custom);
	return custom_callable;
}

MonoObject *GDMonoCallable::create_mono_delegate_from_callable(MonoDomain *p_domain, const Callable &p_callable) {
	return nullptr;
}
