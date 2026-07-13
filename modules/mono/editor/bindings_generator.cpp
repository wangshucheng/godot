#include "bindings_generator.h"

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/io/file_access.h"
#include "core/variant/variant.h"
#include "core/templates/local_vector.h"

#include <stdio.h>

// Hand-written wrapper classes that already exist in GodotSharp.
// The generator skips these to avoid duplicates.
static const char *existing_wrappers[] = {
	"Object", "RefCounted", "Node", "Resource", "PackedScene",
	"CanvasItem", "CanvasLayer", "Control", "Label",
	"WebSocketPeer", "SceneTree", "InputEvent",
	// Static/helper classes already defined in GodotSharp
	"Input", "GD", "Platform", "Reflection", "Runtime", "Tasking",
	"Callable", "SignalAwaiter", "GodotSynchronizationContext",
	"PreserveAttribute",
	nullptr
};

static bool is_existing_wrapper(const String &p_class_name) {
	for (int i = 0; existing_wrappers[i] != nullptr; i++) {
		if (p_class_name == existing_wrappers[i]) {
			return true;
		}
	}
	return false;
}

// Godot types that are too complex to auto-generate (typed arrays, etc.)
static bool is_supported_return_type(const PropertyInfo &p_prop) {
	switch (p_prop.type) {
		case Variant::NIL:
		case Variant::BOOL:
		case Variant::INT:
		case Variant::FLOAT:
		case Variant::STRING:
		case Variant::VECTOR2:
		case Variant::VECTOR2I:
		case Variant::VECTOR3:
		case Variant::VECTOR3I:
		case Variant::VECTOR4:
		case Variant::VECTOR4I:
		case Variant::COLOR:
		case Variant::OBJECT:
		case Variant::CALLABLE:
		case Variant::SIGNAL:
		case Variant::STRING_NAME:
		case Variant::NODE_PATH:
		case Variant::RID:
			return true;
		case Variant::ARRAY:
		case Variant::PACKED_BYTE_ARRAY:
		case Variant::PACKED_INT32_ARRAY:
		case Variant::PACKED_INT64_ARRAY:
		case Variant::PACKED_FLOAT32_ARRAY:
		case Variant::PACKED_FLOAT64_ARRAY:
		case Variant::PACKED_STRING_ARRAY:
		case Variant::PACKED_VECTOR2_ARRAY:
		case Variant::PACKED_VECTOR3_ARRAY:
		case Variant::PACKED_COLOR_ARRAY:
		case Variant::DICTIONARY:
			// Arrays/dicts are supported via Variant
			return true;
		default:
			return false;
	}
}

void BindingsGenerator::handle_cmdline_args(const List<String> &p_cmdline_args) {
	bool do_generate = false;
	String output_path = "GeneratedBindings.cs";

	for (const String &arg : p_cmdline_args) {
		if (arg == "--generate-csharp-bindings") {
			do_generate = true;
		} else if (arg.begins_with("--bindings-output=")) {
			do_generate = true;
			output_path = arg.substr(String("--bindings-output=").length());
		}
	}

	if (do_generate) {
		printf("[BindingsGenerator] Generating C# bindings to %s...\n", output_path.utf8().get_data());
		bool ok = generate(output_path);
		if (ok) {
			printf("[BindingsGenerator] Success! Bindings written to %s\n", output_path.utf8().get_data());
		} else {
			printf("[BindingsGenerator] FAILED to generate bindings.\n");
		}
	}
}

// Prefix C# reserved keywords with '@' to make them valid identifiers.
static String sanitize_csharp_identifier(const String &p_name) {
	static const char *reserved[] = {
		"abstract", "as", "base", "bool", "break", "byte", "case", "catch",
		"char", "checked", "class", "const", "continue", "decimal", "default",
		"delegate", "do", "double", "else", "enum", "event", "explicit",
		"extern", "false", "finally", "fixed", "float", "for", "foreach",
		"goto", "if", "implicit", "in", "int", "interface", "internal", "is",
		"lock", "long", "namespace", "new", "null", "object", "operator",
		"out", "override", "params", "private", "protected", "public",
		"readonly", "ref", "return", "sbyte", "sealed", "short", "sizeof",
		"stackalloc", "static", "string", "struct", "switch", "this", "throw",
		"true", "try", "typeof", "uint", "ulong", "unchecked", "unsafe",
		"ushort", "using", "virtual", "void", "volatile", "while", nullptr
	};
	for (int i = 0; reserved[i] != nullptr; i++) {
		if (p_name == reserved[i]) {
			return "@" + p_name;
		}
	}
	return p_name;
}

String BindingsGenerator::to_pascal_case(const String &p_snake_case) {
	if (p_snake_case.is_empty()) return p_snake_case;

	String result;
	bool capitalize_next = true;
	for (int i = 0; i < p_snake_case.length(); i++) {
		char32_t c = p_snake_case[i];
		// Treat _, /, - as word separators (Godot uses / for grouped properties)
		if (c == '_' || c == '/' || c == '-') {
			capitalize_next = true;
		} else if (capitalize_next) {
			result += String::char_uppercase(c);
			capitalize_next = false;
		} else {
			result += c;
		}
	}
	// If result starts with a digit, prefix with underscore (valid C# identifier)
	if (result.length() > 0 && result[0] >= '0' && result[0] <= '9') {
		result = "_" + result;
	}
	return result;
}

String BindingsGenerator::godot_type_to_csharp(int p_variant_type, const StringName &p_class_hint) {
	switch (p_variant_type) {
		case Variant::NIL:
			return "object";
		case Variant::BOOL:
			return "bool";
		case Variant::INT:
			return "long";
		case Variant::FLOAT:
			return "double";
		case Variant::STRING:
		case Variant::STRING_NAME:
		case Variant::NODE_PATH:
			return "string";
		case Variant::VECTOR2:
			return "Vector2";
		case Variant::VECTOR2I:
			return "Vector2I";
		case Variant::VECTOR3:
			return "Vector3";
		case Variant::VECTOR3I:
			return "Vector3I";
		case Variant::VECTOR4:
			return "Vector4";
		case Variant::VECTOR4I:
			return "Vector4I";
		case Variant::COLOR:
			return "Color";
		case Variant::CALLABLE:
			return "Callable";
		case Variant::SIGNAL:
			return "Signal";
		case Variant::RID:
			return "RID";
		case Variant::OBJECT:
		if (p_class_hint != StringName() && p_class_hint != String("Object")) {
			String hint = String(p_class_hint);
			// class_name can be comma-separated (e.g. "CameraAttributesPractical,CameraAttributesPhysical")
			// Take the first type from the list.
			int comma_pos = hint.find(",");
			if (comma_pos >= 0) {
				hint = hint.substr(0, comma_pos);
			}
			if (hint.is_empty() || hint == "Object") {
				return "Godot.Object";
			}
			return hint;
		}
		return "Godot.Object";
		case Variant::ARRAY:
		case Variant::PACKED_BYTE_ARRAY:
		case Variant::PACKED_INT32_ARRAY:
		case Variant::PACKED_INT64_ARRAY:
		case Variant::PACKED_FLOAT32_ARRAY:
		case Variant::PACKED_FLOAT64_ARRAY:
		case Variant::PACKED_STRING_ARRAY:
		case Variant::PACKED_VECTOR2_ARRAY:
		case Variant::PACKED_VECTOR3_ARRAY:
		case Variant::PACKED_COLOR_ARRAY:
			return "Godot.Collections.Array";
		case Variant::DICTIONARY:
			return "Godot.Collections.Dictionary";
		default:
			return "object";
	}
}

bool BindingsGenerator::should_generate_class(const StringName &p_class_name) {
	String name = p_class_name;

	// Skip classes that don't exist
	if (!ClassDB::class_exists(p_class_name)) return false;

	// Skip already hand-written wrappers
	if (is_existing_wrapper(name)) return false;

	// Skip internal/hidden classes
	if (name.begins_with("_") || name.begins_with("@")) return false;

	// Skip editor-only and tool classes that aren't useful at runtime
	// (but keep them if they're commonly used)
	static const char *skip_classes[] = {
		"EditorDebuggerPlugin", "EditorDebuggerSession", "EditorDebuggerTree",
		"EditorExportPlugin", "EditorExportPlatform", "EditorExportPlatformPC",
		"EditorFeatureProfile", "EditorFileSystem", "EditorFileSystemDirectory",
		"EditorFileSystemImportFormatSupportQuery", "EditorImportPlugin",
		"EditorInspector", "EditorInspectorPlugin", "EditorInterface",
		"EditorNode3DGizmo", "EditorNode3DGizmoPlugin", "EditorPaths",
		"EditorPlugin", "EditorProperty", "EditorResourceConversionPlugin",
		"EditorResourcePicker", "EditorResourcePreview", "EditorResourcePreviewGenerator",
		"EditorSceneFormatImporter", "EditorScenePostImport", "EditorScenePostImportPlugin",
		"EditorScript", "EditorScriptPicker", "EditorSelection",
		"EditorSettings", "EditorSpinSlider", "EditorSyntaxHighlighter",
		"EditorTranslationParserPlugin", "EditorUndoRedoManager",
		"EditorVCSInterface", "FileSystemDock", "ScriptCreateDialog",
		"ScriptEditor", "ScriptEditorBase", "FXEditor",
		"ResourceImporter", "ResourceImporterBMFont", "ResourceImporterCSVTranslation",
		"ResourceImporterDynamicFont", "ResourceImporterImage", "ResourceImporterImageFont",
		"ResourceImporterOBJ", "ResourceImporterScene", "ResourceImporterShaderFile",
		"ResourceImporterTexture", "ResourceImporterTextureAtlas", "ResourceImporterWAV",
		nullptr
	};

	for (int i = 0; skip_classes[i] != nullptr; i++) {
		if (name == skip_classes[i]) return false;
	}

	return true;
}

String BindingsGenerator::get_csharp_base_class(const StringName &p_class_name) {
	StringName parent = ClassDB::get_parent_class(p_class_name);

	if (parent == StringName()) return "Godot.Object";

	String parent_str = parent;

	// Map to existing C# wrapper
	if (parent_str == "Object") return "Godot.Object";
	if (parent_str == "RefCounted") return "Godot.RefCounted";
	if (parent_str == "Node") return "Godot.Node";
	if (parent_str == "Resource") return "Godot.Resource";
	if (parent_str == "CanvasItem") return "Godot.CanvasItem";
	if (parent_str == "CanvasLayer") return "Godot.CanvasLayer";
	if (parent_str == "Control") return "Godot.Control";
	if (parent_str == "Label") return "Godot.Label";
	if (parent_str == "WebSocketPeer") return "Godot.WebSocketPeer";
	if (parent_str == "SceneTree") return "Godot.SceneTree";
	if (parent_str == "InputEvent") return "Godot.InputEvent";
	if (parent_str == "PackedScene") return "Godot.PackedScene";

	// For other parents, check if they'll be generated
	if (should_generate_class(parent)) {
		return String(parent);
	}

	// Default to Godot.Object for unknown parents
	return "Godot.Object";
}

bool BindingsGenerator::should_generate_method(const String &p_class_name, const String &p_method_name) {
	// Skip private methods (start with _)
	if (p_method_name.begins_with("_")) return false;

	// Skip virtual methods (they're meant to be overridden, not called)
	static const char *virtual_methods[] = {
		"ready", "process", "physics_process", "enter_tree", "exit_tree",
		"input", "unhandled_input", "shortcut_input", "unhandled_key_input",
		"notification", "get_configuration_warnings", "get_configuration_warning",
		"to_string", "get_property_list", "property_can_revert", "property_get_revert",
		"validate_property", "_get", "_set", "_get_property_list", "_to_string",
		"draw", "get_drag_data", "can_drop_data", "drop_data", "gui_input",
		"make_custom_tooltip", "_make_custom_tooltip",
		nullptr
	};
	for (int i = 0; virtual_methods[i] != nullptr; i++) {
		if (p_method_name == virtual_methods[i]) return false;
	}

	// Skip notification/internal methods
	if (p_method_name.begins_with("_internal_")) return false;

	// Skip methods that would conflict with System.Object methods in C#
	static const char *object_method_conflicts[] = {
		"get_type",  // Conflicts with Object.GetType()
		"to_string", // Already in virtual_methods, but listed for clarity
		nullptr
	};
	for (int i = 0; object_method_conflicts[i] != nullptr; i++) {
		if (p_method_name == object_method_conflicts[i]) return false;
	}

	return true;
}

String BindingsGenerator::generate_class(const StringName &p_class_name) {
	String class_name = p_class_name;
	String cs_class_name = class_name; // Keep original name for type mapping
	String base_class = get_csharp_base_class(p_class_name);

	String source;
	source += "    // Auto-generated from ClassDB: " + class_name + "\n";
	source += "    public class " + cs_class_name + " : " + base_class + " {\n";

	// Constructor — match hand-written wrapper patterns (base() and base(IntPtr))
	source += "        public " + cs_class_name + "() : base() {}\n";
	source += "        internal " + cs_class_name + "(System.IntPtr ptr) : base(ptr) {}\n\n";

	// Properties (p_no_inheritance=true: only this class's own properties)
	{
		List<PropertyInfo> props;
		ClassDB::get_property_list(p_class_name, &props, true);
		int prop_count = 0;
		for (const PropertyInfo &pi : props) {
			// Skip internal/group properties
			if (pi.usage & (PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_SUBGROUP)) {
				continue;
			}
			if (!(pi.usage & PROPERTY_USAGE_STORAGE)) {
				continue;
			}
			if (pi.name.is_empty() || pi.name == "resource_path" || pi.name == "resource_name") {
				// Skip properties already in Resource wrapper
				if (base_class == "Godot.Resource" || base_class == "Godot.Node") {
					continue;
				}
			}
			if (pi.name == "name" && base_class == "Godot.Node") {
				continue; // Already in Node wrapper
			}

			if (!is_supported_return_type(pi)) continue;

			String cs_type = godot_type_to_csharp(pi.type, pi.class_name);
			String cs_name = to_pascal_case(pi.name);
			if (cs_name.is_empty()) continue;

			// Avoid name conflicts with class name
			if (cs_name == cs_class_name) cs_name = cs_name + "Property";

			source += "        public " + cs_type + " " + cs_name + " {\n";
			source += "            get => (" + cs_type + ")Get(\"" + pi.name + "\");\n";
			source += "            set => Set(\"" + pi.name + "\", value);\n";
			source += "        }\n";
			prop_count++;
		}
		if (prop_count > 0) source += "\n";
	}

	// Methods (p_no_inheritance=true: only this class's own methods)
	{
		List<MethodInfo> methods;
		ClassDB::get_method_list(p_class_name, &methods, true);
		int method_count = 0;
		for (const MethodInfo &mi : methods) {
			if (!should_generate_method(class_name, mi.name)) continue;

			// Check return type
			if (!is_supported_return_type(mi.return_val)) continue;

			// Check all parameters are supported
			bool params_ok = true;
			for (int i = 0; i < mi.arguments.size(); i++) {
				if (!is_supported_return_type(mi.arguments[i])) {
					params_ok = false;
					break;
				}
			}
			if (!params_ok) continue;

			String method_name = mi.name;
			String cs_method_name = to_pascal_case(method_name);
			if (cs_method_name.is_empty()) continue;
			if (cs_method_name == cs_class_name) cs_method_name = "Call" + cs_method_name;

			// Build parameter list
			String param_list;
			String call_args;
			for (int i = 0; i < mi.arguments.size(); i++) {
				const PropertyInfo &pi = mi.arguments[i];
				String cs_type = godot_type_to_csharp(pi.type, pi.class_name);
				String param_name = to_pascal_case(pi.name);
				if (param_name.is_empty()) param_name = "arg" + String::num_int64(i);
				// Lowercase first letter for param name
				if (param_name.length() > 0) {
					param_name = String::char_lowercase(param_name[0]) + param_name.substr(1);
				}
				// Escape C# reserved keywords (e.g. "class" -> "@class")
				param_name = sanitize_csharp_identifier(param_name);

				if (i > 0) {
					param_list += ", ";
					call_args += ", ";
				}
				param_list += cs_type + " " + param_name;
				call_args += param_name;
			}

			// Build return type
			String ret_type = godot_type_to_csharp(mi.return_val.type, mi.return_val.class_name);
			bool has_return = (mi.return_val.type != Variant::NIL);

			// Generate method
			if (has_return) {
				String cast = "(" + ret_type + ")";
				if (ret_type == "string") cast = "(string)";
				else if (ret_type == "bool") cast = "(bool)";
				else if (ret_type == "long") cast = "(long)";
				else if (ret_type == "double") cast = "(double)";

				source += "        public " + ret_type + " " + cs_method_name + "(" + param_list + ") {\n";
				source += "            return " + cast + "Call(\"" + method_name + "\"";
				if (!call_args.is_empty()) source += ", " + call_args;
				source += ");\n";
				source += "        }\n";
			} else {
				source += "        public void " + cs_method_name + "(" + param_list + ") {\n";
				source += "            Call(\"" + method_name + "\"";
				if (!call_args.is_empty()) source += ", " + call_args;
				source += ");\n";
				source += "        }\n";
			}
			method_count++;
		}
		if (method_count > 0) source += "\n";
	}

	source += "    }\n\n";
	return source;
}

bool BindingsGenerator::generate(const String &p_output_path) {
	LocalVector<StringName> classes;
	ClassDB::get_class_list(classes);

	// Sort classes by name to ensure consistent output
	LocalVector<StringName> sorted_classes;
	for (uint32_t i = 0; i < classes.size(); i++) {
		if (!should_generate_class(classes[i])) continue;
		sorted_classes.push_back(classes[i]);
	}

	// Simple bubble sort (classes list is not huge)
	for (uint32_t i = 0; i < sorted_classes.size(); i++) {
		for (uint32_t j = i + 1; j < sorted_classes.size(); j++) {
			if (String(sorted_classes[j]) < String(sorted_classes[i])) {
				StringName tmp = sorted_classes[i];
				sorted_classes[i] = sorted_classes[j];
				sorted_classes[j] = tmp;
			}
		}
	}

	printf("[BindingsGenerator] Generating %d classes...\n", (int)sorted_classes.size());

	// Generate header
	String output;
	output += "// Auto-generated by BindingsGenerator from ClassDB metadata.\n";
	output += "// DO NOT EDIT MANUALLY.\n";
	output += "// Generated: " + String(__DATE__) + " " + String(__TIME__) + "\n\n";
	output += "using System;\n";
	output += "using System.Runtime.CompilerServices;\n\n";
	output += "namespace Godot {\n\n";

	// Generate struct types for basic value types (skip those already in GodotMathTypes.cs)
	output += "    // Value type structs not already defined in GodotMathTypes.cs\n";
	output += "    public struct Vector2I { public int X, Y; public Vector2I(int x, int y) { X=x; Y=y; } }\n";
	output += "    public struct Vector3I { public int X, Y, Z; public Vector3I(int x, int y, int z) { X=x; Y=y; Z=z; } }\n";
	output += "    public struct Vector4 { public double X, Y, Z, W; public Vector4(double x, double y, double z, double w) { X=x; Y=y; Z=z; W=w; } }\n";
	output += "    public struct Vector4I { public int X, Y, Z, W; public Vector4I(int x, int y, int z, int w) { X=x; Y=y; Z=z; W=w; } }\n";
	output += "    public struct RID { public uint Id; public RID(uint id) { Id=id; } }\n";
	output += "    public struct Signal { public Godot.Object Source; public string Name; }\n\n";

	// Collection namespaces
	output += "    namespace Collections {\n";
	output += "        public class Array : Godot.Object {\n";
	output += "            public Array() : base() {}\n";
	output += "            internal Array(System.IntPtr ptr) : base(ptr) {}\n";
	output += "            public int Count => (int)Call(\"size\");\n";
	output += "            public Godot.Object this[int index] => (Godot.Object)Call(\"get\", index);\n";
	output += "        }\n";
	output += "        public class Dictionary : Godot.Object {\n";
	output += "            public Dictionary() : base() {}\n";
	output += "            internal Dictionary(System.IntPtr ptr) : base(ptr) {}\n";
	output += "            public int Count => (int)Call(\"size\");\n";
	output += "        }\n";
	output += "    }\n\n";

	// Generate each class
	int generated_count = 0;
	for (uint32_t i = 0; i < sorted_classes.size(); i++) {
		String class_src = generate_class(sorted_classes[i]);
		output += class_src;
		generated_count++;

		// Progress report every 100 classes
		if (generated_count % 100 == 0) {
			printf("[BindingsGenerator]   %d/%d classes generated...\n",
			       generated_count, (int)sorted_classes.size());
		}
	}

	output += "}\n";

	// Write to file
	Ref<FileAccess> f = FileAccess::open(p_output_path, FileAccess::WRITE);
	if (f.is_null()) {
		printf("[BindingsGenerator] ERROR: Cannot open %s for writing\n", p_output_path.utf8().get_data());
		return false;
	}

	// Write UTF-8 (no BOM, no null terminator - C# compiler doesn't expect them)
	f->store_buffer((const uint8_t *)output.utf8().get_data(), output.utf8().length());

	printf("[BindingsGenerator] Generated %d classes, %d bytes\n",
	       generated_count, (int)output.utf8().length());
	return true;
}
