// ICall bindings for Control - direct C++ method calls.
// 覆盖 UI 开发高频方法: 位置/尺寸/锚点/偏移/可见/鼠标过滤/聚焦/modulate/tooltip/cursor。
// 所有 float-returning icalls 返回 int64 位模式以兼容 WASM 解释器签名约束。
// C# 侧用 DoubleLongUnion / FloatIntUnion 还原位模式。

#include "../../mono_gd/interop/gd_mono_interop_variant.h"
#include "../../utils/mono_logger.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#include "core/math/vector2.h"
#include "core/math/rect2.h"
#include "core/math/color.h"
#include "scene/main/node.h"
#include "scene/gui/control.h"
#include <mono/mono-publib.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" {
char *mono_string_to_utf8(MonoString *s);
void mono_free(void *ptr);
}

namespace {

// Helpers: float 位模式互转（WASM 安全）
static inline int64_t float_to_int64_bits(float v) {
	int32_t i32;
	memcpy(&i32, &v, sizeof(int32_t));
	return (int64_t)i32;
}

// Vector2 编码: x 在低 32 位, y 在高 32 位（与 Node2D 一致）
static inline int64_t vec2_to_int64(const Vector2 &v) {
	int32_t x_bits, y_bits;
	memcpy(&x_bits, &v.x, sizeof(int32_t));
	memcpy(&y_bits, &v.y, sizeof(int32_t));
	return ((int64_t)y_bits << 32) | (uint32_t)x_bits;
}

// Color 编码: 4 个 float 分别用 4 个 int32 透传，但 icall 参数最多 4 个，所以用全局调用 + 单值返回。
// 这里 Color 作为 4 个连续 int32 参数传递（与 GodotObject.SetColor 模式一致）。

// === Position ===
static void icall_Control_SetPosition(int64_t p_node, int32_t x_bits, int32_t y_bits) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	float x, y;
	memcpy(&x, &x_bits, sizeof(float));
	memcpy(&y, &y_bits, sizeof(float));
	ctrl->set_position(Vector2(x, y));
}

static int64_t icall_Control_GetPosition(int64_t p_node) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return 0;
	return vec2_to_int64(ctrl->get_position());
}

// === Size ===
static void icall_Control_SetSize(int64_t p_node, int32_t w_bits, int32_t h_bits) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	float w, h;
	memcpy(&w, &w_bits, sizeof(float));
	memcpy(&h, &h_bits, sizeof(float));
	ctrl->set_size(Vector2(w, h));
}

static int64_t icall_Control_GetSize(int64_t p_node) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return 0;
	return vec2_to_int64(ctrl->get_size());
}

// === Global Position ===
static void icall_Control_SetGlobalPosition(int64_t p_node, int32_t x_bits, int32_t y_bits) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	float x, y;
	memcpy(&x, &x_bits, sizeof(float));
	memcpy(&y, &y_bits, sizeof(float));
	ctrl->set_global_position(Vector2(x, y));
}

static int64_t icall_Control_GetGlobalPosition(int64_t p_node) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return 0;
	return vec2_to_int64(ctrl->get_global_position());
}

// === Global Size (read-only in Godot 4) ===
// Godot 4 Control 没有 get_global_size()，用 get_global_rect().size 替代。
static int64_t icall_Control_GetGlobalSize(int64_t p_node) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return 0;
	return vec2_to_int64(ctrl->get_global_rect().size);
}

// === Rect (position + size) ===
// 用 4 个 int32 透传（与 Node2D Transform 模式一致）
static void icall_Control_SetRect(int64_t p_node, int32_t x_bits, int32_t y_bits, int32_t w_bits, int32_t h_bits) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	float x, y, w, h;
	memcpy(&x, &x_bits, sizeof(float));
	memcpy(&y, &y_bits, sizeof(float));
	memcpy(&w, &w_bits, sizeof(float));
	memcpy(&h, &h_bits, sizeof(float));
	ctrl->set_position(Vector2(x, y));
	ctrl->set_size(Vector2(w, h));
}

// === Visible ===
static void icall_Control_SetVisible(int64_t p_node, mono_bool p_visible) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	ctrl->set_visible(p_visible != 0);
}

static mono_bool icall_Control_IsVisible(int64_t p_node) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return false;
	return ctrl->is_visible();
}

// === Modulate ===
// 4 个 int32 (RGBA bits)
static void icall_Control_SetModulate(int64_t p_node, int32_t r_bits, int32_t g_bits, int32_t b_bits, int32_t a_bits) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	float r, g, b, a;
	memcpy(&r, &r_bits, sizeof(float));
	memcpy(&g, &g_bits, sizeof(float));
	memcpy(&b, &b_bits, sizeof(float));
	memcpy(&a, &a_bits, sizeof(float));
	ctrl->set_modulate(Color(r, g, b, a));
}

// 用 4 个连续 int32 返回太复杂，且 icall 只能返回单值。
// 改为 4 个独立 getter（与 Node3D GetPositionX/Y/Z 模式一致）。
// 这里用 int64 携带 RGBA 前 2 个（RG），另一个返回 BA。
// 简化: 提供 4 个独立 getter
static int64_t icall_Control_GetModulateR(int64_t p_node) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return 0;
	return float_to_int64_bits(ctrl->get_modulate().r);
}
static int64_t icall_Control_GetModulateG(int64_t p_node) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return 0;
	return float_to_int64_bits(ctrl->get_modulate().g);
}
static int64_t icall_Control_GetModulateB(int64_t p_node) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return 0;
	return float_to_int64_bits(ctrl->get_modulate().b);
}
static int64_t icall_Control_GetModulateA(int64_t p_node) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return 0;
	return float_to_int64_bits(ctrl->get_modulate().a);
}

// === Mouse Filter ===
// MouseFilter enum: STOP=0, PASS=1, IGNORE=2
static void icall_Control_SetMouseFilter(int64_t p_node, int32_t p_filter) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	ctrl->set_mouse_filter((Control::MouseFilter)p_filter);
}

static int32_t icall_Control_GetMouseFilter(int64_t p_node) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return (int32_t)Control::MOUSE_FILTER_STOP;
	return (int32_t)ctrl->get_mouse_filter();
}

// === Focus ===
// FocusMode enum: NONE=0, CLICK=1, ALL=2
static void icall_Control_SetFocusMode(int64_t p_node, int32_t p_mode) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	ctrl->set_focus_mode((Control::FocusMode)p_mode);
}

static int32_t icall_Control_GetFocusMode(int64_t p_node) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return (int32_t)Control::FOCUS_NONE;
	return (int32_t)ctrl->get_focus_mode();
}

static void icall_Control_GrabFocus(int64_t p_node) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	ctrl->grab_focus();
}

static void icall_Control_ReleaseFocus(int64_t p_node) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	ctrl->release_focus();
}

static mono_bool icall_Control_HasFocus(int64_t p_node) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return false;
	return ctrl->has_focus();
}

// === Tooltip ===
static void icall_Control_SetTooltipText(int64_t p_node, MonoString *p_text) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl || !p_text) return;
	char *utf8 = mono_string_to_utf8(p_text);
	if (!utf8) return;
	ctrl->set_tooltip_text(String::utf8(utf8));
	mono_free(utf8);
}

static MonoString *icall_Control_GetTooltipText(int64_t p_node) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return mono_string_new(mono_domain_get(), "");
	String tip = ctrl->get_tooltip_text();
	CharString cs = tip.utf8();
	return mono_string_new(mono_domain_get(), cs.get_data());
}

// === Cursor Shape ===
// CursorShape enum: ARROW=0, IBEAM=1, POINTING_HAND=2, CROSS=3, WAIT=4, BUSY=5, DRAG=6, CAN_DROP=7, NO_DROP=8, FORBIDDEN=9, VSIZE=10, HSIZE=11, BDIAGSIZE=12, FDIAGSIZE=13, MOVE=14, VSPLIT=15, HSPLIT=16, HELP=17
static void icall_Control_SetDefaultCursorShape(int64_t p_node, int32_t p_shape) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	ctrl->set_default_cursor_shape((Control::CursorShape)p_shape);
}

static int32_t icall_Control_GetDefaultCursorShape(int64_t p_node) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return (int32_t)Control::CURSOR_ARROW;
	return (int32_t)ctrl->get_default_cursor_shape();
}

// === Size Flags ===
static void icall_Control_SetHSizeFlags(int64_t p_node, int32_t p_flags) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	ctrl->set_h_size_flags(p_flags);
}

static int32_t icall_Control_GetHSizeFlags(int64_t p_node) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return 0;
	return ctrl->get_h_size_flags();
}

static void icall_Control_SetVSizeFlags(int64_t p_node, int32_t p_flags) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	ctrl->set_v_size_flags(p_flags);
}

static int32_t icall_Control_GetVSizeFlags(int64_t p_node) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return 0;
	return ctrl->get_v_size_flags();
}

// === Anchor (single side: 0=Left, 1=Top, 2=Right, 3=Bottom) ===
// 用 side 索引避免为 4 个 side 各写一对 icall
// 注意：Godot 4 把 Side 提到全局枚举（core/math/math_defs.h），不是 Control::Side。
static void icall_Control_SetAnchor(int64_t p_node, int32_t p_side, int32_t p_value_bits) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	float value;
	memcpy(&value, &p_value_bits, sizeof(float));
	ctrl->set_anchor((Side)p_side, value);
}

static int64_t icall_Control_GetAnchor(int64_t p_node, int32_t p_side) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return 0;
	return float_to_int64_bits(ctrl->get_anchor((Side)p_side));
}

// === Offset (single side) ===
static void icall_Control_SetOffset(int64_t p_node, int32_t p_side, int32_t p_value_bits) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	float value;
	memcpy(&value, &p_value_bits, sizeof(float));
	ctrl->set_offset((Side)p_side, value);
}

static int64_t icall_Control_GetOffset(int64_t p_node, int32_t p_side) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return 0;
	return float_to_int64_bits(ctrl->get_offset((Side)p_side));
}

// === Anchors Preset ===
// 用 int32 透传 LayoutPreset 枚举（值域 0-19）+ keep_offset bool
static void icall_Control_SetAnchorsPreset(int64_t p_node, int32_t p_preset, mono_bool p_keep_offset) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	ctrl->set_anchors_preset((Control::LayoutPreset)p_preset, p_keep_offset != 0);
}

// === Anchors & Offsets Together (common preset application) ===
// Godot 4 的 set_anchors_and_offsets_preset 第二参数是 LayoutPresetMode（不是 bool），
// PRESET_MODE_KEEP_SIZE 是语义上最接近旧 keep_offset=true 的选项。
static void icall_Control_SetAnchorsOffsetsPreset(int64_t p_node, int32_t p_preset, mono_bool p_keep_offset) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	ctrl->set_anchors_and_offsets_preset((Control::LayoutPreset)p_preset,
			p_keep_offset ? Control::PRESET_MODE_KEEP_SIZE : Control::PRESET_MODE_MINSIZE);
}

// === Grow Direction ===
// GrowDirection enum: BEGIN=0, END=1, BOTH=2
static void icall_Control_SetHGrowDirection(int64_t p_node, int32_t p_dir) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	ctrl->set_h_grow_direction((Control::GrowDirection)p_dir);
}

static void icall_Control_SetVGrowDirection(int64_t p_node, int32_t p_dir) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	ctrl->set_v_grow_direction((Control::GrowDirection)p_dir);
}

// === Rotation (Control 在 Godot 4 支持旋转，单位弧度) ===
static void icall_Control_SetRotation(int64_t p_node, int32_t p_angle_bits) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	float angle;
	memcpy(&angle, &p_angle_bits, sizeof(float));
	ctrl->set_rotation(angle);
}

static int64_t icall_Control_GetRotation(int64_t p_node) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return 0;
	return float_to_int64_bits(ctrl->get_rotation());
}

// === Pivot Offset ===
static void icall_Control_SetPivotOffset(int64_t p_node, int32_t x_bits, int32_t y_bits) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return;
	float x, y;
	memcpy(&x, &x_bits, sizeof(float));
	memcpy(&y, &y_bits, sizeof(float));
	ctrl->set_pivot_offset(Vector2(x, y));
}

static int64_t icall_Control_GetPivotOffset(int64_t p_node) {
	Control *ctrl = (Control *)(intptr_t)p_node;
	if (!ctrl) return 0;
	return vec2_to_int64(ctrl->get_pivot_offset());
}

// === ClassDB validation ===
static void scan_and_validate_control_methods() {
	// Note: Godot 4 Control 没有 get_global_size()，用 get_global_rect().size 替代。
	const char *essential_methods[] = {
		"set_position", "get_position", "set_size", "get_size",
		"set_global_position", "get_global_position", "get_global_rect",
		"set_visible", "is_visible",
		"set_modulate", "get_modulate",
		"set_mouse_filter", "get_mouse_filter",
		"set_focus_mode", "get_focus_mode",
		"grab_focus", "release_focus", "has_focus",
		"set_tooltip_text", "get_tooltip_text",
		"set_default_cursor_shape", "get_default_cursor_shape",
		"set_h_size_flags", "get_h_size_flags",
		"set_v_size_flags", "get_v_size_flags",
		"set_anchor", "get_anchor", "set_offset", "get_offset",
		"set_anchors_preset", "set_anchors_and_offsets_preset",
		"set_h_grow_direction", "set_v_grow_direction",
		"set_rotation", "get_rotation",
		"set_pivot_offset", "get_pivot_offset",
		nullptr
	};

	for (int i = 0; essential_methods[i] != nullptr; i++) {
		if (!ClassDB::has_method("Control", essential_methods[i])) {
			MonoLogger::log_warning(vformat("ClassDB: Control::%s not found", essential_methods[i]));
		}
	}
}

} // anonymous namespace

namespace GDMonoInterop {

void register_control_icalls() {
	MonoLogger::log("Registering Control icalls...");
	scan_and_validate_control_methods();

	// Position
	mono_add_internal_call("Godot.Control::godot_icall_Control_SetPosition", (const void *)icall_Control_SetPosition);
	mono_add_internal_call("Godot.Control::godot_icall_Control_GetPosition", (const void *)icall_Control_GetPosition);
	// Size
	mono_add_internal_call("Godot.Control::godot_icall_Control_SetSize", (const void *)icall_Control_SetSize);
	mono_add_internal_call("Godot.Control::godot_icall_Control_GetSize", (const void *)icall_Control_GetSize);
	// Global Position
	mono_add_internal_call("Godot.Control::godot_icall_Control_SetGlobalPosition", (const void *)icall_Control_SetGlobalPosition);
	mono_add_internal_call("Godot.Control::godot_icall_Control_GetGlobalPosition", (const void *)icall_Control_GetGlobalPosition);
	mono_add_internal_call("Godot.Control::godot_icall_Control_GetGlobalSize", (const void *)icall_Control_GetGlobalSize);
	// Rect (position+size 组合设置)
	mono_add_internal_call("Godot.Control::godot_icall_Control_SetRect", (const void *)icall_Control_SetRect);
	// Visible
	mono_add_internal_call("Godot.Control::godot_icall_Control_SetVisible", (const void *)icall_Control_SetVisible);
	mono_add_internal_call("Godot.Control::godot_icall_Control_IsVisible", (const void *)icall_Control_IsVisible);
	// Modulate
	mono_add_internal_call("Godot.Control::godot_icall_Control_SetModulate", (const void *)icall_Control_SetModulate);
	mono_add_internal_call("Godot.Control::godot_icall_Control_GetModulateR", (const void *)icall_Control_GetModulateR);
	mono_add_internal_call("Godot.Control::godot_icall_Control_GetModulateG", (const void *)icall_Control_GetModulateG);
	mono_add_internal_call("Godot.Control::godot_icall_Control_GetModulateB", (const void *)icall_Control_GetModulateB);
	mono_add_internal_call("Godot.Control::godot_icall_Control_GetModulateA", (const void *)icall_Control_GetModulateA);
	// Mouse Filter
	mono_add_internal_call("Godot.Control::godot_icall_Control_SetMouseFilter", (const void *)icall_Control_SetMouseFilter);
	mono_add_internal_call("Godot.Control::godot_icall_Control_GetMouseFilter", (const void *)icall_Control_GetMouseFilter);
	// Focus
	mono_add_internal_call("Godot.Control::godot_icall_Control_SetFocusMode", (const void *)icall_Control_SetFocusMode);
	mono_add_internal_call("Godot.Control::godot_icall_Control_GetFocusMode", (const void *)icall_Control_GetFocusMode);
	mono_add_internal_call("Godot.Control::godot_icall_Control_GrabFocus", (const void *)icall_Control_GrabFocus);
	mono_add_internal_call("Godot.Control::godot_icall_Control_ReleaseFocus", (const void *)icall_Control_ReleaseFocus);
	mono_add_internal_call("Godot.Control::godot_icall_Control_HasFocus", (const void *)icall_Control_HasFocus);
	// Tooltip
	mono_add_internal_call("Godot.Control::godot_icall_Control_SetTooltipText", (const void *)icall_Control_SetTooltipText);
	mono_add_internal_call("Godot.Control::godot_icall_Control_GetTooltipText", (const void *)icall_Control_GetTooltipText);
	// Cursor Shape
	mono_add_internal_call("Godot.Control::godot_icall_Control_SetDefaultCursorShape", (const void *)icall_Control_SetDefaultCursorShape);
	mono_add_internal_call("Godot.Control::godot_icall_Control_GetDefaultCursorShape", (const void *)icall_Control_GetDefaultCursorShape);
	// Size Flags
	mono_add_internal_call("Godot.Control::godot_icall_Control_SetHSizeFlags", (const void *)icall_Control_SetHSizeFlags);
	mono_add_internal_call("Godot.Control::godot_icall_Control_GetHSizeFlags", (const void *)icall_Control_GetHSizeFlags);
	mono_add_internal_call("Godot.Control::godot_icall_Control_SetVSizeFlags", (const void *)icall_Control_SetVSizeFlags);
	mono_add_internal_call("Godot.Control::godot_icall_Control_GetVSizeFlags", (const void *)icall_Control_GetVSizeFlags);
	// Anchor (single side via side index)
	mono_add_internal_call("Godot.Control::godot_icall_Control_SetAnchor", (const void *)icall_Control_SetAnchor);
	mono_add_internal_call("Godot.Control::godot_icall_Control_GetAnchor", (const void *)icall_Control_GetAnchor);
	// Offset
	mono_add_internal_call("Godot.Control::godot_icall_Control_SetOffset", (const void *)icall_Control_SetOffset);
	mono_add_internal_call("Godot.Control::godot_icall_Control_GetOffset", (const void *)icall_Control_GetOffset);
	// Anchors Preset
	mono_add_internal_call("Godot.Control::godot_icall_Control_SetAnchorsPreset", (const void *)icall_Control_SetAnchorsPreset);
	mono_add_internal_call("Godot.Control::godot_icall_Control_SetAnchorsOffsetsPreset", (const void *)icall_Control_SetAnchorsOffsetsPreset);
	// Grow Direction
	mono_add_internal_call("Godot.Control::godot_icall_Control_SetHGrowDirection", (const void *)icall_Control_SetHGrowDirection);
	mono_add_internal_call("Godot.Control::godot_icall_Control_SetVGrowDirection", (const void *)icall_Control_SetVGrowDirection);
	// Rotation
	mono_add_internal_call("Godot.Control::godot_icall_Control_SetRotation", (const void *)icall_Control_SetRotation);
	mono_add_internal_call("Godot.Control::godot_icall_Control_GetRotation", (const void *)icall_Control_GetRotation);
	// Pivot Offset
	mono_add_internal_call("Godot.Control::godot_icall_Control_SetPivotOffset", (const void *)icall_Control_SetPivotOffset);
	mono_add_internal_call("Godot.Control::godot_icall_Control_GetPivotOffset", (const void *)icall_Control_GetPivotOffset);

	MonoLogger::log("Control icalls registered (40 methods)");
}

} // namespace GDMonoInterop
