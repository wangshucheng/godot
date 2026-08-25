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

// Identity validation: returns true iff p_native_obj currently has a
// binding AND that binding's GCHandle target is exactly p_cs_obj.
// Guards against stale NativePtr + native address reuse freeing the
// WRONG live object.
bool binding_belongs_to(MonoObject *p_cs_obj, Object *p_native_obj);

// H8: deferred free queue for finalizer-thread safety.
// enqueue_deferred_free is safe to call from the GC finalizer thread;
// flush_deferred_free must be called on the main thread.
// Entries carry the binding serial captured at enqueue time; at flush the
// serial must still match, otherwise the address was reused and the entry
// is stale (skipped).
void enqueue_deferred_free(Object *p_obj);
int flush_deferred_free();

// Deferred unbind queue for the ~Object() destroyed-callback path.
// notify_native_destroyed may run while a concurrent GC is mid-mark/sweep;
// ANY Mono API there (gchandle get/new/free, field writes) races the
// collector and corrupts the sgen heap (observed: first GC at ~frame 1658
// -> delayed crash in mono_class_init_internal). So the destructor path
// only unlinks the maps (pure C++) and defers all GCHandle work here.
// flush_pending_unbinds must be called on the main thread once per frame
// (from CSharpLanguage::frame, alongside flush_deferred_free).
void flush_pending_unbinds();

}
