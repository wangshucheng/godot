// ICall bindings for Node3D - direct C++ method calls (faster than Object::callp).
// All float-returning icalls return int64 bit-patterns to comply with the WASM
// interpreter's do_icall signature constraints (no direct double returns).
// The C# side uses FloatIntUnion to reinterpret the bits back to float.
//
// Vector3 (3 floats = 96 bits) does not fit in a single int64, so getters are
// split per-axis: GetPositionX/Y/Z each return an int64 bit-pattern. Setters
// take 3 int32 bit-patterns (x_bits, y_bits, z_bits) in one call.

#include "../../mono_gd/interop/gd_mono_interop_variant.h"
#include "../../utils/mono_logger.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#include "scene/main/node.h"
#include "scene/3d/node_3d.h"
#include <mono/mono-publib.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" {
char *mono_string_to_utf8(MonoString *s);
void mono_free(void *ptr);
}

namespace {

static inline int64_t float_to_int64_bits(float v) {
	int32_t i32;
	memcpy(&i32, &v, sizeof(i32));
	return (int64_t)i32;
}

static inline float int32_bits_to_float(int32_t bits) {
	float f;
	memcpy(&f, &bits, sizeof(float));
	return f;
}

// ============== Position (local) ==============
static void icall_Node3D_SetPosition(int64_t p_node, int32_t x_bits, int32_t y_bits, int32_t z_bits) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return;
	node->set_position(Vector3(int32_bits_to_float(x_bits), int32_bits_to_float(y_bits), int32_bits_to_float(z_bits)));
}
static int64_t icall_Node3D_GetPositionX(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_position().x);
}
static int64_t icall_Node3D_GetPositionY(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_position().y);
}
static int64_t icall_Node3D_GetPositionZ(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_position().z);
}

// ============== Rotation (local, radians) ==============
static void icall_Node3D_SetRotation(int64_t p_node, int32_t x_bits, int32_t y_bits, int32_t z_bits) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return;
	node->set_rotation(Vector3(int32_bits_to_float(x_bits), int32_bits_to_float(y_bits), int32_bits_to_float(z_bits)));
}
static int64_t icall_Node3D_GetRotationX(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_rotation().x);
}
static int64_t icall_Node3D_GetRotationY(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_rotation().y);
}
static int64_t icall_Node3D_GetRotationZ(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_rotation().z);
}

// ============== Scale (local) ==============
static void icall_Node3D_SetScale(int64_t p_node, int32_t x_bits, int32_t y_bits, int32_t z_bits) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return;
	node->set_scale(Vector3(int32_bits_to_float(x_bits), int32_bits_to_float(y_bits), int32_bits_to_float(z_bits)));
}
static int64_t icall_Node3D_GetScaleX(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_scale().x);
}
static int64_t icall_Node3D_GetScaleY(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_scale().y);
}
static int64_t icall_Node3D_GetScaleZ(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_scale().z);
}

// ============== GlobalPosition ==============
static void icall_Node3D_SetGlobalPosition(int64_t p_node, int32_t x_bits, int32_t y_bits, int32_t z_bits) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return;
	node->set_global_position(Vector3(int32_bits_to_float(x_bits), int32_bits_to_float(y_bits), int32_bits_to_float(z_bits)));
}
static int64_t icall_Node3D_GetGlobalPositionX(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_global_position().x);
}
static int64_t icall_Node3D_GetGlobalPositionY(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_global_position().y);
}
static int64_t icall_Node3D_GetGlobalPositionZ(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_global_position().z);
}

// ============== GlobalRotation (radians) ==============
static void icall_Node3D_SetGlobalRotation(int64_t p_node, int32_t x_bits, int32_t y_bits, int32_t z_bits) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return;
	node->set_global_rotation(Vector3(int32_bits_to_float(x_bits), int32_bits_to_float(y_bits), int32_bits_to_float(z_bits)));
}
static int64_t icall_Node3D_GetGlobalRotationX(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_global_rotation().x);
}
static int64_t icall_Node3D_GetGlobalRotationY(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_global_rotation().y);
}
static int64_t icall_Node3D_GetGlobalRotationZ(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_global_rotation().z);
}

// ============== GlobalScale (verb form: multiplies current scale) ==============
// Node3D has no set_global_scale/get_global_scale; only `global_scale(Vector3)`
// which scales the node's global transform by the given vector.
static void icall_Node3D_GlobalScale(int64_t p_node, int32_t x_bits, int32_t y_bits, int32_t z_bits) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return;
	node->global_scale(Vector3(int32_bits_to_float(x_bits), int32_bits_to_float(y_bits), int32_bits_to_float(z_bits)));
}

// ============== Visibility ==============
static void icall_Node3D_SetVisible(int64_t p_node, mono_bool p_visible) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return;
	node->set_visible(p_visible != 0);
}
static mono_bool icall_Node3D_IsVisible(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return false;
	return node->is_visible();
}

// ============== Transform (full Transform3D, 12 floats) ==============
// Pass 12 int32 bit-patterns: basis row0.xyz, row1.xyz, row2.xyz, origin.xyz.
// C# side encodes via FloatIntUnion, C++ side reconstructs Transform3D.
static void icall_Node3D_SetTransform(int64_t p_node,
		int32_t b0x, int32_t b0y, int32_t b0z,
		int32_t b1x, int32_t b1y, int32_t b1z,
		int32_t b2x, int32_t b2y, int32_t b2z,
		int32_t ox, int32_t oy, int32_t oz) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return;
	Basis basis(
		Vector3(int32_bits_to_float(b0x), int32_bits_to_float(b0y), int32_bits_to_float(b0z)),
		Vector3(int32_bits_to_float(b1x), int32_bits_to_float(b1y), int32_bits_to_float(b1z)),
		Vector3(int32_bits_to_float(b2x), int32_bits_to_float(b2y), int32_bits_to_float(b2z)));
	Vector3 origin(int32_bits_to_float(ox), int32_bits_to_float(oy), int32_bits_to_float(oz));
	node->set_transform(Transform3D(basis, origin));
}

// GetTransform: returns 4 int64 values, each packing 2 float bit-patterns.
// To avoid returning a MonoArray (which has its own WASM dispatch overhead),
// we expose per-element getters. The C# side calls them sequentially.
// To keep icall count manageable, we expose only the 4 axes (rows) packed.
// Row0 XY -> int64, Row0 Z + Row1 X -> int64, Row1 YZ -> int64, ... (6 calls)
// Simpler: 12 per-element getters, each returning int64 bit-pattern.
static int64_t icall_Node3D_GetTransformB0X(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_transform().basis.rows[0].x);
}
static int64_t icall_Node3D_GetTransformB0Y(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_transform().basis.rows[0].y);
}
static int64_t icall_Node3D_GetTransformB0Z(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_transform().basis.rows[0].z);
}
static int64_t icall_Node3D_GetTransformB1X(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_transform().basis.rows[1].x);
}
static int64_t icall_Node3D_GetTransformB1Y(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_transform().basis.rows[1].y);
}
static int64_t icall_Node3D_GetTransformB1Z(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_transform().basis.rows[1].z);
}
static int64_t icall_Node3D_GetTransformB2X(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_transform().basis.rows[2].x);
}
static int64_t icall_Node3D_GetTransformB2Y(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_transform().basis.rows[2].y);
}
static int64_t icall_Node3D_GetTransformB2Z(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_transform().basis.rows[2].z);
}
static int64_t icall_Node3D_GetTransformOX(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_transform().origin.x);
}
static int64_t icall_Node3D_GetTransformOY(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_transform().origin.y);
}
static int64_t icall_Node3D_GetTransformOZ(int64_t p_node) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return 0;
	return float_to_int64_bits(node->get_transform().origin.z);
}

// ============== Utility methods ==============
// Node3D::rotate_x/y/z(angle) - increment rotation around local axis
static void icall_Node3D_RotateX(int64_t p_node, int32_t angle_bits) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return;
	node->rotate_x(int32_bits_to_float(angle_bits));
}
static void icall_Node3D_RotateY(int64_t p_node, int32_t angle_bits) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return;
	node->rotate_y(int32_bits_to_float(angle_bits));
}
static void icall_Node3D_RotateZ(int64_t p_node, int32_t angle_bits) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return;
	node->rotate_z(int32_bits_to_float(angle_bits));
}

// Node3D::translate(offset) - local-space translate
static void icall_Node3D_Translate(int64_t p_node, int32_t x_bits, int32_t y_bits, int32_t z_bits) {
	Node3D *node = (Node3D *)(intptr_t)p_node;
	if (!node) return;
	node->translate(Vector3(int32_bits_to_float(x_bits), int32_bits_to_float(y_bits), int32_bits_to_float(z_bits)));
}

// ClassDB validation
static void scan_and_validate_node3d_methods() {
	const char *essential_methods[] = {
		"set_position", "get_position",
		"set_rotation", "get_rotation",
		"set_scale", "get_scale",
		"set_global_position", "get_global_position",
		"set_global_rotation", "get_global_rotation",
		"global_scale",
		"set_transform", "get_transform",
		"set_visible", "is_visible",
		"rotate_x", "rotate_y", "rotate_z", "translate",
		nullptr
	};
	for (int i = 0; essential_methods[i] != nullptr; i++) {
		if (!ClassDB::has_method("Node3D", essential_methods[i])) {
			MonoLogger::log_warning(vformat("ClassDB: Node3D::%s not found", essential_methods[i]));
		}
	}
}

} // anonymous namespace

namespace GDMonoInterop {

void register_node3d_icalls() {
	MonoLogger::log("Registering Node3D icalls...");
	scan_and_validate_node3d_methods();

	// Position (local)
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_SetPosition", (const void *)icall_Node3D_SetPosition);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetPositionX", (const void *)icall_Node3D_GetPositionX);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetPositionY", (const void *)icall_Node3D_GetPositionY);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetPositionZ", (const void *)icall_Node3D_GetPositionZ);

	// Rotation (local, radians)
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_SetRotation", (const void *)icall_Node3D_SetRotation);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetRotationX", (const void *)icall_Node3D_GetRotationX);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetRotationY", (const void *)icall_Node3D_GetRotationY);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetRotationZ", (const void *)icall_Node3D_GetRotationZ);

	// Scale (local)
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_SetScale", (const void *)icall_Node3D_SetScale);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetScaleX", (const void *)icall_Node3D_GetScaleX);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetScaleY", (const void *)icall_Node3D_GetScaleY);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetScaleZ", (const void *)icall_Node3D_GetScaleZ);

	// GlobalPosition
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_SetGlobalPosition", (const void *)icall_Node3D_SetGlobalPosition);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetGlobalPositionX", (const void *)icall_Node3D_GetGlobalPositionX);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetGlobalPositionY", (const void *)icall_Node3D_GetGlobalPositionY);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetGlobalPositionZ", (const void *)icall_Node3D_GetGlobalPositionZ);

	// GlobalRotation
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_SetGlobalRotation", (const void *)icall_Node3D_SetGlobalRotation);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetGlobalRotationX", (const void *)icall_Node3D_GetGlobalRotationX);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetGlobalRotationY", (const void *)icall_Node3D_GetGlobalRotationY);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetGlobalRotationZ", (const void *)icall_Node3D_GetGlobalRotationZ);

	// GlobalScale (verb: multiply current scale)
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GlobalScale", (const void *)icall_Node3D_GlobalScale);

	// Visibility
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_SetVisible", (const void *)icall_Node3D_SetVisible);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_IsVisible", (const void *)icall_Node3D_IsVisible);

	// Transform (full 12-float set)
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_SetTransform", (const void *)icall_Node3D_SetTransform);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetTransformB0X", (const void *)icall_Node3D_GetTransformB0X);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetTransformB0Y", (const void *)icall_Node3D_GetTransformB0Y);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetTransformB0Z", (const void *)icall_Node3D_GetTransformB0Z);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetTransformB1X", (const void *)icall_Node3D_GetTransformB1X);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetTransformB1Y", (const void *)icall_Node3D_GetTransformB1Y);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetTransformB1Z", (const void *)icall_Node3D_GetTransformB1Z);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetTransformB2X", (const void *)icall_Node3D_GetTransformB2X);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetTransformB2Y", (const void *)icall_Node3D_GetTransformB2Y);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetTransformB2Z", (const void *)icall_Node3D_GetTransformB2Z);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetTransformOX", (const void *)icall_Node3D_GetTransformOX);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetTransformOY", (const void *)icall_Node3D_GetTransformOY);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_GetTransformOZ", (const void *)icall_Node3D_GetTransformOZ);

	// Utility: rotate_x/y/z, translate
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_RotateX", (const void *)icall_Node3D_RotateX);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_RotateY", (const void *)icall_Node3D_RotateY);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_RotateZ", (const void *)icall_Node3D_RotateZ);
	mono_add_internal_call("Godot.Node3D::godot_icall_Node3D_Translate", (const void *)icall_Node3D_Translate);

	MonoLogger::log("Node3D icalls registered (35 methods)");
}

} // namespace GDMonoInterop
