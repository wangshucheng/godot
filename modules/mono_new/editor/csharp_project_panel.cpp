// CSharpProjectPanel: editor bottom panel for C# project management.
//
// Shows project status (csproj/sln existence, last compile result), a list of
// .cs files, and buttons to create/refresh project files, compile, and open
// the project folder in the OS file explorer.

#include "csharp_project_panel.h"
#include "csharp_editor.h"

#include "../mono_runtime/csharp_debugger.h"
#include "../mono_runtime/gd_mono.h"
#include "../utils/mono_logger.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "core/string/ustring.h"
#include "scene/gui/dialogs.h"

void CSharpProjectPanel::_bind_methods() {
}

CSharpProjectPanel::CSharpProjectPanel() {
	set_custom_minimum_size(Size2(0, 200));
	// Enable internal process for polling the SDB attach status.
	set_process_internal(true);

	VBoxContainer *vbox = memnew(VBoxContainer);
	add_child(vbox);
	vbox->set_anchors_preset(Control::PRESET_FULL_RECT);
	vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	vbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	// --- Toolbar ---
	HBoxContainer *toolbar = memnew(HBoxContainer);
	vbox->add_child(toolbar);

	btn_create = memnew(Button);
	btn_create->set_text("Create/Refresh Project");
	btn_create->set_tooltip_text("Generate or update .csproj and .sln files");
	btn_create->connect(SceneStringName(pressed), callable_mp(this, &CSharpProjectPanel::_on_create_pressed));
	toolbar->add_child(btn_create);

	btn_compile = memnew(Button);
	btn_compile->set_text("Compile");
	btn_compile->set_tooltip_text("Compile all C# files into a project DLL");
	btn_compile->connect(SceneStringName(pressed), callable_mp(this, &CSharpProjectPanel::_on_compile_pressed));
	toolbar->add_child(btn_compile);

	btn_refresh_files = memnew(Button);
	btn_refresh_files->set_text("Refresh Files");
	btn_refresh_files->set_tooltip_text("Refresh the .cs file list");
	btn_refresh_files->connect(SceneStringName(pressed), callable_mp(this, &CSharpProjectPanel::_on_refresh_files_pressed));
	toolbar->add_child(btn_refresh_files);

	btn_open_folder = memnew(Button);
	btn_open_folder->set_text("Open Folder");
	btn_open_folder->set_tooltip_text("Open the project directory in the file explorer");
	btn_open_folder->connect(SceneStringName(pressed), callable_mp(this, &CSharpProjectPanel::_on_open_folder_pressed));
	toolbar->add_child(btn_open_folder);

	toolbar->add_child(memnew(VSeparator));

	// --- Debugger controls ---
	// The SDB agent must be configured before mono_jit_init_version() (i.e. at
	// engine startup), so we cannot hot-toggle it. The button shows current
	// state and instructs the user to restart with --mono-debugger=PORT.
	btn_debug_toggle = memnew(Button);
	btn_debug_toggle->set_text("Debugger: Off");
	btn_debug_toggle->set_tooltip_text("Toggle the Mono SDB debugger (requires editor restart)");
	btn_debug_toggle->connect(SceneStringName(pressed), callable_mp(this, &CSharpProjectPanel::_on_debug_toggle_pressed));
	toolbar->add_child(btn_debug_toggle);

	debug_port_label = memnew(Label);
	debug_port_label->set_text("Port: (disabled)");
	toolbar->add_child(debug_port_label);

	debug_attach_label = memnew(Label);
	debug_attach_label->set_text("Not active");
	debug_attach_label->set_tooltip_text("SDB agent attach status");
	toolbar->add_child(debug_attach_label);

	toolbar->add_child(memnew(VSeparator));

	status_label = memnew(Label);
	status_label->set_text("Ready");
	toolbar->add_child(status_label);
	status_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	// --- Output log ---
	output_log = memnew(RichTextLabel);
	vbox->add_child(output_log);
	output_log->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	output_log->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	output_log->set_scroll_follow(true);
	output_log->set_use_bbcode(true);

	// --- CS files list (compact) ---
	HBoxContainer *bottom_row = memnew(HBoxContainer);
	vbox->add_child(bottom_row);
	bottom_row->set_custom_minimum_size(Size2(0, 120));

	cs_files_list = memnew(ItemList);
	bottom_row->add_child(cs_files_list);
	cs_files_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	cs_files_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	cs_files_list->set_tooltip_text("C# files in the project directory");
}

void CSharpProjectPanel::_notification(int p_what) {
	if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {
		if (is_visible_in_tree()) {
			refresh();
		}
	}
	// Poll the SDB attach status periodically while the panel is visible so
	// the user sees when VS Code connects/disconnects.
	if (p_what == NOTIFICATION_INTERNAL_PROCESS) {
		if (is_visible_in_tree()) {
			refresh_debugger_state();
		}
	}
}

void CSharpProjectPanel::set_status(const String &p_text, bool p_error) {
	if (p_error) {
		status_label->add_theme_color_override("font_color", Color(1.0f, 0.4f, 0.4f));
	} else {
		status_label->add_theme_color_override("font_color", Color(0.4f, 1.0f, 0.4f));
	}
	status_label->set_text(p_text);
}

void CSharpProjectPanel::append_output(const String &p_text) {
	output_log->append_text(p_text + "\n");
}

void CSharpProjectPanel::refresh_cs_files() {
	cs_files_list->clear();

	String project_dir = ProjectSettings::get_singleton()->globalize_path("res://");
	Ref<DirAccess> dir = DirAccess::open(project_dir);
	if (dir.is_null()) {
		cs_files_list->add_item("(cannot open project dir)");
		return;
	}

	List<String> dirs_to_scan;
	dirs_to_scan.push_back(project_dir);

	int count = 0;
	while (!dirs_to_scan.is_empty() && count < 200) {
		String current_dir = dirs_to_scan.front()->get();
		dirs_to_scan.pop_front();

		Ref<DirAccess> d = DirAccess::open(current_dir);
		if (d.is_null()) continue;

		d->list_dir_begin();
		String fname = d->get_next();
		while (!fname.is_empty()) {
			if (fname == "." || fname == ".." || fname.begins_with(".")) {
				fname = d->get_next();
				continue;
			}
			String full = current_dir.path_join(fname);
			if (d->current_is_dir()) {
				// Skip common non-source directories
				if (fname != ".mono" && fname != ".git" && fname != ".import" &&
						fname != "addons" && fname != ".godot") {
					dirs_to_scan.push_back(full);
				}
			} else if (fname.ends_with(".cs")) {
				String rel = full.substr(project_dir.length());
				cs_files_list->add_item(rel);
				count++;
			}
			fname = d->get_next();
		}
		d->list_dir_end();
	}

	if (count == 0) {
		cs_files_list->add_item("(no .cs files found)");
	}
}

void CSharpProjectPanel::refresh() {
	refresh_cs_files();
	refresh_debugger_state();

	String csproj = csharp_editor_get_csproj_path();
	String sln = csharp_editor_get_sln_path();

	bool has_csproj = FileAccess::exists(csproj);
	bool has_sln = FileAccess::exists(sln);

	String status;
	if (has_csproj && has_sln) {
		status = "Project files exist";
	} else if (has_csproj) {
		status = ".csproj exists (missing .sln)";
	} else {
		status = "No project files (click Create)";
	}
	set_status(status, !has_csproj);

	append_output("[b]C# Project Status[/b]");
	append_output("  .csproj: " + csproj + (has_csproj ? " [OK]" : " [MISSING]"));
	append_output("  .sln:    " + sln + (has_sln ? " [OK]" : " [MISSING]"));
}

void CSharpProjectPanel::_on_create_pressed() {
	append_output("\n[b]> Creating/refreshing project files...[/b]");
	bool ok = csharp_editor_ensure_project_solution();
	if (ok) {
		set_status("Project files created", false);
		append_output("  [color=green]OK[/color] - .csproj and .sln ready");
	} else {
		set_status("Failed to create project files", true);
		append_output("  [color=red]FAILED[/color] - see log for details");
	}
	refresh();
}

void CSharpProjectPanel::_on_compile_pressed() {
	append_output("\n[b]> Compiling C# project...[/b]");
	set_status("Compiling...", false);

	bool ok = csharp_editor_compile_project();
	if (ok) {
		set_status("Compilation succeeded", false);
		append_output("  [color=green]OK[/color] - assembly compiled successfully");

		// Try to load the new assembly
		GDMono *gdmono = GDMono::get_singleton();
		if (gdmono) {
			gdmono->clear_user_assemblies();
			append_output("  Assembly cleared. It will be loaded on next play/reload.");
		}
	} else {
		set_status("Compilation failed", true);
		append_output("  [color=red]FAILED[/color] - check compiler output above");
	}
}

void CSharpProjectPanel::_on_open_folder_pressed() {
	String project_dir = ProjectSettings::get_singleton()->globalize_path("res://");
	OS::get_singleton()->shell_open(project_dir);
	append_output("\n[b]> Opened project folder:[/b] " + project_dir);
}

void CSharpProjectPanel::_on_refresh_files_pressed() {
	refresh_cs_files();
	append_output("\n[b]> Refreshed .cs file list[/b]");
}

void CSharpProjectPanel::refresh_debugger_state() {
	if (!btn_debug_toggle || !debug_port_label || !debug_attach_label) {
		return;
	}

	bool requested = CSharpDebugger::is_requested();
	int port = CSharpDebugger::get_requested_port();
	bool attached = CSharpDebugger::is_attached();

	if (requested) {
		btn_debug_toggle->set_text("Debugger: On");
		debug_port_label->set_text("Port: " + itos(port));
		if (attached) {
			debug_attach_label->set_text("Attached");
			debug_attach_label->add_theme_color_override("font_color", Color(0.4f, 1.0f, 0.4f));
		} else {
			debug_attach_label->set_text("Listening (waiting for IDE)");
			debug_attach_label->add_theme_color_override("font_color", Color(1.0f, 0.8f, 0.4f));
		}
	} else {
		btn_debug_toggle->set_text("Debugger: Off");
		debug_port_label->set_text("Port: (disabled)");
		debug_attach_label->set_text("Not active");
		debug_attach_label->add_theme_color_override("font_color", Color(0.6f, 0.6f, 0.6f));
	}
}

void CSharpProjectPanel::_on_debug_toggle_pressed() {
	// The SDB agent must be configured before mono_jit_init_version(), so
	// we cannot hot-toggle it. Show instructions instead.
	bool requested = CSharpDebugger::is_requested();

	String message;
	if (requested) {
		int port = CSharpDebugger::get_requested_port();
		message = "Mono SDB debugger is currently ACTIVE on port " + itos(port) + ".\n\n";
		if (CSharpDebugger::is_attached()) {
			message += "An IDE is currently attached.\n";
		} else {
			message += "No IDE is attached yet. In VS Code:\n";
			message += "  1. Install the 'Mono Debug' extension\n";
			message += "  2. Add to .vscode/launch.json:\n";
			message += "     {\n";
			message += "       \"type\": \"mono\",\n";
			message += "       \"request\": \"attach\",\n";
			message += "       \"address\": \"localhost\",\n";
			message += "       \"port\": " + itos(port) + "\n";
			message += "     }\n";
			message += "  3. Press F5 to attach\n";
		}
		message += "\nTo disable the debugger, restart Godot without --mono-debugger.";
	} else {
		message = "Mono SDB debugger is currently OFF.\n\n";
		message += "To enable it, restart Godot with the command-line argument:\n";
		message += "  --mono-debugger=" + itos(CSharpDebugger::DEFAULT_DEBUGGER_PORT) + "\n\n";
		message += "Then attach VS Code (with Mono Debug extension) using launch.json:\n";
		message += "  {\n";
		message += "    \"type\": \"mono\",\n";
		message += "    \"request\": \"attach\",\n";
		message += "    \"address\": \"localhost\",\n";
		message += "    \"port\": " + itos(CSharpDebugger::DEFAULT_DEBUGGER_PORT) + "\n";
		message += "  }";
	}

	AcceptDialog *dlg = memnew(AcceptDialog);
	dlg->set_title("Mono SDB Debugger");
	dlg->set_text(message);
	dlg->set_exclusive(true);
	add_child(dlg);
	dlg->popup_centered(Vector2i(600, 400));

	// Log the message too so it's searchable.
	append_output("\n[b]> Debugger toggle:[/b]");
	append_output(message.replace("\n", "\n  "));

	refresh_debugger_state();
}
