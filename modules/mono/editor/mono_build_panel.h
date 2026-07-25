/**************************************************************************/
/*  mono_build_panel.h                                                    */
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

#pragma once

#ifdef TOOLS_ENABLED

#include "editor/docks/editor_dock.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/rich_text_label.h"

// P4: Mono Build Panel — bottom dock showing dotnet build output.
//
// UI structure (mirrors EditorLog pattern):
//   [Build button] [status label]
//   [RichTextLabel output (BBCode, scroll-follow)]
//
// build_project() routes stdout/stderr here via append_output().
// Lines containing ": error " are highlighted red (BBCode).
// v1 is synchronous-blocking; the status label shows "构建中…" during build.
class MonoBuildPanel : public EditorDock {
	GDCLASS(MonoBuildPanel, EditorDock);

	static MonoBuildPanel *singleton;

	RichTextLabel *output = nullptr;
	Button *build_button = nullptr;
	Label *status = nullptr;

	void _on_build_pressed();

protected:
	static void _bind_methods();

public:
	static MonoBuildPanel *get_singleton() { return singleton; }

	// Append build output text. Lines containing ": error " are rendered red.
	// p_text is raw dotnet output (may contain multiple lines).
	void append_output(const String &p_text);

	// Clear the output panel.
	void clear_output();

	// Set the status label text (e.g., "构建中…", "构建成功", "构建失败").
	void set_status(const String &p_text, bool p_error = false);

	MonoBuildPanel();
	~MonoBuildPanel();
};

#endif // TOOLS_ENABLED
