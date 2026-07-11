#pragma once

#include "core/object/object.h"
#include <mono/metadata/object.h>
#include <mono/metadata/appdomain.h>
#include <cstdint>

class RefCounted;

namespace mono_gc_bridge {

void init(MonoDomain *p_domain);
void shutdown();

// Regular Object binding (weak GCHandle)
uint32_t tie_managed_to_native(MonoObject *p_cs_obj, Object *p_native_obj, bool p_weak = true);
void notify_native_destroyed(Object *p_obj);

// RefCounted binding (strong GCHandle + reference()/unreference())
// C# wrapper holds a strong reference to keep native alive.
// C# Dispose calls release_refcounted_binding to release the reference.
uint32_t tie_managed_to_refcounted(MonoObject *p_cs_obj, RefCounted *p_native_obj);
void release_refcounted_binding(RefCounted *p_obj);
bool is_refcounted_binding(Object *p_native_obj);

MonoObject *get_managed(Object *p_native);
Object *get_native(MonoObject *p_managed);

bool is_native_alive(Object *p_native);
void object_predelete_notification(Object *p_obj);

}
