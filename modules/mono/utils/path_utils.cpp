#include "path_utils.h"
#include "core/os/os.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"

namespace Path {

String find_executable(const String &p_name) {
	return "";
}

String join(const String &p_a, const String &p_b) {
	return p_a.path_join(p_b);
}

String join(const String &p_a, const String &p_b, const String &p_c) {
	return p_a.path_join(p_b).path_join(p_c);
}

String join(const String &p_a, const String &p_b, const String &p_c, const String &p_d) {
	return p_a.path_join(p_b).path_join(p_c).path_join(p_d);
}

String cwd() {
	return OS::get_singleton()->get_cwd();
}

String abspath(const String &p_path) {
	if (p_path.is_absolute_path()) {
		return p_path.simplify_path();
	}
	return cwd().path_join(p_path).simplify_path();
}

String realpath(const String &p_path) {
	return abspath(p_path);
}

String relative_to(const String &p_path, const String &p_relative_to) {
	String rel_to = abspath(p_relative_to);
	String abs_p = abspath(p_path);
	if (abs_p.begins_with(rel_to)) {
		String result = abs_p.substr(rel_to.length());
		if (result.begins_with("/") || result.begins_with("\\")) {
			result = result.substr(1);
		}
		return result;
	}
	return p_path;
}

String get_csharp_project_name() {
	if (ProjectSettings::get_singleton()) {
		String name = ProjectSettings::get_singleton()->get_setting("application/config/name", "GodotProject");
		if (!name.is_empty()) {
			return name;
		}
	}
	return "GodotProject";
}
} // namespace Path
