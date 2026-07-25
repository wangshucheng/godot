/**************************************************************************/
/*  naming_utils.h                                                        */
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

// A3: C# identifier naming utilities. Ported verbatim from upstream 1963b2f
// modules/mono/utils/naming_utils.{h,cpp}. Used by bindings_generator and
// csharp_script::make_template to convert Godot snake_case names to
// PascalCase/camelCase C# identifiers with upstream-compatible overrides
// (e.g. BitMap→Bitmap, JSONRPC→JsonRpc, Object→GodotObject).

// Convert a PascalCase identifier to canonical PascalCase, applying
// hardcoded name/part overrides (e.g. "BitMap"→"Bitmap", "Object"→"GodotObject").
String pascal_to_pascal_case(const String &p_identifier);

// Convert a snake_case identifier to PascalCase. p_input_is_upper indicates
// the input uses UPPER_SNAKE_CASE convention (forces lowercase of non-first
// letters in each part). Applies the same overrides as pascal_to_pascal_case.
String snake_to_pascal_case(const String &p_identifier, bool p_input_is_upper = false);

// Convert a snake_case identifier to camelCase (first part lowercase, rest
// capitalized). p_input_is_upper same as above.
String snake_to_camel_case(const String &p_identifier, bool p_input_is_upper = false);
