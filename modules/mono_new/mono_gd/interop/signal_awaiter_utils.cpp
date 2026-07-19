#include "signal_awaiter_utils.h"
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
#include <cstdint>
#include <cstring>

// Forward-declare Mono API functions from headers that have missing transitive
// dependencies in our minimal Mono header set (mono/metadata/object.h,
// appdomain.h, class.h, etc.). These are linked statically from
// libmonosgen-2.0.a.
extern "C" {
MonoObject *mono_gchandle_get_target(uint32_t handle);
void mono_gchandle_free(uint32_t handle);
MonoClass *mono_object_get_class(MonoObject *obj);
MonoImage *mono_get_corlib(void);
MonoClass *mono_class_from_name(MonoImage *image, const char *name_space, const char *name);
MonoMethod *mono_class_get_method_from_name(MonoClass *klass, const char *name, int param_count);
// mono_array_addr_with_size is already declared in <mono/mono-publib.h> (line 239).
// mono_array_setref is declared as a function in mono-publib.h but is NOT exported
// from our static libmonosgen-2.0.a (linker error LNK2019). The object.h version
// is a macro that uses mono_gc_wbarrier_set_arrayref instead, so we forward-declare
// that and inline the macro expansion below.
void mono_gc_wbarrier_set_arrayref(MonoArray *arr, void *slot_ptr, MonoObject *value);
MonoObject *mono_runtime_invoke(MonoMethod *method, void *obj, void **params, MonoObject **exc);
}

// ---------------------------------------------------------------------------
// SignalAwaiterCallable
// ---------------------------------------------------------------------------

bool SignalAwaiterCallable::compare_equal(const CallableCustom *p_a, const CallableCustom *p_b) {
	const SignalAwaiterCallable *a = static_cast<const SignalAwaiterCallable *>(p_a);
	const SignalAwaiterCallable *b = static_cast<const SignalAwaiterCallable *>(p_b);
	return a->source_id == b->source_id && a->signal == b->signal && a->awaiter_gchandle == b->awaiter_gchandle;
}

bool SignalAwaiterCallable::compare_less(const CallableCustom *p_a, const CallableCustom *p_b) {
	const SignalAwaiterCallable *a = static_cast<const SignalAwaiterCallable *>(p_a);
	const SignalAwaiterCallable *b = static_cast<const SignalAwaiterCallable *>(p_b);
	if (a->source_id != b->source_id) return a->source_id < b->source_id;
	if (a->signal != b->signal) return String(a->signal) < String(b->signal);
	return a->awaiter_gchandle < b->awaiter_gchandle;
}

uint32_t SignalAwaiterCallable::hash() const {
	return hash_one_uint64((uint64_t)source_id) ^ hash_one_uint64((uint64_t)awaiter_gchandle);
}

String SignalAwaiterCallable::get_as_text() const {
	return "SignalAwaiterCallable";
}

CallableCustom::CompareEqualFunc SignalAwaiterCallable::get_compare_equal_func() const {
	return compare_equal;
}

CallableCustom::CompareLessFunc SignalAwaiterCallable::get_compare_less_func() const {
	return compare_less;
}

ObjectID SignalAwaiterCallable::get_object() const {
	return source_id;
}

int SignalAwaiterCallable::get_argument_count(bool &r_is_valid) const {
	// Signal argument count is variable; report as invalid so the engine does not enforce it.
	r_is_valid = false;
	return 0;
}

void SignalAwaiterCallable::call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_call_error) const {
	r_return_value = Variant();
	r_call_error.error = Callable::CallError::CALL_OK;

	GDMono *gdmono = GDMono::get_singleton();
	if (!gdmono) {
		r_call_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return;
	}

	MonoDomain *domain = gdmono->get_scripts_domain();
	if (!domain) {
		r_call_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return;
	}

	// Retrieve the C# SignalAwaiter via the stored GC handle.
	MonoObject *awaiter_obj = mono_gchandle_get_target(awaiter_gchandle);
	if (!awaiter_obj) {
		// Awaiter was already collected; nothing to do.
		return;
	}

	MonoClass *cls = mono_object_get_class(awaiter_obj);
	if (!cls) {
		return;
	}

	// Find the SignalCallback(object[]) method on the awaiter.
	MonoMethod *method = mono_class_get_method_from_name(cls, "SignalCallback", 1);
	if (!method) {
		MonoLogger::log_warning("SignalAwaiterCallable::call: SignalCallback method not found");
		return;
	}

	// Build the object[] argument array from the signal's Variant arguments.
	MonoImage *corlib = mono_get_corlib();
	MonoClass *object_class = corlib ? mono_class_from_name(corlib, "System", "Object") : nullptr;
	if (!object_class) {
		MonoLogger::log_warning("SignalAwaiterCallable::call: System.Object class not found");
		return;
	}
	MonoArray *args_array = mono_array_new(domain, object_class, p_argcount);
	for (int i = 0; i < p_argcount; i++) {
		MonoObject *elem = GDMonoInterop::variant_to_mono_object(domain, *p_arguments[i]);
		// Inline expansion of mono_array_setref() macro from <mono/metadata/object.h>:
		//   void **__p = (void **) mono_array_addr ((array), void*, (index));
		//   mono_gc_wbarrier_set_arrayref ((array), __p, (MonoObject*)(value));
		// where mono_array_addr is: ((type*)mono_array_addr_with_size(array, sizeof(type), index))
		void **slot = (void **)mono_array_addr_with_size(args_array, sizeof(void *), i);
		if (slot) {
			mono_gc_wbarrier_set_arrayref(args_array, slot, elem);
		}
	}

	void *params[1] = { args_array };
	MonoObject *exc = nullptr;
	mono_runtime_invoke(method, awaiter_obj, params, &exc);
	if (exc) {
		MonoLogger::log_warning("SignalAwaiterCallable::call: exception in SignalCallback");
	}
}

SignalAwaiterCallable::SignalAwaiterCallable(Object *p_source, uint32_t p_awaiter_gchandle, const StringName &p_signal)
	: source_id(p_source ? p_source->get_instance_id() : ObjectID()),
	  awaiter_gchandle(p_awaiter_gchandle),
	  signal(p_signal) {
}

SignalAwaiterCallable::~SignalAwaiterCallable() {
	// S3 修复: 不再在 C++ 析构时释放 gchandle。
	// C# SignalAwaiter 终结器 (~SignalAwaiter) 会释放 _selfHandle，
	// C++ 再释放会导致双重释放（同一个 handle 被 free 两次，UB）。
	// gchandle 的所有权归 C# SignalAwaiter 对象所有。
	// 这里仅清零标记，不调用 mono_gchandle_free。
	awaiter_gchandle = 0;
}

// ---------------------------------------------------------------------------
// GDSignalAwaiter namespace
// ---------------------------------------------------------------------------

bool GDSignalAwaiter::connect_signal_awaiter(Object *p_source, const StringName &p_signal, uint32_t p_awaiter_gchandle) {
	if (!p_source || p_awaiter_gchandle == 0) return false;
	if (!p_source->has_signal(p_signal)) {
		MonoLogger::log_warning(vformat("connect_signal_awaiter: object has no signal '%s'", String(p_signal)));
		return false;
	}

	SignalAwaiterCallable *custom = memnew(SignalAwaiterCallable(p_source, p_awaiter_gchandle, p_signal));
	Callable target(custom);
	Error err = p_source->connect(p_signal, target, Object::CONNECT_ONE_SHOT);
	if (err != OK) {
		MonoLogger::log_warning(vformat("connect_signal_awaiter: connect failed for '%s' (err=%d)", String(p_signal), err));
		// custom is freed by Callable when construction fails; memdelete defensively.
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// ICall registration
// ---------------------------------------------------------------------------

// C# icall: godot_icall_SignalAwaiter_Connect(long sourcePtr, string signal, long targetPtr, long awaiterHandle)
// awaiterHandle is the GCHandle.ToIntPtr().ToInt64() value — the raw 32-bit GC handle in low bits.
static void icall_SignalAwaiter_Connect(int64_t p_source_ptr, MonoString *p_signal, int64_t p_target_ptr, int64_t p_awaiter_handle) {
	uint32_t gchandle = (uint32_t)(p_awaiter_handle & 0xFFFFFFFF);
	if (!p_source_ptr || !p_signal || gchandle == 0) return;

	Object *source = (Object *)(intptr_t)p_source_ptr;
	char *utf8 = mono_string_to_utf8(p_signal);
	StringName signal_name(String::utf8(utf8));
	mono_free(utf8);

	GDSignalAwaiter::connect_signal_awaiter(source, signal_name, gchandle);
}

// C# icall: godot_icall_SignalAwaiter_Disconnect(long sourcePtr, string signal, long awaiterHandle)
// Not strictly needed since we use CONNECT_ONE_SHOT, but provided for symmetry.
static void icall_SignalAwaiter_Disconnect(int64_t p_source_ptr, MonoString *p_signal, int64_t p_awaiter_handle) {
	// One-shot connections auto-disconnect; this is a no-op for now.
	(void)p_source_ptr;
	(void)p_signal;
	(void)p_awaiter_handle;
}

void GDSignalAwaiter::register_icalls() {
	mono_add_internal_call("Godot.SignalAwaiter::godot_icall_SignalAwaiter_Connect",
		(const void *)icall_SignalAwaiter_Connect);
	mono_add_internal_call("Godot.SignalAwaiter::godot_icall_SignalAwaiter_Disconnect",
		(const void *)icall_SignalAwaiter_Disconnect);
}
