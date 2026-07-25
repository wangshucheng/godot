/**************************************************************************/
/*  string_utils.h                                                        */
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

#include "core/string/ustring.h"

// A3: C# string utilities. Ported from upstream 1963b2f modules/mono/utils/string_utils.{h,cpp}.
// str_format/str_format_new are NOT ported — they have no consumers in the
// current codebase (per spec §A3). is_csharp_keyword/escape_csharp_keyword
// are TOOLS-only (editor-only consumption in bindings_generator).

// Format a string with up to 6 positional %s/%0-%5 placeholders.
// Used by upstream mono editor for runtime string formatting.
String sformat(const String &p_text, const String &p1 = String(), const String &p2 = String(),
		const String &p3 = String(), const String &p4 = String(), const String &p5 = String(), const String &p6 = String());

#ifdef TOOLS_ENABLED
// Returns true if p_name is a C# reserved keyword (79 keywords).
bool is_csharp_keyword(const String &p_name);

// Prefixes C# reserved keywords with '@' to make them valid identifiers.
// e.g. "class" → "@class", "event" → "@event". Non-keywords returned as-is.
String escape_csharp_keyword(const String &p_name);
#endif

// Read a UTF-8 file fully into r_content. Returns OK or an error code.
Error read_all_file_utf8(const String &p_path, String &r_content);
