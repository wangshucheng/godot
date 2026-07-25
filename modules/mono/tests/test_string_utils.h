/**************************************************************************/
/*  test_string_utils.h                                                   */
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

#include "tests/test_macros.h"

#include "../utils/string_utils.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "core/string/ustring.h"

// A3: Unit tests for string_utils. Verifies:
//   - sformat: positional %s and %0-%5 placeholder substitution, edge cases
//   - is_csharp_keyword / escape_csharp_keyword (TOOLS_ENABLED only):
//     C# reserved keywords (77 in our impl), '@'-prefix escaping, non-keyword
//     passthrough, case-sensitivity
//   - read_all_file_utf8: read existing file (utf-8 content), error on
//     non-existent path, error on empty path
// Run: godot --test "[StringUtils]"

namespace TestStringUtils {

// ============================================================================
// sformat tests
// ============================================================================

TEST_CASE("[StringUtils] sformat with no placeholders returns input") {
	// No %s or %N markers — string should pass through unchanged.
	CHECK(sformat("hello world") == "hello world");
	CHECK(sformat("") == "");
	CHECK(sformat("a") == "a");
	CHECK(sformat("100% done") == "100% done"); // '%' not followed by valid marker
}

TEST_CASE("[StringUtils] sformat with single %s placeholder") {
	CHECK(sformat("Hello %s!", "World") == "Hello World!");
	CHECK(sformat("%s", "alone") == "alone");
	CHECK(sformat("prefix %s", "value") == "prefix value");
	CHECK(sformat("%s suffix", "value") == "value suffix");
}

TEST_CASE("[StringUtils] sformat with multiple %s placeholders") {
	// %s consumes positional args in order (findex increments per %s use).
	CHECK(sformat("%s %s", "a", "b") == "a b");
	CHECK(sformat("%s-%s-%s", "x", "y", "z") == "x-y-z");
	CHECK(sformat("%s %s %s %s %s %s", "1", "2", "3", "4", "5", "6") == "1 2 3 4 5 6");
}

TEST_CASE("[StringUtils] sformat with explicit positional %0-%5") {
	// %N explicitly picks the N-th argument (0-based).
	CHECK(sformat("%0", "a") == "a");
	CHECK(sformat("%0 %1", "a", "b") == "a b");
	CHECK(sformat("%1 %0", "a", "b") == "b a"); // reverse order
	CHECK(sformat("%0-%1-%2-%3-%4-%5", "0", "1", "2", "3", "4", "5") == "0-1-2-3-4-5");
}

TEST_CASE("[StringUtils] sformat mixing %s and positional") {
	// Mix of %s (sequential) and %N (explicit). %s increments findex independently
	// of %N (which directly indexes args by digit).
	// Trace for "%s = %1" with args ("key","value"):
	//   %s  → findex=0, uses args[0]="key", findex becomes 1
	//   %1  → uses args[1]="value"
	CHECK(sformat("%s = %1", "key", "value") == "key = value");
	// Trace for "%0 %s %1" with args ("a","b","c"):
	//   %0  → uses args[0]="a" (findex unchanged, stays 0)
	//   %s  → findex=0, uses args[0]="a", findex becomes 1
	//   %1  → uses args[1]="b"
	CHECK(sformat("%0 %s %1", "a", "b", "c") == "a a b");
}

TEST_CASE("[StringUtils] sformat with empty arguments") {
	// Empty String arguments should produce empty substitutions.
	CHECK(sformat("[%s]", String()) == "[]");
	CHECK(sformat("%s%s", "a", String()) == "a");
}

TEST_CASE("[StringUtils] sformat edge cases") {
	// Length < 2: pass through unchanged.
	CHECK(sformat("") == "");
	CHECK(sformat("a") == "a");
	// Lone '%' at end of string (no following char) — pass through.
	CHECK(sformat("trailing %", "x") == "trailing %");
	// '%' followed by invalid char — pass through unchanged.
	CHECK(sformat("100%d", String()) == "100%d");
}

#ifdef TOOLS_ENABLED
// ============================================================================
// is_csharp_keyword / escape_csharp_keyword tests (TOOLS_ENABLED only)
// ============================================================================

TEST_CASE("[StringUtils] is_csharp_keyword recognizes common keywords") {
	// Sample of the C# reserved keywords (string_utils.cpp implements 77).
	CHECK(is_csharp_keyword("abstract"));
	CHECK(is_csharp_keyword("class"));
	CHECK(is_csharp_keyword("event"));
	CHECK(is_csharp_keyword("public"));
	CHECK(is_csharp_keyword("static"));
	CHECK(is_csharp_keyword("void"));
	CHECK(is_csharp_keyword("return"));
	CHECK(is_csharp_keyword("if"));
	CHECK(is_csharp_keyword("else"));
	CHECK(is_csharp_keyword("while"));
	CHECK(is_csharp_keyword("for"));
	CHECK(is_csharp_keyword("foreach"));
	CHECK(is_csharp_keyword("new"));
	CHECK(is_csharp_keyword("this"));
	CHECK(is_csharp_keyword("base"));
	CHECK(is_csharp_keyword("namespace"));
	CHECK(is_csharp_keyword("using"));
	CHECK(is_csharp_keyword("string"));
	CHECK(is_csharp_keyword("object"));
	CHECK(is_csharp_keyword("bool"));
	CHECK(is_csharp_keyword("int"));
	CHECK(is_csharp_keyword("float"));
	CHECK(is_csharp_keyword("double"));
	CHECK(is_csharp_keyword("typeof"));
	CHECK(is_csharp_keyword("stackalloc"));
	CHECK(is_csharp_keyword("unchecked"));
	CHECK(is_csharp_keyword("unsafe"));
}

TEST_CASE("[StringUtils] is_csharp_keyword returns false for non-keywords") {
	CHECK_FALSE(is_csharp_keyword("Godot"));
	CHECK_FALSE(is_csharp_keyword("Node"));
	CHECK_FALSE(is_csharp_keyword("myMethod"));
	CHECK_FALSE(is_csharp_keyword(""));
	CHECK_FALSE(is_csharp_keyword("Class")); // case-sensitive: 'Class' != 'class'
	CHECK_FALSE(is_csharp_keyword("Public")); // case-sensitive
	CHECK_FALSE(is_csharp_keyword("int32")); // substring, not exact match
	CHECK_FALSE(is_csharp_keyword("event_handler")); // underscore-suffixed
	// Contextual keywords are NOT reserved (C# language spec).
	CHECK_FALSE(is_csharp_keyword("var"));
	CHECK_FALSE(is_csharp_keyword("async"));
	CHECK_FALSE(is_csharp_keyword("await"));
	CHECK_FALSE(is_csharp_keyword("yield"));
	CHECK_FALSE(is_csharp_keyword("partial"));
}

TEST_CASE("[StringUtils] escape_csharp_keyword prefixes keywords with @") {
	CHECK(escape_csharp_keyword("class") == "@class");
	CHECK(escape_csharp_keyword("event") == "@event");
	CHECK(escape_csharp_keyword("return") == "@return");
	CHECK(escape_csharp_keyword("static") == "@static");
	CHECK(escape_csharp_keyword("void") == "@void");
	CHECK(escape_csharp_keyword("namespace") == "@namespace");
	CHECK(escape_csharp_keyword("this") == "@this");
	CHECK(escape_csharp_keyword("base") == "@base");
}

TEST_CASE("[StringUtils] escape_csharp_keyword passes non-keywords through") {
	CHECK(escape_csharp_keyword("Godot") == "Godot");
	CHECK(escape_csharp_keyword("Node") == "Node");
	CHECK(escape_csharp_keyword("myMethod") == "myMethod");
	CHECK(escape_csharp_keyword("") == "");
	CHECK(escape_csharp_keyword("Class") == "Class"); // not a keyword (case-sensitive)
	CHECK(escape_csharp_keyword("public_field") == "public_field");
}
#endif // TOOLS_ENABLED

// ============================================================================
// read_all_file_utf8 tests
// ============================================================================

TEST_CASE("[StringUtils] read_all_file_utf8 reads existing file") {
	// Strategy: write a temp file with known UTF-8 content next to the test
	// binary (bin/ directory, sandbox-allowed since it's inside the project
	// tree), then read it back.
	const String exe_path = OS::get_singleton()->get_executable_path();
	const String tmp_dir = exe_path.get_base_dir();
	const String test_filename = "godot_test_string_utils_utf8.txt";
	const String test_path = tmp_dir.path_join(test_filename);

	const String expected = "Hello, UTF-8! 123\n";
	{
		Ref<FileAccess> f = FileAccess::open(test_path, FileAccess::WRITE);
		CHECK_MESSAGE(f.is_valid(), "Cannot open temp file for write");
		if (!f.is_valid()) {
			return; // Skip test if file cannot be opened (sandbox restriction)
		}
		f->store_string(expected);
	}

	String content;
	Error err = read_all_file_utf8(test_path, content);
	CHECK(err == OK);
	CHECK(content == expected);

	// Cleanup.
	Ref<DirAccess> da = DirAccess::open(tmp_dir);
	if (da.is_valid()) {
		da->remove(test_filename);
	}
}

TEST_CASE("[StringUtils] read_all_file_utf8 returns error on missing file") {
	String content;
	Error err = read_all_file_utf8("res://nonexistent_string_utils_test_file.txt", content);
	CHECK(err != OK);
}

TEST_CASE("[StringUtils] read_all_file_utf8 returns error on empty path") {
	String content;
	Error err = read_all_file_utf8("", content);
	CHECK(err != OK);
}

} // namespace TestStringUtils
