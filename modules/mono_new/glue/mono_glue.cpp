#include "mono_glue.h"
#include "../utils/mono_logger.h"
#include "core/os/os.h"
#include "core/math/math_funcs.h"
#include <mono/mono-publib.h>
#include <cstring>

extern "C" {
char *mono_string_to_utf8(MonoString *s);
void mono_free(void *ptr);
void mono_add_internal_call(const char *name, const void *method);
}

static void godot_icall_GD_Print(MonoString *msg) {
	if (msg) {
		char *utf8 = mono_string_to_utf8(msg);
		if (utf8) {
			MonoLogger::log(String("C#: ") + String::utf8(utf8));
			mono_free(utf8);
		}
	}
}

static void godot_icall_GD_PrintErr(MonoString *msg) {
	if (msg) {
		char *utf8 = mono_string_to_utf8(msg);
		if (utf8) {
			MonoLogger::log_error(String("C# Error: ") + String::utf8(utf8));
			mono_free(utf8);
		}
	}
}

static int64_t godot_icall_GD_Randi() {
	return (int64_t)Math::rand();
}

static double godot_icall_GD_Randf() {
	return Math::randd();
}

static mono_bool godot_icall_Input_IsKeyPressed(int64_t key) {
	return false;
}

static mono_bool godot_icall_Input_IsActionPressed(MonoString *action) {
	return false;
}

static void godot_icall_Object_EmitSignal(MonoObject *nativePtr, MonoString *signal, MonoArray *args) {
}

void mono_glue_register_icalls() {
	mono_add_internal_call("Godot.GD::godot_icall_GD_Print", (const void *)godot_icall_GD_Print);
	mono_add_internal_call("Godot.GD::godot_icall_GD_PrintErr", (const void *)godot_icall_GD_PrintErr);
	mono_add_internal_call("Godot.GD::godot_icall_GD_Randi", (const void *)godot_icall_GD_Randi);
	mono_add_internal_call("Godot.GD::godot_icall_GD_Randf", (const void *)godot_icall_GD_Randf);
	mono_add_internal_call("Godot.Input::godot_icall_Input_IsKeyPressed", (const void *)godot_icall_Input_IsKeyPressed);
	mono_add_internal_call("Godot.Input::godot_icall_Input_IsActionPressed", (const void *)godot_icall_Input_IsActionPressed);
	mono_add_internal_call("Godot.GodotObject::godot_icall_Object_EmitSignal", (const void *)godot_icall_Object_EmitSignal);

	MonoLogger::log("Registered C# internal calls");
}

void mono_glue_init() {
    mono_glue_register_icalls();
}
