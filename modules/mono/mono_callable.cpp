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

	// 2026-07-27 rework: the delegate is now the ORIGINAL managed delegate
	// (Godot.Callable.From passes it through unwrapped). Box each argument
	// with the EXACT type from the delegate's Invoke signature — Variant::INT
	// would otherwise arrive as Int64 and mono_runtime_delegate_invoke would
	// throw a type mismatch for e.g. Action<int> (found by csharp_test 24e).
	if (p_argcount == 0) {
		result = mono_runtime_delegate_invoke((MonoObject *)delegate, nullptr, &exc);
	} else {
		enum { MAX_CALL_ARGS = 16 };
		MonoMethodSignature *sig = (invoke_method && param_count == p_argcount && p_argcount <= MAX_CALL_ARGS)
				? mono_method_signature(invoke_method)
				: nullptr;

		void **params = (void **)alloca(sizeof(void *) * p_argcount);
		MonoObject **obj_args = (MonoObject **)alloca(sizeof(MonoObject *) * p_argcount);
		uint64_t *storage = (uint64_t *)alloca(sizeof(uint64_t) * p_argcount);

		void *sig_iter = nullptr;
		bool types_ok = sig != nullptr;
		for (int i = 0; types_ok && i < p_argcount; i++) {
			MonoType *pt = mono_signature_get_params(sig, &sig_iter);
			const Variant &v = *p_arguments[i];
			switch (pt ? mono_type_get_type(pt) : MONO_TYPE_OBJECT) {
				case MONO_TYPE_BOOLEAN: { bool b = (bool)v; storage[i] = 0; memcpy(&storage[i], &b, sizeof(b)); params[i] = &storage[i]; break; }
				case MONO_TYPE_I1: { int8_t b = (int8_t)(int64_t)v; storage[i] = 0; memcpy(&storage[i], &b, sizeof(b)); params[i] = &storage[i]; break; }
				case MONO_TYPE_U1: { uint8_t b = (uint8_t)(int64_t)v; storage[i] = 0; memcpy(&storage[i], &b, sizeof(b)); params[i] = &storage[i]; break; }
				case MONO_TYPE_I2: { int16_t b = (int16_t)(int64_t)v; storage[i] = 0; memcpy(&storage[i], &b, sizeof(b)); params[i] = &storage[i]; break; }
				case MONO_TYPE_U2: { uint16_t b = (uint16_t)(int64_t)v; storage[i] = 0; memcpy(&storage[i], &b, sizeof(b)); params[i] = &storage[i]; break; }
				case MONO_TYPE_I4: { int32_t b = (int32_t)(int64_t)v; storage[i] = 0; memcpy(&storage[i], &b, sizeof(b)); params[i] = &storage[i]; break; }
				case MONO_TYPE_U4: { uint32_t b = (uint32_t)(int64_t)v; storage[i] = 0; memcpy(&storage[i], &b, sizeof(b)); params[i] = &storage[i]; break; }
				case MONO_TYPE_I8: { int64_t b = (int64_t)v; memcpy(&storage[i], &b, sizeof(b)); params[i] = &storage[i]; break; }
				case MONO_TYPE_U8: { uint64_t b = (uint64_t)(int64_t)v; memcpy(&storage[i], &b, sizeof(b)); params[i] = &storage[i]; break; }
				case MONO_TYPE_R4: { float b = (float)(double)v; storage[i] = 0; memcpy(&storage[i], &b, sizeof(b)); params[i] = &storage[i]; break; }
				case MONO_TYPE_R8: { double b = (double)v; memcpy(&storage[i], &b, sizeof(b)); params[i] = &storage[i]; break; }
				case MONO_TYPE_STRING:
				case MONO_TYPE_CLASS:
				case MONO_TYPE_OBJECT:
				case MONO_TYPE_SZARRAY:
				case MONO_TYPE_ARRAY:
				case MONO_TYPE_VALUETYPE:
					obj_args[i] = variant_to_mono_object(domain, v);
					params[i] = &obj_args[i];
					break;
				default:
					types_ok = false;
					break;
			}
		}

		if (!types_ok) {
			// Fallback: signature unavailable or unsupported param type —
			// box everything as objects (previous behavior).
			for (int i = 0; i < p_argcount; i++) {
				obj_args[i] = variant_to_mono_object(domain, *p_arguments[i]);
				params[i] = &obj_args[i];
			}
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
