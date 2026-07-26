/**************************************************************************/
/*  mono_build_panel.cpp                                                  */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#ifdef TOOLS_ENABLED

#include "mono_build_panel.h"

#include "../csharp_script.h"
#include "core/config/engine.h"
#include "core/object/callable_mp.h"
#include "editor/editor_interface.h"
#include "editor/editor_string_names.h"
#include "editor/settings/editor_command_palette.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/separator.h"

MonoBuildPanel *MonoBuildPanel::singleton = nullptr;

void MonoBuildPanel::_bind_methods() {
}

void MonoBuildPanel::_on_build_pressed() {
	// Delegate to CSharpLanguage::build_project() which routes output here.
	if (CSharpLanguage::get_singleton()) {
		CSharpLanguage::get_singleton()->build_project();
	}
}

void MonoBuildPanel::append_output(const String &p_text) {
	if (!output) {
		return;
	}

	// Split into lines so we can highlight error lines.
	//
	// P1-#6 fix: do NOT call xml_escape(). RichTextLabel::add_text() adds
	// text verbatim — it does not parse BBCode or decode XML entities, so
	// escaping produces visible "&lt;" / "&gt;" literals in the build output.
	// C# compiler diagnostics are full of generics (`List<int>`) and XML doc
	// tags (`<summary>`), which were being shown as "&lt;summary&gt;" before.
	// BBCode parsing only happens via append_text()/parse_bbcode(), not
	// add_text(), so passing the raw line is safe — color is applied by the
	// surrounding push_color()/pop() pair.
	Vector<String> lines = p_text.split("\n", false);
	for (int i = 0; i < lines.size(); i++) {
		const String &line = lines[i];

		if (line.contains(": error ")) {
			output->push_color(Color(1.0f, 0.3f, 0.3f));
			output->add_text(line);
			output->pop();
		} else if (line.contains(": warning ")) {
			output->push_color(Color(1.0f, 0.8f, 0.4f));
			output->add_text(line);
			output->pop();
		} else {
			output->add_text(line);
		}
		output->add_newline();
	}
}

void MonoBuildPanel::clear_output() {
	if (output) {
		output->clear();
	}
}

void MonoBuildPanel::set_status(const String &p_text, bool p_error) {
	if (!status) {
		return;
	}
	status->set_text(p_text);
	if (p_error) {
		status->add_theme_color_override("font_color", Color(1.0f, 0.3f, 0.3f));
	} else {
		status->remove_theme_color_override("font_color");
	}
}

MonoBuildPanel::MonoBuildPanel() {
	singleton = this;

	set_name(TTRC("Mono"));
	set_title(TTRC("Mono Build"));
	set_dock_shortcut(ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_mono_bottom_panel", TTRC("Toggle Mono Build Dock"), KeyModifierMask::ALT | Key::M));
	set_default_slot(EditorDock::DOCK_SLOT_BOTTOM);
	set_available_layouts(EditorDock::DOCK_LAYOUT_VERTICAL | EditorDock::DOCK_LAYOUT_FLOATING);

	// Main vertical container.
	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->set_v_size_flags(SIZE_EXPAND_FILL);
	vbox->set_h_size_flags(SIZE_EXPAND_FILL);
	add_child(vbox);

	// Top bar: build button + status label.
	HBoxContainer *topbar = memnew(HBoxContainer);
	vbox->add_child(topbar);

	build_button = memnew(Button);
	build_button->set_text(TTRC("Build"));
	build_button->set_tooltip_text(TTRC("Rebuild the C# project (dotnet build)"));
	build_button->connect(SceneStringName(pressed), callable_mp(this, &MonoBuildPanel::_on_build_pressed));
	topbar->add_child(build_button);

	topbar->add_child(memnew(VSeparator));

	status = memnew(Label);
	status->set_text(TTRC("Ready"));
	status->set_h_size_flags(SIZE_EXPAND_FILL);
	topbar->add_child(status);

	// Output area: RichTextLabel with BBCode for colored error/warning lines.
	output = memnew(RichTextLabel);
	output->set_use_bbcode(true);
	output->set_scroll_follow(true);
	output->set_selection_enabled(true);
	output->set_context_menu_enabled(true);
	output->set_focus_mode(FOCUS_CLICK);
	output->set_v_size_flags(SIZE_EXPAND_FILL);
	output->set_h_size_flags(SIZE_EXPAND_FILL);
	output->set_custom_minimum_size(Size2(0, 90 * EDSCALE));
	vbox->add_child(output);
}

MonoBuildPanel::~MonoBuildPanel() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

#endif // TOOLS_ENABLED
