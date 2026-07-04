#pragma once

#include "core/object/object.h"
#include "core/templates/hash_map.h"
#include <mono/metadata/object.h>
#include <mono/metadata/appdomain.h>

namespace mono_bridge {

void init(MonoDomain *p_domain);
void shutdown();

Object *unmanaged_get_from_ptr(intptr_t p_native_ptr);
Object *unmanaged_get(MonoObject *p_cs_obj);
MonoObject *managed_get_or_create(Object *p_obj, MonoClass *p_class = nullptr);

void tie_native_ptr(MonoObject *p_cs_obj, Object *p_obj);
void unregister_object(Object *p_obj);

MonoDomain *get_domain();

MonoClass *get_godot_object_class();
MonoClass *get_godot_node_class();

void cache_godot_classes(MonoImage *p_godot_image);

}
