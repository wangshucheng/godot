/**************************************************************************/
/*  csharp_notify_dispatch.cpp                                            */
/**************************************************************************/
/*                         Phase 0.2 Implementation                     */
/*                                                                        */
/* See header (csharp_notify_dispatch.h) for design rationale.            */
/*                                                                        */
/* This file holds only static dispatch metadata. The resolution and      */
/* invocation logic lives in CSharpInstance::notification() (friend),     */
/* which keeps the dispatch table as a plain data carrier.                */
/*                                                                        */
/**************************************************************************/

#include "csharp_notify_dispatch.h"

#include "scene/main/node.h"

// Argument provider enum: how to construct the single Variant argument
// for 1-arg notification methods (_Process(double), _PhysicsProcess(double),
// _Notification(int)).
enum class NotifyArgProvider {
	NONE, // 0-arg method
	DELTA_PROCESS, // node->get_process_delta_time()
	DELTA_PHYSICS, // node->get_physics_process_delta_time()
	NOTIFICATION_ID, // p_notification itself
};

// Static spec for each notification ID we cache.
// notification == -1 means "no fixed ID" (e.g., _Notification(int) is called
// for every notification; _ToString() is called via to_string()).
struct NotifySpec {
	int notification; // Godot notification ID, or -1 for variable
	NotifyEntryIndex entry_index;
	const char *cs_method_name;
	int arg_count;
	NotifyArgProvider arg_provider;
};

// Order matches NotifyEntryIndex enum values for O(1) array access.
static const NotifySpec NOTIFY_SPECS[] = {
	{ Node::NOTIFICATION_READY, NOTIFY_ENTRY_READY, "_Ready", 0, NotifyArgProvider::NONE },
	{ Node::NOTIFICATION_ENTER_TREE, NOTIFY_ENTRY_ENTER_TREE, "_EnterTree", 0, NotifyArgProvider::NONE },
	{ Node::NOTIFICATION_EXIT_TREE, NOTIFY_ENTRY_EXIT_TREE, "_ExitTree", 0, NotifyArgProvider::NONE },
	{ Node::NOTIFICATION_PROCESS, NOTIFY_ENTRY_PROCESS, "_Process", 1, NotifyArgProvider::DELTA_PROCESS },
	{ Node::NOTIFICATION_PHYSICS_PROCESS, NOTIFY_ENTRY_PHYSICS_PROC, "_PhysicsProcess", 1, NotifyArgProvider::DELTA_PHYSICS },
	{ -1, NOTIFY_ENTRY_NOTIFICATION, "_Notification", 1, NotifyArgProvider::NOTIFICATION_ID },
	{ -1, NOTIFY_ENTRY_TOSTRING, "_ToString", 0, NotifyArgProvider::NONE },
};

// Lookup the spec for a given notification ID.
// Returns nullptr if the notification is not one of the cached high-frequency
// types (caller should fall back to the generic _Notification(int) path).
const NotifySpec *csharp_notify_find_spec(int p_notification) {
	for (const NotifySpec &spec : NOTIFY_SPECS) {
		if (spec.notification == p_notification) {
			return &spec;
		}
	}
	return nullptr;
}

// Lookup spec by entry index (used by _Notification and _ToString callers
// that don't have a notification ID directly).
const NotifySpec *csharp_notify_find_spec_by_entry(NotifyEntryIndex p_index) {
	for (const NotifySpec &spec : NOTIFY_SPECS) {
		if (spec.entry_index == p_index) {
			return &spec;
		}
	}
	return nullptr;
}

// Spec accessors — keep NotifySpec layout private to this translation unit.
const char *csharp_notify_spec_method_name(const NotifySpec *p_spec) {
	return p_spec ? p_spec->cs_method_name : nullptr;
}

int csharp_notify_spec_arg_count(const NotifySpec *p_spec) {
	return p_spec ? p_spec->arg_count : 0;
}

int csharp_notify_spec_arg_provider(const NotifySpec *p_spec) {
	if (!p_spec) return 0;
	return static_cast<int>(p_spec->arg_provider);
}

NotifyEntryIndex csharp_notify_spec_entry_index(const NotifySpec *p_spec) {
	if (!p_spec) return NOTIFY_ENTRY_READY; // safe default
	return p_spec->entry_index;
}
