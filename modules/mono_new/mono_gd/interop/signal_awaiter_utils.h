#ifndef SIGNAL_AWAITER_UTILS_H
#define SIGNAL_AWAITER_UTILS_H

#include "core/object/object.h"
#include "core/string/string_name.h"
#include "core/variant/callable.h"
#include "core/variant/variant.h"

#include <mono/mono-publib.h>

// SignalAwaiterCallable: a CallableCustom that, when invoked (by the engine
// emitting the connected signal), forwards the arguments to the C# SignalAwaiter
// object's SignalCallback(object[]) method via mono_runtime_invoke.
class SignalAwaiterCallable : public CallableCustom {
	ObjectID source_id;
	uint32_t awaiter_gchandle; // Mono GC handle (strong) pinning the C# SignalAwaiter
	StringName signal;

	static bool compare_equal(const CallableCustom *p_a, const CallableCustom *p_b);
	static bool compare_less(const CallableCustom *p_a, const CallableCustom *p_b);

public:
	uint32_t hash() const override;
	String get_as_text() const override;
	CompareEqualFunc get_compare_equal_func() const override;
	CompareLessFunc get_compare_less_func() const override;
	ObjectID get_object() const override;
	int get_argument_count(bool &r_is_valid) const override;
	void call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_call_error) const override;

	SignalAwaiterCallable(Object *p_source, uint32_t p_awaiter_gchandle, const StringName &p_signal);
	virtual ~SignalAwaiterCallable();
};

namespace GDSignalAwaiter {
// Register icalls for SignalAwaiter.
void register_icalls();
// Connect a one-shot signal awaiter: connects `p_signal` on `p_source` to a
// SignalAwaiterCallable that forwards to the C# awaiter pinned by `p_awaiter_gchandle`.
// Returns true on success.
bool connect_signal_awaiter(Object *p_source, const StringName &p_signal, uint32_t p_awaiter_gchandle);
} // namespace GDSignalAwaiter

#endif // SIGNAL_AWAITER_UTILS_H
