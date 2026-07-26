/**************************************************************************/
/*  mono_script_metadata.cpp                                              */
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

#include "mono_script_metadata.h"

#include <mono/metadata/appdomain.h>
#include <mono/metadata/attrdefs.h>
#include <mono/metadata/class.h>
#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>

#include <cstring>

#include "core/string/ustring.h"
#include "core/templates/hash_set.h"
#include "core/variant/variant.h"

#include "../mono_host.h"

namespace mono_script_meta {

// ---------------------------------------------------------------------------
// has_attribute: check attribute by class name (string comparison, AOT-safe)
// ---------------------------------------------------------------------------
bool has_attribute(MonoCustomAttrInfo *p_info, const char *p_attr_name) {
	if (!p_info || p_info->num_attrs <= 0) {
		return false;
	}

	for (int i = 0; i < p_info->num_attrs; i++) {
		const MonoCustomAttrEntry &entry = p_info->attrs[i];
		if (!entry.ctor) {
			continue;
		}
		MonoClass *attr_class = mono_method_get_class(entry.ctor);
		if (!attr_class) {
			continue;
		}
		const char *cname = mono_class_get_name(attr_class);
		if (cname && strcmp(cname, p_attr_name) == 0) {
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// mono_type_to_variant_type: full MonoType* → Variant::Type mapping
// ---------------------------------------------------------------------------
Variant::Type mono_type_to_variant_type(MonoType *p_type) {
	if (!p_type) {
		return Variant::NIL;
	}

	MonoTypeEnum t = (MonoTypeEnum)mono_type_get_type(p_type);

	switch (t) {
		case MONO_TYPE_BOOLEAN:
			return Variant::BOOL;

		case MONO_TYPE_I1:
		case MONO_TYPE_I2:
		case MONO_TYPE_I4:
		case MONO_TYPE_I8:
		case MONO_TYPE_U1:
		case MONO_TYPE_U2:
		case MONO_TYPE_U4:
		case MONO_TYPE_U8:
			return Variant::INT;

		case MONO_TYPE_R4:
		case MONO_TYPE_R8:
			return Variant::FLOAT;

		case MONO_TYPE_STRING:
			return Variant::STRING;

		case MONO_TYPE_VALUETYPE: {
			// Could be a Godot math struct or an enum.
			MonoClass *klass = mono_class_from_mono_type(p_type);
			if (!klass) {
				return Variant::NIL;
			}

			// Enums are value types with mono_class_is_enum() == true.
			if (mono_class_is_enum(klass)) {
				return Variant::INT;
			}

			// Godot math structs: compare class name + namespace.
			const char *cname = mono_class_get_name(klass);
			const char *cns = mono_class_get_namespace(klass);
			if (!cname) {
				return Variant::NIL;
			}

			// Only match types in the "Godot" namespace.
			if (cns && strcmp(cns, "Godot") == 0) {
				if (strcmp(cname, "Vector2") == 0) return Variant::VECTOR2;
				if (strcmp(cname, "Vector2I") == 0) return Variant::VECTOR2I;
				if (strcmp(cname, "Vector3") == 0) return Variant::VECTOR3;
				if (strcmp(cname, "Vector3I") == 0) return Variant::VECTOR3I;
				if (strcmp(cname, "Vector4") == 0) return Variant::VECTOR4;
				if (strcmp(cname, "Vector4I") == 0) return Variant::VECTOR4I;
				if (strcmp(cname, "Rect2") == 0) return Variant::RECT2;
				if (strcmp(cname, "Rect2I") == 0) return Variant::RECT2I;
				if (strcmp(cname, "Transform2D") == 0) return Variant::TRANSFORM2D;
				if (strcmp(cname, "Plane") == 0) return Variant::PLANE;
				if (strcmp(cname, "Quaternion") == 0) return Variant::QUATERNION;
				if (strcmp(cname, "AABB") == 0) return Variant::AABB;
				if (strcmp(cname, "Basis") == 0) return Variant::BASIS;
				if (strcmp(cname, "Transform3D") == 0) return Variant::TRANSFORM3D;
				if (strcmp(cname, "Color") == 0) return Variant::COLOR;
				if (strcmp(cname, "Projection") == 0) return Variant::PROJECTION;
			}

			// Unsupported value type (custom struct, etc.)
			return Variant::NIL;
		}

		case MONO_TYPE_OBJECT:
		case MONO_TYPE_CLASS: {
			// Could be a Godot.Object derivative or a collection wrapper.
			MonoClass *klass = mono_class_from_mono_type(p_type);
			if (!klass) {
				return Variant::NIL;
			}

			const char *cname = mono_class_get_name(klass);
			const char *cns = mono_class_get_namespace(klass);
			if (!cname) {
				return Variant::NIL;
			}

			// Godot.Collections.Dictionary / Godot.Collections.Array wrappers.
			if (cns && strcmp(cns, "Godot.Collections") == 0) {
				if (strcmp(cname, "Dictionary") == 0) return Variant::DICTIONARY;
				if (strcmp(cname, "Array") == 0) return Variant::ARRAY;
			}

			// Godot.Object derivatives (check via mono_class_is_subclass_of).
			// We cache the Godot.Object class pointer at first use.
			static MonoClass *godot_object_class = nullptr;
			if (!godot_object_class) {
				// P1-#1 fix: load_godotsharp() failure is non-fatal (host still
				// initializes). get_godotsharp_assembly() returns nullptr in
				// that case → mono_assembly_get_image(nullptr) crashes. Guard.
				MonoAssembly *gs_asm = MonoHost::get_singleton()->get_godotsharp_assembly();
				if (!gs_asm) {
					return Variant::NIL;
				}
				MonoImage *img = mono_assembly_get_image(gs_asm);
				if (img) {
					godot_object_class = mono_class_from_name(img, "Godot", "Object");
				}
			}
			if (godot_object_class && mono_class_is_subclass_of(klass, godot_object_class, false)) {
				return Variant::OBJECT;
			}

			return Variant::NIL;
		}

		default:
			return Variant::NIL;
	}
}

// ---------------------------------------------------------------------------
// collect_exported_members: walk class hierarchy, collect [Export] fields/props
// ---------------------------------------------------------------------------
void collect_exported_members(MonoClass *p_class, List<ExportedMember> &r_out) {
	if (!p_class) {
		return;
	}

	// Cache the Godot.Object class to know when to stop walking up.
	static MonoClass *godot_object_class = nullptr;
	if (!godot_object_class) {
		// P1-#1 fix: guard against GodotSharp.dll load failure (see above).
		MonoAssembly *gs_asm = MonoHost::get_singleton()->get_godotsharp_assembly();
		if (!gs_asm) {
			return; // cannot walk hierarchy without Godot.Object anchor
		}
		MonoImage *img = mono_assembly_get_image(gs_asm);
		if (img) {
			godot_object_class = mono_class_from_name(img, "Godot", "Object");
		}
	}

	// Track seen names to deduplicate (first occurrence wins, which is the
	// most-derived class since we walk bottom-up).
	HashSet<StringName> seen;

	MonoClass *klass = p_class;
	while (klass) {
		// Collect fields with [Export].
		void *iter = nullptr;
		MonoClassField *field = nullptr;
		while ((field = mono_class_get_fields(klass, &iter)) != nullptr) {
			// Skip static and literal fields.
			uint32_t flags = mono_field_get_flags(field);
			if (flags & MONO_FIELD_ATTR_STATIC) {
				continue;
			}

			MonoCustomAttrInfo *ai = mono_custom_attrs_from_field(klass, field);
			if (!ai) {
				continue;
			}
			bool is_export = has_attribute(ai, "ExportAttribute");
			mono_custom_attrs_free(ai);
			if (!is_export) {
				continue;
			}

			const char *fname = mono_field_get_name(field);
			if (!fname) {
				continue;
			}
			StringName name = StringName(String::utf8(fname));
			if (seen.has(name)) {
				continue;
			}
			seen.insert(name);

			MonoType *ftype = mono_field_get_type(field);
			Variant::Type vtype = mono_type_to_variant_type(ftype);

			// P1-#2 fix: skip unsupported types. NIL means the type mapper
			// couldn't map this MonoType to a Variant type (custom struct,
			// unsupported array, etc.). Including it would add a bad entry to
			// the Inspector with STORAGE|EDITOR usage that can't be edited.
			if (vtype == Variant::NIL) {
				continue;
			}

			ExportedMember m;
			m.name = name;
			m.type = vtype;
			m.is_field = true;
			m.field = field;
			m.prop = nullptr;
			r_out.push_back(m);
		}

		// Collect properties with [Export].
		iter = nullptr;
		MonoProperty *prop = nullptr;
		while ((prop = mono_class_get_properties(klass, &iter)) != nullptr) {
			// Skip static properties (check getter method flags).
			MonoMethod *getter = mono_property_get_get_method(prop);
			if (!getter) {
				continue;
			}
			uint32_t flags = mono_method_get_flags(getter, nullptr);
			if (flags & MONO_METHOD_ATTR_STATIC) {
				continue;
			}

			MonoCustomAttrInfo *ai = mono_custom_attrs_from_property(klass, prop);
			if (!ai) {
				continue;
			}
			bool is_export = has_attribute(ai, "ExportAttribute");
			mono_custom_attrs_free(ai);
			if (!is_export) {
				continue;
			}

			const char *pname = mono_property_get_name(prop);
			if (!pname) {
				continue;
			}
			StringName name = StringName(String::utf8(pname));
			if (seen.has(name)) {
				continue;
			}
			seen.insert(name);

			MonoMethodSignature *sig = mono_method_signature(getter);
			MonoType *ret_type = sig ? mono_signature_get_return_type(sig) : nullptr;
			Variant::Type vtype = mono_type_to_variant_type(ret_type);

			ExportedMember m;
			m.name = name;
			m.type = vtype;
			m.is_field = false;
			m.field = nullptr;
			m.prop = prop;
			r_out.push_back(m);
		}

		// Walk up to parent class. Stop at Godot.Object (exclusive).
		if (klass == godot_object_class) {
			break;
		}
		klass = mono_class_get_parent(klass);
	}
}

// ---------------------------------------------------------------------------
// collect_signals: find [Signal]-marked nested delegates, build MethodInfo
//
// P1-#3 fix: walk up the class hierarchy (along mono_class_get_parent) to
// collect signals declared in C# base classes too. Without this, only
// signals declared directly on p_class appeared in the signal panel — any
// [Signal] delegate declared in a base C# class was invisible. Behavior
// mirrors collect_exported_members: stop at Godot.Object (exclusive),
// deduplicate by signal name (first occurrence wins, i.e. the most-derived
// class override takes precedence).
// ---------------------------------------------------------------------------
void collect_signals(MonoClass *p_class, List<MethodInfo> &r_out) {
	if (!p_class) {
		return;
	}

	// Cache the Godot.Object class to know when to stop walking up.
	static MonoClass *godot_object_class = nullptr;
	if (!godot_object_class) {
		// P1-#1 fix: guard against GodotSharp.dll load failure (see
		// mono_type_to_variant_type for the same pattern). Without this,
		// mono_assembly_get_image(nullptr) would crash on editor startup
		// when GodotSharp.dll failed to load.
		MonoAssembly *gs_asm = MonoHost::get_singleton()->get_godotsharp_assembly();
		if (!gs_asm) {
			return; // cannot walk hierarchy without Godot.Object anchor
		}
		MonoImage *img = mono_assembly_get_image(gs_asm);
		if (img) {
			godot_object_class = mono_class_from_name(img, "Godot", "Object");
		}
	}

	// Track seen names to deduplicate (first occurrence wins, which is the
	// most-derived class since we walk bottom-up).
	HashSet<StringName> seen;

	MonoClass *klass = p_class;
	while (klass) {
		// Iterate nested types of the current class in the hierarchy.
		void *iter = nullptr;
		MonoClass *nested = nullptr;
		while ((nested = mono_class_get_nested_types(klass, &iter)) != nullptr) {
			// Must be a delegate: parent class is MulticastDelegate.
			MonoClass *parent = mono_class_get_parent(nested);
			if (!parent) {
				continue;
			}
			const char *parent_name = mono_class_get_name(parent);
			if (!parent_name || strcmp(parent_name, "MulticastDelegate") != 0) {
				continue;
			}

			// Must have [Signal] attribute at class level.
			if (!class_has_attribute(nested, "SignalAttribute")) {
				continue;
			}

			// Delegate name must end with "EventHandler" (convention: <SignalName>EventHandler).
			const char *delegate_name = mono_class_get_name(nested);
			if (!delegate_name) {
				continue;
			}
			String dname = String::utf8(delegate_name);
			if (!dname.ends_with("EventHandler")) {
				continue;
			}
			// Extract signal name: remove "EventHandler" suffix.
			String signal_name = dname.substr(0, dname.length() - String("EventHandler").length());
			StringName signal_sn = StringName(signal_name);
			if (seen.has(signal_sn)) {
				continue;
			}
			seen.insert(signal_sn);

			// Find the Invoke method and build MethodInfo from its signature.
			void *method_iter = nullptr;
			MonoMethod *method = nullptr;
			while ((method = mono_class_get_methods(nested, &method_iter)) != nullptr) {
				const char *mname = mono_method_get_name(method);
				if (!mname || strcmp(mname, "Invoke") != 0) {
					continue;
				}

				MonoMethodSignature *sig = mono_method_signature(method);
				if (!sig) {
					break;
				}

				MethodInfo mi;
				mi.name = signal_sn;
				mi.return_val.type = Variant::NIL; // Signals must return void → NIL in Godot.

				// Build parameter list.
				uint32_t param_count = mono_signature_get_param_count(sig);
				void *param_iter = nullptr;
				MonoType *param_type = nullptr;
				for (uint32_t i = 0; i < param_count; i++) {
					param_type = mono_signature_get_params(sig, &param_iter);
					if (!param_type) {
						break;
					}

					PropertyInfo pi;
					pi.type = mono_type_to_variant_type(param_type);
					pi.name = "arg" + String::num_int64(i); // Placeholder; real names need mono_parameter_get_name
					mi.arguments.push_back(pi);
				}

				r_out.push_back(mi);
				break; // Only one Invoke method per delegate.
			}
		}

		// Walk up to parent class. Stop at Godot.Object (exclusive).
		if (klass == godot_object_class) {
			break;
		}
		klass = mono_class_get_parent(klass);
	}
}

// ---------------------------------------------------------------------------
// class_has_attribute: convenience wrapper for class-level attributes
// ---------------------------------------------------------------------------
bool class_has_attribute(MonoClass *p_class, const char *p_attr_name) {
	if (!p_class) {
		return false;
	}

	MonoCustomAttrInfo *ai = mono_custom_attrs_from_class(p_class);
	if (!ai) {
		return false;
	}
	bool result = has_attribute(ai, p_attr_name);
	mono_custom_attrs_free(ai);
	return result;
}

} // namespace mono_script_meta
