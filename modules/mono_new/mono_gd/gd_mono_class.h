#ifndef GD_MONO_CLASS_H
#define GD_MONO_CLASS_H

#include "core/string/ustring.h"
#include "core/variant/variant.h"

#include <mono/mono-publib.h>

class GDMonoClass {
	MonoClass *mono_class = nullptr;
	MonoImage *mono_image = nullptr;
	String namespace_name;
	String class_name;
	bool valid = false;

public:
	_FORCE_INLINE_ MonoClass *get_raw_class() { return mono_class; }
	_FORCE_INLINE_ bool is_valid() const { return valid; }

	MonoMethod *get_method(const StringName &p_name, int p_param_count = 0);
	bool has_method(const StringName &p_name);
	MonoProperty *get_property(const StringName &p_name);
	MonoField *get_field(const StringName &p_name);

	GDMonoClass(const String &p_namespace, const String &p_class, MonoImage *p_image = nullptr);
	~GDMonoClass();
};

#endif // GD_MONO_CLASS_H
