#pragma once

// CSharpProjectEditorPlugin: registers the CSharpProjectPanel as a bottom
// panel in the Godot editor and adds a "C# Project" item to the Tools menu.

#include "editor/plugins/editor_plugin.h"

class CSharpProjectPanel;

class CSharpProjectEditorPlugin : public EditorPlugin {
	GDCLASS(CSharpProjectEditorPlugin, EditorPlugin);

private:
	CSharpProjectPanel *panel = nullptr;
	Button *bottom_button = nullptr;

	void _tool_menu_callback();

protected:
	static void _bind_methods() {}

public:
	virtual String get_plugin_name() const override { return "CSharpProject"; }

	CSharpProjectEditorPlugin();
	~CSharpProjectEditorPlugin();
};
