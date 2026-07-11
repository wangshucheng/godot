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
#include "core/config/engine.h"
#include "scene/gui/label.h"
#include "scene/gui/control.h"
#include "scene/main/scene_tree.h"
#include "scene/main/canvas_layer.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "modules/websocket/websocket_peer.h"
#include "servers/text/text_server.h"
#include <mono/metadata/image.h>
#include <mono/metadata/blob.h>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>

using namespace mono_variant;
using namespace mono_bridge;

// Matches our DLL: Godot.Bridge::godot_icall_GD_Print(string message)
static void godot_icall_GD_Print(MonoString *message) {
	if (message) {
		char *utf8 = mono_string_to_utf8(message);
		if (utf8) {
			printf("%s\n", utf8);
			mono_free(utf8);
		}
	} else {
		printf("\n");
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

// Matches our DLL: Godot.Bridge::godot_icall_Object_Ctor(object thisObj) -> native int
static intptr_t godot_icall_Object_Ctor(MonoObject *p_this_obj) {
	if (!p_this_obj) {
		return 0;
	}

	// Check if NativePtr is already set (e.g., by CSharpScript before calling constructor)
	MonoClass *klass = mono_object_get_class(p_this_obj);
	MonoClassField *native_ptr_field = nullptr;
	for (MonoClass *k = klass; k && !native_ptr_field; k = mono_class_get_parent(k)) {
		native_ptr_field = mono_class_get_field_from_name(k, "NativePtr");
	}
	if (native_ptr_field) {
		intptr_t existing_ptr = 0;
		mono_field_get_value(p_this_obj, native_ptr_field, &existing_ptr);
		if (existing_ptr != 0) {
			return existing_ptr;
		}
	}

	// No existing native ptr — create new native object.
	// Walk up the C# class hierarchy to find the first class that ClassDB can instantiate.
	for (MonoClass *k = klass; k; k = mono_class_get_parent(k)) {
		const char *cname = mono_class_get_name(k);
		const char *cns = mono_class_get_namespace(k);
		if (!cname || !cns || strcmp(cns, "Godot") != 0) continue;
		StringName class_name(cname);
		if (ClassDB::can_instantiate(class_name)) {
			Object *obj = ClassDB::instantiate(class_name);
			if (obj) {
				mono_bridge::tie_native_ptr(p_this_obj, obj);
				return (intptr_t)obj;
			}
		}
	}

	return 0;
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

// ============================================================
// WASM-safe icalls: These bypass Mono WASM interpreter bugs by
// performing string/int operations in C++ instead of C#.
// The Mono WASM interpreter crashes with "function signature
// mismatch" on: int.ToString(), string concat with int, method
// calls returning string, etc. These icalls move all such
// operations to the C++ side.
// ============================================================

// C++-side int to string conversion (replaces int.ToString())
static MonoString *godot_icall_Int_ToString(int32_t value) {
	MonoDomain *domain = mono_domain_get();
	char buf[16];
	snprintf(buf, sizeof(buf), "%d", value);
	return mono_string_new(domain, buf);
}

// C++-side string concat with int (replaces "prefix" + int)
static MonoString *godot_icall_String_ConcatInt(MonoString *prefix, int32_t value) {
	MonoDomain *domain = mono_domain_get();
	char *utf8 = prefix ? mono_string_to_utf8(prefix) : nullptr;
	char buf[16];
	snprintf(buf, sizeof(buf), "%d", value);

	// Concatenate
	String result = String(utf8 ? utf8 : "") + String(buf);
	if (utf8) mono_free(utf8);
	return mono_string_new(domain, result.utf8().get_data());
}

// C++-side: set Label text to "FPS: N" directly (no C# string ops at all)
static void godot_icall_Label_SetFpsText(intptr_t label_ptr, int32_t fps) {
	if (label_ptr == 0) return;
	Object *obj = (Object *)label_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return;

	Label *label = Object::cast_to<Label>(obj);
	if (!label) return;

	char buf[16];
	snprintf(buf, sizeof(buf), "FPS: %d", fps);
	label->set_text(buf);
}

// C++-side: set any Object property to an int-formatted string (no C# string ops)
static void godot_icall_Object_SetIntText(intptr_t obj_ptr, MonoString *prop_name, int32_t value) {
	if (obj_ptr == 0) return;
	Object *obj = (Object *)obj_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return;

	char *name_utf8 = mono_string_to_utf8(prop_name);
	StringName prop(name_utf8);
	mono_free(name_utf8);

	char buf[16];
	snprintf(buf, sizeof(buf), "%d", value);
	obj->set(prop, String(buf));
}

// C++-side: get engine FPS (replaces C# arithmetic in _Process)
static int32_t godot_icall_Engine_GetFps() {
	Engine *engine = Engine::get_singleton();
	if (!engine) return 0;
	return (int32_t)engine->get_frames_per_second();
}

// C++-side: set Label text with a prefix + int (e.g. "Frames: 1234")
static void godot_icall_Label_SetPrefixedInt(intptr_t label_ptr, MonoString *prefix, int32_t value) {
	if (label_ptr == 0) return;
	Object *obj = (Object *)label_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return;

	Label *label = Object::cast_to<Label>(obj);
	if (!label) return;

	char *utf8 = prefix ? mono_string_to_utf8(prefix) : nullptr;
	char buf[32];
	snprintf(buf, sizeof(buf), "%s%d", utf8 ? utf8 : "", value);
	if (utf8) mono_free(utf8);
	label->set_text(buf);
}

// C++-side: append a log line to a Label's existing text
// (reads current text, appends "\n" + new message, sets it back)
static void godot_icall_Label_AppendLog(intptr_t label_ptr, MonoString *message) {
	if (label_ptr == 0) return;
	Object *obj = (Object *)label_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return;

	Label *label = Object::cast_to<Label>(obj);
	if (!label) return;

	String current = label->get_text();
	char *utf8 = message ? mono_string_to_utf8(message) : nullptr;
	String new_text = current + "\n" + String(utf8 ? utf8 : "");
	if (utf8) mono_free(utf8);
	label->set_text(new_text);
}

// Set Control position (x, y) - WASM-safe, no Vector2 needed in C#.
static void godot_icall_Control_SetPosition(intptr_t ctrl_ptr, int32_t x, int32_t y) {
	if (ctrl_ptr == 0) return;
	Object *obj = (Object *)ctrl_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return;
	Control *ctrl = Object::cast_to<Control>(obj);
	if (!ctrl) return;
	ctrl->set_position(Vector2((real_t)x, (real_t)y));
}

// ============================================================
// Debug UI icalls: Global pointer model (WASM-safe).
// C++ side creates and manages a single debug Label; C# never
// touches pointers, never does string/int ops. All text building
// is done in C++ to avoid Mono WASM interpreter signature mismatch.
// ============================================================

static Label *_g_debug_label = nullptr;
static Vector<String> _g_debug_lines;
static const int MAX_DEBUG_LINES = 100;

// Find/create the debug label (added to scene root via CanvasLayer)
static Label *_ensure_debug_label() {
	if (_g_debug_label) return _g_debug_label;

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree) {
		printf("[Mono] ERROR: Cannot get SceneTree for debug UI\n");
		fflush(stdout);
		return nullptr;
	}
	Window *root = tree->get_root();
	if (!root) {
		printf("[Mono] ERROR: Cannot get root Window for debug UI\n");
		fflush(stdout);
		return nullptr;
	}

	CanvasLayer *layer = memnew(CanvasLayer);
	layer->set_layer(100);
	root->add_child(layer);

	Label *label = memnew(Label);
	label->set_anchors_preset(Control::PRESET_TOP_LEFT);
	label->set_position(Vector2(10, 10));
	label->set_size(Vector2(1200, 800));
	label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	label->add_theme_font_size_override("font_size", 16);
	label->add_theme_color_override("font_color", Color(1, 1, 1, 1));
	label->add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.8f));
	label->add_theme_constant_override("shadow_offset_x", 2);
	label->add_theme_constant_override("shadow_offset_y", 2);
	_g_debug_lines.clear();
	label->set_text("");
	layer->add_child(label);

	_g_debug_label = label;
	printf("[Mono] Debug UI label created and added to scene.\n");
	fflush(stdout);
	return _g_debug_label;
}

static void _refresh_debug_label() {
	if (!_g_debug_label) return;
	String full;
	for (int i = 0; i < _g_debug_lines.size(); i++) {
		if (i > 0) full += "\n";
		full += _g_debug_lines[i];
	}
	_g_debug_label->set_text(full);
}

// Init debug UI: creates the label if needed. Returns 0 on success.
static int32_t godot_icall_DebugUi_Init() {
	Label *l = _ensure_debug_label();
	return l ? 0 : -1;
}

// Clear all text lines.
static void godot_icall_DebugUi_Clear() {
	_g_debug_lines.clear();
	_refresh_debug_label();
}

// Append a text line.
static void godot_icall_DebugUi_AddLine(MonoString *line) {
	if (!_ensure_debug_label()) return;
	char *utf8 = line ? mono_string_to_utf8(line) : nullptr;
	String s(utf8 ? utf8 : "");
	if (utf8) mono_free(utf8);
	_g_debug_lines.append(s);
	while (_g_debug_lines.size() > MAX_DEBUG_LINES) {
		_g_debug_lines.remove_at(0);
	}
	_refresh_debug_label();
}

// Append a line: prefix + int value (all string/int concat done in C++).
static void godot_icall_DebugUi_AddLineInt(MonoString *prefix, int32_t value) {
	if (!_ensure_debug_label()) return;
	char *utf8 = prefix ? mono_string_to_utf8(prefix) : nullptr;
	char buf[32];
	snprintf(buf, sizeof(buf), "%d", value);
	String s = String(utf8 ? utf8 : "") + String(buf);
	if (utf8) mono_free(utf8);
	_g_debug_lines.append(s);
	while (_g_debug_lines.size() > MAX_DEBUG_LINES) {
		_g_debug_lines.remove_at(0);
	}
	_refresh_debug_label();
}

// ============================================================
// WebSocket icalls: WASM-safe WebSocket operations
// Uses a global WebSocketPeer pointer to avoid passing/returning
// pointers through icalls, which triggers Mono WASM interpreter
// "function signature mismatch" errors.
// All string operations done in C++. NO strings returned to C#.
// ============================================================

// Global WebSocket peer - avoids pointer passing through icall boundary.
// Single-connection model is sufficient for testing.
static WebSocketPeer *_ws_global_peer = nullptr;

// Track state and stats entirely in C++ (no string returned to C#).
static int32_t _ws_last_state = -1;
static int _ws_poll_count = 0;
static int _ws_send_count = 0;
static int _ws_recv_count = 0;
static String _ws_last_message;

// Create + connect in one step. Returns Error code (0 = OK, -1 = create failed).
static int32_t godot_icall_WebSocket_Init(MonoString *url) {
	if (_ws_global_peer) {
		_ws_global_peer->close();
		memdelete(_ws_global_peer);
		_ws_global_peer = nullptr;
	}

	WebSocketPeer *peer = WebSocketPeer::create();
	if (!peer) {
		printf("[Mono] ERROR: WebSocketPeer::create() returned null!\n");
		fflush(stdout);
		return -1;
	}
	_ws_global_peer = peer;
	_ws_last_state = -1;
	_ws_poll_count = 0;
	_ws_send_count = 0;
	_ws_recv_count = 0;
	_ws_last_message = "";

	char *utf8 = url ? mono_string_to_utf8(url) : nullptr;
	if (!utf8) return -1;

	String ws_url(utf8);
	mono_free(utf8);

	Error err = peer->connect_to_url(ws_url);
	printf("[Mono] WebSocket connect_to_url => Error %d, state=%d\n", (int)err, (int)peer->get_ready_state());
	fflush(stdout);
	return (int32_t)err;
}

// Poll the WebSocket and process received messages (store in C++ buffer).
// Returns current ready state.
static int32_t godot_icall_WebSocket_PollAndGetState() {
	if (!_ws_global_peer) return 3; // CLOSED
	_ws_global_peer->poll();
	_ws_poll_count++;

	// Drain all available packets into C++-side buffer (last message kept)
	int avail = _ws_global_peer->get_available_packet_count();
	while (avail > 0) {
		const uint8_t *buffer = nullptr;
		int buffer_size = 0;
		Error err = _ws_global_peer->get_packet(&buffer, buffer_size);
		if (err == OK && buffer && buffer_size > 0) {
			_ws_last_message = String::utf8((const char *)buffer, buffer_size);
			_ws_recv_count++;
		}
		avail--;
	}

	int32_t cur_state = (int32_t)_ws_global_peer->get_ready_state();
	if (cur_state != _ws_last_state) {
		printf("[Mono] WS state changed: %d -> %d\n", (int)_ws_last_state, (int)cur_state);
		fflush(stdout);
		_ws_last_state = cur_state;
	}
	return cur_state;
}

// Poll only (no state return - kept for compatibility if needed)
static void godot_icall_WebSocket_Poll() {
	godot_icall_WebSocket_PollAndGetState();
}

// Get WebSocket ready state: 0=CONNECTING, 1=OPEN, 2=CLOSING, 3=CLOSED
static int32_t godot_icall_WebSocket_GetState() {
	if (!_ws_global_peer) return 3;
	return (int32_t)_ws_global_peer->get_ready_state();
}

// Send a text message. Returns Error code (0 = OK)
static int32_t godot_icall_WebSocket_SendText(MonoString *text) {
	if (!_ws_global_peer) return -1;

	char *utf8 = text ? mono_string_to_utf8(text) : nullptr;
	if (!utf8) return -1;

	String msg(utf8);
	mono_free(utf8);

	Error err = _ws_global_peer->send_text(msg);
	if (err == OK) _ws_send_count++;
	return (int32_t)err;
}

// Send prefixed text: builds "prefix<count>" in C++ (no string ops in C#).
// Increments internal send counter automatically. Returns Error code.
static int32_t godot_icall_WebSocket_SendPrefixedInt(MonoString *prefix, int32_t value) {
	if (!_ws_global_peer) return -1;

	char *utf8 = prefix ? mono_string_to_utf8(prefix) : nullptr;
	char buf[32];
	snprintf(buf, sizeof(buf), "%d", value);
	String msg;
	if (utf8) {
		msg = String(utf8) + String(buf);
		mono_free(utf8);
	} else {
		msg = String(buf);
	}

	Error err = _ws_global_peer->send_text(msg);
	if (err == OK) _ws_send_count++;
	printf("[Mono] WS send_prefixed_int => %s (err=%d)\n", msg.utf8().get_data(), (int)err);
	fflush(stdout);
	return (int32_t)err;
}

// Get send/recv counts (safe - returns int only, no string).
static int32_t godot_icall_WebSocket_GetSendCount() {
	return _ws_send_count;
}
static int32_t godot_icall_WebSocket_GetRecvCount() {
	return _ws_recv_count;
}

// Append last received message to debug UI (all done in C++ - no string to C#).
static void godot_icall_WebSocket_ShowLastMessage() {
	if (_ws_last_message.is_empty()) return;
	_g_debug_lines.append(_ws_last_message);
	while (_g_debug_lines.size() > MAX_DEBUG_LINES) {
		_g_debug_lines.remove_at(0);
	}
	_refresh_debug_label();
}

// Get the number of available packets (messages)
static int32_t godot_icall_WebSocket_GetPacketCount() {
	if (!_ws_global_peer) return 0;
	return _ws_global_peer->get_available_packet_count();
}

// Close the WebSocket connection
static void godot_icall_WebSocket_Close() {
	if (!_ws_global_peer) return;
	_ws_global_peer->close();
}

void godot_register_icalls() {
	// All internalcalls are declared in Godot.Bridge (matching our compiled GodotSharp.dll)
	mono_add_internal_call("Godot.Bridge::godot_icall_GD_Print", (const void *)godot_icall_GD_Print);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Free", (const void *)godot_icall_Object_Free);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Get", (const void *)godot_icall_Object_Get);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Set", (const void *)godot_icall_Object_Set);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Call", (const void *)godot_icall_Object_Call);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Ctor", (const void *)godot_icall_Object_Ctor);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_BindNativePtr", (const void *)godot_icall_Object_BindNativePtr);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_IsInstanceValid", (const void *)godot_icall_Object_IsInstanceValid);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Connect", (const void *)godot_icall_Object_Connect);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Disconnect", (const void *)godot_icall_Object_Disconnect);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_IsConnected", (const void *)godot_icall_Object_IsConnected);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_EmitSignal", (const void *)godot_icall_Object_EmitSignal);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_HasSignal", (const void *)godot_icall_Object_HasSignal);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_InstantiateFromNative", (const void *)godot_icall_Object_InstantiateFromNative);
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
	mono_add_internal_call("Godot.Bridge::godot_icall_ResourceLoader_Load", (const void *)godot_icall_ResourceLoader_Load);
	mono_add_internal_call("Godot.Bridge::godot_icall_PackedScene_Instantiate", (const void *)godot_icall_PackedScene_Instantiate);
	mono_add_internal_call("Godot.Bridge::godot_icall_Platform_GetRuntimeInfo", (const void *)godot_icall_Platform_GetRuntimeInfo);
	mono_add_internal_call("Godot.Bridge::godot_icall_Input_IsKeyPressed", (const void *)godot_icall_Input_IsKeyPressed);
	mono_add_internal_call("Godot.Bridge::godot_icall_Input_IsMouseButtonPressed", (const void *)godot_icall_Input_IsMouseButtonPressed);
	mono_add_internal_call("Godot.Bridge::godot_icall_Input_GetMousePosition", (const void *)godot_icall_Input_GetMousePosition);

	// WASM-safe icalls (bypass Mono WASM interpreter string/int bugs)
	mono_add_internal_call("Godot.Bridge::godot_icall_Int_ToString", (const void *)godot_icall_Int_ToString);
	mono_add_internal_call("Godot.Bridge::godot_icall_String_ConcatInt", (const void *)godot_icall_String_ConcatInt);
	mono_add_internal_call("Godot.Bridge::godot_icall_Label_SetFpsText", (const void *)godot_icall_Label_SetFpsText);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_SetIntText", (const void *)godot_icall_Object_SetIntText);
	mono_add_internal_call("Godot.Bridge::godot_icall_Engine_GetFps", (const void *)godot_icall_Engine_GetFps);
	mono_add_internal_call("Godot.Bridge::godot_icall_Label_SetPrefixedInt", (const void *)godot_icall_Label_SetPrefixedInt);
	mono_add_internal_call("Godot.Bridge::godot_icall_Label_AppendLog", (const void *)godot_icall_Label_AppendLog);
	mono_add_internal_call("Godot.Bridge::godot_icall_Control_SetPosition", (const void *)godot_icall_Control_SetPosition);

	// WebSocket icalls (global pointer model - no pointer passing through icall boundary)
	mono_add_internal_call("Godot.Bridge::godot_icall_WebSocket_Init", (const void *)godot_icall_WebSocket_Init);
	mono_add_internal_call("Godot.Bridge::godot_icall_WebSocket_PollAndGetState", (const void *)godot_icall_WebSocket_PollAndGetState);
	mono_add_internal_call("Godot.Bridge::godot_icall_WebSocket_Poll", (const void *)godot_icall_WebSocket_Poll);
	mono_add_internal_call("Godot.Bridge::godot_icall_WebSocket_GetState", (const void *)godot_icall_WebSocket_GetState);
	mono_add_internal_call("Godot.Bridge::godot_icall_WebSocket_SendText", (const void *)godot_icall_WebSocket_SendText);
	mono_add_internal_call("Godot.Bridge::godot_icall_WebSocket_SendPrefixedInt", (const void *)godot_icall_WebSocket_SendPrefixedInt);
	mono_add_internal_call("Godot.Bridge::godot_icall_WebSocket_GetSendCount", (const void *)godot_icall_WebSocket_GetSendCount);
	mono_add_internal_call("Godot.Bridge::godot_icall_WebSocket_GetRecvCount", (const void *)godot_icall_WebSocket_GetRecvCount);
	mono_add_internal_call("Godot.Bridge::godot_icall_WebSocket_ShowLastMessage", (const void *)godot_icall_WebSocket_ShowLastMessage);
	mono_add_internal_call("Godot.Bridge::godot_icall_WebSocket_GetPacketCount", (const void *)godot_icall_WebSocket_GetPacketCount);
	mono_add_internal_call("Godot.Bridge::godot_icall_WebSocket_Close", (const void *)godot_icall_WebSocket_Close);

	// Debug UI icalls (global pointer model - no pointer passing, no C# string ops)
	mono_add_internal_call("Godot.Bridge::godot_icall_DebugUi_Init", (const void *)godot_icall_DebugUi_Init);
	mono_add_internal_call("Godot.Bridge::godot_icall_DebugUi_Clear", (const void *)godot_icall_DebugUi_Clear);
	mono_add_internal_call("Godot.Bridge::godot_icall_DebugUi_AddLine", (const void *)godot_icall_DebugUi_AddLine);
	mono_add_internal_call("Godot.Bridge::godot_icall_DebugUi_AddLineInt", (const void *)godot_icall_DebugUi_AddLineInt);

	printf("[Mono] Registered all internal calls (Godot.Bridge::*).\n");
	fflush(stdout);
}
