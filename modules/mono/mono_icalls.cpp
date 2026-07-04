#include "mono_icalls.h"
#include "mono_variant.h"
#include "mono_bridge.h"
#include "mono_gc_bridge.h"
#include "core/os/os.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "scene/main/node.h"
#include <cstdio>
#include <cstdarg>
#include <cstdlib>

using namespace mono_variant;
using namespace mono_bridge;

static void godot_icall_GD_Print(MonoString *message) {
	if (!message) {
		printf("\n");
		fflush(stdout);
		return;
	}
	char *utf8 = mono_string_to_utf8(message);
	if (utf8) {
		printf("%s\n", utf8);
		mono_free(utf8);
	}
	fflush(stdout);
}

static bool godot_icall_Object_IsInstanceValid(intptr_t native_ptr) {
	if (native_ptr == 0) return false;
	Object *obj = (Object *)native_ptr;
	return mono_gc_bridge::is_native_alive(obj);
}

static void godot_icall_Object_Free(intptr_t native_ptr) {
	if (native_ptr == 0) return;
	Object *obj = (Object *)native_ptr;
	if (obj->is_class("Node")) {
		Node *node = Object::cast_to<Node>(obj);
		if (node && node->is_inside_tree()) {
			node->queue_free();
			return;
		}
	}
	mono_gc_bridge::notify_native_destroyed(obj);
	memdelete(obj);
}

static MonoObject *godot_icall_Object_Get(intptr_t native_ptr, MonoString *p_name) {
	if (native_ptr == 0) return nullptr;
	Object *obj = (Object *)native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return nullptr;
	char *name_utf8 = mono_string_to_utf8(p_name);
	StringName prop_name(name_utf8);
	mono_free(name_utf8);
	Variant result = obj->get(prop_name);
	return variant_to_mono_object(get_domain(), result);
}

static void godot_icall_Object_Set(intptr_t native_ptr, MonoString *p_name, MonoObject *p_value) {
	if (native_ptr == 0) return;
	Object *obj = (Object *)native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return;
	char *name_utf8 = mono_string_to_utf8(p_name);
	StringName prop_name(name_utf8);
	mono_free(name_utf8);
	Variant v = mono_object_to_variant(p_value);
	obj->set(prop_name, v);
}

static MonoObject *godot_icall_Object_Call(intptr_t native_ptr, MonoString *p_method, MonoArray *p_args) {
	if (native_ptr == 0) return nullptr;
	Object *obj = (Object *)native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return nullptr;

	char *method_utf8 = mono_string_to_utf8(p_method);
	StringName method_name(method_utf8);
	mono_free(method_utf8);

	int argcount = 0;
	const Variant **args = nullptr;

	if (p_args) {
		uintptr_t iter = 0;
		argcount = (int)mono_array_length(p_args);
		if (argcount > 0) {
			args = (const Variant **)alloca(sizeof(const Variant *) * argcount);
			for (int i = 0; i < argcount; i++) {
				MonoObject *arg = mono_array_get(p_args, MonoObject *, i);
				Variant *v = (Variant *)alloca(sizeof(Variant));
				*v = mono_object_to_variant(arg);
				args[i] = v;
			}
		}
	}

	Callable::CallError error;
	Variant result = obj->callp(method_name, args, argcount, error);

	if (error.error != Callable::CallError::CALL_OK) {
		printf("[Mono] Call error on %s: error=%d\n", String(method_name).utf8().get_data(), (int)error.error);
		return nullptr;
	}

	return variant_to_mono_object(get_domain(), result);
}

static intptr_t godot_icall_Node_GetNode(intptr_t native_ptr, MonoString *p_path) {
	if (native_ptr == 0) return 0;
	Object *obj = (Object *)native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return 0;
	Node *node = Object::cast_to<Node>(obj);
	if (!node) return 0;

	char *path_utf8 = mono_string_to_utf8(p_path);
	NodePath np(path_utf8);
	mono_free(path_utf8);

	Node *child = node->get_node_or_null(np);
	if (!child) return 0;

	return (intptr_t)child;
}

static intptr_t godot_icall_Object_Ctor(MonoObject *p_this_obj) {
	if (!p_this_obj) {
		printf("[Mono] godot_icall_Object_Ctor: p_this_obj is null\n");
		return 0;
	}

	MonoClass *klass = mono_object_get_class(p_this_obj);
	const char *class_name_cstr = mono_class_get_name(klass);
	if (!class_name_cstr || class_name_cstr[0] == '\0') {
		printf("[Mono] godot_icall_Object_Ctor: could not get class name from MonoObject\n");
		return 0;
	}

	StringName class_name(class_name_cstr);
	Object *obj = ClassDB::instantiate(class_name);
	if (!obj) {
		printf("[Mono] Failed to instantiate class: %s\n", class_name_cstr);
		return 0;
	}

	mono_gc_bridge::tie_managed_to_native(p_this_obj, obj);
	return (intptr_t)obj;
}

static void godot_icall_Console_WriteLine_raw(MonoString *message) {
	if (message == nullptr) {
		printf("\n");
		fflush(stdout);
		return;
	}
	char *utf8_str = mono_string_to_utf8(message);
	if (utf8_str != nullptr) {
		printf("%s\n", utf8_str);
		mono_free(utf8_str);
	} else {
		printf("\n");
	}
	fflush(stdout);
}

void godot_register_icalls() {
	mono_add_internal_call("Godot.Bridge::godot_icall_GD_Print", (const void *)godot_icall_GD_Print);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Free", (const void *)godot_icall_Object_Free);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_IsInstanceValid", (const void *)godot_icall_Object_IsInstanceValid);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Get", (const void *)godot_icall_Object_Get);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Set", (const void *)godot_icall_Object_Set);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Call", (const void *)godot_icall_Object_Call);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Ctor", (const void *)godot_icall_Object_Ctor);
	mono_add_internal_call("Godot.Bridge::godot_icall_Node_GetNode", (const void *)godot_icall_Node_GetNode);
	mono_add_internal_call("HelloWorld.ConsoleBridge::godot_icall_Console_WriteLine", (const void *)godot_icall_Console_WriteLine_raw);
	printf("[Mono] Registered internal calls (with GC bridge).\n");
}
