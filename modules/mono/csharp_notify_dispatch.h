/**************************************************************************/
/*  csharp_notify_dispatch.h                                              */
/**************************************************************************/
/*                         Phase 0.2 Design                              */
/*                                                                        */
/* Purpose: C++ per-instance notification dispatch cache.                */
/*                                                                        */
/* Background:                                                            */
/*   Phase 0.1 delegate probe (see docs/spike_2026-07-28_phase0_         */
/*   delegate_probe.md) confirmed that mono_runtime_invoke is the         */
/*   only viable path on WASM interpreter (mono_compile_method returns    */
/*   interpreter thunk 0x5e5, not directly callable from C++).           */
/*                                                                        */
/*   This cache eliminates per-notification overhead that does not        */
/*   depend on the invoke mechanism:                                      */
/*     1. Notification ID -> CS method name mapping (was a loop)        */
/*     2. find_method() call (walks class hierarchy each time)          */
/*     3. mono_method_get_class() "is overridden" check (per call)       */
/*                                                                        */
/* Design choice (Option B per architecture doc):                         */
/*   All platforms use mono_runtime_invoke. The cache only removes        */
/*   resolution redundancy. JIT-side ftn_ptr direct call is deferred      */
/*   until profiler data justifies the additional complexity.             */
/*                                                                        */
/* Lifecycle:                                                             */
/*   - Constructed default-initialized with CSharpInstance               */
/*   - Lazily resolves entries on first notification of each ID          */
/*   - Invalidated by clear() on hot reload (N3 image-swap fix)          */
/*                                                                        */
/**************************************************************************/

#pragma once

#include "core/string/string_name.h"

typedef struct _MonoClass MonoClass;
typedef struct _MonoMethod MonoMethod;

class CSharpInstance;

// Notification entry indices. Stable for array indexing.
// Must stay synchronized with NOTIFY_SPECS in the .cpp file.
enum NotifyEntryIndex : int {
	NOTIFY_ENTRY_READY = 0,
	NOTIFY_ENTRY_ENTER_TREE = 1,
	NOTIFY_ENTRY_EXIT_TREE = 2,
	NOTIFY_ENTRY_PROCESS = 3,
	NOTIFY_ENTRY_PHYSICS_PROC = 4,
	NOTIFY_ENTRY_NOTIFICATION = 5, // _Notification(int) — invoked for every notif
	NOTIFY_ENTRY_TOSTRING = 6, // _ToString() — invoked via to_string()
	NOTIFY_ENTRY_COUNT = 7,
};

// Per-notification cache entry.
// One per notification ID; resolved lazily on first use.
struct NotifyEntry {
	MonoMethod *method = nullptr; // Resolved method, or nullptr if class doesn't define it
	MonoClass *declaring_class = nullptr; // Class that declares the method (for overridden check)
	bool resolved = false; // Has this entry been resolved (looked up) at least once
};

// Dispatch table for CSharpInstance.
// Owns a fixed-size array of NotifyEntry, one per high-frequency notification.
// Resolution and invocation logic lives in CSharpInstance (friend) so the
// dispatch table itself stays a plain data carrier.
class CSharpNotifyDispatch {
	friend class CSharpInstance;

public:
	// Reset all entries. Call on hot reload to invalidate stale MonoMethod*
	// references (N3 image-swap fix: pre-reload instances keep their old
	// mono_object, so method pointers must be re-resolved lazily).
	void clear() {
		for (int i = 0; i < NOTIFY_ENTRY_COUNT; i++) {
			entries[i].method = nullptr;
			entries[i].declaring_class = nullptr;
			entries[i].resolved = false;
		}
	}

	// Access entry by index. Resolves on demand by caller.
	NotifyEntry &get_entry(NotifyEntryIndex idx) { return entries[idx]; }
	const NotifyEntry &get_entry(NotifyEntryIndex idx) const { return entries[idx]; }

private:
	NotifyEntry entries[NOTIFY_ENTRY_COUNT];
};

// Static spec table lookup (defined in csharp_notify_dispatch.cpp).
// Returns nullptr if p_notification is not one of the cached high-frequency
// types (Node::NOTIFICATION_READY / ENTER_TREE / EXIT_TREE / PROCESS /
// PHYSICS_PROCESS). Caller should fall back to the generic _Notification(int)
// path for unhandled IDs.
struct NotifySpec;
const NotifySpec *csharp_notify_find_spec(int p_notification);
const NotifySpec *csharp_notify_find_spec_by_entry(NotifyEntryIndex p_index);

// Spec accessors (so callers don't need to know NotifySpec layout details).
const char *csharp_notify_spec_method_name(const NotifySpec *p_spec);
int csharp_notify_spec_arg_count(const NotifySpec *p_spec);
// arg_provider values (mirrors NotifyArgProvider enum in .cpp).
// 0 = NONE (0-arg method)
// 1 = DELTA_PROCESS (node->get_process_delta_time())
// 2 = DELTA_PHYSICS (node->get_physics_process_delta_time())
// 3 = NOTIFICATION_ID (pass p_notification itself)
int csharp_notify_spec_arg_provider(const NotifySpec *p_spec);
// Entry index for this spec (used to index CSharpNotifyDispatch::entries_).
NotifyEntryIndex csharp_notify_spec_entry_index(const NotifySpec *p_spec);
