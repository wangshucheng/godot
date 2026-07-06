#include "gd_mono_class.h"

#include "../mono_runtime/gd_mono.h"

MonoMethod *GDMonoClass::get_method(const StringName &p_name, int p_param_count) {
	if (!mono_class)
		return nullptr;
	return mono_class_get_method_from_name(mono_class, String(p_name).utf8().get_data(), p_param_count);
}

bool GDMonoClass::has_method(const StringName &p_name) {
	return get_method(p_name, -1) != nullptr;
}

MonoProperty *GDMonoClass::get_property(const StringName &p_name) {
	if (!mono_class)
		return nullptr;
	return mono_class_get_property_from_name(mono_class, String(p_name).utf8().get_data());
}

MonoField *GDMonoClass::get_field(const StringName &p_name) {
	if (!mono_class)
		return nullptr;
	return mono_class_get_field_from_name(mono_class, String(p_name).utf8().get_data());
}

GDMonoClass::GDMonoClass(const String &p_namespace, const String &p_class, MonoImage *p_image) {
	namespace_name = p_namespace;
	class_name = p_class;

	if (!p_image && GDMono::get_singleton()) {
		p_image = mono_get_corlib();
	}

	mono_image = p_image;
	if (!mono_image)
		return;

	mono_class = mono_class_from_name(mono_image,
			namespace_name.utf8().get_data(),
			class_name.utf8().get_data());

	if (mono_class) {
		valid = true;
	}
}

GDMonoClass::GDMonoClass(MonoClass *p_raw_class) {
	mono_class = p_raw_class;
	if (mono_class) {
		mono_image = mono_class_get_image(mono_class);
		namespace_name = mono_class_get_namespace(mono_class);
		class_name = mono_class_get_name(mono_class);
		valid = true;
	}
}

GDMonoClass::~GDMonoClass() {
}
