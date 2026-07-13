#include "mono_gc_bridge.h"
#include "core/object/ref_counted.h"
#include "core/error/error_macros.h"
#include <cstdio>
#include <cstdint>
#include <mutex>

namespace mono_gc_bridge {

static MonoDomain *domain = nullptr;

struct ObjectBinding {
	uint32_t weak_gchandle;
	Object *native_ptr;
};

static HashMap<Object *, ObjectBinding> native_to_managed;
static HashMap<uint32_t, Object *> managed_to_native;

// Track RefCounted bindings separately (strong GCHandle + reference())
static HashSet<RefCounted *> refcounted_bindings;

// Mutex protecting all global HashMap/HashSet access (thread safety).
static std::mutex gc_bridge_mutex;

static MonoClassField *find_nativeptr_field(MonoClass *p_klass) {
	for (MonoClass *k = p_klass; k; k = mono_class_get_parent(k)) {
		MonoClassField *field = mono_class_get_field_from_name(k, "NativePtr");
		if (field) return field;
	}
	return nullptr;
}

static MonoClassField *find_gchandle_field(MonoClass *p_klass) {
	for (MonoClass *k = p_klass; k; k = mono_class_get_parent(k)) {
		MonoClassField *field = mono_class_get_field_from_name(k, "_bridgeGCHandle");
		if (field) return field;
	}
	return nullptr;
}

static void set_gchandle_field(MonoObject *p_cs_obj, uint32_t p_gch) {
	if (!p_cs_obj) return;
	MonoClass *klass = mono_object_get_class(p_cs_obj);
	MonoClassField *field = find_gchandle_field(klass);
	if (field) {
		mono_field_set_value(p_cs_obj, field, &p_gch);
	}
}

void init(MonoDomain *p_domain) {
	domain = p_domain;
	printf("[Mono] GC bridge initialized.\n");
	fflush(stdout);
}

void shutdown() {
	std::lock_guard<std::mutex> lock(gc_bridge_mutex);
	for (auto &pair : native_to_managed) {
		mono_gchandle_free(pair.value.weak_gchandle);
	}
	native_to_managed.clear();
	managed_to_native.clear();
	refcounted_bindings.clear();
	domain = nullptr;
	printf("[Mono] GC bridge shut down.\n");
	fflush(stdout);
}

uint32_t tie_managed_to_native(MonoObject *p_cs_obj, Object *p_native_obj, bool p_weak) {
	if (!p_cs_obj || !p_native_obj) return 0;
	std::lock_guard<std::mutex> lock(gc_bridge_mutex);

	if (native_to_managed.has(p_native_obj)) {
		uint32_t old = native_to_managed[p_native_obj].weak_gchandle;
		mono_gchandle_free(old);
		managed_to_native.erase(old);
	}

	uint32_t gch;
	if (p_weak) {
		gch = mono_gchandle_new_weakref(p_cs_obj, true);
	} else {
		gch = mono_gchandle_new(p_cs_obj, false);
	}

	ObjectBinding binding;
	binding.weak_gchandle = gch;
	binding.native_ptr = p_native_obj;
	native_to_managed[p_native_obj] = binding;
	managed_to_native[gch] = p_native_obj;

	set_gchandle_field(p_cs_obj, gch);

	return gch;
}

// =============================================
// RefCounted binding: strong GCHandle + reference()
// =============================================

uint32_t tie_managed_to_refcounted(MonoObject *p_cs_obj, RefCounted *p_native_obj) {
	if (!p_cs_obj || !p_native_obj) return 0;
	std::lock_guard<std::mutex> lock(gc_bridge_mutex);

	// If there's an existing binding, release it first
	if (native_to_managed.has((Object *)p_native_obj)) {
		ObjectBinding &old = native_to_managed[(Object *)p_native_obj];
		mono_gchandle_free(old.weak_gchandle);
		managed_to_native.erase(old.weak_gchandle);
		native_to_managed.erase((Object *)p_native_obj);
	}

	// Strong GCHandle: prevents C# wrapper from being GC'd
	uint32_t gch = mono_gchandle_new(p_cs_obj, false);

	ObjectBinding binding;
	binding.weak_gchandle = gch;
	binding.native_ptr = (Object *)p_native_obj;
	native_to_managed[(Object *)p_native_obj] = binding;
	managed_to_native[gch] = (Object *)p_native_obj;

	set_gchandle_field(p_cs_obj, gch);

	// NOTE: Caller is responsible for calling reference() before this function.
	// - Object_Ctor: ClassDB::instantiate gives refcount=1 (serves as C# ref)
	// - ResourceLoader_Load: explicit rc->reference() before return
	// - managed_get_or_create: rc->reference() before constructor call

	// Track as RefCounted binding
	refcounted_bindings.insert(p_native_obj);

	return gch;
}

void release_refcounted_binding(RefCounted *p_obj) {
	if (!p_obj) return;

	// Phase 1: under lock, collect binding data and remove from maps.
	// We must NOT call unreference()/memdelete() under the lock because
	// the destructor chain may trigger callbacks (notification, signals,
	// script callbacks) that re-enter the GC bridge and would deadlock.
	MonoObject *cs_target = nullptr;
	MonoClassField *nativeptr_field = nullptr;
	MonoClassField *gch_field = nullptr;
	uint32_t gch_to_free = 0;
	bool should_delete = false;

	{
		std::lock_guard<std::mutex> lock(gc_bridge_mutex);
		if (!refcounted_bindings.has(p_obj)) return;
		if (!native_to_managed.has((Object *)p_obj)) return;

		ObjectBinding &binding = native_to_managed[(Object *)p_obj];

		// Capture data needed for C# field clearing (outside lock)
		cs_target = mono_gchandle_get_target(binding.weak_gchandle);
		if (cs_target) {
			MonoClass *klass = mono_object_get_class(cs_target);
			nativeptr_field = find_nativeptr_field(klass);
			gch_field = find_gchandle_field(klass);
		}

		gch_to_free = binding.weak_gchandle;

		// Release strong GCHandle and remove from maps
		mono_gchandle_free(binding.weak_gchandle);
		managed_to_native.erase(binding.weak_gchandle);
		native_to_managed.erase((Object *)p_obj);
		refcounted_bindings.erase(p_obj);
	}

	// Phase 2: outside lock, clear C# fields and release native reference.
	// Clear C# NativePtr before releasing (prevents dangling pointer access)
	if (cs_target) {
		if (nativeptr_field) {
			intptr_t zero = 0;
			mono_field_set_value(cs_target, nativeptr_field, &zero);
		}
		if (gch_field) {
			uint32_t zero_gch = 0;
			mono_field_set_value(cs_target, gch_field, &zero_gch);
		}
	}

	// Release C#'s reference. If refcount reaches 0, delete the object.
	// This is safe outside the lock - unreference() is thread-safe (atomic),
	// and memdelete's destructor chain can safely re-enter the GC bridge.
	if (p_obj->unreference()) {
		memdelete(p_obj);
	}
}

bool is_refcounted_binding(Object *p_native_obj) {
	if (!p_native_obj) return false;
	RefCounted *rc = Object::cast_to<RefCounted>(p_native_obj);
	if (!rc) return false;
	std::lock_guard<std::mutex> lock(gc_bridge_mutex);
	return refcounted_bindings.has(rc);
}

void notify_native_destroyed(Object *p_obj) {
	if (!p_obj) return;

	// Phase 1: under lock, collect binding data and remove from maps.
	MonoObject *cs_target = nullptr;
	MonoClassField *nativeptr_field = nullptr;
	MonoClassField *gch_field = nullptr;
	uint32_t gch_to_free = 0;
	bool is_rc_binding = false;
	RefCounted *rc = nullptr;

	{
		std::lock_guard<std::mutex> lock(gc_bridge_mutex);
		if (!native_to_managed.has(p_obj)) return;

		ObjectBinding &binding = native_to_managed[p_obj];

		// Check if this is a RefCounted binding
		rc = Object::cast_to<RefCounted>(p_obj);
		is_rc_binding = (rc && refcounted_bindings.has(rc));

		// Capture data for C# field clearing (outside lock)
		cs_target = mono_gchandle_get_target(binding.weak_gchandle);
		if (cs_target) {
			MonoClass *klass = mono_object_get_class(cs_target);
			nativeptr_field = find_nativeptr_field(klass);
			gch_field = find_gchandle_field(klass);
		}

		gch_to_free = binding.weak_gchandle;

		mono_gchandle_free(binding.weak_gchandle);
		managed_to_native.erase(binding.weak_gchandle);
		native_to_managed.erase(p_obj);

		// If RefCounted binding, remove from tracking set
		// Note: do NOT call unreference() here - the native is already being destroyed
		if (is_rc_binding) {
			refcounted_bindings.erase(rc);
		}
	}

	// Phase 2: outside lock, clear C# fields.
	// Mono API calls are safe here since we no longer hold the mutex.
	if (cs_target) {
		if (nativeptr_field) {
			intptr_t zero = 0;
			mono_field_set_value(cs_target, nativeptr_field, &zero);
		}
		if (gch_field) {
			uint32_t zero_gch = 0;
			mono_field_set_value(cs_target, gch_field, &zero_gch);
		}
	}
}

MonoObject *get_managed(Object *p_native) {
	if (!p_native) return nullptr;
	std::lock_guard<std::mutex> lock(gc_bridge_mutex);
	if (!native_to_managed.has(p_native)) return nullptr;
	ObjectBinding &binding = native_to_managed[p_native];
	MonoObject *target = mono_gchandle_get_target(binding.weak_gchandle);
	if (!target) {
		mono_gchandle_free(binding.weak_gchandle);
		managed_to_native.erase(binding.weak_gchandle);
		native_to_managed.erase(p_native);
		return nullptr;
	}
	return target;
}

Object *get_native(MonoObject *p_managed) {
	if (!p_managed) return nullptr;
	std::lock_guard<std::mutex> lock(gc_bridge_mutex);

	MonoClass *klass = mono_object_get_class(p_managed);
	MonoClassField *gch_field = find_gchandle_field(klass);
	if (gch_field) {
		uint32_t gch = 0;
		mono_field_get_value(p_managed, gch_field, &gch);
		if (gch != 0 && managed_to_native.has(gch)) {
			MonoObject *target = mono_gchandle_get_target(gch);
			if (target == p_managed) {
				return managed_to_native[gch];
			}
		}
	}

	for (auto &pair : native_to_managed) {
		MonoObject *t = mono_gchandle_get_target(pair.value.weak_gchandle);
		if (t == p_managed) return pair.key;
	}
	return nullptr;
}

bool is_native_alive(Object *p_native) {
	if (!p_native) return false;
	std::lock_guard<std::mutex> lock(gc_bridge_mutex);
	if (!native_to_managed.has(p_native)) return false;
	ObjectBinding &binding = native_to_managed[p_native];
	MonoObject *target = mono_gchandle_get_target(binding.weak_gchandle);
	if (!target) {
		mono_gchandle_free(binding.weak_gchandle);
		managed_to_native.erase(binding.weak_gchandle);
		native_to_managed.erase(p_native);
		return false;
	}
	return true;
}

void object_predelete_notification(Object *p_obj) {
	notify_native_destroyed(p_obj);
}

}
