#pragma once

#include "core/string/ustring.h"

namespace Path {

String find_executable(const String &p_name);

String join(const String &p_a, const String &p_b);
String join(const String &p_a, const String &p_b, const String &p_c);
String join(const String &p_a, const String &p_b, const String &p_c, const String &p_d);

String cwd();
String abspath(const String &p_path);
String realpath(const String &p_path);
String relative_to(const String &p_path, const String &p_relative_to);

String get_csharp_project_name();
} // namespace Path
