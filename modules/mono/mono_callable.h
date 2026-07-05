#pragma once

#include <mono/metadata/object.h>
#include <mono/metadata/appdomain.h>
#include "core/variant/callable.h"
#include "core/object/object_id.h"

class CallableCustomMono : public CallableCustom {
	uint32_t gchandle = 0;
	MonoDomain *domain = nullptr;
	MonoMethod *invoke_method = nullptr;
	int param_count = 0;

	static bool compare_equal(const CallableCustom *p_a, const CallableCustom *p_b);
	static bool compare_less(const CallableCustom *p_a, const CallableCustom *p_b);

public:
	uint32_t hash() const override;
	String get_as_text() const override;
	CompareEqualFunc get_compare_equal_func() const override;
	CompareLessFunc get_compare_less_func() const override;
	StringName get_method() const override;
	ObjectID get_object() const override;
	int get_argument_count(bool &r_is_valid) const override;
	bool is_valid() const override;
	void call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_call_error) const override;
	Error rpc(int p_peer_id, const Variant **p_arguments, int p_argcount, Callable::CallError &r_call_error) const override;

	void set_delegate(uint32_t p_gchandle, MonoDomain *p_domain);

	CallableCustomMono();
	~CallableCustomMono();
};
