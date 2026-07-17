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

// Extended math types (Godot 4.x)
MonoObject *variant_to_mono_vector4(MonoDomain *p_domain, real_t p_x, real_t p_y, real_t p_z, real_t p_w);
MonoObject *variant_to_mono_vector2i(MonoDomain *p_domain, int32_t p_x, int32_t p_y);
MonoObject *variant_to_mono_vector3i(MonoDomain *p_domain, int32_t p_x, int32_t p_y, int32_t p_z);
MonoObject *variant_to_mono_vector4i(MonoDomain *p_domain, int32_t p_x, int32_t p_y, int32_t p_z, int32_t p_w);
MonoObject *variant_to_mono_rect2i(MonoDomain *p_domain, int32_t p_x, int32_t p_y, int32_t p_w, int32_t p_h);
MonoObject *variant_to_mono_quaternion(MonoDomain *p_domain, real_t p_x, real_t p_y, real_t p_z, real_t p_w);
MonoObject *variant_to_mono_plane(MonoDomain *p_domain, real_t p_nx, real_t p_ny, real_t p_nz, real_t p_d);
MonoObject *variant_to_mono_aabb(MonoDomain *p_domain, real_t p_px, real_t p_py, real_t p_pz, real_t p_sx, real_t p_sy, real_t p_sz);
MonoObject *variant_to_mono_basis(MonoDomain *p_domain, const real_t *p_elements); // 9 elements
MonoObject *variant_to_mono_transform2d(MonoDomain *p_domain, const real_t *p_elements); // 6 elements
MonoObject *variant_to_mono_transform3d(MonoDomain *p_domain, const real_t *p_basis, real_t p_ox, real_t p_oy, real_t p_oz);

bool mono_object_to_bool(MonoObject *p_obj, bool *r_ok = nullptr);
int64_t mono_object_to_int(MonoObject *p_obj, bool *r_ok = nullptr);
double mono_object_to_float(MonoObject *p_obj, bool *r_ok = nullptr);
intptr_t mono_object_to_intptr(MonoObject *p_obj, bool *r_ok = nullptr);
String mono_object_to_native_string(MonoObject *p_obj);

bool mono_object_to_vector2(MonoObject *p_obj, real_t *r_x, real_t *r_y);
bool mono_object_to_vector3(MonoObject *p_obj, real_t *r_x, real_t *r_y, real_t *r_z);
bool mono_object_to_color(MonoObject *p_obj, float *r_r, float *r_g, float *r_b, float *r_a);
bool mono_object_to_rect2(MonoObject *p_obj, real_t *r_x, real_t *r_y, real_t *r_w, real_t *r_h);

// Extended math type readers
bool mono_object_to_vector4(MonoObject *p_obj, real_t *r_x, real_t *r_y, real_t *r_z, real_t *r_w);
bool mono_object_to_vector2i(MonoObject *p_obj, int32_t *r_x, int32_t *r_y);
bool mono_object_to_vector3i(MonoObject *p_obj, int32_t *r_x, int32_t *r_y, int32_t *r_z);
bool mono_object_to_vector4i(MonoObject *p_obj, int32_t *r_x, int32_t *r_y, int32_t *r_z, int32_t *r_w);
bool mono_object_to_rect2i(MonoObject *p_obj, int32_t *r_x, int32_t *r_y, int32_t *r_w, int32_t *r_h);
bool mono_object_to_quaternion(MonoObject *p_obj, real_t *r_x, real_t *r_y, real_t *r_z, real_t *r_w);
bool mono_object_to_plane(MonoObject *p_obj, real_t *r_nx, real_t *r_ny, real_t *r_nz, real_t *r_d);
bool mono_object_to_aabb(MonoObject *p_obj, real_t *r_px, real_t *r_py, real_t *r_pz, real_t *r_sx, real_t *r_sy, real_t *r_sz);

void cache_mono_corlib_classes();
void cache_godot_math_classes(MonoImage *p_godot_image);

MonoClass *get_intptr_class();
MonoClass *get_vector2_class();
MonoClass *get_vector3_class();
MonoClass *get_color_class();
MonoClass *get_rect2_class();
MonoClass *get_vector4_class();
MonoClass *get_quaternion_class();
MonoClass *get_basis_class();
MonoClass *get_transform3d_class();
MonoClass *get_transform2d_class();
MonoClass *get_plane_class();
MonoClass *get_aabb_class();
MonoClass *get_vector2i_class();
MonoClass *get_vector3i_class();
MonoClass *get_vector4i_class();
MonoClass *get_rect2i_class();

}
