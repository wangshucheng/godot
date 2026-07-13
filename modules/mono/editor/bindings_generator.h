#pragma once

#include "core/string/ustring.h"
#include "core/string/string_name.h"
#include "core/templates/list.h"

// BindingsGenerator: generates C# wrapper classes from Godot's ClassDB metadata.
//
// Usage: godot --generate-csharp-bindings [--output=path]
//
// The generator iterates all registered ClassDB classes, extracts their methods,
// properties, and signals, and emits a single C# source file containing typed
// wrapper classes. The generated wrappers use the existing string-based
// Call/Get/Set pattern (WASM-safe, no new icalls required).
//
// Generated classes follow this pattern:
//   public class Node : Object {
//       public Node() : base("Node") {}
//       public string Name { get => Get("name") as string; set => Set("name", value); }
//       public void AddChild(Node child) { Call("add_child", child); }
//   }
class BindingsGenerator {
public:
	// Entry point called from register_types.cpp when --generate-csharp-bindings
	// is present on the command line.
	static void handle_cmdline_args(const List<String> &p_cmdline_args);

private:
	// Generate the complete bindings file and write it to p_output_path.
	// Returns true on success.
	static bool generate(const String &p_output_path);

	// Generate C# source for a single class. Returns the source text.
	static String generate_class(const StringName &p_class_name);

	// Map a Godot Variant type to a C# type name.
	static String godot_type_to_csharp(int p_variant_type, const StringName &p_class_hint = StringName());

	// Check if a class should be included in bindings (skip internal/no-API classes).
	static bool should_generate_class(const StringName &p_class_name);

	// Get the C# base class name for a Godot class.
	static String get_csharp_base_class(const StringName &p_class_name);

	// Sanitize a Godot method/property name to valid C# identifier (snake_case -> PascalCase).
	static String to_pascal_case(const String &p_snake_case);

	// Check if a method should be generated (skip private, virtual, deprecated).
	static bool should_generate_method(const String &p_class_name, const String &p_method_name);
};
