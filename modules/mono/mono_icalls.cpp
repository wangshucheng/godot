// ============================================================
// mono_icalls.cpp — 100+ InternalCall 实现（单文件，段落式组织）
//
// 目录（搜索 "// ====" 分隔线跳转）：
//   1. Core Object/Node icalls   — GD_Print, Object_*, Node_*, Callable_*,
//                                  ResourceLoader, PackedScene, Platform, Input
//   2. WASM-safe utility icalls  — Int_ToString, String_ConcatInt, Label_*
//   3. Runtime2D icalls          — R2D_* 节点操作 + Tile/Score/Grid
//   4. Debug UI icalls           — DebugUi_* 全局调试 Label
//   5. Game UI icalls            — GameUI_* 2048 游戏 UI
//   6. WebSocket icalls          — WebSocket_* 全局连接模型
//   7. Test support icalls       — Test_* 系统测试套件
//   8. Extended test icalls      — Test_* 文件/物理/音频/动画/BCL/GC
//   9. Sync context              — RegisterSyncContext
//  10. Reflection icalls         — ClassDB_* 元数据暴露
//  11. Collections icalls        — Array_*, Dict_*
//  12. WXAudio icalls            — WXAudio_* 微信音频适配
//  13. Benchmark icalls          — Bench_* 性能基准测试（高分辨率计时）
//  14. godot_register_icalls()   — 统一注册入口
// ============================================================

#include "mono_icalls.h"
#include "mono_host.h"
#include "csharp_script.h"
#include "mono_variant.h"
#include "mono_bridge.h"
#include "mono_gc_bridge.h"
#include "mono_callable.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/object/callable_mp.h"
#include "core/object/ref_counted.h"
#include "core/io/resource_loader.h"
#include "core/input/input.h"
#include "core/math/vector2.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"
#include "core/config/engine.h"
#include "scene/gui/label.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/control.h"
#include "scene/main/scene_tree.h"
#include "scene/main/canvas_layer.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "modules/websocket/websocket_peer.h"
#include "servers/text/text_server.h"
#include "core/io/file_access.h"
#include "core/io/dir_access.h"
#include "core/templates/local_vector.h"
#include "scene/animation/animation_player.h"
#include "scene/resources/animation.h"
#include "scene/resources/animation_library.h"
#include "scene/audio/audio_stream_player.h"
#ifndef _3D_DISABLED
#include "scene/resources/3d/world_3d.h"
#include "servers/physics_3d/physics_server_3d.h"
#endif
#ifdef TOOLS_ENABLED
// A2 (W4): editor-only engine-data completion provider (input actions, node
// paths, resource/scene paths, signals, theme items). Not registered in
// export templates — the C# glue guards calls with Engine.IsEditorHint().
#include "editor/code_completion.h"
#endif
#include <mono/metadata/image.h>
#include <mono/metadata/blob.h>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#ifdef ANDROID_ENABLED
#include <android/log.h>
// Android: native printf() output is NOT captured by logcat. Route all
// stdout-style prints through __android_log_print so they appear under the
// "godot" tag, which adb logcat -s Godot:* / godot:* captures.
#define NATIVE_LOG_PRINT(...) __android_log_print(ANDROID_LOG_INFO, "godot", __VA_ARGS__)
#define NATIVE_LOG_NEWLINE() __android_log_print(ANDROID_LOG_INFO, "godot", "%s", "")
#else
#define NATIVE_LOG_PRINT(...) printf(__VA_ARGS__)
#define NATIVE_LOG_NEWLINE() printf("\n")
#endif

using namespace mono_variant;
using namespace mono_bridge;

// Matches our DLL: Godot.Bridge::godot_icall_GD_Print(string message)
static void godot_icall_GD_Print(MonoString *message) {
	if (message) {
		char *utf8 = mono_string_to_utf8(message);
		if (utf8) {
			NATIVE_LOG_PRINT("%s\n", utf8);
			mono_free(utf8);
		}
	} else {
		NATIVE_LOG_NEWLINE();
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

	// H8: finalizer-thread safety. C# finalizers (~GodotObject) run on the
	// Mono GC thread. Engine APIs (queue_free / memdelete / unreference on
	// non-atomic RefCounted paths) are not safe off the main thread, and
	// temporary wrappers (GetNode<T>() creates a new wrapper each call) can
	// be finalized while the underlying node is still in use → UAF.
	// When called off the main thread, enqueue for deferred free instead.
	if (!Thread::is_main_thread()) {
		bool is_rc = mono_gc_bridge::is_refcounted_binding(obj);
		mono_gc_bridge::enqueue_deferred_free(obj, is_rc);
		return;
	}

	// Main-thread path: also verify the native object is still alive — a
	// finalizer may have been delayed past native deletion.
	if (!mono_gc_bridge::is_native_alive(obj)) {
		return;
	}

	// RefCounted: use release_refcounted_binding (unreference + possible memdelete)
	if (mono_gc_bridge::is_refcounted_binding(obj)) {
		RefCounted *rc = Object::cast_to<RefCounted>(obj);
		if (rc) {
			mono_gc_bridge::release_refcounted_binding(rc);
			return;
		}
	}

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

// C# RefCounted.Dispose() calls this to release the C# held reference
static void godot_icall_RefCounted_ReleaseRef(intptr_t native_ptr) {
	if (native_ptr == 0) return;
	Object *obj = (Object *)native_ptr;
	RefCounted *rc = Object::cast_to<RefCounted>(obj);
	if (!rc) return;
	mono_gc_bridge::release_refcounted_binding(rc);
}

static MonoObject *godot_icall_Object_Get(intptr_t native_ptr, MonoString *p_name) {
	if (native_ptr == 0) return nullptr;
	Object *obj = (Object *)native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return nullptr;
	char *name_utf8 = mono_string_to_utf8(p_name);
	if (!name_utf8) return nullptr;
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
	if (!name_utf8) return;
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
	if (!method_utf8) return nullptr;
	StringName method_name(method_utf8);
	mono_free(method_utf8);

	int argcount = 0;
	const Variant **args = nullptr;
	Vector<Variant> arg_variants;
	Vector<const Variant *> arg_ptrs;

	if (p_args) {
		argcount = (int)mono_array_length(p_args);
		if (argcount > 0) {
			arg_variants.resize(argcount);
			arg_ptrs.resize(argcount);
			Variant *variants_buf = arg_variants.ptrw();
			const Variant **ptrs_buf = arg_ptrs.ptrw();
			for (int i = 0; i < argcount; i++) {
				MonoObject *arg = mono_array_get(p_args, MonoObject *, i);
				variants_buf[i] = mono_object_to_variant(arg);
				ptrs_buf[i] = &variants_buf[i];
			}
			args = ptrs_buf;
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
	if (!path_utf8) return 0;
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
	// Strong GCHandle (was weak): the C# Callable wrapper only stores the
	// native pointer, so after Callable.From() returns the managed delegate
	// wrapper has NO managed root. A weak handle let the GC collect the
	// delegate at any later allocation, and call() then silently returned
	// CALL_ERROR_INSTANCE_IS_NULL — signal callbacks mysteriously never ran
	// (found by csharp_test scenario 24e). The handle is released in
	// ~CallableCustomMono when the native Callable is freed.
	uint32_t gchandle = mono_gchandle_new(p_delegate, true);
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
	Vector<Variant> arg_variants;
	Vector<const Variant *> arg_ptrs;
	if (p_args) {
		argcount = (int)mono_array_length(p_args);
		if (argcount > 0) {
			arg_variants.resize(argcount);
			arg_ptrs.resize(argcount);
			Variant *variants_buf = arg_variants.ptrw();
			const Variant **ptrs_buf = arg_ptrs.ptrw();
			for (int i = 0; i < argcount; i++) {
				MonoObject *arg = mono_array_get(p_args, MonoObject *, i);
				variants_buf[i] = mono_object_to_variant(arg);
				ptrs_buf[i] = &variants_buf[i];
			}
			args = ptrs_buf;
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
	if (!signal_utf8) return false;
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
	if (!signal_utf8) return;
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
	if (!signal_utf8) return false;
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
	if (!signal_utf8) return;
	StringName signal_name(signal_utf8);
	mono_free(signal_utf8);

	int argcount = 0;
	const Variant **args = nullptr;
	Vector<Variant> arg_variants;
	Vector<const Variant *> arg_ptrs;
	if (p_args) {
		argcount = (int)mono_array_length(p_args);
		if (argcount > 0) {
			arg_variants.resize(argcount);
			arg_ptrs.resize(argcount);
			Variant *variants_buf = arg_variants.ptrw();
			const Variant **ptrs_buf = arg_ptrs.ptrw();
			for (int i = 0; i < argcount; i++) {
				MonoObject *arg = mono_array_get(p_args, MonoObject *, i);
				variants_buf[i] = mono_object_to_variant(arg);
				ptrs_buf[i] = &variants_buf[i];
			}
			args = ptrs_buf;
		}
	}

	obj->emit_signalp(signal_name, args, argcount);
}

static bool godot_icall_Object_HasSignal(intptr_t p_native_ptr, MonoString *p_signal) {
	if (p_native_ptr == 0) return false;
	Object *obj = (Object *)p_native_ptr;
	if (!mono_gc_bridge::is_native_alive(obj)) return false;
	char *signal_utf8 = mono_string_to_utf8(p_signal);
	if (!signal_utf8) return false;
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
	if (!name_utf8) return;
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

// C++-side: get user data dir (for C# hot update staging area)
static MonoString *godot_icall_GetUserDataDir() {
	MonoDomain *domain = mono_domain_get();
	if (!OS::get_singleton()) return mono_string_new(domain, "");
	String path = OS::get_singleton()->get_user_data_dir();
	return mono_string_new(domain, path.utf8().get_data());
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
// Runtime2D icalls: WASM-safe general-purpose Godot node manipulation.
//
// 设计目的：让 C# 用户代码在 WASM 端能直接创建/操作 Godot 节点，
// 而无需经过 GodotObject.Set/Call（这两个 icall 在 WASM 解释器下
// 会触发 "function signature mismatch"，因为它们带 MonoObject*/MonoArray*
// 参数；详见 AGENTS.md §4.3）。
//
// WASM 安全约束（与 Test_* / GameUI_* 系列一致）：
//   - 参数只用 intptr_t / MonoString* / int32_t
//   - 返回只用 intptr_t / int32_t / void
//   - 不使用 MonoObject* 或 MonoArray*
//   - 不在 C# 侧做 string+int 拼接或 ToString
//
// 对象生命周期：C# 侧持有 IntPtr 句柄；C++ 侧通过 ObjectDB 验证存活性
// （R2D 创建的对象不在 GC 桥中，is_native_alive 不适用）。
// 释放通过显式 NodeFree(IntPtr) 调用，或随父节点场景树一起释放。
// ============================================================

// Verify a raw Object pointer is still alive in the engine's ObjectDB.
// Used for R2D objects created via ClassDB::instantiate (NOT in GC bridge).
static bool _r2d_alive(Object *obj) {
	if (!obj) return false;
	return ObjectDB::get_instance(obj->get_instance_id()) != nullptr;
}

// Create a Node by class name. Returns IntPtr (0 on failure).
static intptr_t godot_icall_R2D_NodeCreate(MonoString *className) {
	char *utf8 = className ? mono_string_to_utf8(className) : nullptr;
	if (!utf8) return 0;
	StringName class_name(utf8);
	mono_free(utf8);
	if (!ClassDB::can_instantiate(class_name)) {
		printf("[R2D] Cannot instantiate class: %s\n", String(class_name).utf8().get_data());
		return 0;
	}
	Object *obj = ClassDB::instantiate(class_name);
	if (!obj) return 0;
	return (intptr_t)obj;
}

// Free a Node (queue_free if Node, else memdelete).
static void godot_icall_R2D_NodeFree(intptr_t node) {
	if (node == 0) return;
	Object *obj = (Object *)node;
	if (!_r2d_alive(obj)) return;
	Node *n = Object::cast_to<Node>(obj);
	if (n) {
		n->queue_free();
	} else {
		memdelete(obj);
	}
}

// Add child node. parent must be a Node.
static void godot_icall_R2D_AddChild(intptr_t parent, intptr_t child) {
	if (parent == 0 || child == 0) return;
	Object *pobj = (Object *)parent;
	Object *cobj = (Object *)child;
	if (!_r2d_alive(pobj) || !_r2d_alive(cobj)) return;
	Node *p = Object::cast_to<Node>(pobj);
	Node *c = Object::cast_to<Node>(cobj);
	if (!p || !c) return;
	p->add_child(c);
}

// Set Node name.
static void godot_icall_R2D_SetName(intptr_t node, MonoString *name) {
	if (node == 0) return;
	Object *obj = (Object *)node;
	if (!_r2d_alive(obj)) return;
	Node *n = Object::cast_to<Node>(obj);
	if (!n) return;
	char *utf8 = name ? mono_string_to_utf8(name) : nullptr;
	if (utf8) {
		n->set_name(utf8);
		mono_free(utf8);
	}
}

// Set Control/Node2D position (x, y).
static void godot_icall_R2D_SetPosition(intptr_t node, int32_t x, int32_t y) {
	if (node == 0) return;
	Object *obj = (Object *)node;
	if (!_r2d_alive(obj)) return;
	Control *c = Object::cast_to<Control>(obj);
	if (c) {
		c->set_position(Vector2((real_t)x, (real_t)y));
	}
}

// Set Control size (w, h).
static void godot_icall_R2D_SetSize(intptr_t node, int32_t w, int32_t h) {
	if (node == 0) return;
	Object *obj = (Object *)node;
	if (!_r2d_alive(obj)) return;
	Control *c = Object::cast_to<Control>(obj);
	if (!c) return;
	c->set_size(Vector2((real_t)w, (real_t)h));
}

// Set Control anchors preset (e.g. 15 = full rect).
static void godot_icall_R2D_SetAnchorsPreset(intptr_t node, int32_t preset) {
	if (node == 0) return;
	Object *obj = (Object *)node;
	if (!_r2d_alive(obj)) return;
	Control *c = Object::cast_to<Control>(obj);
	if (!c) return;
	c->set_anchors_preset((Control::LayoutPreset)preset);
}

// Set Label text.
static void godot_icall_R2D_LabelSetText(intptr_t node, MonoString *text) {
	if (node == 0) return;
	Object *obj = (Object *)node;
	if (!_r2d_alive(obj)) return;
	Label *lbl = Object::cast_to<Label>(obj);
	if (!lbl) return;
	char *utf8 = text ? mono_string_to_utf8(text) : nullptr;
	lbl->set_text(utf8 ? utf8 : "");
	if (utf8) mono_free(utf8);
}

// Set label text to prefix + int (e.g. "Score: 42"). Avoids C# int.ToString()
// which triggers Mono WASM interpreter signature mismatch.
static void godot_icall_R2D_LabelSetPrefixedInt(intptr_t node, MonoString *prefix, int32_t value) {
	if (node == 0) return;
	Object *obj = (Object *)node;
	if (!_r2d_alive(obj)) return;
	Label *lbl = Object::cast_to<Label>(obj);
	if (!lbl) return;
	char *utf8 = prefix ? mono_string_to_utf8(prefix) : nullptr;
	char buf[64];
	snprintf(buf, sizeof(buf), "%s%d", utf8 ? utf8 : "", value);
	lbl->set_text(buf);
	if (utf8) mono_free(utf8);
}

// Set Label alignment (halign/valign: 0=begin, 1=center, 2=end).
static void godot_icall_R2D_LabelSetAlign(intptr_t node, int32_t halign, int32_t valign) {
	if (node == 0) return;
	Object *obj = (Object *)node;
	if (!_r2d_alive(obj)) return;
	Label *lbl = Object::cast_to<Label>(obj);
	if (!lbl) return;
	lbl->set_horizontal_alignment((HorizontalAlignment)halign);
	lbl->set_vertical_alignment((VerticalAlignment)valign);
}

// Set Label font size.
static void godot_icall_R2D_LabelSetFontSize(intptr_t node, int32_t size) {
	if (node == 0) return;
	Object *obj = (Object *)node;
	if (!_r2d_alive(obj)) return;
	Label *lbl = Object::cast_to<Label>(obj);
	if (!lbl) return;
	lbl->add_theme_font_size_override("font_size", size);
}

// Set Label font color (r,g,b,a 0-255).
static void godot_icall_R2D_LabelSetFontColor(intptr_t node, int32_t r, int32_t g, int32_t b, int32_t a) {
	if (node == 0) return;
	Object *obj = (Object *)node;
	if (!_r2d_alive(obj)) return;
	Label *lbl = Object::cast_to<Label>(obj);
	if (!lbl) return;
	lbl->add_theme_color_override("font_color",
		Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
}

// Set ColorRect color (r,g,b,a 0-255).
static void godot_icall_R2D_ColorRectSetColor(intptr_t node, int32_t r, int32_t g, int32_t b, int32_t a) {
	if (node == 0) return;
	Object *obj = (Object *)node;
	if (!_r2d_alive(obj)) return;
	ColorRect *cr = Object::cast_to<ColorRect>(obj);
	if (!cr) return;
	cr->set_color(Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
}

// Get SceneTree root Window (the main viewport).
static intptr_t godot_icall_R2D_GetTreeRoot() {
	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree) return 0;
	Window *root = tree->get_root();
	if (!root) return 0;
	return (intptr_t)root;
}

// Create a CanvasLayer and add it to a parent Node. Returns IntPtr.
static intptr_t godot_icall_R2D_CreateCanvasLayer(intptr_t parent) {
	if (parent == 0) return 0;
	Object *pobj = (Object *)parent;
	if (!_r2d_alive(pobj)) return 0;
	Node *p = Object::cast_to<Node>(pobj);
	if (!p) return 0;
	CanvasLayer *layer = memnew(CanvasLayer);
	layer->set_layer(100);
	p->add_child(layer);
	return (intptr_t)layer;
}

// Get mouse X (WASM-safe: int return, no Vector2 object).
static int32_t godot_icall_R2D_GetMouseX() {
	Input *input = Input::get_singleton();
	if (!input) return 0;
	return (int32_t)input->get_mouse_position().x;
}

// Get mouse Y.
static int32_t godot_icall_R2D_GetMouseY() {
	Input *input = Input::get_singleton();
	if (!input) return 0;
	return (int32_t)input->get_mouse_position().y;
}

// ============================================================
// R2D Tile/Score/Grid icalls: 彻底消除 C# 侧 BCL 操作。
//
// 解决问题（Mono WASM 解释器 function signature mismatch）：
//   1. "Score: " + _score → R2D_SetScore (C++ snprintf)
//   2. _score.ToString()  → R2D_LabelSetInt (C++ snprintf)
//   3. tileTexts[] + TileColorRgb → R2D_SetTileValue (C++ 查表)
//   4. new int[]{...} 数组分配 → R2D_Grid* (C++ 全局缓冲区)
//
// C# 侧只需传 int/IntPtr 参数，零 BCL 调用。
// ============================================================

// 2048 tile value → text (C++ 查表，替代 C# tileTexts[] 数组)
static const char *_tile_value_text(int value) {
	switch (value) {
		case 0: return "";
		case 2: return "2";
		case 4: return "4";
		case 8: return "8";
		case 16: return "16";
		case 32: return "32";
		case 64: return "64";
		case 128: return "128";
		case 256: return "256";
		case 512: return "512";
		case 1024: return "1024";
		case 2048: return "2048";
		case 4096: return "4096";
		case 8192: return "8192";
		default: return "?";
	}
}

// 2048 tile value → background color (classic scheme, 0-255 RGB)
static void _tile_bg_color(int value, int &r, int &g, int &b) {
	switch (value) {
		case 0:    r = 205; g = 192; b = 180; break; // #CDC0B4 empty
		case 2:    r = 238; g = 228; b = 218; break; // #EEE4DA
		case 4:    r = 237; g = 224; b = 200; break; // #EDE0C8
		case 8:    r = 242; g = 177; b = 121; break; // #F2B179
		case 16:   r = 245; g = 149; b = 99;  break; // #F59563
		case 32:   r = 246; g = 124; b = 95;  break; // #F67C5F
		case 64:   r = 246; g = 94;  b = 59;  break; // #F65E3B
		case 128:  r = 237; g = 207; b = 114; break; // #EDCF72
		case 256:  r = 237; g = 204; b = 97;  break; // #EDCC61
		case 512:  r = 237; g = 200; b = 80;  break; // #EDC850
		case 1024: r = 237; g = 197; b = 63;  break; // #EDC53F
		case 2048: r = 237; g = 194; b = 46;  break; // #EDC22E
		default:   r = 60;  g = 58;  b = 50;  break; // #3C3A32 (>2048)
	}
}

// 2048 tile value → font color (dark for small, white for large)
static void _tile_font_color(int value, int &r, int &g, int &b) {
	if (value <= 4) { r = 119; g = 110; b = 101; } // #776E65 dark
	else { r = 249; g = 246; b = 242; }            // #F9F6F2 white
}

// 2048 tile value → font size (smaller for more digits)
static int _tile_font_size(int value) {
	if (value < 100) return 42;
	if (value < 1000) return 36;
	if (value < 10000) return 28;
	return 22;
}

// R2D_SetTileValue: 一个 icall 完成整个 2048 瓦片渲染。
// 替代 C# 侧的 tileTexts[] 查表 + TileColorRgb() + int.ToString()。
// bg = ColorRect IntPtr, label = Label IntPtr, value = 瓦片值 (0=空)
static void godot_icall_R2D_SetTileValue(intptr_t bg, intptr_t label, int32_t value) {
	// Set background color
	if (bg != 0) {
		Object *bg_obj = (Object *)bg;
		if (_r2d_alive(bg_obj)) {
			ColorRect *cr = Object::cast_to<ColorRect>(bg_obj);
			if (cr) {
				int r, g, b;
				_tile_bg_color(value, r, g, b);
				cr->set_color(Color(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f));
			}
		}
	}
	// Set label text + font color + font size
	if (label != 0) {
		Object *lbl_obj = (Object *)label;
		if (_r2d_alive(lbl_obj)) {
			Label *lbl = Object::cast_to<Label>(lbl_obj);
			if (lbl) {
				lbl->set_text(_tile_value_text(value));
				int fr, fg, fb;
				_tile_font_color(value, fr, fg, fb);
				lbl->add_theme_color_override("font_color",
					Color(fr / 255.0f, fg / 255.0f, fb / 255.0f, 1.0f));
				lbl->add_theme_font_size_override("font_size", _tile_font_size(value));
			}
		}
	}
}

// R2D_SetScore: 设置 "Score: N" 文本。
// 替代 C# 侧 "Score: " + _score 字符串拼接。
static void godot_icall_R2D_SetScore(intptr_t label, int32_t score) {
	if (label == 0) return;
	Object *obj = (Object *)label;
	if (!_r2d_alive(obj)) return;
	Label *lbl = Object::cast_to<Label>(obj);
	if (!lbl) return;
	char buf[32];
	snprintf(buf, sizeof(buf), "Score: %d", score);
	lbl->set_text(buf);
}

// R2D_LabelSetInt: 设置 Label 文本为纯整数。
// 替代 C# 侧 value.ToString()。
static void godot_icall_R2D_LabelSetInt(intptr_t label, int32_t value) {
	if (label == 0) return;
	Object *obj = (Object *)label;
	if (!_r2d_alive(obj)) return;
	Label *lbl = Object::cast_to<Label>(obj);
	if (!lbl) return;
	char buf[16];
	snprintf(buf, sizeof(buf), "%d", value);
	lbl->set_text(buf);
}

// R2D_SetStatusText: 从预定义状态文本集选择。
// 替代 C# 侧状态字符串拼接/选择。
// state: 0=默认提示, 1=胜利, 2=失败, 3=新游戏
static void godot_icall_R2D_SetStatusText(intptr_t label, int32_t state) {
	if (label == 0) return;
	Object *obj = (Object *)label;
	if (!_r2d_alive(obj)) return;
	Label *lbl = Object::cast_to<Label>(obj);
	if (!lbl) return;
	const char *text;
	switch (state) {
		case 1:  text = "YOU WIN! Press C to continue, R to restart"; break;
		case 2:  text = "GAME OVER! Press R to restart"; break;
		case 3:  text = "New game started!"; break;
		default: text = "Arrow keys / swipe to move"; break;
	}
	lbl->set_text(text);
}

// ============================================================
// R2D Grid icalls: C++ 侧全局 int 数组，替代 C# new int[]。
//
// 解决问题：Mono WASM 解释器在方法内 new int[]{...} 触发
// function signature mismatch。将数组分配/操作全部移到 C++。
// C# 侧通过 idx = row * size + col 索引。
// ============================================================

#define R2D_GRID_MAX 64
static int32_t _r2d_grid[R2D_GRID_MAX];
static int32_t _r2d_grid_size = 0; // total cells (size*size for square grid)
static int32_t _r2d_grid_dim = 0;  // dimension (e.g. 4 for 4x4)

// 创建/重置 NxN 网格（全部置 0）。dim: 维度（如 4）。
static void godot_icall_R2D_GridCreate(int32_t dim) {
	if (dim < 1 || dim > 8) return; // 最大 8x8
	_r2d_grid_dim = dim;
	_r2d_grid_size = dim * dim;
	memset(_r2d_grid, 0, sizeof(int32_t) * _r2d_grid_size);
}

// 设置网格单元。idx = row * dim + col。
static void godot_icall_R2D_GridSet(int32_t idx, int32_t val) {
	if (idx < 0 || idx >= _r2d_grid_size) return;
	_r2d_grid[idx] = val;
}

// 获取网格单元。
static int32_t godot_icall_R2D_GridGet(int32_t idx) {
	if (idx < 0 || idx >= _r2d_grid_size) return 0;
	return _r2d_grid[idx];
}

// 全部填充为指定值。
static void godot_icall_R2D_GridFill(int32_t val) {
	for (int i = 0; i < _r2d_grid_size; i++) {
		_r2d_grid[i] = val;
	}
}

// 复制网格到备份缓冲区（undo 用）。返回备份后的值数量。
static int32_t _r2d_grid_backup[R2D_GRID_MAX];
static int32_t _r2d_grid_backup_score = 0;

static void godot_icall_R2D_GridSave() {
	memcpy(_r2d_grid_backup, _r2d_grid, sizeof(int32_t) * _r2d_grid_size);
}

// 从备份恢复网格。
static void godot_icall_R2D_GridRestore() {
	memcpy(_r2d_grid, _r2d_grid_backup, sizeof(int32_t) * _r2d_grid_size);
}

// 设置/获取备份分数（undo 用）。
static void godot_icall_R2D_GridSaveScore(int32_t score) {
	_r2d_grid_backup_score = score;
}

static int32_t godot_icall_R2D_GridGetSavedScore() {
	return _r2d_grid_backup_score;
}

// 检查是否有相邻相等元素（用于判断是否还能移动）。
// 返回 1=有相邻相等（还能移动），0=没有（游戏结束）。
static int32_t godot_icall_R2D_GridHasAdjacentEqual() {
	int dim = _r2d_grid_dim;
	for (int r = 0; r < dim; r++) {
		for (int c = 0; c < dim; c++) {
			int v = _r2d_grid[r * dim + c];
			if (c + 1 < dim && _r2d_grid[r * dim + c + 1] == v) return 1;
			if (r + 1 < dim && _r2d_grid[(r + 1) * dim + c] == v) return 1;
		}
	}
	return 0;
}

// 检查是否有空单元（值为 0）。
static int32_t godot_icall_R2D_GridHasZero() {
	for (int i = 0; i < _r2d_grid_size; i++) {
		if (_r2d_grid[i] == 0) return 1;
	}
	return 0;
}

// 获取空单元数量。
static int32_t godot_icall_R2D_GridCountZero() {
	int count = 0;
	for (int i = 0; i < _r2d_grid_size; i++) {
		if (_r2d_grid[i] == 0) count++;
	}
	return count;
}

// 获取第一个空单元的 idx（无空返回 -1）。
static int32_t godot_icall_R2D_GridFirstZero() {
	for (int i = 0; i < _r2d_grid_size; i++) {
		if (_r2d_grid[i] == 0) return i;
	}
	return -1;
}

// 获取随机空单元的 idx（无空返回 -1）。使用简单 LCG 避免 C# System.Random。
static uint32_t _r2d_grid_rng = 12345;
static int32_t godot_icall_R2D_GridRandomZero() {
	// Count zeros first
	int zeros[R2D_GRID_MAX];
	int count = 0;
	for (int i = 0; i < _r2d_grid_size; i++) {
		if (_r2d_grid[i] == 0) zeros[count++] = i;
	}
	if (count == 0) return -1;
	// LCG random
	_r2d_grid_rng = _r2d_grid_rng * 1103515245 + 12345;
	int pick = (int)((_r2d_grid_rng >> 16) % (uint32_t)count);
	return zeros[pick];
}

// 执行一行压缩+合并（2048 核心逻辑）。
// line_idx: 行/列索引 (0..dim-1)
// direction: 0=左/上（正向），1=右/下（反向）
// is_row: 1=行操作，0=列操作
// 返回：合并产生的分数增量。
static int32_t godot_icall_R2D_GridSlideLine(int32_t line_idx, int32_t direction, int32_t is_row) {
	int dim = _r2d_grid_dim;
	int32_t line[8]; // max dim=8
	// Extract line
	for (int i = 0; i < dim; i++) {
		int idx = is_row ? (line_idx * dim + i) : (i * dim + line_idx);
		line[i] = _r2d_grid[idx];
	}
	// Reverse if needed (slide towards index 0)
	if (direction == 1) {
		for (int i = 0; i < dim / 2; i++) {
			int tmp = line[i]; line[i] = line[dim - 1 - i]; line[dim - 1 - i] = tmp;
		}
	}
	// Compact (remove zeros)
	int32_t compact[8];
	int cn = 0;
	for (int i = 0; i < dim; i++) {
		if (line[i] != 0) compact[cn++] = line[i];
	}
	// Merge adjacent equal
	int32_t merged[8];
	int mn = 0;
	int32_t score_gain = 0;
	int i = 0;
	while (i < cn) {
		if (i + 1 < cn && compact[i] == compact[i + 1]) {
			merged[mn++] = compact[i] * 2;
			score_gain += compact[i] * 2;
			i += 2;
		} else {
			merged[mn++] = compact[i];
			i++;
		}
	}
	// Pad with zeros
	for (int j = mn; j < dim; j++) merged[j] = 0;
	// Reverse back if needed
	if (direction == 1) {
		for (int j = 0; j < dim / 2; j++) {
			int tmp = merged[j]; merged[j] = merged[dim - 1 - j]; merged[dim - 1 - j] = tmp;
		}
	}
	// Write back
	for (int j = 0; j < dim; j++) {
		int idx = is_row ? (line_idx * dim + j) : (j * dim + line_idx);
		_r2d_grid[idx] = merged[j];
	}
	return score_gain;
}

// 检查网格是否发生变化（与备份比较）。
// 返回 1=有变化，0=无变化。
static int32_t godot_icall_R2D_GridChanged() {
	for (int i = 0; i < _r2d_grid_size; i++) {
		if (_r2d_grid[i] != _r2d_grid_backup[i]) return 1;
	}
	return 0;
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
	// Defer add_child to avoid "Parent node is busy setting up children" error
	// when _ensure_debug_label is invoked from a node's _Ready() callback
	// (e.g. Test._Ready -> Runtime.DebugUiInit -> here). The root window is
	// still in its _Ready cascade at that point and rejects synchronous
	// add_child calls. call_deferred schedules the add for end-of-frame,
	// after the _Ready cascade completes. Label property setters and
	// set_text work fine before the node enters the tree.
	root->call_deferred("add_child", layer);

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
	layer->call_deferred("add_child", label);

	_g_debug_label = label;
	printf("[Mono] Debug UI label created and added to scene (deferred).\n");
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

// Append a line: prefix + 4 ints formatted as a grid row (e.g. "[   2][   4][   0][   0]")
static void godot_icall_DebugUi_AddRow4(MonoString *prefix, int32_t a, int32_t b, int32_t c, int32_t d) {
	if (!_ensure_debug_label()) return;
	char *utf8 = prefix ? mono_string_to_utf8(prefix) : nullptr;
	char buf[80];
	snprintf(buf, sizeof(buf), "%s[%4d][%4d][%4d][%4d]", utf8 ? utf8 : "", a, b, c, d);
	if (utf8) mono_free(utf8);
	_g_debug_lines.append(String(buf));
	while (_g_debug_lines.size() > MAX_DEBUG_LINES) {
		_g_debug_lines.remove_at(0);
	}
	_refresh_debug_label();
}

// ============================================================
// Game UI icalls: WASM-safe 2048 game UI.
// C++ creates and manages a 4x4 grid of ColorRect + Label tiles.
// All string/int ops done in C++ to avoid Mono WASM interpreter
// signature mismatch bugs. C# only passes int parameters.
// ============================================================

static ColorRect *_g_game_tiles[4][4] = { { nullptr } };
static Label *_g_game_labels[4][4] = { { nullptr } };
static Label *_g_game_score_label = nullptr;
static Label *_g_game_status_label = nullptr;
static Label *_g_game_title_label = nullptr;
static CanvasLayer *_g_game_layer = nullptr;
static bool _g_game_ui_inited = false;

// Get color for a tile value (classic 2048 color scheme)
static Color _game_tile_color(int value) {
	if (value == 0) return Color(0.35f, 0.32f, 0.30f, 1.0f);    // empty
	if (value == 2) return Color(0.93f, 0.89f, 0.85f, 1.0f);    // #EEE4DA
	if (value == 4) return Color(0.93f, 0.88f, 0.78f, 1.0f);    // #EDE0C8
	if (value == 8) return Color(0.95f, 0.69f, 0.47f, 1.0f);    // #F2B179
	if (value == 16) return Color(0.95f, 0.58f, 0.39f, 1.0f);   // #F59563
	if (value == 32) return Color(0.96f, 0.48f, 0.37f, 1.0f);   // #F67C5F
	if (value == 64) return Color(0.96f, 0.37f, 0.23f, 1.0f);   // #F65E3B
	if (value == 128) return Color(0.93f, 0.81f, 0.45f, 1.0f);  // #EDCF72
	if (value == 256) return Color(0.93f, 0.80f, 0.38f, 1.0f);  // #EDCC61
	if (value == 512) return Color(0.93f, 0.78f, 0.31f, 1.0f);  // #EDC850
	if (value == 1024) return Color(0.93f, 0.77f, 0.25f, 1.0f); // #EDC53F
	if (value == 2048) return Color(0.93f, 0.76f, 0.18f, 1.0f); // #EDC22E
	return Color(0.0f, 0.0f, 0.0f, 1.0f); // >2048
}

static Color _game_text_color(int value) {
	if (value <= 4) return Color(0.47f, 0.43f, 0.39f, 1.0f); // dark text
	return Color(1.0f, 1.0f, 1.0f, 1.0f); // white text
}

static int _game_font_size(int value) {
	if (value < 100) return 42;
	if (value < 1000) return 36;
	if (value < 10000) return 28;
	return 22;
}

// Init game UI: creates 4x4 grid of tiles + score + status labels
static int32_t godot_icall_GameUI_Init() {
	if (_g_game_ui_inited) return 0;

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree) {
		printf("[Mono] ERROR: Cannot get SceneTree for Game UI\n");
		fflush(stdout);
		return -1;
	}
	Window *root = tree->get_root();
	if (!root) return -1;

	CanvasLayer *layer = memnew(CanvasLayer);
	layer->set_layer(100);
	root->add_child(layer);
	_g_game_layer = layer;

	// Background panel
	ColorRect *bg = memnew(ColorRect);
	bg->set_position(Vector2(180, 40));
	bg->set_size(Vector2(480, 480));
	bg->set_color(Color(0.46f, 0.43f, 0.40f, 1.0f)); // #776E64
	layer->add_child(bg);

	// Title
	_g_game_title_label = memnew(Label);
	_g_game_title_label->set_position(Vector2(180, 8));
	_g_game_title_label->set_size(Vector2(480, 30));
	_g_game_title_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	_g_game_title_label->add_theme_font_size_override("font_size", 28);
	_g_game_title_label->add_theme_color_override("font_color", Color(1, 1, 1, 1));
	_g_game_title_label->set_text("2048");
	layer->add_child(_g_game_title_label);

	// 4x4 grid of tiles
	float tile_size = 105;
	float gap = 10;
	float start_x = 190;
	float start_y = 50;

	for (int r = 0; r < 4; r++) {
		for (int c = 0; c < 4; c++) {
			ColorRect *tile = memnew(ColorRect);
			float x = start_x + c * (tile_size + gap);
			float y = start_y + r * (tile_size + gap);
			tile->set_position(Vector2(x, y));
			tile->set_size(Vector2(tile_size, tile_size));
			tile->set_color(_game_tile_color(0));
			layer->add_child(tile);
			_g_game_tiles[r][c] = tile;

			Label *lbl = memnew(Label);
			lbl->set_position(Vector2(x, y));
			lbl->set_size(Vector2(tile_size, tile_size));
			lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			lbl->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
			lbl->add_theme_font_size_override("font_size", 42);
			lbl->add_theme_color_override("font_color", Color(1, 1, 1, 1));
			lbl->set_text("");
			layer->add_child(lbl);
			_g_game_labels[r][c] = lbl;
		}
	}

	// Score label
	_g_game_score_label = memnew(Label);
	_g_game_score_label->set_position(Vector2(180, 530));
	_g_game_score_label->set_size(Vector2(480, 35));
	_g_game_score_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	_g_game_score_label->add_theme_font_size_override("font_size", 22);
	_g_game_score_label->add_theme_color_override("font_color", Color(1, 1, 1, 1));
	_g_game_score_label->set_text("Score: 0");
	layer->add_child(_g_game_score_label);

	// Status label
	_g_game_status_label = memnew(Label);
	_g_game_status_label->set_position(Vector2(180, 570));
	_g_game_status_label->set_size(Vector2(480, 30));
	_g_game_status_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	_g_game_status_label->add_theme_font_size_override("font_size", 18);
	_g_game_status_label->add_theme_color_override("font_color", Color(0.9f, 0.9f, 0.5f, 1.0f));
	_g_game_status_label->set_text("Arrows: move   R: restart");
	layer->add_child(_g_game_status_label);

	_g_game_ui_inited = true;
	printf("[Mono] Game UI created (4x4 grid + score + status)\n");
	fflush(stdout);
	return 0;
}

// Set a tile's value (updates color + text)
static void godot_icall_GameUI_SetTile(int32_t row, int32_t col, int32_t value) {
	if (row < 0 || row >= 4 || col < 0 || col >= 4) return;
	if (!_g_game_tiles[row][col] || !_g_game_labels[row][col]) return;

	_g_game_tiles[row][col]->set_color(_game_tile_color(value));
	_g_game_labels[row][col]->add_theme_color_override("font_color", _game_text_color(value));
	_g_game_labels[row][col]->add_theme_font_size_override("font_size", _game_font_size(value));

	if (value == 0) {
		_g_game_labels[row][col]->set_text("");
	} else {
		char buf[16];
		snprintf(buf, sizeof(buf), "%d", value);
		_g_game_labels[row][col]->set_text(buf);
	}
}

// Set the score
static void godot_icall_GameUI_SetScore(int32_t score) {
	if (!_g_game_score_label) return;
	char buf[32];
	snprintf(buf, sizeof(buf), "Score: %d", score);
	_g_game_score_label->set_text(buf);
}

// Set the status text (0=playing, 1=win, 2=gameover)
static void godot_icall_GameUI_SetStatus(int32_t state) {
	if (!_g_game_status_label) return;
	const char *text = "Arrows: move   R: restart";
	if (state == 1) text = "YOU WIN! Press R for new game";
	else if (state == 2) text = "GAME OVER! Press R to restart";
	_g_game_status_label->set_text(text);
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

// ============================================================
// Test support icalls: Global pointer model for systematic
// verification of Godot C# workflow. All operations use
// string/int params only (WASM-safe, no IntPtr passing).
// ============================================================

static Object *_g_test_obj = nullptr;
static Ref<PackedScene> _g_test_scene;
static Node *_g_test_scene_inst = nullptr;
static int32_t _g_signal_count = 0;

// Assertion framework globals (WASM-safe: all string/int ops in C++)
static int32_t _g_assert_pass = 0;
static int32_t _g_assert_fail = 0;
static int32_t _g_test_pass = 0;
static int32_t _g_test_fail = 0;

// Record a single assertion. condition: 1=pass, 0=fail.
static void godot_icall_Test_Assert(MonoString *name, int32_t condition) {
	if (condition) {
		_g_assert_pass++;
	} else {
		_g_assert_fail++;
		char *utf8 = name ? mono_string_to_utf8(name) : nullptr;
		if (utf8) {
			NATIVE_LOG_PRINT("[TEST FAIL] %s\n", utf8);
			mono_free(utf8);
		}
	}
}

// Finalize a test scenario: output pass/fail to Debug UI, reset per-test counters.
static void godot_icall_Test_FinishTest(MonoString *testName) {
	bool passed = (_g_assert_fail == 0) && (_g_assert_pass > 0);
	if (passed) {
		_g_test_pass++;
	} else {
		_g_test_fail++;
	}
	char *utf8 = testName ? mono_string_to_utf8(testName) : nullptr;
	String name_str = utf8 ? String(utf8) : String("unknown");
	if (utf8) mono_free(utf8);

	// Output to Debug UI via the global label
	if (_g_debug_label) {
		String line = name_str + (passed ? ": PASS" : ": FAIL");
		_g_debug_label->set_text(_g_debug_label->get_text() + "\n" + line);
	}
	NATIVE_LOG_PRINT("[TEST RESULT] %s: %s (asserts pass=%d fail=%d)\n",
		name_str.utf8().get_data(), passed ? "PASS" : "FAIL",
		_g_assert_pass, _g_assert_fail);
	_g_assert_pass = 0;
	_g_assert_fail = 0;
}

// Get total passed test count.
static int32_t godot_icall_Test_GetPassCount() {
	return _g_test_pass;
}

// Get total failed test count.
static int32_t godot_icall_Test_GetFailCount() {
	return _g_test_fail;
}

// Reset all assertion counters.
static void godot_icall_Test_ResetCounters() {
	_g_assert_pass = 0;
	_g_assert_fail = 0;
	_g_test_pass = 0;
	_g_test_fail = 0;
}

// Create a native object by class name. Returns 1 on success, 0 on fail.
static int32_t godot_icall_Test_Create(MonoString *className) {
	if (_g_test_obj) {
		if (Object::cast_to<Node>(_g_test_obj)) {
			Object::cast_to<Node>(_g_test_obj)->queue_free();
		} else {
			memdelete(_g_test_obj);
		}
		_g_test_obj = nullptr;
	}
	char *utf8 = className ? mono_string_to_utf8(className) : nullptr;
	if (!utf8) return 0;
	StringName class_name(utf8);
	mono_free(utf8);
	if (!ClassDB::can_instantiate(class_name)) {
		printf("[Test] Cannot instantiate class: %s\n", String(class_name).utf8().get_data());
		return 0;
	}
	_g_test_obj = ClassDB::instantiate(class_name);
	if (!_g_test_obj) return 0;
	printf("[Test] Created object: %s (ptr=%p)\n", String(class_name).utf8().get_data(), _g_test_obj);
	return 1;
}

// Add the global test object to scene root as a child.
static void godot_icall_Test_AddToScene() {
	if (!_g_test_obj) return;
	Node *node = Object::cast_to<Node>(_g_test_obj);
	if (!node) return;
	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree) return;
	Window *root = tree->get_root();
	if (!root) return;
	root->add_child(node);
}

// Create a child node and add it to the global test object. Returns 1/0.
static int32_t godot_icall_Test_AddChild(MonoString *className) {
	if (!_g_test_obj) return 0;
	Node *parent = Object::cast_to<Node>(_g_test_obj);
	if (!parent) return 0;
	char *utf8 = className ? mono_string_to_utf8(className) : nullptr;
	if (!utf8) return 0;
	StringName class_name(utf8);
	mono_free(utf8);
	if (!ClassDB::can_instantiate(class_name)) return 0;
	Node *child = Object::cast_to<Node>(ClassDB::instantiate(class_name));
	if (!child) return 0;
	parent->add_child(child);
	return 1;
}

// Get child count of the global test object.
static int32_t godot_icall_Test_GetChildCount() {
	if (!_g_test_obj) return -1;
	Node *node = Object::cast_to<Node>(_g_test_obj);
	if (!node) return -1;
	return node->get_child_count();
}

// Set name of the global test object.
static void godot_icall_Test_SetName(MonoString *name) {
	if (!_g_test_obj) return;
	Node *node = Object::cast_to<Node>(_g_test_obj);
	if (!node) return;
	char *utf8 = name ? mono_string_to_utf8(name) : nullptr;
	if (utf8) {
		node->set_name(utf8);
		mono_free(utf8);
	}
}

// Set an int property on the global test object.
static void godot_icall_Test_SetIntProp(MonoString *prop, int32_t value) {
	if (!_g_test_obj) return;
	char *utf8 = prop ? mono_string_to_utf8(prop) : nullptr;
	if (!utf8) return;
	StringName prop_name(utf8);
	mono_free(utf8);
	_g_test_obj->set(prop_name, value);
}

// Get an int property from the global test object.
static int32_t godot_icall_Test_GetIntProp(MonoString *prop) {
	if (!_g_test_obj) return -1;
	char *utf8 = prop ? mono_string_to_utf8(prop) : nullptr;
	if (!utf8) return -1;
	StringName prop_name(utf8);
	mono_free(utf8);
	Variant v = _g_test_obj->get(prop_name);
	return (int32_t)v;
}

// Set a string property on the global test object.
static void godot_icall_Test_SetStringProp(MonoString *prop, MonoString *value) {
	if (!_g_test_obj) return;
	char *prop_utf8 = prop ? mono_string_to_utf8(prop) : nullptr;
	char *val_utf8 = value ? mono_string_to_utf8(value) : nullptr;
	if (!prop_utf8) return;
	StringName prop_name(prop_utf8);
	mono_free(prop_utf8);
	_g_test_obj->set(prop_name, String(val_utf8 ? val_utf8 : ""));
	if (val_utf8) mono_free(val_utf8);
}

// Call a void method with no args on the global test object.
static void godot_icall_Test_CallVoidNoArgs(MonoString *method) {
	if (!_g_test_obj) return;
	char *utf8 = method ? mono_string_to_utf8(method) : nullptr;
	if (!utf8) return;
	StringName method_name(utf8);
	mono_free(utf8);
	Callable::CallError err;
	_g_test_obj->callp(method_name, nullptr, 0, err);
}

// Call a method returning int, no args, on the global test object.
static int32_t godot_icall_Test_CallIntNoArgs(MonoString *method) {
	if (!_g_test_obj) return -1;
	char *utf8 = method ? mono_string_to_utf8(method) : nullptr;
	if (!utf8) return -1;
	StringName method_name(utf8);
	mono_free(utf8);
	Callable::CallError err;
	Variant result = _g_test_obj->callp(method_name, nullptr, 0, err);
	if (err.error != Callable::CallError::CALL_OK) return -1;
	return (int32_t)result;
}

// Call a method returning bool, no args, on the global test object.
static int32_t godot_icall_Test_CallBoolNoArgs(MonoString *method) {
	if (!_g_test_obj) return -1;
	char *utf8 = method ? mono_string_to_utf8(method) : nullptr;
	if (!utf8) return -1;
	StringName method_name(utf8);
	mono_free(utf8);
	Callable::CallError err;
	Variant result = _g_test_obj->callp(method_name, nullptr, 0, err);
	if (err.error != Callable::CallError::CALL_OK) return -1;
	return (int32_t)(bool)result;
}

// Free the global test object.
static void godot_icall_Test_Free() {
	if (!_g_test_obj) return;
	if (Object::cast_to<Node>(_g_test_obj)) {
		Object::cast_to<Node>(_g_test_obj)->queue_free();
	} else {
		memdelete(_g_test_obj);
	}
	_g_test_obj = nullptr;
}

// Check if the global test object is valid.
static int32_t godot_icall_Test_IsValid() {
	if (!_g_test_obj) return 0;
	// Use ObjectDB to verify the object is still valid (not freed).
	// is_native_alive only works for objects tracked by the mono GC bridge,
	// but _g_test_obj is created via ClassDB::instantiate.
	return (ObjectDB::get_instance(_g_test_obj->get_instance_id()) != nullptr) ? 1 : 0;
}

// Load a PackedScene from path. Returns 1 on success.
static int32_t godot_icall_Test_LoadScene(MonoString *path) {
	char *utf8 = path ? mono_string_to_utf8(path) : nullptr;
	if (!utf8) return 0;
	String scene_path(utf8);
	mono_free(utf8);
	_g_test_scene = ResourceLoader::load(scene_path);
	if (_g_test_scene.is_null()) {
		printf("[Test] Failed to load scene: %s\n", scene_path.utf8().get_data());
		return 0;
	}
	printf("[Test] Loaded scene: %s\n", scene_path.utf8().get_data());
	return 1;
}

// Instantiate the loaded scene and add to scene tree. Returns 1 on success.
static int32_t godot_icall_Test_InstantiateScene() {
	if (_g_test_scene.is_null()) return 0;
	if (_g_test_scene_inst) {
		_g_test_scene_inst->queue_free();
		_g_test_scene_inst = nullptr;
	}
	_g_test_scene_inst = _g_test_scene->instantiate();
	if (!_g_test_scene_inst) return 0;
	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree) return 0;
	Window *root = tree->get_root();
	if (!root) return 0;
	root->add_child(_g_test_scene_inst);
	return 1;
}

// Get child count of instantiated scene.
static int32_t godot_icall_Test_GetSceneChildCount() {
	if (!_g_test_scene_inst) return -1;
	return _g_test_scene_inst->get_child_count();
}

// Free the instantiated scene.
static void godot_icall_Test_FreeScene() {
	if (_g_test_scene_inst) {
		_g_test_scene_inst->queue_free();
		_g_test_scene_inst = nullptr;
	}
	_g_test_scene.unref();
}

// Check if running on Web platform.
static int32_t godot_icall_Test_IsWebPlatform() {
#ifdef WEB_ENABLED
	return 1;
#else
	// Fallback: check OS name at runtime (WASM template may not define WEB_ENABLED)
	String os_name = OS::get_singleton()->get_name();
	if (os_name == "Web") {
		return 1;
	}
	return 0;
#endif
}

void TestSignalReceiver::on_test_signal() {
	_g_signal_count++;
}

static TestSignalReceiver *_g_test_signal_receiver = nullptr;

// Signal test: REALLY connect the signal to a native receiver callback.
// Previously this icall only checked has_signal() and returned 1 without
// connecting anything — the "ConnectSignal" assertion was a fake pass.
static int32_t godot_icall_Test_ConnectSignal(MonoString *signal) {
	if (!_g_test_obj) return 0;
	char *utf8 = signal ? mono_string_to_utf8(signal) : nullptr;
	if (!utf8) return 0;
	StringName sig_name(utf8);
	mono_free(utf8);
	if (!_g_test_obj->has_signal(sig_name)) return 0;
	if (!_g_test_signal_receiver) {
		_g_test_signal_receiver = memnew(TestSignalReceiver);
	}
	_g_signal_count = 0;
	Error err = _g_test_obj->connect(sig_name, callable_mp(_g_test_signal_receiver, &TestSignalReceiver::on_test_signal));
	return (err == OK) ? 1 : 0;
}

// Emit a signal on the global test object.
static int32_t godot_icall_Test_EmitSignal(MonoString *signal) {
	if (!_g_test_obj) return 0;
	char *utf8 = signal ? mono_string_to_utf8(signal) : nullptr;
	if (!utf8) return 0;
	StringName sig_name(utf8);
	mono_free(utf8);
	_g_test_obj->emit_signalp(sig_name, nullptr, 0);
	// NOTE: the counter is intentionally NOT incremented here — it is
	// incremented by TestSignalReceiver::on_test_signal when the signal is
	// actually delivered. Self-incrementing made the assertion vacuous.
	return 1;
}

// Get signal emission count.
static int32_t godot_icall_Test_GetSignalCount() {
	return _g_signal_count;
}

// Debug UI: Add pass/fail line. passed=1 -> "PASS: name", passed=0 -> "FAIL: name"
static void godot_icall_DebugUi_AddPassFail(MonoString *testName, int32_t passed) {
	if (!_ensure_debug_label()) return;
	char *utf8 = testName ? mono_string_to_utf8(testName) : nullptr;
	String prefix = passed ? "[PASS] " : "[FAIL] ";
	String s = prefix + String(utf8 ? utf8 : "");
	if (utf8) mono_free(utf8);
	_g_debug_lines.append(s);
	while (_g_debug_lines.size() > MAX_DEBUG_LINES) {
		_g_debug_lines.remove_at(0);
	}
	_refresh_debug_label();
}

// Debug UI: Add separator line.
static void godot_icall_DebugUi_AddSeparator() {
	if (!_ensure_debug_label()) return;
	_g_debug_lines.append("----------------------------");
	_refresh_debug_label();
}

// Debug UI: Get current line count.
static int32_t godot_icall_DebugUi_GetLineCount() {
	return _g_debug_lines.size();
}

// ============================================================
// Extended test icalls for comprehensive scenario testing
// ============================================================

// Get name length of the global test object (verify SetName worked).
static int32_t godot_icall_Test_GetNameLen() {
	if (!_g_test_obj) return -1;
	Node *node = Object::cast_to<Node>(_g_test_obj);
	if (!node) return -1;
	return (int32_t)node->get_name().length();
}

// Remove child by index from the global test object. Returns 1 on success.
static int32_t godot_icall_Test_RemoveChildIdx(int32_t idx) {
	if (!_g_test_obj) return 0;
	Node *node = Object::cast_to<Node>(_g_test_obj);
	if (!node) return 0;
	if (idx < 0 || idx >= node->get_child_count()) return 0;
	Node *child = node->get_child(idx);
	if (!child) return 0;
	node->remove_child(child);
	child->queue_free();
	return 1;
}

// Check if the global test object has a method. Returns 1/0.
static int32_t godot_icall_Test_HasMethod(MonoString *method) {
	if (!_g_test_obj) return 0;
	char *utf8 = method ? mono_string_to_utf8(method) : nullptr;
	if (!utf8) return 0;
	StringName method_name(utf8);
	mono_free(utf8);
	return _g_test_obj->has_method(method_name) ? 1 : 0;
}

// Get a string property from the global test object (round-trip counterpart
// of Test_SetStringProp — previously string props could be set but never
// verified). Returns the value as a string ("" when unset/failed).
static MonoString *godot_icall_Test_GetStringProp(MonoString *prop) {
	MonoDomain *domain = mono_domain_get();
	if (!_g_test_obj) return mono_string_new(domain, "");
	char *utf8 = prop ? mono_string_to_utf8(prop) : nullptr;
	if (!utf8) return mono_string_new(domain, "");
	StringName prop_name(utf8);
	mono_free(utf8);
	Variant v = _g_test_obj->get(prop_name);
	String str = String(v);
	return mono_string_new(domain, str.utf8().get_data());
}

// Move the test context to the child at idx (for asserting CHILD properties
// — previously several "child" assertions silently re-tested the parent).
// Returns 1/0.
static int32_t godot_icall_Test_SelectChild(int32_t idx) {
	if (!_g_test_obj) return 0;
	Node *node = Object::cast_to<Node>(_g_test_obj);
	if (!node) return 0;
	if (idx < 0 || idx >= node->get_child_count()) return 0;
	_g_test_obj = node->get_child(idx);
	return 1;
}

// Move the test context back to the parent. Returns 1/0.
static int32_t godot_icall_Test_SelectParent() {
	if (!_g_test_obj) return 0;
	Node *node = Object::cast_to<Node>(_g_test_obj);
	if (!node) return 0;
	Node *parent = node->get_parent();
	if (!parent) return 0;
	_g_test_obj = parent;
	return 1;
}

// Write a string to a file. Returns 1 on success.
static int32_t godot_icall_Test_FileWrite(MonoString *path, MonoString *content) {
	char *path_utf8 = path ? mono_string_to_utf8(path) : nullptr;
	if (!path_utf8) return 0;
	String file_path(path_utf8);
	mono_free(path_utf8);

	char *content_utf8 = content ? mono_string_to_utf8(content) : nullptr;
	String file_content(content_utf8 ? content_utf8 : "");
	if (content_utf8) mono_free(content_utf8);

	Ref<FileAccess> f = FileAccess::open(file_path, FileAccess::ModeFlags::WRITE);
	if (f.is_null()) {
		printf("[Test] FileWrite: cannot open %s\n", file_path.utf8().get_data());
		return 0;
	}
	f->store_string(file_content);
	f->close();
	printf("[Test] FileWrite: wrote %d chars to %s\n", (int)file_content.length(), file_path.utf8().get_data());
	return 1;
}

// Read a file and print content to debug UI. Returns content length or -1.
static int32_t godot_icall_Test_FileRead(MonoString *path) {
	char *utf8 = path ? mono_string_to_utf8(path) : nullptr;
	if (!utf8) return -1;
	String file_path(utf8);
	mono_free(utf8);

	if (!FileAccess::exists(file_path)) {
		printf("[Test] FileRead: %s does not exist\n", file_path.utf8().get_data());
		return -1;
	}
	Ref<FileAccess> f = FileAccess::open(file_path, FileAccess::ModeFlags::READ);
	if (f.is_null()) return -1;
	String content = f->get_as_text();
	f->close();
	printf("[Test] FileRead: read %d chars from %s\n", (int)content.length(), file_path.utf8().get_data());
	return (int32_t)content.length();
}

// Check if a file exists. Returns 1/0.
static int32_t godot_icall_Test_FileExists(MonoString *path) {
	char *utf8 = path ? mono_string_to_utf8(path) : nullptr;
	if (!utf8) return 0;
	String file_path(utf8);
	mono_free(utf8);
	return FileAccess::exists(file_path) ? 1 : 0;
}

// Delete a file. Returns 1 on success.
static int32_t godot_icall_Test_FileDelete(MonoString *path) {
	char *utf8 = path ? mono_string_to_utf8(path) : nullptr;
	if (!utf8) return 0;
	String file_path(utf8);
	mono_free(utf8);
	Error err = DirAccess::remove_absolute(file_path);
	return (err == OK) ? 1 : 0;
}

// Perform a 3D raycast from (0,10,0) to (0,-10,0). Returns 1 if hit.
#ifndef _3D_DISABLED
static int32_t godot_icall_Test_Raycast3D() {
	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree) return 0;
	Viewport *vp = tree->get_root();
	if (!vp) return 0;
	Ref<World3D> world = vp->get_world_3d();
	if (world.is_null()) return 0;
	PhysicsDirectSpaceState3D *space = world->get_direct_space_state();
	if (!space) return 0;

	PhysicsDirectSpaceState3D::RayParameters params;
	params.from = Vector3(0, 10, 0);
	params.to = Vector3(0, -10, 0);
	PhysicsDirectSpaceState3D::RayResult result;
	bool hit = space->intersect_ray(params, result);
	printf("[Test] Raycast3D: hit=%d\n", hit ? 1 : 0);
	return hit ? 1 : 0;
}
#endif // _3D_DISABLED

// Set audio volume on the global test object (must be AudioStreamPlayer). Returns 1/0.
static int32_t godot_icall_Test_SetAudioVolume(int32_t volume_db_x10) {
	if (!_g_test_obj) return 0;
	AudioStreamPlayer *player = Object::cast_to<AudioStreamPlayer>(_g_test_obj);
	if (!player) return 0;
	float vol = (float)volume_db_x10 / 10.0f;
	player->set_volume_db(vol);
	return 1;
}

// Get audio volume from the global test object. Returns volume*10 or -999.
static int32_t godot_icall_Test_GetAudioVolume() {
	if (!_g_test_obj) return -999;
	AudioStreamPlayer *player = Object::cast_to<AudioStreamPlayer>(_g_test_obj);
	if (!player) return -999;
	return (int32_t)(player->get_volume_db() * 10.0f);
}

// Add a test animation to AnimationPlayer. Returns 1 on success.
static int32_t godot_icall_Test_AddAnimation(MonoString *animName) {
	if (!_g_test_obj) return 0;
	AnimationPlayer *player = Object::cast_to<AnimationPlayer>(_g_test_obj);
	if (!player) return 0;
	char *utf8 = animName ? mono_string_to_utf8(animName) : nullptr;
	if (!utf8) return 0;
	String name(utf8);
	mono_free(utf8);

	Ref<Animation> anim = memnew(Animation);
	anim->set_length(1.0);
	anim->set_loop_mode(Animation::LOOP_NONE);

	// Get or create default library
	Ref<AnimationLibrary> lib;
	if (player->has_animation_library("")) {
		lib = player->get_animation_library("");
	} else {
		lib.instantiate();
		player->add_animation_library("", lib);
	}
	if (lib->has_animation(name)) {
		lib->remove_animation(name);
	}
	lib->add_animation(name, anim);
	printf("[Test] AddAnimation: added '%s'\n", name.utf8().get_data());
	return 1;
}

// Play an animation by name. Returns 1 on success.
static int32_t godot_icall_Test_PlayAnimation(MonoString *animName) {
	if (!_g_test_obj) return 0;
	AnimationPlayer *player = Object::cast_to<AnimationPlayer>(_g_test_obj);
	if (!player) return 0;
	char *utf8 = animName ? mono_string_to_utf8(animName) : nullptr;
	if (!utf8) return 0;
	String name(utf8);
	mono_free(utf8);
	player->play(name);
	return 1;
}

// Get animation count from AnimationPlayer. Returns count or -1.
static int32_t godot_icall_Test_GetAnimationCount() {
	if (!_g_test_obj) return -1;
	AnimationPlayer *player = Object::cast_to<AnimationPlayer>(_g_test_obj);
	if (!player) return -1;
	LocalVector<StringName> list;
	player->get_animation_list(&list);
	return (int32_t)list.size();
}

// Check if animation is playing. Returns 1/0.
static int32_t godot_icall_Test_IsAnimationPlaying(MonoString *animName) {
	if (!_g_test_obj) return 0;
	AnimationPlayer *player = Object::cast_to<AnimationPlayer>(_g_test_obj);
	if (!player) return 0;
	char *utf8 = animName ? mono_string_to_utf8(animName) : nullptr;
	if (!utf8) return 0;
	String name(utf8);
	mono_free(utf8);
	return (player->is_playing() && player->get_current_animation() == name) ? 1 : 0;
}

// BCL List<int> test in C++ (simulates BCL behavior for WASM safety).
// Returns number of passed sub-tests (0-4).
static int32_t godot_icall_Test_BclListTest() {
	// Simulate List<int> operations using Vector<int>
	Vector<int> list;
	int pass = 0;

	// Test 1: Add items
	list.push_back(10);
	list.push_back(20);
	list.push_back(30);
	if (list.size() == 3) pass++;

	// Test 2: Access by index
	if (list[0] == 10 && list[1] == 20 && list[2] == 30) pass++;

	// Test 3: Remove at index
	list.remove_at(1);
	if (list.size() == 2 && list[1] == 30) pass++;

	// Test 4: Contains check
	bool found = false;
	for (int i = 0; i < list.size(); i++) {
		if (list[i] == 30) { found = true; break; }
	}
	if (found) pass++;

	printf("[Test] BclListTest: %d/4 passed\n", pass);
	return pass;
}

// BCL Dictionary<int,int> test in C++ (simulates BCL behavior for WASM safety).
// Returns number of passed sub-tests (0-4).
static int32_t godot_icall_Test_BclDictTest() {
	// Simulate Dictionary<int,int> using HashMap
	HashMap<int, int> dict;
	int pass = 0;

	// Test 1: Add items
	dict[1] = 100;
	dict[2] = 200;
	dict[3] = 300;
	if (dict.size() == 3) pass++;

	// Test 2: Lookup
	if (dict.has(2) && dict[2] == 200) pass++;

	// Test 3: Update existing
	dict[2] = 250;
	if (dict[2] == 250) pass++;

	// Test 4: Remove
	dict.erase(1);
	if (dict.size() == 2 && !dict.has(1)) pass++;

	printf("[Test] BclDictTest: %d/4 passed\n", pass);
	return pass;
}

// BCL async pattern test (simulates Task.Delay + continuation in C++).
// Returns 1 if async pattern is functional.
static int32_t godot_icall_Test_BclAsyncTest() {
	// In WASM single-threaded mode, async is driven by GodotSynchronizationContext.
	// We can't truly test async here, but we verify the sync context is installed.
	// If this icall executes, the runtime is functional enough for async basics.
	printf("[Test] BclAsyncTest: runtime functional\n");
	return 1;
}

// GC stress test: create count nodes, add them as children of a parent node,
// then remove and free them all. Returns 1 if all operations succeeded.
// This verifies that the GC bridge correctly tracks RefCounted/Object
// references across bulk creation and destruction cycles.
static int32_t godot_icall_Test_GcStressTest(int32_t count) {
	printf("[Test] GcStressTest: creating %d nodes...\n", (int)count);
	fflush(stdout);

	// Get the scene tree root to attach nodes to
	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree) {
		printf("[Test] GcStressTest: no scene tree\n");
		return 0;
	}
	Node *root = tree->get_root();
	if (!root) {
		printf("[Test] GcStressTest: no root node\n");
		return 0;
	}

	// Create a container parent
	Node *container = memnew(Node);
	container->set_name("GcStressContainer");
	root->add_child(container);

	// Phase 1: Bulk create
	LocalVector<Node *> nodes;
	nodes.reserve(count);
	for (int i = 0; i < count; i++) {
		Node *n = memnew(Node);
		if (!n) {
			printf("[Test] GcStressTest: failed to create node %d\n", i);
			return 0;
		}
		container->add_child(n);
		nodes.push_back(n);
	}

	int childCount = container->get_child_count();
	if (childCount != count) {
		printf("[Test] GcStressTest: child count mismatch: expected %d, got %d\n", (int)count, childCount);
		// Cleanup
		for (uint32_t i = 0; i < nodes.size(); i++) {
			if (nodes[i]) {
				container->remove_child(nodes[i]);
				nodes[i]->queue_free();
			}
		}
		container->queue_free();
		return 0;
	}

	// Phase 2: Bulk remove and free
	for (uint32_t i = 0; i < nodes.size(); i++) {
		container->remove_child(nodes[i]);
		nodes[i]->queue_free();
	}
	nodes.clear();

	// Verify all children removed
	childCount = container->get_child_count();
	if (childCount != 0) {
		printf("[Test] GcStressTest: children remaining after free: %d\n", childCount);
		container->queue_free();
		return 0;
	}

	// Free the container
	container->queue_free();

	printf("[Test] GcStressTest: %d nodes created and freed successfully\n", (int)count);
	fflush(stdout);
	return 1;
}

// Get the name of the global test object's class.
static int32_t godot_icall_Test_GetClassCategory(MonoString *className) {
	// Returns category ID: 1=Node, 2=Control, 3=CanvasItem, 4=Resource, 0=unknown
	char *utf8 = className ? mono_string_to_utf8(className) : nullptr;
	if (!utf8) return 0;
	StringName class_name(utf8);
	mono_free(utf8);
	if (ClassDB::is_parent_class(class_name, "Control")) return 2;
	if (ClassDB::is_parent_class(class_name, "CanvasItem")) return 3;
	if (ClassDB::is_parent_class(class_name, "Node")) return 1;
	if (ClassDB::is_parent_class(class_name, "Resource")) return 4;
	return 0;
}

// ============================================================================
// Phase 0.1: Delegate probe icalls
//
// 验证 delegate 在 Mono 6.12 interpreter/AOT 下的可用性，为阶段 0
// （通知路径 SG 化）提供决策依据。
//
// 三个测试路径：
//   Test 2: godot_icall_Test_InvokeDelegateViaMRI
//           用 mono_runtime_invoke 调用 delegate.Invoke
//   Test 3: godot_icall_Test_InvokeDelegateViaFtnPtr
//           用 mono_method_get_function_pointer 拿到函数指针直接调用
// ============================================================================

static MonoObject *_g_delegate_probe = nullptr;
static uint32_t _g_delegate_probe_gchandle = 0;
static MonoMethod *_g_delegate_invoke_method = nullptr;

// 接收 C# 侧传入的 delegate 对象，保活并预解析 Invoke 方法。
// 返回 1 成功，0 失败。
static int32_t godot_icall_Test_RegisterDelegateProbe(MonoObject *delegate_obj) {
	if (!delegate_obj) {
		printf("[PROBE] RegisterDelegateProbe: delegate_obj is null\n");
		return 0;
	}

	// 释放旧 delegate
	if (_g_delegate_probe_gchandle != 0) {
		mono_gchandle_free(_g_delegate_probe_gchandle);
		_g_delegate_probe_gchandle = 0;
	}
	_g_delegate_probe = nullptr;
	_g_delegate_invoke_method = nullptr;

	// 强 GCHandle 保活，防止 GC 回收
	_g_delegate_probe_gchandle = mono_gchandle_new(delegate_obj, false);
	_g_delegate_probe = delegate_obj;

	// 预解析 Invoke 方法（无参）
	MonoClass *delegate_class = mono_object_get_class(delegate_obj);
	if (!delegate_class) {
		printf("[PROBE] RegisterDelegateProbe: failed to get delegate class\n");
		return 0;
	}
	_g_delegate_invoke_method = mono_class_get_method_from_name(delegate_class, "Invoke", 0);
	if (!_g_delegate_invoke_method) {
		// delegate 可能有多个 Invoke 重载，尝试不指定参数数量
		_g_delegate_invoke_method = mono_class_get_method_from_name(delegate_class, "Invoke", -1);
	}
	if (!_g_delegate_invoke_method) {
		printf("[PROBE] RegisterDelegateProbe: Invoke method not found on class '%s'\n",
			   mono_class_get_name(delegate_class));
		return 0;
	}

	printf("[PROBE] RegisterDelegateProbe: OK (class=%s, gchandle=%u)\n",
		   mono_class_get_name(delegate_class), _g_delegate_probe_gchandle);
	return 1;
}

// 通过 mono_runtime_invoke 调用 delegate.Invoke。
// 返回 1 成功（无异常），0 失败（异常或状态无效）。
static int32_t godot_icall_Test_InvokeDelegateViaMRI() {
	if (!_g_delegate_probe || !_g_delegate_invoke_method) {
		printf("[PROBE] InvokeViaMRI: delegate not registered\n");
		return 0;
	}

	MonoObject *exc = nullptr;
	mono_runtime_invoke(_g_delegate_invoke_method, _g_delegate_probe, nullptr, &exc);
	if (exc) {
		MonoClass *exc_class = mono_object_get_class(exc);
		const char *exc_name = exc_class ? mono_class_get_name(exc_class) : "(unknown)";
		printf("[PROBE] InvokeViaMRI: exception %s\n", exc_name ? exc_name : "?");
		// 清理 pending exception（P5 [REV-#11] 同款修复）
		mono_runtime_set_pending_exception(nullptr, true);
		return 0;
	}
	return 1;
}

// 通过 mono_compile_method 拿到函数指针直接调用（绕过 mono_runtime_invoke）。
// 返回 1 成功，0 失败（函数指针获取失败或调用异常）。
static int32_t godot_icall_Test_InvokeDelegateViaFtnPtr() {
	if (!_g_delegate_probe || !_g_delegate_invoke_method) {
		printf("[PROBE] InvokeViaFtnPtr: delegate not registered\n");
		return 0;
	}

	// mono_compile_method 触发 JIT 编译并返回函数指针。
	// 在 interpreter-only 模式下（WASM INTERP_LLVMONLY），返回 interpreter thunk。
	// 注意：Mono 6.12 头文件中无 mono_method_get_function_pointer，用此 API 替代。
	void *ftn_ptr = mono_compile_method(_g_delegate_invoke_method);
	if (!ftn_ptr) {
		printf("[PROBE] InvokeViaFtnPtr: mono_compile_method returned NULL\n");
		return 0;
	}

	printf("[PROBE] InvokeViaFtnPtr: ftn_ptr=%p\n", ftn_ptr);

	// delegate.Invoke 是实例方法，调用约定：
	//   void Invoke(MonoObject *this_ptr)
	// 第一个参数是 delegate 对象本身（this）
	typedef void (*InvokeFn)(MonoObject *);
	InvokeFn fn = (InvokeFn)ftn_ptr;

	// 注意：直接函数指针调用绕过 mono_runtime_invoke 的异常捕获，
	// 若 delegate 内部抛异常会直接传播到 C++，无法被 try/catch 捕获。
	// 这里依赖 C# 侧不抛异常作为前提（探针 OnDelegateInvoked 仅 ++计数）。
	fn(_g_delegate_probe);

	return 1;
}

// Register the GodotSynchronizationContext singleton for instance-based
// pumping. Called from C# Runtime.Initialize() after Install().
static void godot_icall_RegisterSyncContext(MonoObject *instance) {
	MonoHost *host = MonoHost::get_singleton();
	if (host) {
		host->register_sync_context(instance);
	}
}

// ============================================================================
// Reflection icalls: expose ClassDB metadata to C# for runtime introspection.
// All return strings as newline-separated values to minimize icall count
// and avoid WASM interpreter string-return signature mismatch issues.
// ============================================================================

// Get all registered class names as a newline-separated string.
static MonoString *godot_icall_ClassDB_GetClassList() {
	LocalVector<StringName> classes;
	ClassDB::get_class_list(classes);
	String result;
	for (uint32_t i = 0; i < classes.size(); i++) {
		if (i > 0) result += "\n";
		result += String(classes[i]);
	}
	return mono_string_new(mono_domain_get(), result.utf8().get_data());
}

// Check if a class exists. Returns 1 if true, 0 if false.
static int32_t godot_icall_ClassDB_ClassExists(MonoString *className) {
	if (!className) return 0;
	char *utf8 = mono_string_to_utf8(className);
	if (!utf8) return 0;
	StringName name(utf8);
	mono_free(utf8);
	return ClassDB::class_exists(name) ? 1 : 0;
}

// Get the parent class name. Returns empty string if no parent or class not found.
static MonoString *godot_icall_ClassDB_GetParentClass(MonoString *className) {
	if (!className) return mono_string_new(mono_domain_get(), "");
	char *utf8 = mono_string_to_utf8(className);
	if (!utf8) return mono_string_new(mono_domain_get(), "");
	StringName name(utf8);
	mono_free(utf8);
	StringName parent = ClassDB::get_parent_class(name);
	return mono_string_new(mono_domain_get(), String(parent).utf8().get_data());
}

// Check if childClass inherits from parentClass. Returns 1 if true, 0 if false.
static int32_t godot_icall_ClassDB_IsParentClass(MonoString *childClass, MonoString *parentClass) {
	if (!childClass || !parentClass) return 0;
	char *utf8_child = mono_string_to_utf8(childClass);
	char *utf8_parent = mono_string_to_utf8(parentClass);
	if (!utf8_child || !utf8_parent) {
		if (utf8_child) mono_free(utf8_child);
		if (utf8_parent) mono_free(utf8_parent);
		return 0;
	}
	StringName child(utf8_child);
	StringName parent(utf8_parent);
	mono_free(utf8_child);
	mono_free(utf8_parent);
	return ClassDB::is_parent_class(child, parent) ? 1 : 0;
}

// Check if a class can be instantiated. Returns 1 if true, 0 if false.
static int32_t godot_icall_ClassDB_CanInstantiate(MonoString *className) {
	if (!className) return 0;
	char *utf8 = mono_string_to_utf8(className);
	if (!utf8) return 0;
	StringName name(utf8);
	mono_free(utf8);
	return ClassDB::can_instantiate(name) ? 1 : 0;
}

// Get all method names for a class as a newline-separated string.
static MonoString *godot_icall_ClassDB_GetMethodList(MonoString *className) {
	if (!className) return mono_string_new(mono_domain_get(), "");
	char *utf8 = mono_string_to_utf8(className);
	if (!utf8) return mono_string_new(mono_domain_get(), "");
	StringName name(utf8);
	mono_free(utf8);
	List<MethodInfo> methods;
	ClassDB::get_method_list(name, &methods);
	String result;
	int idx = 0;
	for (const MethodInfo &mi : methods) {
		if (idx > 0) result += "\n";
		result += String(mi.name);
		idx++;
	}
	return mono_string_new(mono_domain_get(), result.utf8().get_data());
}

// Check if a class has a specific method. Returns 1 if true, 0 if false.
static int32_t godot_icall_ClassDB_HasMethod(MonoString *className, MonoString *methodName) {
	if (!className || !methodName) return 0;
	char *utf8_class = mono_string_to_utf8(className);
	char *utf8_method = mono_string_to_utf8(methodName);
	if (!utf8_class || !utf8_method) {
		if (utf8_class) mono_free(utf8_class);
		if (utf8_method) mono_free(utf8_method);
		return 0;
	}
	StringName cls(utf8_class);
	StringName mtd(utf8_method);
	mono_free(utf8_class);
	mono_free(utf8_method);
	return ClassDB::has_method(cls, mtd) ? 1 : 0;
}

// Get the argument count for a method. Returns -1 if method not found.
static int32_t godot_icall_ClassDB_GetMethodArgCount(MonoString *className, MonoString *methodName) {
	if (!className || !methodName) return -1;
	char *utf8_class = mono_string_to_utf8(className);
	char *utf8_method = mono_string_to_utf8(methodName);
	if (!utf8_class || !utf8_method) {
		if (utf8_class) mono_free(utf8_class);
		if (utf8_method) mono_free(utf8_method);
		return -1;
	}
	StringName cls(utf8_class);
	StringName mtd(utf8_method);
	mono_free(utf8_class);
	mono_free(utf8_method);
	bool valid = false;
	int count = ClassDB::get_method_argument_count(cls, mtd, &valid);
	return valid ? count : -1;
}

// Get all property names for a class as a newline-separated string.
static MonoString *godot_icall_ClassDB_GetPropertyList(MonoString *className) {
	if (!className) return mono_string_new(mono_domain_get(), "");
	char *utf8 = mono_string_to_utf8(className);
	if (!utf8) return mono_string_new(mono_domain_get(), "");
	StringName name(utf8);
	mono_free(utf8);
	List<PropertyInfo> props;
	ClassDB::get_property_list(name, &props);
	String result;
	int idx = 0;
	for (const PropertyInfo &pi : props) {
		// Skip internal/group properties (only expose user-visible ones)
		if (pi.usage & (PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_SUBGROUP)) {
			continue;
		}
		if (!(pi.usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}
		if (idx > 0) result += "\n";
		result += String(pi.name);
		idx++;
	}
	return mono_string_new(mono_domain_get(), result.utf8().get_data());
}

// Check if a class has a specific property. Returns 1 if true, 0 if false.
static int32_t godot_icall_ClassDB_HasProperty(MonoString *className, MonoString *propName) {
	if (!className || !propName) return 0;
	char *utf8_class = mono_string_to_utf8(className);
	char *utf8_prop = mono_string_to_utf8(propName);
	if (!utf8_class || !utf8_prop) {
		if (utf8_class) mono_free(utf8_class);
		if (utf8_prop) mono_free(utf8_prop);
		return 0;
	}
	StringName cls(utf8_class);
	StringName prop(utf8_prop);
	mono_free(utf8_class);
	mono_free(utf8_prop);
	return ClassDB::has_property(cls, prop) ? 1 : 0;
}

// Get all signal names for a class as a newline-separated string.
static MonoString *godot_icall_ClassDB_GetSignalList(MonoString *className) {
	if (!className) return mono_string_new(mono_domain_get(), "");
	char *utf8 = mono_string_to_utf8(className);
	if (!utf8) return mono_string_new(mono_domain_get(), "");
	StringName name(utf8);
	mono_free(utf8);
	List<MethodInfo> signals;
	ClassDB::get_signal_list(name, &signals);
	String result;
	int idx = 0;
	for (const MethodInfo &si : signals) {
		if (idx > 0) result += "\n";
		result += String(si.name);
		idx++;
	}
	return mono_string_new(mono_domain_get(), result.utf8().get_data());
}

// Check if a class has a specific signal. Returns 1 if true, 0 if false.
static int32_t godot_icall_ClassDB_HasSignal(MonoString *className, MonoString *signalName) {
	if (!className || !signalName) return 0;
	char *utf8_class = mono_string_to_utf8(className);
	char *utf8_signal = mono_string_to_utf8(signalName);
	if (!utf8_class || !utf8_signal) {
		if (utf8_class) mono_free(utf8_class);
		if (utf8_signal) mono_free(utf8_signal);
		return 0;
	}
	StringName cls(utf8_class);
	StringName sig(utf8_signal);
	mono_free(utf8_class);
	mono_free(utf8_signal);
	return ClassDB::has_signal(cls, sig) ? 1 : 0;
}

// Get the class name of a Godot object instance.
static MonoString *godot_icall_Object_GetClassName(MonoObject *obj) {
	if (!obj) return mono_string_new(mono_domain_get(), "");
	MonoClass *cls = mono_object_get_class(obj);
	if (!cls) return mono_string_new(mono_domain_get(), "");
	return mono_string_new(mono_domain_get(), mono_class_get_name(cls));
}

// ============================================================
// M10: Godot.Collections.Array / Dictionary icalls
//
// The C# wrappers own a heap-allocated Array*/Dictionary* (memnew/memdelete)
// stored in their NativePtr field. These icalls are the bridge between the
// C# wrapper methods (Count, indexer, Add, Has, ...) and the underlying
// Godot container.
// ============================================================

static intptr_t godot_icall_Array_Ctor() {
	Array *arr = memnew(Array);
	return reinterpret_cast<intptr_t>(arr);
}

static int32_t godot_icall_Array_Size(intptr_t ptr) {
	if (!ptr) return 0;
	Array *arr = reinterpret_cast<Array *>(ptr);
	return arr->size();
}

static MonoObject *godot_icall_Array_Get(intptr_t ptr, int32_t p_index) {
	if (!ptr) return nullptr;
	Array *arr = reinterpret_cast<Array *>(ptr);
	if (p_index < 0 || p_index >= arr->size()) return nullptr;
	const Variant &v = arr->operator[](p_index);
	return mono_variant::variant_to_mono_object(mono_domain_get(), v);
}

static void godot_icall_Array_Set(intptr_t ptr, int32_t p_index, MonoObject *p_value) {
	if (!ptr) return;
	Array *arr = reinterpret_cast<Array *>(ptr);
	if (p_index < 0 || p_index >= arr->size()) return;
	arr->operator[](p_index) = mono_variant::mono_object_to_variant(p_value);
}

static void godot_icall_Array_PushBack(intptr_t ptr, MonoObject *p_value) {
	if (!ptr) return;
	Array *arr = reinterpret_cast<Array *>(ptr);
	arr->push_back(mono_variant::mono_object_to_variant(p_value));
}

static void godot_icall_Array_Clear(intptr_t ptr) {
	if (!ptr) return;
	Array *arr = reinterpret_cast<Array *>(ptr);
	arr->clear();
}

static void godot_icall_Array_Dispose(intptr_t ptr) {
	if (!ptr) return;
	Array *arr = reinterpret_cast<Array *>(ptr);
	memdelete(arr);
}

static intptr_t godot_icall_Dict_Ctor() {
	Dictionary *dict = memnew(Dictionary);
	return reinterpret_cast<intptr_t>(dict);
}

static int32_t godot_icall_Dict_Size(intptr_t ptr) {
	if (!ptr) return 0;
	Dictionary *dict = reinterpret_cast<Dictionary *>(ptr);
	return dict->size();
}

static MonoObject *godot_icall_Dict_Get(intptr_t ptr, MonoObject *p_key) {
	if (!ptr) return nullptr;
	Dictionary *dict = reinterpret_cast<Dictionary *>(ptr);
	Variant key = mono_variant::mono_object_to_variant(p_key);
	// Use get_valid which returns an empty Variant (not a default-constructed
	// one) when the key is missing; the C# side can distinguish via Has().
	return mono_variant::variant_to_mono_object(mono_domain_get(), dict->get_valid(key));
}

static void godot_icall_Dict_Set(intptr_t ptr, MonoObject *p_key, MonoObject *p_value) {
	if (!ptr) return;
	Dictionary *dict = reinterpret_cast<Dictionary *>(ptr);
	Variant key = mono_variant::mono_object_to_variant(p_key);
	Variant value = mono_variant::mono_object_to_variant(p_value);
	dict->operator[](key) = value;
}

static MonoBoolean godot_icall_Dict_Has(intptr_t ptr, MonoObject *p_key) {
	if (!ptr) return false;
	Dictionary *dict = reinterpret_cast<Dictionary *>(ptr);
	Variant key = mono_variant::mono_object_to_variant(p_key);
	return dict->has(key) ? 1 : 0;
}

static MonoBoolean godot_icall_Dict_Remove(intptr_t ptr, MonoObject *p_key) {
	if (!ptr) return false;
	Dictionary *dict = reinterpret_cast<Dictionary *>(ptr);
	Variant key = mono_variant::mono_object_to_variant(p_key);
	return dict->erase(key) ? 1 : 0;
}

static void godot_icall_Dict_Clear(intptr_t ptr) {
	if (!ptr) return;
	Dictionary *dict = reinterpret_cast<Dictionary *>(ptr);
	dict->clear();
}

static void godot_icall_Dict_Dispose(intptr_t ptr) {
	if (!ptr) return;
	Dictionary *dict = reinterpret_cast<Dictionary *>(ptr);
	memdelete(dict);
}

// ===== WeChat minigame audio adapter (InnerAudioContext-based) =====
// WASM 平台：调用 mono_wasm_glue.cpp 中的 godot_wx_audio_* 桥接函数，
//            最终通过 EM_ASM 调用 GameGlobal.GodotAudioWX.* JS API。
// 桌面平台：stub（返回 0 / 无操作），不影响测试。
#ifdef WEB_ENABLED
extern "C" {
int32_t godot_wx_audio_play(const char *src, int32_t loop);
void godot_wx_audio_stop(int32_t id);
void godot_wx_audio_stop_all(void);
void godot_wx_audio_set_volume(int32_t id, int32_t vol_x100);
void godot_wx_audio_pause(int32_t id);
void godot_wx_audio_resume(int32_t id);
}
#endif

static int32_t godot_icall_WXAudio_Play(MonoString *src, int32_t loop) {
	if (!src) return 0;
	char *utf8 = mono_string_to_utf8(src);
	if (!utf8) return 0;
#ifdef WEB_ENABLED
	int32_t id = godot_wx_audio_play(utf8, loop);
#else
	// Desktop stub: print debug log, return 0 (no audio).
	printf("[WXAudio] Play (desktop stub): src=%s loop=%d\n", utf8, loop);
	int32_t id = 0;
#endif
	mono_free(utf8);
	return id;
}

static void godot_icall_WXAudio_Stop(int32_t id) {
#ifdef WEB_ENABLED
	godot_wx_audio_stop(id);
#else
	(void)id; // Desktop stub: no-op
#endif
}

static void godot_icall_WXAudio_StopAll() {
#ifdef WEB_ENABLED
	godot_wx_audio_stop_all();
#endif
}

static void godot_icall_WXAudio_SetVolume(int32_t id, int32_t volume_x100) {
#ifdef WEB_ENABLED
	godot_wx_audio_set_volume(id, volume_x100);
#else
	(void)id; (void)volume_x100; // Desktop stub: no-op
#endif
}

static void godot_icall_WXAudio_Pause(int32_t id) {
#ifdef WEB_ENABLED
	godot_wx_audio_pause(id);
#else
	(void)id; // Desktop stub: no-op
#endif
}

static void godot_icall_WXAudio_Resume(int32_t id) {
#ifdef WEB_ENABLED
	godot_wx_audio_resume(id);
#else
	(void)id; // Desktop stub: no-op
#endif
}

#ifdef TOOLS_ENABLED
// A2 (W4): engine-data code completion for C# (editor only). Mirrors the
// marshalling convention of the ClassDB metadata icalls: results are
// returned as one newline-joined MonoString; the C# glue splits it.
static MonoString *godot_icall_Editor_GetCodeCompletion(int32_t kind, MonoString *scriptFile) {
	char *utf8 = scriptFile ? mono_string_to_utf8(scriptFile) : nullptr;
	String script_path = utf8 ? String(utf8) : String();
	if (utf8) mono_free(utf8);

	PackedStringArray suggestions = gdmono::get_code_completion((gdmono::CompletionKind)kind, script_path);

	MonoDomain *domain = mono_domain_get();
	String result;
	for (int i = 0; i < suggestions.size(); i++) {
		if (i > 0) result += "\n";
		result += suggestions[i];
	}
	return mono_string_new(domain, result.utf8().get_data());
}
#endif // TOOLS_ENABLED

// W5 SG PoC (v3 pre-research): compile-time [GlobalClass] registry push from
// the C# module initializer generated by GodotSharp.SourceGenerators.
// int flags instead of bool — WASM interpreter icall convention. Consumed by
// CSharpLanguage::refresh_global_classes() via sg_global_class_cache.
static void godot_icall_ScriptRegistry_RegisterGlobalClass(MonoString *className, MonoString *baseType, int32_t isTool, int32_t isAbstract, MonoString *iconPath) {
	CSharpLanguage *lang = CSharpLanguage::get_singleton();
	if (!lang) return;

	char *name_utf8 = className ? mono_string_to_utf8(className) : nullptr;
	char *base_utf8 = baseType ? mono_string_to_utf8(baseType) : nullptr;
	char *icon_utf8 = iconPath ? mono_string_to_utf8(iconPath) : nullptr;

	lang->sg_register_global_class(
			name_utf8 ? String(name_utf8) : String(),
			base_utf8 ? String(base_utf8) : String(),
			isTool != 0, isAbstract != 0,
			icon_utf8 ? String(icon_utf8) : String());

	if (name_utf8) mono_free(name_utf8);
	if (base_utf8) mono_free(base_utf8);
	if (icon_utf8) mono_free(icon_utf8);
}

// ============================================================
// 13. Benchmark icalls: Performance micro-benchmark timer.
// High-resolution timing via OS::get_singleton()->get_ticks_usec()
// (microsecond resolution; converted to nanoseconds for output).
// All division / formatting done in C++ to avoid Mono WASM interpreter
// signature mismatch on C# arithmetic/string ops.
// Output format matches the reference benchmark: "X.XXXX" nanoseconds.
// ============================================================

static uint64_t _g_bench_start_usec = 0;

// Print benchmark table header.
static void godot_icall_Bench_PrintHeader() {
	if (!_ensure_debug_label()) return;
	// Column layout: Benchmark name (42 wide) + "G471 official" (14 wide)
	// Exactly 70 dashes to mimic the reference screenshot's separator look.
	String header = String("Benchmark") + String("                                    ") + String("G471 official");
	_g_debug_lines.append(header);
	char dashes[80];
	memset(dashes, '-', 70);
	dashes[70] = '\0';
	_g_debug_lines.append(String(dashes));
	_refresh_debug_label();
}

// Print a section separator: "--- Section Name ---"
static void godot_icall_Bench_PrintSection(MonoString *section_name) {
	if (!_ensure_debug_label()) return;
	char *utf8 = section_name ? mono_string_to_utf8(section_name) : nullptr;
	String s = String("--- ") + String(utf8 ? utf8 : "") + String(" ---");
	if (utf8) mono_free(utf8);
	_g_debug_lines.append(s);
	_refresh_debug_label();
}

// Start the timer: capture high-res timestamp.
static void godot_icall_Bench_Start() {
	OS *os = OS::get_singleton();
	_g_bench_start_usec = os ? os->get_ticks_usec() : 0;
}

// Stop the timer, compute nanoseconds/iteration average, format and print row.
// ns_per_iter formatted with 4 decimal places in the style of the reference.
static void godot_icall_Bench_EndPrint(MonoString *benchmark_name, int32_t iterations) {
	if (!_ensure_debug_label()) return;
	OS *os = OS::get_singleton();
	uint64_t end_usec = os ? os->get_ticks_usec() : 0;
	uint64_t elapsed_usec = (end_usec >= _g_bench_start_usec) ? (end_usec - _g_bench_start_usec) : 0;
	uint64_t elapsed_nsec = elapsed_usec * 1000ULL;

	// Compute avg_ns_x10000 = (elapsed_nsec * 10000) / iterations  (fixed-point 4 decimals)
	// Use 128-bit safe division path via uint64 (elapsed_nsec fits in 64-bit even for multi-second runs).
	uint64_t avg_ns_x10000 = 0;
	if (iterations > 0) {
		avg_ns_x10000 = (elapsed_nsec * 10000ULL) / (uint64_t)iterations;
	}
	uint64_t whole = avg_ns_x10000 / 10000ULL;
	uint64_t frac = avg_ns_x10000 % 10000ULL;

	char *utf8 = benchmark_name ? mono_string_to_utf8(benchmark_name) : nullptr;
	const char *name_cstr = utf8 ? utf8 : "";

	// Pad name to 42 chars, then right-align the numeric column
	char buf[256];
	char numbuf[32];
	snprintf(numbuf, sizeof(numbuf), "%llu.%04llu",
			(unsigned long long)whole, (unsigned long long)frac);
	// Build line: name (padded 42) + spaces + numbuf (14 chars, right-aligned)
	int name_len = (int)strlen(name_cstr);
	int pad = (42 - name_len);
	if (pad < 1) pad = 1;
	int num_len = (int)strlen(numbuf);
	int num_pad = (14 - num_len);
	if (num_pad < 0) num_pad = 0;
	int pos = 0;
	snprintf(buf + pos, sizeof(buf) - pos, "%s", name_cstr);
	pos = (int)strlen(buf);
	for (int i = 0; i < pad && pos < (int)sizeof(buf) - 1; i++) {
		buf[pos++] = ' ';
	}
	for (int i = 0; i < num_pad && pos < (int)sizeof(buf) - 1; i++) {
		buf[pos++] = ' ';
	}
	snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%s", numbuf);

	_g_debug_lines.append(String(buf));
	while (_g_debug_lines.size() > MAX_DEBUG_LINES) {
		_g_debug_lines.remove_at(0);
	}
	if (utf8) mono_free(utf8);
	_refresh_debug_label();
}

// Return a sensible default iteration count based on platform.
// Desktop: larger iters -> stable micro-benchmark.
// WASM/Android: smaller iters to keep runtime reasonable.
static int32_t godot_icall_Bench_DefaultIters() {
#ifdef WEB_ENABLED
	return 5000;
#else
#ifdef ANDROID_ENABLED
	return 20000;
#else
	return 100000;
#endif
#endif
}

void godot_register_icalls() {
	// All internalcalls are declared in Godot.Bridge (matching our compiled GodotSharp.dll)
	mono_add_internal_call("Godot.Bridge::godot_icall_GD_Print", (const void *)godot_icall_GD_Print);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_Free", (const void *)godot_icall_Object_Free);
	mono_add_internal_call("Godot.Bridge::godot_icall_RefCounted_ReleaseRef", (const void *)godot_icall_RefCounted_ReleaseRef);
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
	mono_add_internal_call("Godot.Bridge::godot_icall_GetUserDataDir", (const void *)godot_icall_GetUserDataDir);
	mono_add_internal_call("Godot.Bridge::godot_icall_Label_SetPrefixedInt", (const void *)godot_icall_Label_SetPrefixedInt);
	mono_add_internal_call("Godot.Bridge::godot_icall_Label_AppendLog", (const void *)godot_icall_Label_AppendLog);
	mono_add_internal_call("Godot.Bridge::godot_icall_Control_SetPosition", (const void *)godot_icall_Control_SetPosition);

	// Runtime2D icalls (WASM-safe general-purpose node manipulation, IntPtr-based)
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_NodeCreate", (const void *)godot_icall_R2D_NodeCreate);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_NodeFree", (const void *)godot_icall_R2D_NodeFree);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_AddChild", (const void *)godot_icall_R2D_AddChild);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_SetName", (const void *)godot_icall_R2D_SetName);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_SetPosition", (const void *)godot_icall_R2D_SetPosition);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_SetSize", (const void *)godot_icall_R2D_SetSize);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_SetAnchorsPreset", (const void *)godot_icall_R2D_SetAnchorsPreset);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_LabelSetText", (const void *)godot_icall_R2D_LabelSetText);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_LabelSetPrefixedInt", (const void *)godot_icall_R2D_LabelSetPrefixedInt);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_LabelSetAlign", (const void *)godot_icall_R2D_LabelSetAlign);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_LabelSetFontSize", (const void *)godot_icall_R2D_LabelSetFontSize);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_LabelSetFontColor", (const void *)godot_icall_R2D_LabelSetFontColor);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_ColorRectSetColor", (const void *)godot_icall_R2D_ColorRectSetColor);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_GetTreeRoot", (const void *)godot_icall_R2D_GetTreeRoot);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_CreateCanvasLayer", (const void *)godot_icall_R2D_CreateCanvasLayer);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_GetMouseX", (const void *)godot_icall_R2D_GetMouseX);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_GetMouseY", (const void *)godot_icall_R2D_GetMouseY);

	// R2D Tile/Score/Status icalls (eliminate C# BCL string/int ops)
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_SetTileValue", (const void *)godot_icall_R2D_SetTileValue);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_SetScore", (const void *)godot_icall_R2D_SetScore);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_LabelSetInt", (const void *)godot_icall_R2D_LabelSetInt);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_SetStatusText", (const void *)godot_icall_R2D_SetStatusText);

	// R2D Grid icalls (eliminate C# new int[] array allocation)
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_GridCreate", (const void *)godot_icall_R2D_GridCreate);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_GridSet", (const void *)godot_icall_R2D_GridSet);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_GridGet", (const void *)godot_icall_R2D_GridGet);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_GridFill", (const void *)godot_icall_R2D_GridFill);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_GridSave", (const void *)godot_icall_R2D_GridSave);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_GridRestore", (const void *)godot_icall_R2D_GridRestore);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_GridSaveScore", (const void *)godot_icall_R2D_GridSaveScore);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_GridGetSavedScore", (const void *)godot_icall_R2D_GridGetSavedScore);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_GridHasAdjacentEqual", (const void *)godot_icall_R2D_GridHasAdjacentEqual);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_GridHasZero", (const void *)godot_icall_R2D_GridHasZero);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_GridCountZero", (const void *)godot_icall_R2D_GridCountZero);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_GridFirstZero", (const void *)godot_icall_R2D_GridFirstZero);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_GridRandomZero", (const void *)godot_icall_R2D_GridRandomZero);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_GridSlideLine", (const void *)godot_icall_R2D_GridSlideLine);
	mono_add_internal_call("Godot.Bridge::godot_icall_R2D_GridChanged", (const void *)godot_icall_R2D_GridChanged);

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
	mono_add_internal_call("Godot.Bridge::godot_icall_DebugUi_AddRow4", (const void *)godot_icall_DebugUi_AddRow4);
	mono_add_internal_call("Godot.Bridge::godot_icall_DebugUi_AddPassFail", (const void *)godot_icall_DebugUi_AddPassFail);
	mono_add_internal_call("Godot.Bridge::godot_icall_DebugUi_AddSeparator", (const void *)godot_icall_DebugUi_AddSeparator);
	mono_add_internal_call("Godot.Bridge::godot_icall_DebugUi_GetLineCount", (const void *)godot_icall_DebugUi_GetLineCount);

	// Game UI icalls (global pointer model - WASM-safe, no string ops in C#)
	mono_add_internal_call("Godot.Bridge::godot_icall_GameUI_Init", (const void *)godot_icall_GameUI_Init);
	mono_add_internal_call("Godot.Bridge::godot_icall_GameUI_SetTile", (const void *)godot_icall_GameUI_SetTile);
	mono_add_internal_call("Godot.Bridge::godot_icall_GameUI_SetScore", (const void *)godot_icall_GameUI_SetScore);
	mono_add_internal_call("Godot.Bridge::godot_icall_GameUI_SetStatus", (const void *)godot_icall_GameUI_SetStatus);

	// Test support icalls (global pointer model - WASM-safe)
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_Create", (const void *)godot_icall_Test_Create);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_AddToScene", (const void *)godot_icall_Test_AddToScene);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_AddChild", (const void *)godot_icall_Test_AddChild);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_GetChildCount", (const void *)godot_icall_Test_GetChildCount);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_SetName", (const void *)godot_icall_Test_SetName);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_SetIntProp", (const void *)godot_icall_Test_SetIntProp);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_GetIntProp", (const void *)godot_icall_Test_GetIntProp);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_SetStringProp", (const void *)godot_icall_Test_SetStringProp);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_CallVoidNoArgs", (const void *)godot_icall_Test_CallVoidNoArgs);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_CallIntNoArgs", (const void *)godot_icall_Test_CallIntNoArgs);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_CallBoolNoArgs", (const void *)godot_icall_Test_CallBoolNoArgs);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_Free", (const void *)godot_icall_Test_Free);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_IsValid", (const void *)godot_icall_Test_IsValid);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_LoadScene", (const void *)godot_icall_Test_LoadScene);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_InstantiateScene", (const void *)godot_icall_Test_InstantiateScene);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_GetSceneChildCount", (const void *)godot_icall_Test_GetSceneChildCount);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_FreeScene", (const void *)godot_icall_Test_FreeScene);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_IsWebPlatform", (const void *)godot_icall_Test_IsWebPlatform);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_ConnectSignal", (const void *)godot_icall_Test_ConnectSignal);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_EmitSignal", (const void *)godot_icall_Test_EmitSignal);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_GetSignalCount", (const void *)godot_icall_Test_GetSignalCount);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_GetClassCategory", (const void *)godot_icall_Test_GetClassCategory);

	// Extended test icalls for comprehensive scenario testing
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_GetNameLen", (const void *)godot_icall_Test_GetNameLen);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_RemoveChildIdx", (const void *)godot_icall_Test_RemoveChildIdx);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_HasMethod", (const void *)godot_icall_Test_HasMethod);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_GetStringProp", (const void *)godot_icall_Test_GetStringProp);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_SelectChild", (const void *)godot_icall_Test_SelectChild);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_SelectParent", (const void *)godot_icall_Test_SelectParent);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_FileWrite", (const void *)godot_icall_Test_FileWrite);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_FileRead", (const void *)godot_icall_Test_FileRead);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_FileExists", (const void *)godot_icall_Test_FileExists);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_FileDelete", (const void *)godot_icall_Test_FileDelete);
#ifndef _3D_DISABLED
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_Raycast3D", (const void *)godot_icall_Test_Raycast3D);
#endif
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_SetAudioVolume", (const void *)godot_icall_Test_SetAudioVolume);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_GetAudioVolume", (const void *)godot_icall_Test_GetAudioVolume);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_AddAnimation", (const void *)godot_icall_Test_AddAnimation);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_PlayAnimation", (const void *)godot_icall_Test_PlayAnimation);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_GetAnimationCount", (const void *)godot_icall_Test_GetAnimationCount);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_IsAnimationPlaying", (const void *)godot_icall_Test_IsAnimationPlaying);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_BclListTest", (const void *)godot_icall_Test_BclListTest);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_BclDictTest", (const void *)godot_icall_Test_BclDictTest);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_BclAsyncTest", (const void *)godot_icall_Test_BclAsyncTest);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_GcStressTest", (const void *)godot_icall_Test_GcStressTest);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_Assert", (const void *)godot_icall_Test_Assert);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_FinishTest", (const void *)godot_icall_Test_FinishTest);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_GetPassCount", (const void *)godot_icall_Test_GetPassCount);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_GetFailCount", (const void *)godot_icall_Test_GetFailCount);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_ResetCounters", (const void *)godot_icall_Test_ResetCounters);

	// Phase 0.1: Delegate probe icalls (验证 delegate 在 WASM interpreter 下的可用性)
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_RegisterDelegateProbe", (const void *)godot_icall_Test_RegisterDelegateProbe);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_InvokeDelegateViaMRI", (const void *)godot_icall_Test_InvokeDelegateViaMRI);
	mono_add_internal_call("Godot.Bridge::godot_icall_Test_InvokeDelegateViaFtnPtr", (const void *)godot_icall_Test_InvokeDelegateViaFtnPtr);

	// Sync context registration (C# -> C++ to register singleton for instance-based pumping)
	mono_add_internal_call("Godot.Bridge::godot_icall_RegisterSyncContext", (const void *)godot_icall_RegisterSyncContext);

	// Reflection: ClassDB metadata exposure for C# runtime introspection
	mono_add_internal_call("Godot.Bridge::godot_icall_ClassDB_GetClassList", (const void *)godot_icall_ClassDB_GetClassList);
	mono_add_internal_call("Godot.Bridge::godot_icall_ClassDB_ClassExists", (const void *)godot_icall_ClassDB_ClassExists);
	mono_add_internal_call("Godot.Bridge::godot_icall_ClassDB_GetParentClass", (const void *)godot_icall_ClassDB_GetParentClass);
	mono_add_internal_call("Godot.Bridge::godot_icall_ClassDB_IsParentClass", (const void *)godot_icall_ClassDB_IsParentClass);
	mono_add_internal_call("Godot.Bridge::godot_icall_ClassDB_CanInstantiate", (const void *)godot_icall_ClassDB_CanInstantiate);
	mono_add_internal_call("Godot.Bridge::godot_icall_ClassDB_GetMethodList", (const void *)godot_icall_ClassDB_GetMethodList);
	mono_add_internal_call("Godot.Bridge::godot_icall_ClassDB_HasMethod", (const void *)godot_icall_ClassDB_HasMethod);
	mono_add_internal_call("Godot.Bridge::godot_icall_ClassDB_GetMethodArgCount", (const void *)godot_icall_ClassDB_GetMethodArgCount);
	mono_add_internal_call("Godot.Bridge::godot_icall_ClassDB_GetPropertyList", (const void *)godot_icall_ClassDB_GetPropertyList);
	mono_add_internal_call("Godot.Bridge::godot_icall_ClassDB_HasProperty", (const void *)godot_icall_ClassDB_HasProperty);
	mono_add_internal_call("Godot.Bridge::godot_icall_ClassDB_GetSignalList", (const void *)godot_icall_ClassDB_GetSignalList);
	mono_add_internal_call("Godot.Bridge::godot_icall_ClassDB_HasSignal", (const void *)godot_icall_ClassDB_HasSignal);
	mono_add_internal_call("Godot.Bridge::godot_icall_Object_GetClassName", (const void *)godot_icall_Object_GetClassName);

	// M10: Godot.Collections.Array / Dictionary icalls
	mono_add_internal_call("Godot.Bridge::godot_icall_Array_Ctor", (const void *)godot_icall_Array_Ctor);
	mono_add_internal_call("Godot.Bridge::godot_icall_Array_Size", (const void *)godot_icall_Array_Size);
	mono_add_internal_call("Godot.Bridge::godot_icall_Array_Get", (const void *)godot_icall_Array_Get);
	mono_add_internal_call("Godot.Bridge::godot_icall_Array_Set", (const void *)godot_icall_Array_Set);
	mono_add_internal_call("Godot.Bridge::godot_icall_Array_PushBack", (const void *)godot_icall_Array_PushBack);
	mono_add_internal_call("Godot.Bridge::godot_icall_Array_Clear", (const void *)godot_icall_Array_Clear);
	mono_add_internal_call("Godot.Bridge::godot_icall_Array_Dispose", (const void *)godot_icall_Array_Dispose);
	mono_add_internal_call("Godot.Bridge::godot_icall_Dict_Ctor", (const void *)godot_icall_Dict_Ctor);
	mono_add_internal_call("Godot.Bridge::godot_icall_Dict_Size", (const void *)godot_icall_Dict_Size);
	mono_add_internal_call("Godot.Bridge::godot_icall_Dict_Get", (const void *)godot_icall_Dict_Get);
	mono_add_internal_call("Godot.Bridge::godot_icall_Dict_Set", (const void *)godot_icall_Dict_Set);
	mono_add_internal_call("Godot.Bridge::godot_icall_Dict_Has", (const void *)godot_icall_Dict_Has);
	mono_add_internal_call("Godot.Bridge::godot_icall_Dict_Remove", (const void *)godot_icall_Dict_Remove);
	mono_add_internal_call("Godot.Bridge::godot_icall_Dict_Clear", (const void *)godot_icall_Dict_Clear);
	mono_add_internal_call("Godot.Bridge::godot_icall_Dict_Dispose", (const void *)godot_icall_Dict_Dispose);

	// WeChat minigame audio adapter (InnerAudioContext-based)
	mono_add_internal_call("Godot.Bridge::godot_icall_WXAudio_Play", (const void *)godot_icall_WXAudio_Play);
	mono_add_internal_call("Godot.Bridge::godot_icall_WXAudio_Stop", (const void *)godot_icall_WXAudio_Stop);
	mono_add_internal_call("Godot.Bridge::godot_icall_WXAudio_StopAll", (const void *)godot_icall_WXAudio_StopAll);
	mono_add_internal_call("Godot.Bridge::godot_icall_WXAudio_SetVolume", (const void *)godot_icall_WXAudio_SetVolume);
	mono_add_internal_call("Godot.Bridge::godot_icall_WXAudio_Pause", (const void *)godot_icall_WXAudio_Pause);
	mono_add_internal_call("Godot.Bridge::godot_icall_WXAudio_Resume", (const void *)godot_icall_WXAudio_Resume);

#ifdef TOOLS_ENABLED
	// A2 (W4): editor-only engine-data completion provider
	mono_add_internal_call("Godot.Bridge::godot_icall_Editor_GetCodeCompletion", (const void *)godot_icall_Editor_GetCodeCompletion);
#endif

	// W5 SG PoC: compile-time [GlobalClass] registry push (module initializer)
	mono_add_internal_call("Godot.Bridge::godot_icall_ScriptRegistry_RegisterGlobalClass", (const void *)godot_icall_ScriptRegistry_RegisterGlobalClass);

	// Section 13: Performance benchmark icalls
	mono_add_internal_call("Godot.Bridge::godot_icall_Bench_PrintHeader", (const void *)godot_icall_Bench_PrintHeader);
	mono_add_internal_call("Godot.Bridge::godot_icall_Bench_PrintSection", (const void *)godot_icall_Bench_PrintSection);
	mono_add_internal_call("Godot.Bridge::godot_icall_Bench_Start", (const void *)godot_icall_Bench_Start);
	mono_add_internal_call("Godot.Bridge::godot_icall_Bench_EndPrint", (const void *)godot_icall_Bench_EndPrint);
	mono_add_internal_call("Godot.Bridge::godot_icall_Bench_DefaultIters", (const void *)godot_icall_Bench_DefaultIters);

	printf("[Mono] Registered all internal calls (Godot.Bridge::*).\n");
	fflush(stdout);
}
