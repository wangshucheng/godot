#include "mono_gc_bridge.h"
#include "core/object/ref_counted.h"
#include "core/error/error_macros.h"
#include "scene/main/node.h" // H8: Node::queue_free in flush_deferred_free
#include <cstdio>
#include <cstdint>
#include <mutex>

namespace mono_gc_bridge {

static MonoDomain *domain = nullptr;

// False after shutdown() starts: no Mono API may be touched past that point
// (the runtime may be torn down while engine Objects are still destructing).
static bool active = false;

struct ObjectBinding {
	uint32_t weak_gchandle;
	Object *native_ptr;
	// Unique per tie (monotonic). Detects native address reuse: a freed
	// native's address can be reclaimed by a new object with a NEW binding;
	// comparing serials distinguishes "same binding" from "look-alike".
	uint64_t serial;
};

static HashMap<Object *, ObjectBinding> native_to_managed;
static HashMap<uint32_t, Object *> managed_to_native;

// Track RefCounted bindings separately (strong GCHandle + reference())
static HashSet<RefCounted *> refcounted_bindings;

// Mutex protecting all global HashMap/HashSet access (thread safety).
static std::mutex gc_bridge_mutex;

static uint64_t next_serial = 1;

// Deferred unbind queue (see notify_native_destroyed): GCHandle numbers
// collected on the ~Object() path, drained on the main thread each frame.
static std::mutex pending_unbind_mutex;
static Vector<uint32_t> pending_unbind_queue;

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
	active = true;
	// Root fix (crash: all enemies killed -> freeze -> 0xc0000005 during GC):
	// Only CSharpInstance-attached objects got notify_native_destroyed via
	// NOTIFICATION_PREDELETE. Plain wrappers (Object_Ctor / tie_native_ptr /
	// managed_get_or_create paths) left stale Object* keys in
	// native_to_managed after the engine freed the native. Once the address
	// was reused by a new object, bridge operations (is_native_alive /
	// deferred free / Object_Free) hit the WRONG live object -> premature
	// queue_free / unreference -> heap corruption -> delayed crash inside
	// the Mono GC. Registering the ~Object() hook guarantees every native
	// destruction cleans its binding immediately.
	set_object_destroyed_callback(&object_predelete_notification);
	printf("[Mono] GC bridge initialized (object-destroyed hook registered).\n");
	fflush(stdout);
}

void shutdown() {
	// Unregister FIRST: objects destroyed during/after Mono runtime teardown
	// must not touch Mono APIs (gchandle frees) on a dead runtime.
	set_object_destroyed_callback(nullptr);
	active = false;
	{
		std::lock_guard<std::mutex> lock(gc_bridge_mutex);
		for (auto &pair : native_to_managed) {
			mono_gchandle_free(pair.value.weak_gchandle);
		}
		native_to_managed.clear();
		managed_to_native.clear();
		refcounted_bindings.clear();
	}
	{
		// Also drop any GCHandles that were deferred but never flushed
		// (shutdown may happen between the last frame flush and teardown).
		std::lock_guard<std::mutex> lock(pending_unbind_mutex);
		for (uint32_t gch : pending_unbind_queue) {
			mono_gchandle_free(gch);
		}
		pending_unbind_queue.clear();
	}
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
		native_to_managed.erase(p_native_obj);
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
	binding.serial = next_serial++;
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
	binding.serial = next_serial++;
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
	uint32_t pin_gchandle = 0;

	{
		std::lock_guard<std::mutex> lock(gc_bridge_mutex);
		if (!refcounted_bindings.has(p_obj)) return;
		if (!native_to_managed.has((Object *)p_obj)) return;

		ObjectBinding &binding = native_to_managed[(Object *)p_obj];

		// Capture data needed for C# field clearing (outside lock)
		cs_target = mono_gchandle_get_target(binding.weak_gchandle);
		if (cs_target) {
			// Pin cs_target with a temporary strong handle BEFORE freeing the
			// binding handle: the binding handle is the ONLY strong reference,
			// so freeing it here makes cs_target immediately collectable. A GC
			// running between Phase 1 and Phase 2 would reclaim it, and the
			// mono_object_get_class / mono_field_set_value calls below would
			// read a freed object (0xc0000005, NULL vtable).
			pin_gchandle = mono_gchandle_new(cs_target, false);
			MonoClass *klass = mono_object_get_class(cs_target);
			nativeptr_field = find_nativeptr_field(klass);
			gch_field = find_gchandle_field(klass);
		}

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
	if (pin_gchandle) {
		mono_gchandle_free(pin_gchandle);
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
	if (!p_obj || !active) return;

	// CRITICAL: this runs from Object::~Object() (destroyed-callback), which
	// may execute while a concurrent Mono GC is mid-mark/sweep. ANY Mono API
	// here races the collector:
	//   - mono_gchandle_get_target can return an object already marked dead
	//   - mono_gchandle_new (pin) RESURRECTS such an object whose memory the
	//     sweep phase may already have reclaimed -> the new "root" points at
	//     garbage -> the NEXT GC walk over handle roots crashes inside Mono
	//     (observed: first GC at ~frame 1658, later crash in
	//     mono_class_init_internal)
	//   - mono_field_set_value writes freed heap memory -> sgen heap corrupt
	// So the destructor path only unlinks the maps (pure C++, no Mono API)
	// and defers all GCHandle work to flush_pending_unbinds() on the main
	// thread, which runs at a safe point outside any GC and off the
	// destructor call chain.
	uint32_t gch = 0;
	{
		std::lock_guard<std::mutex> lock(gc_bridge_mutex);
		if (!native_to_managed.has(p_obj)) return;

		ObjectBinding &binding = native_to_managed[p_obj];
		gch = binding.weak_gchandle;

		managed_to_native.erase(gch);
		native_to_managed.erase(p_obj);

		// Remove RefCounted tracking by pointer VALUE only — never
		// dereference p_obj here. This function is also invoked from
		// Object::~Object() via the destroyed-callback, where the dynamic
		// type is already plain Object and cast_to<RefCounted> would fail,
		// leaking the set entry and later misclassifying a reused address.
		// Note: do NOT call unreference() — the native is already dying.
		refcounted_bindings.erase((RefCounted *)p_obj);
	}

	if (gch) {
		std::lock_guard<std::mutex> lock(pending_unbind_mutex);
		pending_unbind_queue.push_back(gch);
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
	// Invoked from Object::~Object() (registered via
	// set_object_destroyed_callback in init()). Must be re-entrancy safe
	// and must not dereference p_obj beyond its use as a map key.
	notify_native_destroyed(p_obj);
}

bool binding_belongs_to(MonoObject *p_cs_obj, Object *p_native_obj) {
	if (!p_cs_obj || !p_native_obj || !active) return false;
	std::lock_guard<std::mutex> lock(gc_bridge_mutex);
	if (!native_to_managed.has(p_native_obj)) return false;
	ObjectBinding &binding = native_to_managed[p_native_obj];
	return mono_gchandle_get_target(binding.weak_gchandle) == p_cs_obj;
}

// ============================================================
// H8: Deferred free queue for finalizer-thread safety.
//
// Problem: godot_icall_Object_Free may be invoked from a non-main
// thread. Engine APIs (queue_free / memdelete / unreference) are not
// safe to call off the main thread, and a temporary wrapper being
// finalized must not release the native it only borrows → UAF.
//
// Fix: when godot_icall_Object_Free is called from a non-main thread,
// it enqueues the object pointer instead of freeing immediately. The
// main thread drains the queue via flush_deferred_free().
//
// Each entry stores the binding serial captured at enqueue time. At
// flush time the binding at that address must still exist AND carry
// the same serial. A mismatch means the original native was destroyed
// (its binding was cleaned by the ~Object() hook) and the address was
// reused by a different object — the stale entry is skipped instead of
// freeing the wrong live object.
// ============================================================

struct DeferredFree {
	Object *obj;
	uint64_t serial; // binding serial at enqueue time; 0 = no binding (already gone)
};

static std::mutex deferred_free_mutex;
static Vector<DeferredFree> deferred_free_queue;

void enqueue_deferred_free(Object *p_obj) {
	if (!p_obj) return;
	uint64_t serial = 0;
	{
		std::lock_guard<std::mutex> lock(gc_bridge_mutex);
		if (active && native_to_managed.has(p_obj)) {
			serial = native_to_managed[p_obj].serial;
		}
	}
	std::lock_guard<std::mutex> lock(deferred_free_mutex);
	deferred_free_queue.push_back({ p_obj, serial });
}

int flush_deferred_free() {
	// Must be called on the main thread. Drains the deferred free queue
	// and releases each object using the same logic as godot_icall_Object_Free.
	// Returns the number of objects freed.
	Vector<DeferredFree> local_queue;
	{
		std::lock_guard<std::mutex> lock(deferred_free_mutex);
		if (deferred_free_queue.is_empty()) return 0;
		local_queue = deferred_free_queue;
		deferred_free_queue.clear();
	}

	int freed = 0;
	for (const DeferredFree &df : local_queue) {
		Object *obj = df.obj;
		if (!obj || df.serial == 0) continue;

		// Identity check: the binding must still exist and be the SAME
		// binding (serial match). A miss or mismatch means the native was
		// already destroyed and possibly its address reused — skip.
		{
			std::lock_guard<std::mutex> lock(gc_bridge_mutex);
			if (!native_to_managed.has(obj)) continue;
			if (native_to_managed[obj].serial != df.serial) continue;
		}

		if (is_refcounted_binding(obj)) {
			RefCounted *rc = Object::cast_to<RefCounted>(obj);
			if (rc) {
				release_refcounted_binding(rc);
				freed++;
				continue;
			}
		}

		if (obj->is_class("Node")) {
			Node *node = Object::cast_to<Node>(obj);
			if (node && node->is_inside_tree()) {
				node->queue_free();
				freed++;
				continue;
			}
		}
		RefCounted *rc = Object::cast_to<RefCounted>(obj);
		if (rc) {
			notify_native_destroyed(obj);
			rc->unreference();
			freed++;
			continue;
		}
		notify_native_destroyed(obj);
		memdelete(obj);
		freed++;
	}
	return freed;
}

void flush_pending_unbinds() {
	// Main thread only. Drains GCHandles deferred by the ~Object() path
	// (see notify_native_destroyed). At this point we are outside any GC
	// and off the destructor call chain, so Mono API use is safe.
	Vector<uint32_t> local_queue;
	{
		std::lock_guard<std::mutex> lock(pending_unbind_mutex);
		if (pending_unbind_queue.is_empty()) return;
		local_queue = pending_unbind_queue;
		pending_unbind_queue.clear();
	}

	for (uint32_t gch : local_queue) {
		if (gch == 0) continue;
		MonoObject *cs_target = mono_gchandle_get_target(gch);
		if (cs_target) {
			// Wrapper still alive (strong handle kept it pinned, or a weak
			// handle whose target is still referenced). Clear its native
			// pointer fields so the C# side can never touch the dead native.
			MonoClass *klass = mono_object_get_class(cs_target);
			MonoClassField *nativeptr_field = find_nativeptr_field(klass);
			MonoClassField *gch_field = find_gchandle_field(klass);
			if (nativeptr_field) {
				intptr_t zero = 0;
				mono_field_set_value(cs_target, nativeptr_field, &zero);
			}
			if (gch_field) {
				uint32_t zero_gch = 0;
				mono_field_set_value(cs_target, gch_field, &zero_gch);
			}
		}
		// Weak handle whose target was collected: get_target returned NULL
		// and there is nothing to clean — just free the handle slot.
		mono_gchandle_free(gch);
	}
}

}
