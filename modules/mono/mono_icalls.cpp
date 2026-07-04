#include "mono_icalls.h"
#include "mono_variant.h"
#include "mono_bridge.h"
#include "core/os/os.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
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

static void godot_icall_Object_Free(intptr_t native_ptr) {
	Object *obj = unmanaged_get_from_ptr(native_ptr);
	if (obj) {
		unregister_object(obj);
		memdelete(obj);
	}
}

static MonoObject *godot_icall_Object_Get(intptr_t native_ptr, MonoString *p_name) {
	Object *obj = unmanaged_get_from_ptr(native_ptr);
	if (!obj) return nullptr;
	char *name_utf8 = mono_string_to_utf8(p_name);
	StringName prop_name(name_utf8);
	mono_free(name_utf8);
	Variant result = obj->get(prop_name);
	return variant_to_mono_object(get_domain(), result);
}

static void godot_icall_Object_Set(intptr_t native_ptr, MonoString *p_name, MonoObject *p_value) {
	Object *obj = unmanaged_get_from_ptr(native_ptr);
	if (!obj) return;
	char *name_utf8 = mono_string_to_utf8(p_name);
	StringName prop_name(name_utf8);
	mono_free(name_utf8);
	Variant v = mono_object_to_variant(p_value);
	obj->set(prop_name, v);
}

static MonoObject *godot_icall_Object_Call(intptr_t native_ptr, MonoString *p_method, MonoArray *p_args) {
	Object *obj = unmanaged_get_from_ptr(native_ptr);
	if (!obj) return nullptr;

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

	Variant result;
	Callable::CallError error;
	obj->callp(method_name, args, argcount, result, error);

	if (error.error != Callable::CallError::CALL_OK) {
		printf("[Mono] Call error on %s: error=%d\n", String(method_name).utf8().get_data(), (int)error.error);
		return nullptr;
	}

	return variant_to_mono_object(get_domain(), result);
}

static intptr_t godot_icall_Node_GetNode(intptr_t native_ptr, MonoString *p_path) {
	Object *obj = unmanaged_get_from_ptr(native_ptr);
	Node *node = Object::cast_to<Node>(obj);
	if (!node) return 0;

	char *path_utf8 = mono_string_to_utf8(p_path);
	NodePath np(path_utf8);
	mono_free(path_utf8);

	Node *child = node->get_node_or_null(np);
	if (!child) return 0;

	return (intptr_t)child;
}

static intptr_t godot_icall_Object_Ctor(MonoString *p_class_name) {
	char *name_utf8 = mono_string_to_utf8(p_class_name);
	StringName class_name(name_utf8);
	mono_free(name_utf8);

	Object *obj = ClassDB::instantiate(class_name);
	if (!obj) {
		printf("[Mono] Failed to instantiate class: %s\n", String(class_name).utf8().get_data());
		return 0;
	}
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
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Get", (const void *)godot_icall_Object_Get);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Set", (const void *)godot_icall_Object_Set);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Call", (const void *)godot_icall_Object_Call);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Ctor", (const void *)godot_icall_Object_Ctor);
	mono_add_internal_call("Godot.Bridge::godot_icall_Node_GetNode", (const void *)godot_icall_Node_GetNode);
	mono_add_internal_call("HelloWorld.ConsoleBridge::godot_icall_Console_WriteLine", (const void *)godot_icall_Console_WriteLine_raw);
	printf("[Mono] Registered internal calls.\n");
}
