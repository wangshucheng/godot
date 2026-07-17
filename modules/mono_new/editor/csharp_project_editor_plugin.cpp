// CSharpProjectEditorPlugin: adds the C# project panel as a bottom panel
// and registers a Tools menu shortcut to toggle its visibility.

#include "csharp_project_editor_plugin.h"
#include "csharp_project_panel.h"

#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "core/object/callable_mp.h"

CSharpProjectEditorPlugin::CSharpProjectEditorPlugin() {
	panel = memnew(CSharpProjectPanel);

	// add_control_to_bottom_panel is deprecated but still the simplest way
	// to register a bottom panel with a toggle button in Godot 4.7.
#ifndef DISABLE_DEPRECATED
	bottom_button = add_control_to_bottom_panel(panel, TTR("C# Project"));
#else
	// Fallback: add to the main screen container directly (no toggle button).
	EditorInterface::get_singleton()->get_editor_main_screen()->add_child(panel);
	panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	bottom_button = nullptr;
#endif

	// Also add a Tools menu entry so users can find the panel.
	add_tool_menu_item(TTR("C# Project"), callable_mp(this, &CSharpProjectEditorPlugin::_tool_menu_callback));
}

CSharpProjectEditorPlugin::~CSharpProjectEditorPlugin() {
	// The base class destructor handles removing controls from containers.
	// We just null our pointers; the panel is freed by the editor.
	if (bottom_button) {
		remove_control_from_bottom_panel(panel);
	}
	if (panel) {
		// panel is a child of the editor; it will be freed when the editor
		// destroys its children. We null the pointer to avoid double-free.
		panel = nullptr;
	}
}

void CSharpProjectEditorPlugin::_tool_menu_callback() {
	if (bottom_button) {
		// Toggle the bottom panel visibility by pressing the button.
		bottom_button->set_pressed(true);
	}
	if (panel) {
		make_bottom_panel_item_visible(panel);
		panel->refresh();
	}
}
