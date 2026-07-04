#pragma once

#include "core/variant/variant.h"
#include "core/object/object.h"
#include <mono/metadata/object.h>
#include <mono/metadata/class.h>

namespace mono_variant {

MonoObject *variant_to_mono_object(MonoDomain *p_domain, const Variant &p_variant);
Variant mono_object_to_variant(MonoObject *p_obj);

MonoString *variant_to_mono_string(MonoDomain *p_domain, const Variant &p_variant);
MonoObject *variant_to_mono_bool(MonoDomain *p_domain, bool p_val);
MonoObject *variant_to_mono_int(MonoDomain *p_domain, int64_t p_val);
MonoObject *variant_to_mono_float(MonoDomain *p_domain, double p_val);
MonoObject *variant_to_mono_intptr(MonoDomain *p_domain, intptr_t p_val);

bool mono_object_to_bool(MonoObject *p_obj, bool *r_ok = nullptr);
int64_t mono_object_to_int(MonoObject *p_obj, bool *r_ok = nullptr);
double mono_object_to_float(MonoObject *p_obj, bool *r_ok = nullptr);
intptr_t mono_object_to_intptr(MonoObject *p_obj, bool *r_ok = nullptr);
String mono_object_to_native_string(MonoObject *p_obj);
Object *mono_object_to_godot_object(MonoObject *p_obj);

void cache_mono_corlib_classes();

MonoClass *get_intptr_class();

}
