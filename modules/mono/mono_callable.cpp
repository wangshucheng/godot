#include "mono_callable.h"
#include "mono_variant.h"
#include "mono_bridge.h"
#include "core/object/object.h"
#include <mono/metadata/appdomain.h>
#include <mono/metadata/exception.h>
#include <mono/metadata/object.h>
#include <mono/metadata/metadata.h>
#include <mono/metadata/loader.h>
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
	r_is_valid = true;
	return param_count;
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

	MonoObject *exc = nullptr;
	MonoObject *result = nullptr;

	// mono_runtime_delegate_invoke expects void** params where each element
	// is a pointer to the argument value (pointer to MonoObject* for ref types)
	if (p_argcount == 0) {
		result = mono_runtime_delegate_invoke((MonoObject *)delegate, nullptr, &exc);
	} else {
		// Use alloca for stack allocation of params array
		void **params = (void **)alloca(sizeof(void *) * p_argcount);
		MonoObject **args = (MonoObject **)alloca(sizeof(MonoObject *) * p_argcount);
		for (int i = 0; i < p_argcount; i++) {
			args[i] = variant_to_mono_object(domain, *p_arguments[i]);
			params[i] = &args[i];
		}
		result = mono_runtime_delegate_invoke((MonoObject *)delegate, params, &exc);
	}

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
	param_count = 0;
	invoke_method = nullptr;

	MonoObject *delegate = mono_gchandle_get_target(gchandle);
	if (!delegate) return;

	MonoClass *del_class = mono_object_get_class(delegate);
	invoke_method = mono_get_delegate_invoke(del_class);
	if (invoke_method) {
		MonoMethodSignature *sig = mono_method_signature(invoke_method);
		param_count = mono_signature_get_param_count(sig);
	}
}

CallableCustomMono::CallableCustomMono() {}

CallableCustomMono::~CallableCustomMono() {
	if (gchandle != 0) {
		mono_gchandle_free(gchandle);
		gchandle = 0;
	}
}
