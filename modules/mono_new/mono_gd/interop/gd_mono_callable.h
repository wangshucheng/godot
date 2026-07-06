#ifndef GD_MONO_CALLABLE_H
#define GD_MONO_CALLABLE_H

#include "core/variant/callable.h"
#include "core/object/object.h"
#include "core/object/object_id.h"

#include <mono/mono-publib.h>

class MonoCallableCustom : public CallableCustom {
	ObjectID object_id;
	MonoObject *delegate_handle;
	uint32_t gchandle;
	StringName method_name;

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

	void release_delegate();
	bool is_valid() const;

	MonoCallableCustom(Object *p_object, MonoObject *p_delegate, const StringName &p_method = StringName());
	virtual ~MonoCallableCustom();
};

namespace GDMonoCallable {
	void register_icalls();
	Callable create_callable_from_mono_delegate(MonoObject *p_delegate);
	MonoObject *create_mono_delegate_from_callable(MonoDomain *p_domain, const Callable &p_callable);
}

#endif // GD_MONO_CALLABLE_H
