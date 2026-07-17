// ICall bindings for Node2D - direct C++ method calls (faster than Object::callp).
// All float-returning icalls return int64 bit-patterns to comply with the WASM
// interpreter's do_icall signature constraints (no direct double returns).
// The C# side uses DoubleLongUnion to reinterpret the bits back to double.

#include "../../mono_gd/interop/gd_mono_interop_variant.h"
#include "../../utils/mono_logger.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#include "scene/main/node.h"
#include "scene/2d/node_2d.h"
#include <mono/mono-publib.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
char *mono_string_to_utf8(MonoString *s);
void mono_free(void *ptr);
}

namespace {

// Helper: reinterpret float bits as int64 (safe across WASM boundary).
static inline int64_t float_to_int64_bits(float v) {
	int32_t i32;
	memcpy(&i32, &v, sizeof(i32));
	return (int64_t)i32;
}

// Helper: reinterpret double bits as int64 (safe across WASM boundary).
static inline int64_t double_to_int64_bits(double v) {
	int64_t i64;
	memcpy(&i64, &v, sizeof(i64));
	return i64;
}

// Node2D::set_position(Vector2)
// C# passes x and y as int32 bit-patterns (FloatIntUnion) to avoid float params.
static void icall_Node2D_SetPosition(int64_t p_node, int32_t x_bits, int32_t y_bits) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return;
	float x, y;
	memcpy(&x, &x_bits, sizeof(float));
	memcpy(&y, &y_bits, sizeof(float));
	node->set_position(Vector2(x, y));
}

// Node2D::get_position() -> returns x_bits in low 32 bits, y_bits in high 32 bits.
// Encoded as int64 to keep the icall signature all-integer (WASM-safe).
static int64_t icall_Node2D_GetPosition(int64_t p_node) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return 0;
	Vector2 pos = node->get_position();
	int32_t x_bits, y_bits;
	memcpy(&x_bits, &pos.x, sizeof(float));
	memcpy(&y_bits, &pos.y, sizeof(float));
	return ((int64_t)y_bits << 32) | (uint32_t)x_bits;
}

// Node2D::set_rotation(float) - takes int32 bit-pattern
static void icall_Node2D_SetRotation(int64_t p_node, int32_t rot_bits) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return;
	float r;
	memcpy(&r, &rot_bits, sizeof(float));
	node->set_rotation(r);
}

// Node2D::get_rotation() -> returns int32 bit-pattern as int64
static int64_t icall_Node2D_GetRotation(int64_t p_node) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return 0;
	float r = node->get_rotation();
	return (int64_t)float_to_int64_bits(r);
}

// Node2D::set_scale(Vector2)
static void icall_Node2D_SetScale(int64_t p_node, int32_t x_bits, int32_t y_bits) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return;
	float x, y;
	memcpy(&x, &x_bits, sizeof(float));
	memcpy(&y, &y_bits, sizeof(float));
	node->set_scale(Vector2(x, y));
}

// Node2D::get_scale() -> packed int64
static int64_t icall_Node2D_GetScale(int64_t p_node) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return 0;
	Vector2 s = node->get_scale();
	int32_t x_bits, y_bits;
	memcpy(&x_bits, &s.x, sizeof(float));
	memcpy(&y_bits, &s.y, sizeof(float));
	return ((int64_t)y_bits << 32) | (uint32_t)x_bits;
}

// Node2D::set_global_position(Vector2)
static void icall_Node2D_SetGlobalPosition(int64_t p_node, int32_t x_bits, int32_t y_bits) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return;
	float x, y;
	memcpy(&x, &x_bits, sizeof(float));
	memcpy(&y, &y_bits, sizeof(float));
	node->set_global_position(Vector2(x, y));
}

// Node2D::get_global_position() -> packed int64
static int64_t icall_Node2D_GetGlobalPosition(int64_t p_node) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return 0;
	Vector2 pos = node->get_global_position();
	int32_t x_bits, y_bits;
	memcpy(&x_bits, &pos.x, sizeof(float));
	memcpy(&y_bits, &pos.y, sizeof(float));
	return ((int64_t)y_bits << 32) | (uint32_t)x_bits;
}

// Node2D::set_global_rotation(float)
static void icall_Node2D_SetGlobalRotation(int64_t p_node, int32_t rot_bits) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return;
	float r;
	memcpy(&r, &rot_bits, sizeof(float));
	node->set_global_rotation(r);
}

// Node2D::get_global_rotation() -> int64 bits
static int64_t icall_Node2D_GetGlobalRotation(int64_t p_node) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return 0;
	float r = node->get_global_rotation();
	return (int64_t)float_to_int64_bits(r);
}

// Node2D::set_global_scale(Vector2)
static void icall_Node2D_SetGlobalScale(int64_t p_node, int32_t x_bits, int32_t y_bits) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return;
	float x, y;
	memcpy(&x, &x_bits, sizeof(float));
	memcpy(&y, &y_bits, sizeof(float));
	node->set_global_scale(Vector2(x, y));
}

// Node2D::get_global_scale() -> packed int64
static int64_t icall_Node2D_GetGlobalScale(int64_t p_node) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return 0;
	Vector2 s = node->get_global_scale();
	int32_t x_bits, y_bits;
	memcpy(&x_bits, &s.x, sizeof(float));
	memcpy(&y_bits, &s.y, sizeof(float));
	return ((int64_t)y_bits << 32) | (uint32_t)x_bits;
}

// Node2D::rotate(float) - increment rotation by delta
static void icall_Node2D_Rotate(int64_t p_node, int32_t delta_bits) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return;
	float d;
	memcpy(&d, &delta_bits, sizeof(float));
	node->rotate(d);
}

// Node2D::move_x(float, bool scaled=false)
static void icall_Node2D_MoveLocalX(int64_t p_node, int32_t delta_bits, mono_bool p_scaled) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return;
	float d;
	memcpy(&d, &delta_bits, sizeof(float));
	node->move_x(d, p_scaled != 0);
}

// Node2D::move_y(float, bool scaled=false)
static void icall_Node2D_MoveLocalY(int64_t p_node, int32_t delta_bits, mono_bool p_scaled) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return;
	float d;
	memcpy(&d, &delta_bits, sizeof(float));
	node->move_y(d, p_scaled != 0);
}

// Node2D::set_z_index(int)
static void icall_Node2D_SetZIndex(int64_t p_node, int64_t p_z) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return;
	node->set_z_index((int)p_z);
}

// Node2D::get_z_index() -> int64
static int64_t icall_Node2D_GetZIndex(int64_t p_node) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return 0;
	return (int64_t)node->get_z_index();
}

// Node2D::set_z_as_relative(bool)
static void icall_Node2D_SetZAsRelative(int64_t p_node, mono_bool p_enable) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return;
	node->set_z_as_relative(p_enable != 0);
}

// Node2D::is_z_relative() -> bool
static mono_bool icall_Node2D_IsZAsRelative(int64_t p_node) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return false;
	return node->is_z_relative();
}

// Node2D::set_y_sort_enabled(bool)
static void icall_Node2D_SetYSortEnabled(int64_t p_node, mono_bool p_enable) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return;
	node->set_y_sort_enabled(p_enable != 0);
}

// Node2D::is_y_sort_enabled() -> bool
static mono_bool icall_Node2D_IsYSortEnabled(int64_t p_node) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return false;
	return node->is_y_sort_enabled();
}

// Node2D::set_visible(bool)
static void icall_Node2D_SetVisible(int64_t p_node, mono_bool p_visible) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return;
	node->set_visible(p_visible != 0);
}

// Node2D::is_visible() -> bool
static mono_bool icall_Node2D_IsVisible(int64_t p_node) {
	Node2D *node = (Node2D *)(intptr_t)p_node;
	if (!node) return false;
	return node->is_visible();
}

// ClassDB validation: verify Node2D has the methods we bind.
static void scan_and_validate_node2d_methods() {
	const char *essential_methods[] = {
		"set_position", "get_position", "set_rotation", "get_rotation",
		"set_scale", "get_scale", "set_global_position", "get_global_position",
		"set_global_rotation", "get_global_rotation", "set_global_scale", "get_global_scale",
		"rotate", "move_local_x", "move_local_y",
		"set_z_index", "get_z_index", "set_z_as_relative", "is_z_relative",
		"set_y_sort_enabled", "is_y_sort_enabled",
		"set_visible", "is_visible",
		nullptr
	};

	for (int i = 0; essential_methods[i] != nullptr; i++) {
		if (!ClassDB::has_method("Node2D", essential_methods[i])) {
			MonoLogger::log_warning(vformat("ClassDB: Node2D::%s not found", essential_methods[i]));
		}
	}
}

} // anonymous namespace

namespace GDMonoInterop {

void register_node2d_icalls() {
	MonoLogger::log("Registering Node2D icalls (auto-generated)...");
	scan_and_validate_node2d_methods();

	// Position
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_SetPosition", (const void *)icall_Node2D_SetPosition);
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_GetPosition", (const void *)icall_Node2D_GetPosition);
	// Rotation
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_SetRotation", (const void *)icall_Node2D_SetRotation);
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_GetRotation", (const void *)icall_Node2D_GetRotation);
	// Scale
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_SetScale", (const void *)icall_Node2D_SetScale);
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_GetScale", (const void *)icall_Node2D_GetScale);
	// Global Position
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_SetGlobalPosition", (const void *)icall_Node2D_SetGlobalPosition);
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_GetGlobalPosition", (const void *)icall_Node2D_GetGlobalPosition);
	// Global Rotation
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_SetGlobalRotation", (const void *)icall_Node2D_SetGlobalRotation);
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_GetGlobalRotation", (const void *)icall_Node2D_GetGlobalRotation);
	// Global Scale
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_SetGlobalScale", (const void *)icall_Node2D_SetGlobalScale);
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_GetGlobalScale", (const void *)icall_Node2D_GetGlobalScale);
	// Rotate / MoveLocal
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_Rotate", (const void *)icall_Node2D_Rotate);
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_MoveLocalX", (const void *)icall_Node2D_MoveLocalX);
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_MoveLocalY", (const void *)icall_Node2D_MoveLocalY);
	// Z-Index
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_SetZIndex", (const void *)icall_Node2D_SetZIndex);
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_GetZIndex", (const void *)icall_Node2D_GetZIndex);
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_SetZAsRelative", (const void *)icall_Node2D_SetZAsRelative);
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_IsZAsRelative", (const void *)icall_Node2D_IsZAsRelative);
	// Y-Sort
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_SetYSortEnabled", (const void *)icall_Node2D_SetYSortEnabled);
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_IsYSortEnabled", (const void *)icall_Node2D_IsYSortEnabled);
	// Visible
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_SetVisible", (const void *)icall_Node2D_SetVisible);
	mono_add_internal_call("Godot.Node2D::godot_icall_Node2D_IsVisible", (const void *)icall_Node2D_IsVisible);

	MonoLogger::log("Node2D icalls registered (23 methods)");
}

} // namespace GDMonoInterop
