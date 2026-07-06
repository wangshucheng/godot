#include "mono_icalls.h"
#include "mono_variant.h"
#include "mono_bridge.h"
#include "mono_gc_bridge.h"
#include "mono_callable.h"
#include "core/os/os.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/io/resource_loader.h"
#include "core/input/input.h"
#include "core/math/vector2.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"
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
	RefCounted *rc = Object::cast_to<RefCounted>(obj);
	if (rc) {
		mono_gc_bridge::notify_native_destroyed(obj);
		rc->unreference();
		return;
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

static intptr_t godot_icall_Node_GetParent(intptr_t native_ptr) {
	if (native_ptr == 0) return 0;
	Object *obj = (Object *)native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return 0;
	Node *node = Object::cast_to<Node>(obj);
	if (!node) return 0;
	Node *parent = node->get_parent();
	return parent ? (intptr_t)parent : 0;
}

static intptr_t godot_icall_Node_GetChild(intptr_t native_ptr, int idx) {
	if (native_ptr == 0) return 0;
	Object *obj = (Object *)native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return 0;
	Node *node = Object::cast_to<Node>(obj);
	if (!node) return 0;
	if (idx < 0 || idx >= node->get_child_count()) return 0;
	Node *child = node->get_child(idx);
	return child ? (intptr_t)child : 0;
}

static int godot_icall_Node_GetChildCount(intptr_t native_ptr) {
	if (native_ptr == 0) return 0;
	Object *obj = (Object *)native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return 0;
	Node *node = Object::cast_to<Node>(obj);
	if (!node) return 0;
	return node->get_child_count();
}

static void godot_icall_Node_AddChild(intptr_t native_ptr, intptr_t child_ptr, bool readable) {
	if (native_ptr == 0 || child_ptr == 0) return;
	Object *obj = (Object *)native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return;
	Node *node = Object::cast_to<Node>(obj);
	Node *child = Object::cast_to<Node>((Object *)child_ptr);
	if (!node || !child) return;
	node->add_child(child, readable);
}

static void godot_icall_Node_RemoveChild(intptr_t native_ptr, intptr_t child_ptr) {
	if (native_ptr == 0 || child_ptr == 0) return;
	Object *obj = (Object *)native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return;
	Node *node = Object::cast_to<Node>(obj);
	Node *child = Object::cast_to<Node>((Object *)child_ptr);
	if (!node || !child) return;
	node->remove_child(child);
}

static void godot_icall_Node_QueueFree(intptr_t native_ptr) {
	if (native_ptr == 0) return;
	Object *obj = (Object *)native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return;
	Node *node = Object::cast_to<Node>(obj);
	if (node) node->queue_free();
}

static void godot_icall_Node_SetProcess(intptr_t native_ptr, bool enable) {
	if (native_ptr == 0) return;
	Object *obj = (Object *)native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return;
	Node *node = Object::cast_to<Node>(obj);
	if (node) node->set_process(enable);
}

static void godot_icall_Node_SetPhysicsProcess(intptr_t native_ptr, bool enable) {
	if (native_ptr == 0) return;
	Object *obj = (Object *)native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return;
	Node *node = Object::cast_to<Node>(obj);
	if (node) node->set_physics_process(enable);
}

static void godot_icall_Node_SetProcessInput(intptr_t native_ptr, bool enable) {
	if (native_ptr == 0) return;
	Object *obj = (Object *)native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return;
	Node *node = Object::cast_to<Node>(obj);
	if (node) node->set_process_input(enable);
}

static intptr_t godot_icall_Node_GetTree(intptr_t native_ptr) {
	if (native_ptr == 0) return 0;
	Object *obj = (Object *)native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return 0;
	Node *node = Object::cast_to<Node>(obj);
	if (!node) return 0;
	SceneTree *tree = node->get_tree();
	return tree ? (intptr_t)tree : 0;
}

static intptr_t godot_icall_Object_Ctor(MonoObject *p_this_obj) {
	if (!p_this_obj) {
		return 0;
	}

	MonoClass *klass = mono_object_get_class(p_this_obj);
	const char *class_name_cstr = mono_class_get_name(klass);
	if (!class_name_cstr || class_name_cstr[0] == '\0') {
		return 0;
	}

	StringName class_name(class_name_cstr);
	if (!ClassDB::can_instantiate(class_name)) {
		return 0;
	}
	Object *obj = ClassDB::instantiate(class_name);
	if (!obj) {
		return 0;
	}

	mono_bridge::tie_native_ptr(p_this_obj, obj);
	return (intptr_t)obj;
}

static void godot_icall_Object_BindNativePtr(MonoObject *p_this_obj, intptr_t native_ptr) {
	if (!p_this_obj || native_ptr == 0) return;
	Object *obj = (Object *)native_ptr;
	mono_bridge::tie_native_ptr(p_this_obj, obj);
}

static intptr_t godot_icall_Callable_CreateFromDelegate(MonoObject *p_delegate) {
	if (!p_delegate) return 0;

	CallableCustomMono *custom = memnew(CallableCustomMono);
	uint32_t gchandle = mono_gchandle_new(p_delegate, false);
	custom->set_delegate(gchandle, get_domain());

	Callable *callable = memnew(Callable(custom));
	return (intptr_t)callable;
}

static MonoObject *godot_icall_Callable_Call(intptr_t p_callable_ptr, MonoArray *p_args) {
	if (p_callable_ptr == 0) return nullptr;
	Callable *callable = (Callable *)p_callable_ptr;
	if (!callable->is_valid()) return nullptr;

	int argcount = 0;
	const Variant **args = nullptr;
	if (p_args) {
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
	Variant result;
	callable->callp(args, argcount, result, error);

	if (error.error == Callable::CallError::CALL_OK) {
		return variant_to_mono_object(get_domain(), result);
	}
	return nullptr;
}

static void godot_icall_Callable_Free(intptr_t p_callable_ptr) {
	if (p_callable_ptr == 0) return;
	Callable *callable = (Callable *)p_callable_ptr;
	memdelete(callable);
}

static bool godot_icall_Object_Connect(intptr_t p_native_ptr, MonoString *p_signal, intptr_t p_callable_ptr, int p_flags) {
	if (p_native_ptr == 0 || p_callable_ptr == 0) return false;
	Object *obj = (Object *)p_native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return false;

	char *signal_utf8 = mono_string_to_utf8(p_signal);
	StringName signal_name(signal_utf8);
	mono_free(signal_utf8);

	Callable *callable = (Callable *)p_callable_ptr;
	Error err = obj->connect(signal_name, *callable, (uint32_t)p_flags);
	return err == OK;
}

static void godot_icall_Object_Disconnect(intptr_t p_native_ptr, MonoString *p_signal, intptr_t p_callable_ptr) {
	if (p_native_ptr == 0 || p_callable_ptr == 0) return;
	Object *obj = (Object *)p_native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return;

	char *signal_utf8 = mono_string_to_utf8(p_signal);
	StringName signal_name(signal_utf8);
	mono_free(signal_utf8);

	Callable *callable = (Callable *)p_callable_ptr;
	obj->disconnect(signal_name, *callable);
}

static bool godot_icall_Object_IsConnected(intptr_t p_native_ptr, MonoString *p_signal, intptr_t p_callable_ptr) {
	if (p_native_ptr == 0 || p_callable_ptr == 0) return false;
	Object *obj = (Object *)p_native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return false;

	char *signal_utf8 = mono_string_to_utf8(p_signal);
	StringName signal_name(signal_utf8);
	mono_free(signal_utf8);

	Callable *callable = (Callable *)p_callable_ptr;
	return obj->is_connected(signal_name, *callable);
}

static void godot_icall_Object_EmitSignal(intptr_t p_native_ptr, MonoString *p_signal, MonoArray *p_args) {
	if (p_native_ptr == 0) return;
	Object *obj = (Object *)p_native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return;

	char *signal_utf8 = mono_string_to_utf8(p_signal);
	StringName signal_name(signal_utf8);
	mono_free(signal_utf8);

	int argcount = 0;
	const Variant **args = nullptr;
	if (p_args) {
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

	obj->emit_signalp(signal_name, args, argcount);
}

static bool godot_icall_Object_HasSignal(intptr_t p_native_ptr, MonoString *p_signal) {
	if (p_native_ptr == 0) return false;
	Object *obj = (Object *)p_native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return false;
	char *signal_utf8 = mono_string_to_utf8(p_signal);
	StringName signal_name(signal_utf8);
	mono_free(signal_utf8);
	return obj->has_signal(signal_name);
}

static intptr_t godot_icall_ResourceLoader_Load(MonoString *p_path) {
	if (!p_path) return 0;
	char *path_utf8 = mono_string_to_utf8(p_path);
	if (!path_utf8 || path_utf8[0] == '\0') {
		if (path_utf8) mono_free(path_utf8);
		return 0;
	}
	String path(path_utf8);
	mono_free(path_utf8);
	Ref<Resource> res = ResourceLoader::load(path);
	if (res.is_null()) return 0;
	RefCounted *rc = Object::cast_to<RefCounted>(res.ptr());
	if (rc) {
		rc->reference();
	}
	return (intptr_t)res.ptr();
}

static intptr_t godot_icall_PackedScene_Instantiate(intptr_t scene_ptr) {
	if (scene_ptr == 0) return 0;
	PackedScene *ps = Object::cast_to<PackedScene>((Object *)scene_ptr);
	if (!ps) return 0;
	Node *instance = ps->instantiate();
	return instance ? (intptr_t)instance : 0;
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

static intptr_t godot_icall_Object_InstantiateFromNative(MonoString *p_class_name) {
	if (!p_class_name) return 0;
	char *name_utf8 = mono_string_to_utf8(p_class_name);
	if (!name_utf8 || name_utf8[0] == '\0') {
		if (name_utf8) mono_free(name_utf8);
		return 0;
	}
	StringName class_name(name_utf8);
	mono_free(name_utf8);
	Object *obj = ClassDB::instantiate(class_name);
	if (!obj) return 0;
	return (intptr_t)obj;
}

static int32_t godot_icall_Platform_GetRuntimeInfo() {
	int32_t flags = 0;
#ifdef MONO_AOT_MODE
	flags |= 1;
#endif
#ifdef WEB_ENABLED
	flags |= 2;
#endif
#ifdef MONO_SINGLE_THREAD
	flags |= 4;
#endif
#ifdef WINDOWS_ENABLED
	flags |= 8;
#endif
#ifdef LINUXBSD_ENABLED
	flags |= 16;
#endif
#ifdef MACOS_ENABLED
	flags |= 32;
#endif
	return flags;
}

static bool godot_icall_Input_IsKeyPressed(int32_t keycode) {
	Input *input = Input::get_singleton();
	if (!input) return false;
	return input->is_key_pressed((Key)keycode);
}

static bool godot_icall_Input_IsMouseButtonPressed(int32_t button) {
	Input *input = Input::get_singleton();
	if (!input) return false;
	return input->is_mouse_button_pressed((MouseButton)button);
}

static MonoObject *godot_icall_Input_GetMousePosition() {
	Input *input = Input::get_singleton();
	MonoDomain *domain = mono_domain_get();
	if (!input) {
		return variant_to_mono_vector2(domain, 0, 0);
	}
	::Vector2 mp = input->get_mouse_position();
	return variant_to_mono_vector2(domain, mp.x, mp.y);
}

void godot_register_icalls() {
	mono_add_internal_call("Godot.Bridge::godot_icall_GD_Print", (const void *)godot_icall_GD_Print);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Free", (const void *)godot_icall_Object_Free);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_IsInstanceValid", (const void *)godot_icall_Object_IsInstanceValid);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Get", (const void *)godot_icall_Object_Get);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Set", (const void *)godot_icall_Object_Set);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Call", (const void *)godot_icall_Object_Call);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Ctor", (const void *)godot_icall_Object_Ctor);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_BindNativePtr", (const void *)godot_icall_Object_BindNativePtr);
	mono_add_internal_call("Godot.Bridge::godot_icall_Node_GetNode", (const void *)godot_icall_Node_GetNode);
	mono_add_internal_call("Godot.Bridge::godot_icall_Node_GetParent", (const void *)godot_icall_Node_GetParent);
	mono_add_internal_call("Godot.Bridge::godot_icall_Node_GetChild", (const void *)godot_icall_Node_GetChild);
	mono_add_internal_call("Godot.Bridge::godot_icall_Node_GetChildCount", (const void *)godot_icall_Node_GetChildCount);
	mono_add_internal_call("Godot.Bridge::godot_icall_Node_AddChild", (const void *)godot_icall_Node_AddChild);
	mono_add_internal_call("Godot.Bridge::godot_icall_Node_RemoveChild", (const void *)godot_icall_Node_RemoveChild);
	mono_add_internal_call("Godot.Bridge::godot_icall_Node_QueueFree", (const void *)godot_icall_Node_QueueFree);
	mono_add_internal_call("Godot.Bridge::godot_icall_Node_SetProcess", (const void *)godot_icall_Node_SetProcess);
	mono_add_internal_call("Godot.Bridge::godot_icall_Node_SetPhysicsProcess", (const void *)godot_icall_Node_SetPhysicsProcess);
	mono_add_internal_call("Godot.Bridge::godot_icall_Node_SetProcessInput", (const void *)godot_icall_Node_SetProcessInput);
	mono_add_internal_call("Godot.Bridge::godot_icall_Node_GetTree", (const void *)godot_icall_Node_GetTree);
	mono_add_internal_call("Godot.Bridge::godot_icall_Callable_CreateFromDelegate", (const void *)godot_icall_Callable_CreateFromDelegate);
	mono_add_internal_call("Godot.Bridge::godot_icall_Callable_Call", (const void *)godot_icall_Callable_Call);
	mono_add_internal_call("Godot.Bridge::godot_icall_Callable_Free", (const void *)godot_icall_Callable_Free);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Connect", (const void *)godot_icall_Object_Connect);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Disconnect", (const void *)godot_icall_Object_Disconnect);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_IsConnected", (const void *)godot_icall_Object_IsConnected);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_EmitSignal", (const void *)godot_icall_Object_EmitSignal);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_HasSignal", (const void *)godot_icall_Object_HasSignal);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_InstantiateFromNative", (const void *)godot_icall_Object_InstantiateFromNative);
	mono_add_internal_call("Godot.Bridge::godot_icall_ResourceLoader_Load", (const void *)godot_icall_ResourceLoader_Load);
	mono_add_internal_call("Godot.Bridge::godot_icall_PackedScene_Instantiate", (const void *)godot_icall_PackedScene_Instantiate);
	mono_add_internal_call("Godot.Bridge::godot_icall_Platform_GetRuntimeInfo", (const void *)godot_icall_Platform_GetRuntimeInfo);
	mono_add_internal_call("Godot.Bridge::godot_icall_Input_IsKeyPressed", (const void *)godot_icall_Input_IsKeyPressed);
	mono_add_internal_call("Godot.Bridge::godot_icall_Input_IsMouseButtonPressed", (const void *)godot_icall_Input_IsMouseButtonPressed);
	mono_add_internal_call("Godot.Bridge::godot_icall_Input_GetMousePosition", (const void *)godot_icall_Input_GetMousePosition);
	mono_add_internal_call("HelloWorld.ConsoleBridge::godot_icall_Console_WriteLine", (const void *)godot_icall_Console_WriteLine_raw);
	printf("[Mono] Registered internal calls (nodes + resources + signals + platform + input).\n");
	fflush(stdout);
}
