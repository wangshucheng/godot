/**************************************************************************/
/*  test_naming_utils.h                                                   */
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

#include "../utils/naming_utils.h"

// A3: Unit tests for naming_utils. Verifies behavior for the spec-mandated
// boundary cases (`_ready`→`Ready`, `HTTPRequest`→`HttpRequest` etc.) and
// upstream overrides (BitMap→Bitmap, JSONRPC→JsonRpc, Object→GodotObject).
// Run: godot --test "[NamingUtils]"

TEST_CASE("[NamingUtils] snake_to_pascal_case basic") {
	CHECK(snake_to_pascal_case("ready") == "Ready");
	CHECK(snake_to_pascal_case("_ready") == "_Ready");
	CHECK(snake_to_pascal_case("get_node_count") == "GetNodeCount");
	CHECK(snake_to_pascal_case("add_child") == "AddChild");
	// Upstream quirk: empty string splits to [""], which is treated as a leading
	// underscore (preserved at beginning). Returns "_" rather than "".
	CHECK(snake_to_pascal_case("") == "_");
}

TEST_CASE("[NamingUtils] snake_to_pascal_case with leading underscore") {
	CHECK(snake_to_pascal_case("_ready") == "_Ready");
	CHECK(snake_to_pascal_case("__internal") == "__Internal");
	CHECK(snake_to_pascal_case("trailing_") == "Trailing_");
}

TEST_CASE("[NamingUtils] snake_to_pascal_case with overrides") {
	CHECK(snake_to_pascal_case("node_path") == "NodePath");
	CHECK(snake_to_pascal_case("ip") == "IP");
	CHECK(snake_to_pascal_case("io") == "IO");
	CHECK(snake_to_pascal_case("uv") == "UV");
	CHECK(snake_to_pascal_case("xr") == "XR");
	CHECK(snake_to_pascal_case("file_name") == "FileName");
}

TEST_CASE("[NamingUtils] snake_to_pascal_case upper snake input") {
	CHECK(snake_to_pascal_case("GET_NODE_COUNT", true) == "GetNodeCount");
	CHECK(snake_to_pascal_case("ADD_CHILD", true) == "AddChild");
}

TEST_CASE("[NamingUtils] snake_to_camel_case basic") {
	CHECK(snake_to_camel_case("get_node_count") == "getNodeCount");
	CHECK(snake_to_camel_case("add_child") == "addChild");
	CHECK(snake_to_camel_case("ready") == "ready");
	// Upstream quirk: same as pascal_case, empty string returns "_".
	CHECK(snake_to_camel_case("") == "_");
}

TEST_CASE("[NamingUtils] snake_to_camel_case with overrides") {
	// First-part override: lowercase the first char of the override.
	CHECK(snake_to_camel_case("node_path") == "nodePath");
	// Upstream quirk: acronym override "IP" only has its FIRST char lowercased
	// for camelCase, leaving 'P' uppercase. Result: "iPAddress" not "ipAddress".
	CHECK(snake_to_camel_case("ip_address") == "iPAddress");
}

TEST_CASE("[NamingUtils] pascal_to_pascal_case name overrides") {
	CHECK(pascal_to_pascal_case("BitMap") == "Bitmap");
	CHECK(pascal_to_pascal_case("JSONRPC") == "JsonRpc");
	CHECK(pascal_to_pascal_case("Object") == "GodotObject");
	CHECK(pascal_to_pascal_case("Thread") == "GodotThread");
	CHECK(pascal_to_pascal_case("System") == "System_");
}

TEST_CASE("[NamingUtils] pascal_to_pascal_case splitting") {
	// Compound PascalCase identifiers should be split and rejoined.
	CHECK(pascal_to_pascal_case("HTTPRequest") == "HttpRequest");
	// Upstream quirk: name_overrides (like JSONRPC→JsonRpc) only apply to the
	// WHOLE identifier, not to split parts. "JSONRPCServer" splits to
	// ["JSONRPC", "Server"]; "JSONRPC" as a PART is not in part_overrides, so
	// it gets default title-casing → "Jsonrpc" + "Server" = "JsonrpcServer".
	CHECK(pascal_to_pascal_case("JSONRPCServer") == "JsonrpcServer");
}

TEST_CASE("[NamingUtils] pascal_to_pascal_case acronym handling") {
	// Acronyms of length 1-2 are upper-cased intact.
	CHECK(pascal_to_pascal_case("Node2D") == "Node2D");
	CHECK(pascal_to_pascal_case("AA") == "AA");
	CHECK(pascal_to_pascal_case("IO") == "IO");
}

TEST_CASE("[NamingUtils] pascal_to_pascal_case empty and short") {
	CHECK(pascal_to_pascal_case("") == "");
	CHECK(pascal_to_pascal_case("A") == "A");
	CHECK(pascal_to_pascal_case("AB") == "AB");
}
