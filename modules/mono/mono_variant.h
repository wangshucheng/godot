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

MonoObject *variant_to_mono_vector2(MonoDomain *p_domain, real_t p_x, real_t p_y);
MonoObject *variant_to_mono_vector3(MonoDomain *p_domain, real_t p_x, real_t p_y, real_t p_z);
MonoObject *variant_to_mono_color(MonoDomain *p_domain, float p_r, float p_g, float p_b, float p_a);
MonoObject *variant_to_mono_rect2(MonoDomain *p_domain, real_t p_x, real_t p_y, real_t p_w, real_t p_h);

bool mono_object_to_bool(MonoObject *p_obj, bool *r_ok = nullptr);
int64_t mono_object_to_int(MonoObject *p_obj, bool *r_ok = nullptr);
double mono_object_to_float(MonoObject *p_obj, bool *r_ok = nullptr);
intptr_t mono_object_to_intptr(MonoObject *p_obj, bool *r_ok = nullptr);
String mono_object_to_native_string(MonoObject *p_obj);
Object *mono_object_to_godot_object(MonoObject *p_obj);

bool mono_object_to_vector2(MonoObject *p_obj, real_t *r_x, real_t *r_y);
bool mono_object_to_vector3(MonoObject *p_obj, real_t *r_x, real_t *r_y, real_t *r_z);
bool mono_object_to_color(MonoObject *p_obj, float *r_r, float *r_g, float *r_b, float *r_a);
bool mono_object_to_rect2(MonoObject *p_obj, real_t *r_x, real_t *r_y, real_t *r_w, real_t *r_h);

void cache_mono_corlib_classes();
void cache_godot_math_classes(MonoImage *p_godot_image);

MonoClass *get_intptr_class();
MonoClass *get_vector2_class();
MonoClass *get_vector3_class();
MonoClass *get_color_class();
MonoClass *get_rect2_class();

}
