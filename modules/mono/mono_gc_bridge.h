#pragma once

#include "core/object/object.h"
#include <mono/metadata/object.h>
#include <mono/metadata/appdomain.h>
#include <cstdint>

namespace mono_gc_bridge {

void init(MonoDomain *p_domain);
void shutdown();

uint32_t tie_managed_to_native(MonoObject *p_cs_obj, Object *p_native_obj, bool p_weak = true);
void notify_native_destroyed(Object *p_obj);

MonoObject *get_managed(Object *p_native);
Object *get_native(MonoObject *p_managed);

bool is_native_alive(Object *p_native);
void object_predelete_notification(Object *p_obj);

}
