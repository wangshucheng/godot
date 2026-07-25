/**************************************************************************/
/*  mono_script_metadata.h                                                */
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

#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>

#include "core/object/method_info.h"
#include "core/string/ustring.h"
#include "core/templates/list.h"
#include "core/variant/variant.h"

// B0 §3.2: C# script metadata reader — direct Mono metadata access (no C# reflection).
// Reads [Export], [Signal], [Tool], [GlobalClass] attributes from MonoClass fields,
// properties, nested types, and class-level metadata. Used by P1 (Inspector),
// P2 (GlobalClass), P3 (Signal), P5 (Tool).
//
// All functions are AOT-safe: they use mono_custom_attrs_from_member + string
// comparison on attribute class names, never instantiating attribute objects.

namespace mono_script_meta {

// Check if a MonoCustomAttrInfo contains an attribute with the given class name.
// p_attr_name is the simple class name (e.g. "ExportAttribute", not "Godot.ExportAttribute").
// This iterates attrs[i].ctor and compares mono_method_get_class(ctor) name.
// Does NOT construct the attribute instance — AOT-safe.
bool has_attribute(MonoCustomAttrInfo *p_info, const char *p_attr_name);

// Map a MonoType* to Variant::Type. Covers bool/int/float/string, all Godot math
// structs (Vector2/Color/etc.), enums, Object derivatives, and collection wrappers.
// Returns Variant::NIL for unsupported types (caller should skip such members).
Variant::Type mono_type_to_variant_type(MonoType *p_type);

// Exported member descriptor (field or property with [Export] attribute).
struct ExportedMember {
	StringName name;       // C# member name (as-is, not snake_case converted)
	Variant::Type type;    // Mapped Variant type
	bool is_field;         // true = field, false = property
	MonoClassField *field; // Valid when is_field=true
	MonoProperty *prop;    // Valid when is_field=false
};

// Collect all [Export]-marked fields and properties from p_class and its base
// classes (walks up the hierarchy until reaching Godot.Object).
// Deduplicates by member name (first occurrence wins).
void collect_exported_members(MonoClass *p_class, List<ExportedMember> &r_out);

// Collect all [Signal]-marked nested delegates from p_class.
// Convention: delegate named <SignalName>EventHandler, parent class is
// MulticastDelegate, has [Signal] attribute. Builds MethodInfo from the
// delegate's Invoke method signature.
void collect_signals(MonoClass *p_class, List<MethodInfo> &r_out);

// Check if p_class has a class-level attribute with the given name.
// Convenience wrapper: gets MonoCustomAttrInfo for the class, then calls has_attribute.
bool class_has_attribute(MonoClass *p_class, const char *p_attr_name);

} // namespace mono_script_meta
