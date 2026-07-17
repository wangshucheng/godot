#pragma once

// CSharpProjectPanel: a Godot editor bottom panel for managing the C# project.
//
// Provides buttons to create/refresh the .csproj/.sln, compile the project,
// and open the project folder. A RichTextLabel shows status and compilation
// output. An ItemList lists all .cs files found in the project directory.

#include "scene/gui/control.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/item_list.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/box_container.h"
#include "scene/gui/separator.h"

class CSharpProjectPanel : public Control {
	GDCLASS(CSharpProjectPanel, Control);

private:
	Label *status_label = nullptr;
	RichTextLabel *output_log = nullptr;
	ItemList *cs_files_list = nullptr;
	Button *btn_create = nullptr;
	Button *btn_compile = nullptr;
	Button *btn_open_folder = nullptr;
	Button *btn_refresh_files = nullptr;

	// Debugger UI
	Button *btn_debug_toggle = nullptr;
	Label *debug_port_label = nullptr;
	Label *debug_attach_label = nullptr;

	void _on_create_pressed();
	void _on_compile_pressed();
	void _on_open_folder_pressed();
	void _on_refresh_files_pressed();
	void _on_debug_toggle_pressed();

	void refresh_cs_files();
	void refresh_debugger_state();
	void set_status(const String &p_text, bool p_error = false);
	void append_output(const String &p_text);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void refresh();

	CSharpProjectPanel();
};
