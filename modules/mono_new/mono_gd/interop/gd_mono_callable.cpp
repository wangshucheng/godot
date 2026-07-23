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
#include "core/os/thread.h"
#include "core/os/mutex.h"

#include <mono/mono-publib.h>
#include <cstdint>
#include <cstring>

bool MonoCallableCustom::compare_equal(const CallableCustom *p_a, const CallableCustom *p_b) {
	const MonoCallableCustom *a = static_cast<const MonoCallableCustom *>(p_a);
	const MonoCallableCustom *b = static_cast<const MonoCallableCustom *>(p_b);
	if (a->object_id != b->object_id) {
		return false;
	}
	// S5 修复: 比较 gchandle 解析后的委托对象而非 gchandle 值本身。
	// 同一委托重新包装（新 gchandle）也能匹配，引擎 disconnect 因此可靠
	// （借鉴参考项目 mono_callable.cpp 的做法）。
	MonoObject *del_a = a->gchandle ? mono_gchandle_get_target(a->gchandle) : nullptr;
	MonoObject *del_b = b->gchandle ? mono_gchandle_get_target(b->gchandle) : nullptr;
	return del_a != nullptr && del_a == del_b;
}

bool MonoCallableCustom::compare_less(const CallableCustom *p_a, const CallableCustom *p_b) {
	const MonoCallableCustom *a = static_cast<const MonoCallableCustom *>(p_a);
	const MonoCallableCustom *b = static_cast<const MonoCallableCustom *>(p_b);
	if (a->object_id != b->object_id) {
		return a->object_id < b->object_id;
	}
	// 与 compare_equal 保持一致：按解析后的委托对象排序。
	MonoObject *del_a = a->gchandle ? mono_gchandle_get_target(a->gchandle) : nullptr;
	MonoObject *del_b = b->gchandle ? mono_gchandle_get_target(b->gchandle) : nullptr;
	return del_a < del_b;
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
	// S2 修复: 用 gchandle 判断而非裸指针
	r_is_valid = (gchandle != 0) && (mono_gchandle_get_target(gchandle) != nullptr);
	return 0;
}

void MonoCallableCustom::call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_call_error) const {
	// S2 修复: 通过 gchandle 现取 delegate，避免 GC 移动后悬垂
	MonoObject *delegate = (gchandle != 0) ? mono_gchandle_get_target(gchandle) : nullptr;
	if (!delegate) {
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

	MonoClass *delegate_class = mono_object_get_class(delegate);
	MonoMethod *invoke_method = mono_class_get_method_from_name(delegate_class, "Invoke", p_argcount);
	if (!invoke_method) {
		r_call_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return;
	}

	// Build params array for mono_runtime_invoke.
	// For value types (double, int, bool), allocate directly on the C++ heap
	// and pass the pointer. This avoids SGen GC boxing/pinning issues in WASM
	// where pinned handles may not be respected by the interpreter.
	// Same approach as CSharpInstance::callp.
	void **params = nullptr;
	void **value_storage = nullptr;
	if (p_argcount > 0) {
		params = (void **)memalloc(sizeof(void *) * p_argcount);
		value_storage = (void **)memalloc(sizeof(void *) * p_argcount);
		for (int i = 0; i < p_argcount; i++) {
			value_storage[i] = nullptr;
			params[i] = nullptr;
		}
		for (int i = 0; i < p_argcount; i++) {
			Variant::Type vt = p_arguments[i]->get_type();
			if (vt == Variant::FLOAT) {
				value_storage[i] = memalloc(sizeof(double));
				*(double *)value_storage[i] = (double)*p_arguments[i];
				params[i] = value_storage[i];
			} else if (vt == Variant::INT) {
				value_storage[i] = memalloc(sizeof(int64_t));
				*(int64_t *)value_storage[i] = (int64_t)*p_arguments[i];
				params[i] = value_storage[i];
			} else if (vt == Variant::BOOL) {
				value_storage[i] = memalloc(sizeof(uint8_t));
				*(uint8_t *)value_storage[i] = (bool)*p_arguments[i] ? 1 : 0;
				params[i] = value_storage[i];
			} else {
				MonoObject *arg_obj = GDMonoInterop::variant_to_mono_object(domain, *p_arguments[i]);
				if (arg_obj) {
					MonoClass *cls = mono_object_get_class(arg_obj);
					if (cls && mono_class_is_valuetype(cls)) {
						// For value type structs (Vector2, Color, etc.), pass
						// pointer to unboxed data. Safe in WASM interpreter
						// mode (no moving GC).
						params[i] = mono_object_unbox(arg_obj);
					} else {
						params[i] = arg_obj;
					}
				}
			}
		}
	}

	MonoObject *exc = nullptr;
	// S2 修复: 使用通过 gchandle 重新获取的 delegate（确保地址有效）
	MonoObject *ret = mono_runtime_invoke(invoke_method, delegate, params, &exc);

	// Free heap-allocated value type memory
	if (value_storage) {
		for (int i = 0; i < p_argcount; i++) {
			if (value_storage[i]) {
				memfree(value_storage[i]);
			}
		}
		memfree(value_storage);
	}
	if (params) {
		memfree(params);
	}

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
}

bool MonoCallableCustom::is_valid() const {
	// S2 修复: 通过 gchandle 验证对象仍存在
	return (gchandle != 0) && (mono_gchandle_get_target(gchandle) != nullptr) && ObjectDB::get_instance(object_id) != nullptr;
}

MonoCallableCustom::MonoCallableCustom(Object *p_object, MonoObject *p_delegate, const StringName &p_method) {
	object_id = p_object ? p_object->get_instance_id() : ObjectID();
	method_name = p_method;
	if (p_delegate) {
		// S2 修复: 使用 pinned gchandle 钉住 delegate，防止 SGen GC 移动后悬垂
		// 注意: 精简版 mono-publib.h 只声明了 mono_gchandle_new(obj, pinned)，无独立 _pinned 后缀 API
		gchandle = mono_gchandle_new(p_delegate, /*pinned=*/1);
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
	MonoImage *gsharp_img = gdmono->get_godotsharp_image();
	if (!gsharp_img) return nullptr;
	MonoClass *cls = mono_class_from_name(gsharp_img, "Godot", "Callable");
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

static void icall_Callable_Call(int64_t p_callable_ptr, MonoArray *p_args, MonoObject **r_ret) {
	if (!p_callable_ptr) return;
	Callable *callable = (Callable *)(intptr_t)p_callable_ptr;
	GDMono *gdmono = GDMono::get_singleton();
	if (!gdmono) return;
	MonoDomain *domain = gdmono->get_scripts_domain();

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

static mono_bool icall_Object_ConnectSignal(int64_t p_native_ptr, MonoString *p_signal, MonoObject *p_callable, mono_bool p_oneshot) {
	if (!p_native_ptr || !p_signal || !p_callable) return false;
	Object *obj = (Object *)(intptr_t)p_native_ptr;
	char *_sig_utf8 = mono_string_to_utf8(p_signal);
	StringName signal_name(String::utf8(_sig_utf8));
	mono_free(_sig_utf8);
	if (!obj->has_signal(signal_name)) return false;

	Callable target = GDMonoCallable::create_callable_from_mono_delegate(p_callable);
	if (p_oneshot) {
		return obj->connect(signal_name, target, Object::CONNECT_ONE_SHOT) == OK;
	} else {
		return obj->connect(signal_name, target) == OK;
	}
}

static mono_bool icall_Object_DisconnectSignal(int64_t p_native_ptr, MonoString *p_signal, MonoObject *p_callable) {
	if (!p_native_ptr || !p_signal || !p_callable) return false;
	Object *obj = (Object *)(intptr_t)p_native_ptr;
	char *_sig_utf8 = mono_string_to_utf8(p_signal);
	StringName signal_name(String::utf8(_sig_utf8));
	mono_free(_sig_utf8);
	if (!obj->has_signal(signal_name)) return false;

	Callable target = GDMonoCallable::create_callable_from_mono_delegate(p_callable);
	obj->disconnect(signal_name, target);
	return true;
}

// Connect a signal using a pre-allocated native Callable pointer (from Callable(GodotObject, string)).
// This avoids delegate marshalling — the callable is already a Godot Callable object.
static mono_bool icall_Object_ConnectSignalNative(int64_t p_native_ptr, MonoString *p_signal, int64_t p_callable_ptr, mono_bool p_oneshot) {
	if (!p_native_ptr || !p_signal || !p_callable_ptr) return false;
	Object *obj = (Object *)(intptr_t)p_native_ptr;
	char *_sig_utf8 = mono_string_to_utf8(p_signal);
	StringName signal_name(String::utf8(_sig_utf8));
	mono_free(_sig_utf8);
	if (!obj->has_signal(signal_name)) return false;

	Callable *callable = (Callable *)(intptr_t)p_callable_ptr;
	if (p_oneshot) {
		return obj->connect(signal_name, *callable, Object::CONNECT_ONE_SHOT) == OK;
	} else {
		return obj->connect(signal_name, *callable) == OK;
	}
}

static mono_bool icall_Object_DisconnectSignalNative(int64_t p_native_ptr, MonoString *p_signal, int64_t p_callable_ptr) {
	if (!p_native_ptr || !p_signal || !p_callable_ptr) return false;
	Object *obj = (Object *)(intptr_t)p_native_ptr;
	char *_sig_utf8 = mono_string_to_utf8(p_signal);
	StringName signal_name(String::utf8(_sig_utf8));
	mono_free(_sig_utf8);
	if (!obj->has_signal(signal_name)) return false;

	Callable *callable = (Callable *)(intptr_t)p_callable_ptr;
	obj->disconnect(signal_name, *callable);
	return true;
}

// Signal class icalls — operate on owner + signal name + native callable pointer.
static mono_bool icall_Signal_Connect(int64_t p_owner_ptr, MonoString *p_signal, int64_t p_callable_ptr, mono_bool p_oneshot) {
	return icall_Object_ConnectSignalNative(p_owner_ptr, p_signal, p_callable_ptr, p_oneshot);
}

static mono_bool icall_Signal_Disconnect(int64_t p_owner_ptr, MonoString *p_signal, int64_t p_callable_ptr) {
	return icall_Object_DisconnectSignalNative(p_owner_ptr, p_signal, p_callable_ptr);
}

static mono_bool icall_Signal_IsConnected(int64_t p_owner_ptr, MonoString *p_signal, int64_t p_callable_ptr) {
	if (!p_owner_ptr || !p_signal || !p_callable_ptr) return false;
	Object *obj = (Object *)(intptr_t)p_owner_ptr;
	char *_sig_utf8 = mono_string_to_utf8(p_signal);
	StringName signal_name(String::utf8(_sig_utf8));
	mono_free(_sig_utf8);
	if (!obj->has_signal(signal_name)) return false;

	Callable *callable = (Callable *)(intptr_t)p_callable_ptr;
	return obj->is_connected(signal_name, *callable);
}

static void icall_Signal_Emit(int64_t p_owner_ptr, MonoString *p_signal, MonoArray *p_args) {
	if (!p_owner_ptr || !p_signal) return;
	Object *obj = (Object *)(intptr_t)p_owner_ptr;
	char *_sig_utf8 = mono_string_to_utf8(p_signal);
	StringName signal_name(String::utf8(_sig_utf8));
	mono_free(_sig_utf8);
	if (!obj->has_signal(signal_name)) return;

	int argcount = p_args ? mono_array_length(p_args) : 0;
	Vector<Variant> args;
	args.resize(argcount);
	Vector<const Variant *> argptrs;
	argptrs.resize(argcount);
	for (int i = 0; i < argcount; i++) {
		MonoObject *arg = mono_array_get(p_args, MonoObject *, i);
		args.write[i] = arg ? GDMonoInterop::mono_object_to_variant(arg) : Variant();
		argptrs.write[i] = &args.write[i];
	}
	obj->emit_signalp(signal_name, (const Variant **)argptrs.ptr(), argcount);
}

// S5 修复(b): C# ~Callable() 终结器跑在 GC 终结器线程，引擎 API 非主线程不安全。
// 借鉴参考项目 H8 方案：非主线程仅入队，由主线程在下一次 callable/signal 相关 icall 时排空。
static Mutex g_native_free_mutex;
static Vector<int64_t> g_deferred_callable_free_queue;
static Vector<int64_t> g_deferred_signal_free_queue;

static void flush_deferred_native_free() {
	Vector<int64_t> pending_callables;
	Vector<int64_t> pending_signals;
	{
		MutexLock lock(g_native_free_mutex);
		pending_callables = g_deferred_callable_free_queue;
		g_deferred_callable_free_queue.clear();
		pending_signals = g_deferred_signal_free_queue;
		g_deferred_signal_free_queue.clear();
	}
	for (int i = 0; i < pending_callables.size(); i++) {
		Callable *callable = (Callable *)(intptr_t)pending_callables[i];
		memdelete(callable);
	}
	for (int i = 0; i < pending_signals.size(); i++) {
		::Signal *signal = (::Signal *)(intptr_t)pending_signals[i];
		memdelete(signal);
	}
}

// Wrap a Delegate into a native Callable and return its pointer (int64).
// Reuses the existing create_callable_from_mono_delegate machinery.
static int64_t icall_Callable_CreateFromDelegatePtr(MonoObject *p_delegate) {
	if (!p_delegate) return 0;
	if (Thread::is_main_thread()) {
		flush_deferred_native_free();
	}
	Callable callable = GDMonoCallable::create_callable_from_mono_delegate(p_delegate);
	if (!callable.is_valid()) return 0;
	Callable *heap_callable = memnew(Callable(callable));
	return (int64_t)(intptr_t)heap_callable;
}

// Free a native Callable pointer allocated by icall_Callable_CreateFromDelegate or icall_Callable_CreateFromTarget.
static void icall_Callable_Free(int64_t p_callable_ptr) {
	if (!p_callable_ptr) return;
	if (Thread::is_main_thread()) {
		Callable *callable = (Callable *)(intptr_t)p_callable_ptr;
		memdelete(callable);
		flush_deferred_native_free();
	} else {
		MutexLock lock(g_native_free_mutex);
		g_deferred_callable_free_queue.push_back(p_callable_ptr);
	}
}

// Free a native Signal pointer allocated by variant_to_mono_object (SIGNAL case).
static void icall_Signal_Free(int64_t p_signal_ptr) {
	if (!p_signal_ptr) return;
	if (Thread::is_main_thread()) {
		::Signal *signal = (::Signal *)(intptr_t)p_signal_ptr;
		memdelete(signal);
		flush_deferred_native_free();
	} else {
		MutexLock lock(g_native_free_mutex);
		g_deferred_signal_free_queue.push_back(p_signal_ptr);
	}
}

void GDMonoCallable::register_icalls() {
	mono_add_internal_call("Godot.Callable::godot_icall_Callable_CreateFromDelegate", (const void *)icall_Callable_CreateFromDelegate);
	mono_add_internal_call("Godot.Callable::godot_icall_Callable_Call", (const void *)icall_Callable_Call);
	mono_add_internal_call("Godot.Callable::godot_icall_Callable_Free", (const void *)icall_Callable_Free);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_ConnectSignal", (const void *)icall_Object_ConnectSignal);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_DisconnectSignal", (const void *)icall_Object_DisconnectSignal);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_ConnectSignalNative", (const void *)icall_Object_ConnectSignalNative);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_DisconnectSignalNative", (const void *)icall_Object_DisconnectSignalNative);
	mono_add_internal_call("Godot.Signal::godot_icall_Signal_Connect", (const void *)icall_Signal_Connect);
	mono_add_internal_call("Godot.Signal::godot_icall_Signal_Disconnect", (const void *)icall_Signal_Disconnect);
	mono_add_internal_call("Godot.Signal::godot_icall_Signal_IsConnected", (const void *)icall_Signal_IsConnected);
	mono_add_internal_call("Godot.Signal::godot_icall_Signal_Emit", (const void *)icall_Signal_Emit);
	mono_add_internal_call("Godot.Signal::godot_icall_Signal_Free", (const void *)icall_Signal_Free);
	mono_add_internal_call("Godot.Callable::godot_icall_Callable_CreateFromDelegatePtr", (const void *)icall_Callable_CreateFromDelegatePtr);
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
