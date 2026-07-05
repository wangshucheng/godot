#include "mono_callable.h"
#include "mono_variant.h"
#include "mono_bridge.h"
#include "core/object/object.h"
#include <mono/metadata/appdomain.h>
#include <mono/metadata/exception.h>
#include <cstdio>
#include <cstring>
#include <cstdarg>

using namespace mono_variant;
using namespace mono_bridge;

bool CallableCustomMono::compare_equal(const CallableCustom *p_a, const CallableCustom *p_b) {
	const CallableCustomMono *a = static_cast<const CallableCustomMono *>(p_a);
	const CallableCustomMono *b = static_cast<const CallableCustomMono *>(p_b);
	if (a->gchandle == 0 || b->gchandle == 0) return false;
	MonoObject *del_a = mono_gchandle_get_target(a->gchandle);
	MonoObject *del_b = mono_gchandle_get_target(b->gchandle);
	if (!del_a || !del_b) return false;
	return del_a == del_b;
}

bool CallableCustomMono::compare_less(const CallableCustom *p_a, const CallableCustom *p_b) {
	const CallableCustomMono *a = static_cast<const CallableCustomMono *>(p_a);
	const CallableCustomMono *b = static_cast<const CallableCustomMono *>(p_b);
	return a->gchandle < b->gchandle;
}

uint32_t CallableCustomMono::hash() const {
	return (uint32_t)gchandle;
}

String CallableCustomMono::get_as_text() const {
	return "CSharpDelegate()";
}

CallableCustom::CompareEqualFunc CallableCustomMono::get_compare_equal_func() const {
	return compare_equal;
}

CallableCustom::CompareLessFunc CallableCustomMono::get_compare_less_func() const {
	return compare_less;
}

StringName CallableCustomMono::get_method() const {
	return StringName("_csharp_delegate");
}

ObjectID CallableCustomMono::get_object() const {
	return ObjectID();
}

int CallableCustomMono::get_argument_count(bool &r_is_valid) const {
	r_is_valid = false;
	return 0;
}

bool CallableCustomMono::is_valid() const {
	if (gchandle == 0) return false;
	MonoObject *target = mono_gchandle_get_target(gchandle);
	return target != nullptr;
}

void CallableCustomMono::call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_call_error) const {
	if (gchandle == 0 || domain == nullptr) {
		r_call_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return;
	}

	MonoObject *delegate = mono_gchandle_get_target(gchandle);
	if (!delegate) {
		r_call_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return;
	}

	MonoClass *del_class = mono_object_get_class(delegate);
	MonoMethod *invoke_method = mono_get_delegate_invoke(del_class);
	if (!invoke_method) {
		r_call_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return;
	}

	MonoClass *obj_class = mono_get_object_class();
	MonoArray *args_array = mono_array_new(domain, obj_class, p_argcount);
	for (int i = 0; i < p_argcount; i++) {
		MonoObject *arg = variant_to_mono_object(domain, *p_arguments[i]);
		mono_array_setref(args_array, i, arg);
	}

	void *invoke_params[1] = { &args_array };

	MonoObject *exc = nullptr;
	MonoObject *result = mono_runtime_invoke(invoke_method, delegate, invoke_params, &exc);

	if (exc) {
		char *exc_msg = mono_string_to_utf8(mono_object_to_string(exc, nullptr));
		printf("[Mono] Exception in C# delegate call: %s\n", exc_msg ? exc_msg : "unknown");
		if (exc_msg) mono_free(exc_msg);
		r_call_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return;
	}

	if (result) {
		r_return_value = mono_object_to_variant(result);
	}

	r_call_error.error = Callable::CallError::CALL_OK;
}

Error CallableCustomMono::rpc(int p_peer_id, const Variant **p_arguments, int p_argcount, Callable::CallError &r_call_error) const {
	r_call_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
	return ERR_UNAVAILABLE;
}

void CallableCustomMono::set_delegate(uint32_t p_gchandle, MonoDomain *p_domain) {
	gchandle = p_gchandle;
	domain = p_domain;
}

CallableCustomMono::CallableCustomMono() {}

CallableCustomMono::~CallableCustomMono() {
	if (gchandle != 0) {
		mono_gchandle_free(gchandle);
		gchandle = 0;
	}
}
