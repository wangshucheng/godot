#include "csharp_script.h"

#include "gd_mono_class.h"
#include "interop/gd_mono_interop_variant.h"
#include "../mono_runtime/csharp_debugger.h"
#include "../mono_runtime/gd_mono.h"
#include "../utils/mono_logger.h"
#include "core/io/file_access.h"
#include "core/object/object.h"
#include <cstdio>
#include "scene/main/node.h"

extern "C" {
MonoClassField *mono_class_get_field_from_name(MonoClass *klass, const char *name);
void mono_field_set_value(MonoObject *obj, MonoClassField *field, void *value);
void mono_field_get_value(MonoObject *obj, MonoClassField *field, void *value);
MonoClass *mono_class_get_parent(MonoClass *klass);
const char *mono_class_get_namespace(MonoClass *klass);
const char *mono_class_get_name(MonoClass *klass);
MonoObject *mono_runtime_invoke(MonoMethod *method, void *obj, void **params, MonoObject **exc);
MonoObject *mono_object_new(MonoDomain *domain, MonoClass *klass);
void mono_runtime_object_init(MonoObject *obj);
MonoObject *mono_value_box(MonoDomain *domain, MonoClass *klass, void *val);
void *mono_object_unbox(MonoObject *obj);
uint32_t mono_gchandle_new(MonoObject *obj, int pinned);
void mono_gchandle_free(uint32_t handle);
MonoClass *mono_get_double_class(void);
MonoClass *mono_get_int64_class(void);
MonoClass *mono_get_boolean_class(void);
char *mono_object_to_string(MonoObject *obj, MonoObject **exc);
MonoClass *mono_object_get_class(MonoObject *obj);
struct _MonoProperty;
typedef struct _MonoProperty MonoProperty;
MonoProperty *mono_class_get_property_from_name(MonoClass *klass, const char *name);
MonoObject *mono_property_get_value(MonoProperty *prop, void *obj, void **params, MonoObject **exc);
char *mono_string_to_utf8(MonoString *s);
void mono_free(void *ptr);
MonoClass *mono_class_from_name(MonoImage *image, const char *name_space, const char *name);
MonoMethod *mono_class_get_method_from_name(MonoClass *klass, const char *name, int param_count);
const char *mono_image_get_name(MonoImage *image);
MonoImage *mono_get_corlib();
// H7 属性系统所需: 获取字段类型的 type enum
int mono_type_get_type(MonoType *type);
// 属性系统扩展: 获取字段的 MonoClass（用于区分 Vector2/Color 等值类型）
MonoClass *mono_type_get_class(MonoType *type);
// 属性系统扩展: 遍历类的所有字段
MonoClassField *mono_class_get_fields(MonoClass *klass, void *iter);
const char *mono_field_get_name(MonoClassField *field);
MonoType *mono_field_get_type(MonoClassField *field);
int mono_class_is_valuetype(MonoClass *klass);
}

// Mono type enum constants (from mono/metadata/metadata.h)
// 仅列出属性系统用到的基本类型
#ifndef MONO_TYPE_I4
#define MONO_TYPE_I4        0x08
#endif
#ifndef MONO_TYPE_I8
#define MONO_TYPE_I8        0x0a
#endif
#ifndef MONO_TYPE_R4
#define MONO_TYPE_R4        0x0c
#endif
#ifndef MONO_TYPE_R8
#define MONO_TYPE_R8        0x0d
#endif
#ifndef MONO_TYPE_STRING
#define MONO_TYPE_STRING    0x0e
#endif
#ifndef MONO_TYPE_BOOLEAN
#define MONO_TYPE_BOOLEAN   0x02
#endif
#ifndef MONO_TYPE_VALUETYPE
#define MONO_TYPE_VALUETYPE 0x11
#endif

// 属性系统: 将 C# 类型名（源码解析得到）映射到 Godot Variant::Type。
// 用于 get_property_list() 构造 PropertyInfo。
static Variant::Type csharp_type_name_to_variant_type(const String &p_type_name) {
	if (p_type_name == "int" || p_type_name == "long" || p_type_name == "short" ||
			p_type_name == "byte" || p_type_name == "sbyte" ||
			p_type_name == "uint" || p_type_name == "ulong" || p_type_name == "ushort" ||
			p_type_name == "Int32" || p_type_name == "Int64") {
		return Variant::INT;
	}
	if (p_type_name == "float" || p_type_name == "double" ||
			p_type_name == "Single" || p_type_name == "Double") {
		return Variant::FLOAT;
	}
	if (p_type_name == "bool" || p_type_name == "Boolean") {
		return Variant::BOOL;
	}
	if (p_type_name == "string" || p_type_name == "String") {
		return Variant::STRING;
	}
	if (p_type_name == "Vector2") return Variant::VECTOR2;
	if (p_type_name == "Vector2i") return Variant::VECTOR2I;
	if (p_type_name == "Vector3") return Variant::VECTOR3;
	if (p_type_name == "Vector3i") return Variant::VECTOR3I;
	if (p_type_name == "Vector4") return Variant::VECTOR4;
	if (p_type_name == "Color") return Variant::COLOR;
	if (p_type_name == "Rect2") return Variant::RECT2;
	if (p_type_name == "Rect2i") return Variant::RECT2I;
	if (p_type_name == "Quaternion") return Variant::QUATERNION;
	if (p_type_name == "Plane") return Variant::PLANE;
	if (p_type_name == "AABB") return Variant::AABB;
	if (p_type_name == "Basis") return Variant::BASIS;
	if (p_type_name == "Transform2D") return Variant::TRANSFORM2D;
	if (p_type_name == "Transform3D") return Variant::TRANSFORM3D;
	return Variant::NIL; // 未知/不支持
}

// 属性系统: 通过 MonoClass 名称判断是否为 Godot 数学类型并返回 Variant::Type。
// 用于 set()/get() 在 MONO_TYPE_VALUETYPE 分支中区分 Vector2/Color 等。
static Variant::Type mono_class_name_to_variant_type(MonoClass *p_class) {
	if (!p_class) return Variant::NIL;
	const char *name = mono_class_get_name(p_class);
	if (!name) return Variant::NIL;
	String class_name = String::utf8(name);
	return csharp_type_name_to_variant_type(class_name);
}

// Convert Godot snake_case method names to C# PascalCase.
// e.g. "_unhandled_input" -> "_UnhandledInput", "set_position" -> "SetPosition"
static StringName snake_to_pascal_case(const StringName &p_name) {
	String s = String(p_name);
	if (s.is_empty()) {
		return p_name;
	}
	bool has_underscore_lower = false;
	for (int i = 0; i < s.length(); i++) {
		if (s[i] == '_' && i + 1 < s.length() && s[i + 1] >= 'a' && s[i + 1] <= 'z') {
			has_underscore_lower = true;
			break;
		}
	}
	if (!has_underscore_lower) {
		return p_name;
	}
	String result;
	for (int i = 0; i < s.length(); i++) {
		char32_t c = s[i];
		if (c == '_' && i + 1 < s.length() && s[i + 1] >= 'a' && s[i + 1] <= 'z') {
			if (i == 0) {
				// Leading underscore: keep it (e.g. _process -> _Process)
				result += '_';
				result += String::chr(s[i + 1]).to_upper();
			} else {
				// Internal underscore: remove it (e.g. _unhandled_input -> _UnhandledInput)
				result += String::chr(s[i + 1]).to_upper();
			}
			i++;
		} else {
			result += c;
		}
	}
	return StringName(result);
}

CSharpLanguage *CSharpLanguage::singleton = nullptr;

Ref<Resource> ResourceFormatLoaderCSharpScript::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	String path = p_original_path.is_empty() ? p_path : p_original_path;

	Ref<CSharpScript> script;
	script.instantiate();

	Error err = script->load_source_code(path);
	if (err != OK) {
		if (r_error) *r_error = err;
		return Ref<Resource>();
	}

	// Set the script path and reload to find the C# class from the loaded assembly.
	// This must happen here so can_instantiate() returns true when the scene loader
	// tries to create a script instance for a node.
	script->set_script_path(path);
	script->reload();

	if (r_error) *r_error = OK;
	return script;
}

void ResourceFormatLoaderCSharpScript::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("cs");
}

bool ResourceFormatLoaderCSharpScript::handles_type(const String &p_type) const {
	return p_type == "Script" || p_type == "CSharpScript";
}

String ResourceFormatLoaderCSharpScript::get_resource_type(const String &p_path) const {
	return "CSharpScript";
}

void CSharpScript::_bind_methods() {
}

bool CSharpScript::can_instantiate() const {
	return valid && mono_class != nullptr;
}

Ref<Script> CSharpScript::get_base_script() const {
	return Ref<Script>();
}

StringName CSharpScript::get_global_name() const {
	return StringName(class_name);
}

bool CSharpScript::inherits_script(const Ref<Script> &p_script) const {
	return false;
}

StringName CSharpScript::get_instance_base_type() const {
	if (mono_class) {
		MonoClass *raw_class = mono_class->get_raw_class();
		while (raw_class) {
			const char *cname = mono_class_get_name(raw_class);
			const char *namespace_name = mono_class_get_namespace(raw_class);

			if (strcmp(namespace_name, "Godot") == 0) {
				if (strcmp(cname, "Node2D") == 0) return StringName("Node2D");
				if (strcmp(cname, "Node3D") == 0) return StringName("Node3D");
				if (strcmp(cname, "Node") == 0) return StringName("Node");
				if (strcmp(cname, "Resource") == 0) return StringName("Resource");
				if (strcmp(cname, "GodotObject") == 0) return StringName("RefCounted");
			}

			raw_class = mono_class_get_parent(raw_class);
		}
		return StringName("Node");
	}
	return StringName();
}

ScriptInstance *CSharpScript::instance_create(Object *p_this) {
	if (!valid || !mono_class) {
		return nullptr;
	}

	CSharpInstance *instance = memnew(CSharpInstance);
	instance->script = Ref<CSharpScript>(this);

	if (!instance->initialize(p_this)) {
		memdelete(instance);
		return nullptr;
	}
	return instance;
}

bool CSharpScript::has_source_code() const {
	return true;
}

String CSharpScript::get_source_code() const {
	return source;
}

void CSharpScript::set_source_code(const String &p_code) {
	source = p_code;
	source_changed_cache = true;
}

Error CSharpScript::reload(bool p_keep_state) {
	if (script_path.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}

	if (!GDMono::get_singleton()) {
		return ERR_UNCONFIGURED;
	}

	valid = false;

	// Derive class name from the script file name (e.g. "Player.cs" -> "Player")
	String file = script_path.get_file();
	class_name = file.get_basename();

	MonoLogger::log(vformat("Loading C# script: %s", script_path));

	MonoClass *klass = GDMono::get_singleton()->find_class(class_name);
	if (!klass) {
		// Class not yet compiled - this is normal for newly created scripts
		MonoLogger::log(vformat("C# class not found (not yet compiled?): %s", class_name));
		return OK; // Return OK so the script resource remains valid
	}

	// Safe pattern: clear pointer before deleting so existing instances
	// don't dereference a dangling pointer during destruction.
	GDMonoClass *old_class = mono_class;
	mono_class = nullptr;
	if (old_class) {
		delete old_class;
	}

	mono_class = new GDMonoClass(klass);
	script_namespace = mono_class->namespace_name;

	valid = true;
	MonoLogger::log(vformat("Loaded C# script class: %s.%s", script_namespace, class_name));

	return OK;
}

#ifdef TOOLS_ENABLED
StringName CSharpScript::get_doc_class_name() const {
	return StringName(class_name);
}

Vector<DocData::ClassDoc> CSharpScript::get_documentation() const {
	return Vector<DocData::ClassDoc>();
}

String CSharpScript::get_class_icon_path() const {
	return "";
}
#endif

bool CSharpScript::has_method(const StringName &p_method) const {
	if (!mono_class) {
		return false;
	}
	return mono_class->has_method(p_method);
}

MethodInfo CSharpScript::get_method_info(const StringName &p_method) const {
	return MethodInfo();
}

bool CSharpScript::is_tool() const {
	return false;
}

bool CSharpScript::is_valid() const {
	return valid;
}

bool CSharpScript::is_abstract() const {
	return false;
}

ScriptLanguage *CSharpScript::get_language() const {
	return CSharpLanguage::get_singleton();
}

bool CSharpScript::has_script_signal(const StringName &p_signal) const {
	return script_signals.has(p_signal);
}

void CSharpScript::get_script_signal_list(List<MethodInfo> *r_signals) const {
	for (const StringName &sig : script_signals) {
		MethodInfo mi;
		mi.name = sig;
		r_signals->push_back(mi);
	}
}

// Parse C# source for [Signal] attribute declarations.
// Recognizes the Godot C# pattern:
//   [Signal]
//   public delegate void MySignalEventHandler(int value);
// The signal name is derived by stripping an optional "EventHandler" suffix
// and converting PascalCase to snake_case (e.g. MySignalEventHandler → my_signal).
void CSharpScript::_parse_signal_declarations() {
	script_signals.clear();
	if (source.is_empty()) return;

	// Simple line-by-line scanner: find [Signal] attributes and extract the
	// delegate name from the following line.
	Vector<String> lines = source.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i].strip_edges();
		if (line == "[Signal]" || line.begins_with("[Signal]")) {
			// Search forward (up to 3 lines) for the delegate declaration.
			for (int j = i + 1; j < MIN(i + 4, lines.size()); j++) {
				String dl = lines[j].strip_edges();
				int delegate_idx = dl.find("delegate");
				if (delegate_idx < 0) continue;
				// Extract the name token after "delegate void " or "delegate bool " etc.
				// Pattern: [modifiers] delegate ReturnType Name(
				int paren = dl.find("(", delegate_idx);
				if (paren < 0) continue;
				String header = dl.substr(delegate_idx, paren - delegate_idx);
				// Split by whitespace and take the last token (the delegate name).
				Vector<String> tokens = header.split(" ", false);
				if (tokens.size() >= 2) {
					String delegate_name = tokens[tokens.size() - 1];
					// Strip "EventHandler" suffix if present.
					if (delegate_name.ends_with("EventHandler")) {
						delegate_name = delegate_name.substr(0, delegate_name.length() - 12);
					}
					// Convert PascalCase to snake_case.
					String snake;
					for (int k = 0; k < delegate_name.length(); k++) {
						char32_t c = delegate_name[k];
						if (k > 0 && c >= 'A' && c <= 'Z') {
							snake += '_';
						}
						if (c >= 'A' && c <= 'Z') {
							snake += String::chr(c + 32);
						} else {
							snake += String::chr(c);
						}
					}
					if (!snake.is_empty()) {
						script_signals.insert(StringName(snake));
					}
				}
				break;
			}
		}
	}
}

// Parse C# source for [Export] attribute declarations on fields.
// Recognizes the Godot C# pattern:
//   [Export]
//   public int Speed = 200;
// or:
//   [Export(PropertyHint.Range, "0,100,1")]
//   public float Health = 100f;
// Also handles [Export] on the same line as the field declaration:
//   [Export] public Vector2 Position = new Vector2(0, 0);
// Extracts: field name + C# type name (for Variant::Type mapping).
void CSharpScript::_parse_export_declarations() {
	exported_fields.clear();
	if (source.is_empty()) return;

	Vector<String> lines = source.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i].strip_edges();
		// Match [Export] or [Export(...)]
		if (!line.begins_with("[Export")) {
			continue;
		}

		// Find the field declaration line (could be same line after ], or next non-attribute line).
		// Strategy: find the closing ']' of [Export...], then look for the field declaration
		// starting from that position to the next few lines.
		String search_start = line;
		int close_bracket = line.find("]");
		if (close_bracket < 0) {
			// Multi-line attribute? Skip (rare in practice).
			continue;
		}

		// The field declaration may be after [Export] on the same line, or on subsequent lines.
		// Gather text from the rest of this line + next 2 lines to find the field.
		String decl_text = line.substr(close_bracket + 1).strip_edges();
		if (decl_text.is_empty()) {
			// Look at next lines for the field declaration.
			for (int j = i + 1; j < MIN(i + 3, lines.size()); j++) {
				String next_line = lines[j].strip_edges();
				if (next_line.is_empty() || next_line.begins_with("[")) continue;
				decl_text = next_line;
				break;
			}
		}

		if (decl_text.is_empty()) continue;

		// Parse field declaration: [modifiers] TypeFieldName = value;
		// or [modifiers] TypeFieldName;
		// We need to extract the type and name.
		// Remove trailing ';' and initializer.
		int semi = decl_text.find(";");
		if (semi >= 0) {
			decl_text = decl_text.substr(0, semi);
		}
		int eq = decl_text.find("=");
		if (eq >= 0) {
			decl_text = decl_text.substr(0, eq);
		}
		decl_text = decl_text.strip_edges(true, true);

		// Remove access modifiers
		for (const char *mod : { "public ", "private ", "protected ", "internal ",
								 "static ", "readonly ", "const " }) {
			String mod_str(mod);
			while (decl_text.begins_with(mod_str)) {
				decl_text = decl_text.substr(mod_str.length()).strip_edges(true, true);
			}
		}

		// Now decl_text should be: "Type FieldName"
		// Split by whitespace; first token = type, second token = name.
		Vector<String> tokens = decl_text.split(" ", false);
		if (tokens.size() >= 2) {
			String type_name = tokens[0];
			String field_name = tokens[1];

			// Validate: field name should be a valid identifier (starts with letter/_).
			if (field_name.is_empty()) continue;
			char32_t first = field_name[0];
			if (!((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z') || first == '_')) {
				continue;
			}

			ExportedField ef;
			ef.name = StringName(field_name);
			ef.type_name = type_name;
			exported_fields.push_back(ef);
		}
	}
}

bool CSharpScript::get_property_default_value(const StringName &p_property, Variant &r_value) const {
	// H7 扩展: 为 [Export] 字段返回类型默认值，支持检查器 revert 与序列化对比。
	// 注意: 真实的 C# 初始化表达式（= 100f）需运行构造函数才能读取，
	// 此处仅返回零值默认；若需精确默认值，应在 CSharpInstance 构造后读取字段。
	for (const ExportedField &ef : exported_fields) {
		if (ef.name != p_property) continue;
		Variant::Type vt = csharp_type_name_to_variant_type(ef.type_name);
		switch (vt) {
			case Variant::INT: r_value = Variant((int64_t)0); return true;
			case Variant::FLOAT: r_value = Variant((double)0); return true;
			case Variant::BOOL: r_value = Variant(false); return true;
			case Variant::STRING: r_value = Variant(String()); return true;
			case Variant::VECTOR2: r_value = Variant(Vector2()); return true;
			case Variant::VECTOR2I: r_value = Variant(Vector2i()); return true;
			case Variant::VECTOR3: r_value = Variant(Vector3()); return true;
			case Variant::VECTOR3I: r_value = Variant(Vector3i()); return true;
			case Variant::VECTOR4: r_value = Variant(Vector4()); return true;
			case Variant::COLOR: r_value = Variant(Color()); return true;
			case Variant::RECT2: r_value = Variant(Rect2()); return true;
			case Variant::RECT2I: r_value = Variant(Rect2i()); return true;
			case Variant::QUATERNION: r_value = Variant(Quaternion()); return true;
			case Variant::PLANE: r_value = Variant(Plane()); return true;
			case Variant::AABB: r_value = Variant(::AABB()); return true;
			case Variant::BASIS: r_value = Variant(Basis()); return true;
			case Variant::TRANSFORM2D: r_value = Variant(Transform2D()); return true;
			case Variant::TRANSFORM3D: r_value = Variant(Transform3D()); return true;
			default: return false;
		}
	}
	return false;
}

void CSharpScript::get_script_method_list(List<MethodInfo> *p_list) const {
}

void CSharpScript::get_script_property_list(List<PropertyInfo> *p_list) const {
	// 静态属性列表（编辑器在实例化前用此方法查询脚本属性）
	for (const ExportedField &ef : exported_fields) {
		Variant::Type vt = csharp_type_name_to_variant_type(ef.type_name);
		if (vt == Variant::NIL) {
			continue;
		}
		PropertyInfo pi;
		pi.name = ef.name;
		pi.type = vt;
		pi.class_name = "C#" + ef.type_name;
		pi.usage = PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_EDITOR;
		p_list->push_back(pi);
	}
}

const Variant CSharpScript::get_rpc_config() const {
	return Variant();
}

Error CSharpScript::load_source_code(const String &p_path) {
	// Actually read the file contents into the source member so the editor
	// and the script resource can display/save the source code.
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (err != OK || f.is_null()) {
		MonoLogger::log_error(vformat("Failed to open C# script file: %s", p_path));
		return err;
	}

	Vector<uint8_t> buffer;
	int64_t len = f->get_length();
	if (len > 0) {
		buffer.resize(len);
		f->get_buffer(buffer.ptrw(), len);
		source = String::utf8((const char *)buffer.ptr(), len);
	} else {
		source = String();
	}

	script_path = p_path;
	source_changed_cache = false;

	// Parse [Signal] declarations from source so has_script_signal works
	// even before the assembly is loaded (editor / inspector support).
	_parse_signal_declarations();
	// Parse [Export] field declarations so get_property_list works
	// even before the assembly is loaded (editor / inspector support).
	_parse_export_declarations();

	return reload();
}

CSharpScript::CSharpScript() : script_list(this) {
	CSharpLanguage *lang = CSharpLanguage::get_singleton();
	if (lang) {
		lang->scripts_list.add(&script_list);
	}
}

CSharpScript::~CSharpScript() {
	script_list.remove_from_list();
	if (mono_class) {
		delete mono_class;
		mono_class = nullptr;
}
}

bool CSharpInstance::set(const StringName &p_name, const Variant &p_value) {
	// H7 修复: 基础属性系统——通过 Mono 反射读写 C# 字段。
	// 支持基本类型 (int, float, bool, string, Vector2, Color)。
	if (!mono_object || !mono_class) return false;

	String name_str(p_name);
	CharString name_utf8 = name_str.utf8();
	MonoClassField *field = mono_class_get_field_from_name(mono_class->get_raw_class(), name_utf8.get_data());
	if (!field) return false;

	MonoType *ftype = mono_field_get_type(field);
	if (!ftype) return false;
	int ttype = mono_type_get_type(ftype);

	switch (ttype) {
		case MONO_TYPE_I4: {
			int32_t val = (int32_t)(int64_t)p_value;
			mono_field_set_value(mono_object, field, &val);
			return true;
		}
		case MONO_TYPE_I8: {
			int64_t val = (int64_t)p_value;
			mono_field_set_value(mono_object, field, &val);
			return true;
		}
		case MONO_TYPE_R4: {
			float val = (float)(double)p_value;
			mono_field_set_value(mono_object, field, &val);
			return true;
		}
		case MONO_TYPE_R8: {
			double val = (double)p_value;
			mono_field_set_value(mono_object, field, &val);
			return true;
		}
		case MONO_TYPE_BOOLEAN: {
			bool val = (bool)p_value;
			mono_field_set_value(mono_object, field, &val);
			return true;
		}
		case MONO_TYPE_STRING: {
			String s = p_value;
			CharString s_utf8 = s.utf8();
			MonoString *mstr = mono_string_new(mono_domain_get(), s_utf8.get_data());
			mono_field_set_value(mono_object, field, &mstr);
			return true;
		}
		case MONO_TYPE_VALUETYPE: {
			// H7 扩展: 支持 Vector2/Vector3/Color 等 Godot 值类型字段
			MonoClass *field_class = mono_type_get_class(ftype);
			if (!field_class) return false;
			Variant::Type vt = mono_class_name_to_variant_type(field_class);
			switch (vt) {
				case Variant::VECTOR2: {
					Vector2 v = p_value;
					mono_field_set_value(mono_object, field, &v);
					return true;
				}
				case Variant::VECTOR2I: {
					Vector2i v = p_value;
					mono_field_set_value(mono_object, field, &v);
					return true;
				}
				case Variant::VECTOR3: {
					Vector3 v = p_value;
					mono_field_set_value(mono_object, field, &v);
					return true;
				}
				case Variant::VECTOR3I: {
					Vector3i v = p_value;
					mono_field_set_value(mono_object, field, &v);
					return true;
				}
				case Variant::VECTOR4: {
					Vector4 v = p_value;
					mono_field_set_value(mono_object, field, &v);
					return true;
				}
				case Variant::COLOR: {
					Color v = p_value;
					mono_field_set_value(mono_object, field, &v);
					return true;
				}
				case Variant::RECT2: {
					Rect2 v = p_value;
					mono_field_set_value(mono_object, field, &v);
					return true;
				}
				case Variant::RECT2I: {
					Rect2i v = p_value;
					mono_field_set_value(mono_object, field, &v);
					return true;
				}
				case Variant::QUATERNION: {
					Quaternion v = p_value;
					mono_field_set_value(mono_object, field, &v);
					return true;
				}
				case Variant::PLANE: {
					Plane v = p_value;
					mono_field_set_value(mono_object, field, &v);
					return true;
				}
				case Variant::AABB: {
					::AABB v = p_value;
					mono_field_set_value(mono_object, field, &v);
					return true;
				}
				case Variant::BASIS: {
					Basis v = p_value;
					mono_field_set_value(mono_object, field, &v);
					return true;
				}
				case Variant::TRANSFORM2D: {
					Transform2D v = p_value;
					mono_field_set_value(mono_object, field, &v);
					return true;
				}
				case Variant::TRANSFORM3D: {
					Transform3D v = p_value;
					mono_field_set_value(mono_object, field, &v);
					return true;
				}
				default:
					return false;
			}
		}
		default:
			return false;
	}
}

bool CSharpInstance::get(const StringName &p_name, Variant &r_ret) const {
	// H7 修复: 基础属性读取
	if (!mono_object || !mono_class) return false;

	String name_str(p_name);
	CharString name_utf8 = name_str.utf8();
	MonoClassField *field = mono_class_get_field_from_name(mono_class->get_raw_class(), name_utf8.get_data());
	if (!field) return false;

	MonoType *ftype = mono_field_get_type(field);
	if (!ftype) return false;
	int ttype = mono_type_get_type(ftype);

	switch (ttype) {
		case MONO_TYPE_I4: {
			int32_t val = 0;
			mono_field_get_value(mono_object, field, &val);
			r_ret = Variant((int64_t)val);
			return true;
		}
		case MONO_TYPE_I8: {
			int64_t val = 0;
			mono_field_get_value(mono_object, field, &val);
			r_ret = Variant(val);
			return true;
		}
		case MONO_TYPE_R4: {
			float val = 0;
			mono_field_get_value(mono_object, field, &val);
			r_ret = Variant((double)val);
			return true;
		}
		case MONO_TYPE_R8: {
			double val = 0;
			mono_field_get_value(mono_object, field, &val);
			r_ret = Variant(val);
			return true;
		}
		case MONO_TYPE_BOOLEAN: {
			bool val = false;
			mono_field_get_value(mono_object, field, &val);
			r_ret = Variant(val);
			return true;
		}
		case MONO_TYPE_STRING: {
			MonoString *mstr = nullptr;
			mono_field_get_value(mono_object, field, &mstr);
			if (mstr) {
				char *utf8 = mono_string_to_utf8(mstr);
				if (utf8) {
					r_ret = Variant(String::utf8(utf8));
					mono_free(utf8);
					return true;
				}
			}
			r_ret = Variant(String());
			return true;
		}
		case MONO_TYPE_VALUETYPE: {
			// H7 扩展: 支持 Vector2/Vector3/Color 等 Godot 值类型字段读取
			MonoClass *field_class = mono_type_get_class(ftype);
			if (!field_class) return false;
			Variant::Type vt = mono_class_name_to_variant_type(field_class);
			switch (vt) {
				case Variant::VECTOR2: {
					Vector2 v;
					mono_field_get_value(mono_object, field, &v);
					r_ret = Variant(v);
					return true;
				}
				case Variant::VECTOR2I: {
					Vector2i v;
					mono_field_get_value(mono_object, field, &v);
					r_ret = Variant(v);
					return true;
				}
				case Variant::VECTOR3: {
					Vector3 v;
					mono_field_get_value(mono_object, field, &v);
					r_ret = Variant(v);
					return true;
				}
				case Variant::VECTOR3I: {
					Vector3i v;
					mono_field_get_value(mono_object, field, &v);
					r_ret = Variant(v);
					return true;
				}
				case Variant::VECTOR4: {
					Vector4 v;
					mono_field_get_value(mono_object, field, &v);
					r_ret = Variant(v);
					return true;
				}
				case Variant::COLOR: {
					Color v;
					mono_field_get_value(mono_object, field, &v);
					r_ret = Variant(v);
					return true;
				}
				case Variant::RECT2: {
					Rect2 v;
					mono_field_get_value(mono_object, field, &v);
					r_ret = Variant(v);
					return true;
				}
				case Variant::RECT2I: {
					Rect2i v;
					mono_field_get_value(mono_object, field, &v);
					r_ret = Variant(v);
					return true;
				}
				case Variant::QUATERNION: {
					Quaternion v;
					mono_field_get_value(mono_object, field, &v);
					r_ret = Variant(v);
					return true;
				}
				case Variant::PLANE: {
					Plane v;
					mono_field_get_value(mono_object, field, &v);
					r_ret = Variant(v);
					return true;
				}
				case Variant::AABB: {
					::AABB v;
					mono_field_get_value(mono_object, field, &v);
					r_ret = Variant(v);
					return true;
				}
				case Variant::BASIS: {
					Basis v;
					mono_field_get_value(mono_object, field, &v);
					r_ret = Variant(v);
					return true;
				}
				case Variant::TRANSFORM2D: {
					Transform2D v;
					mono_field_get_value(mono_object, field, &v);
					r_ret = Variant(v);
					return true;
				}
				case Variant::TRANSFORM3D: {
					Transform3D v;
					mono_field_get_value(mono_object, field, &v);
					r_ret = Variant(v);
					return true;
				}
				default:
					return false;
			}
		}
		default:
			return false;
	}
}

void CSharpInstance::get_property_list(List<PropertyInfo> *p_properties) const {
	// H7 扩展: 暴露 [Export] 字段到检查器/序列化。
	// 依赖 CSharpScript::exported_fields（由 _parse_export_declarations 填充）。
	// 仅暴露能在 Variant 与 C# 之间往返的类型（基本类型 + Godot 数学结构）。
	if (script.is_null()) {
		return;
	}
	for (const CSharpScript::ExportedField &ef : script->exported_fields) {
		Variant::Type vt = csharp_type_name_to_variant_type(ef.type_name);
		if (vt == Variant::NIL) {
			// 未知/不支持的类型——跳过，避免检查器显示空属性
			continue;
		}
		PropertyInfo pi;
		pi.name = ef.name;
		pi.type = vt;
		pi.class_name = "C#" + ef.type_name; // 标注来源，方便检查器分组
		pi.usage = PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_EDITOR;
		p_properties->push_back(pi);
	}
}

Variant::Type CSharpInstance::get_property_type(const StringName &p_name, bool *r_is_valid) const {
	// H7 扩展: 为 [Export] 字段返回正确的 Variant::Type
	if (script.is_null()) {
		if (r_is_valid) *r_is_valid = false;
		return Variant::NIL;
	}
	for (const CSharpScript::ExportedField &ef : script->exported_fields) {
		if (ef.name == p_name) {
			Variant::Type vt = csharp_type_name_to_variant_type(ef.type_name);
			if (r_is_valid) *r_is_valid = (vt != Variant::NIL);
			return vt;
		}
	}
	if (r_is_valid) *r_is_valid = false;
	return Variant::NIL;
}

void CSharpInstance::validate_property(PropertyInfo &p_property) const {
}

bool CSharpInstance::property_can_revert(const StringName &p_name) const {
	// H7 扩展: [Export] 字段支持 revert（回到类型默认值）
	if (!script.is_null()) {
		for (const CSharpScript::ExportedField &ef : script->exported_fields) {
			if (ef.name == p_name) {
				return csharp_type_name_to_variant_type(ef.type_name) != Variant::NIL;
			}
		}
	}
	return false;
}

bool CSharpInstance::property_get_revert(const StringName &p_name, Variant &r_ret) const {
	// H7 扩展: 返回类型默认值（与 CSharpScript::get_property_default_value 一致）
	if (!script.is_null()) {
		return script->get_property_default_value(p_name, r_ret);
	}
	return false;
}

void CSharpInstance::get_method_list(List<MethodInfo> *p_list) const {
}

bool CSharpInstance::has_method(const StringName &p_method) const {
	if (!mono_class) {
		return false;
	}
	if (mono_class->has_method(p_method)) {
		return true;
	}
	// Try PascalCase conversion for snake_case names from GDVIRTUAL
	StringName pascal_name = snake_to_pascal_case(p_method);
	if (pascal_name != p_method) {
		bool found = mono_class->has_method(pascal_name);
		return found;
	}
	return false;
}

Variant CSharpInstance::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	if (!mono_object || !mono_class) {
		r_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return Variant();
	}

	MonoMethod *method = mono_class->get_method(p_method, p_argcount);
	if (!method) {
		// Try PascalCase conversion for snake_case names from GDVIRTUAL
		StringName pascal_name = snake_to_pascal_case(p_method);
		if (pascal_name != p_method) {
			method = mono_class->get_method(pascal_name, p_argcount);
		}
	}
	if (!method) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}

	r_error.error = Callable::CallError::CALL_OK;

	// Build params array for mono_runtime_invoke.
	// For value types (double, int, bool), use mono_value_box to create a
	// GC-managed boxed object, pin it with mono_gchandle_new(pinned=1), then
	// get the unboxed pointer via mono_object_unbox. This ensures the value
	// resides in GC-managed memory that the WASM Mono interpreter can access
	// correctly. The pin prevents SGen from moving the object during the call.
	// Reference types are converted via variant_to_mono_object as before.
	void **params = nullptr;
	uint32_t *gchandles = nullptr;  // tracks pinned GC handles for value types
	if (p_argcount > 0) {
		params = (void **)memalloc(sizeof(void *) * p_argcount);
		gchandles = (uint32_t *)memalloc(sizeof(uint32_t) * p_argcount);
		for (int i = 0; i < p_argcount; i++) {
			gchandles[i] = 0;
			params[i] = nullptr;
		}
		MonoDomain *domain = GDMono::get_singleton() ? GDMono::get_singleton()->get_scripts_domain() : nullptr;
		for (int i = 0; i < p_argcount; i++) {
			Variant::Type vt = p_args[i]->get_type();
			if (vt == Variant::FLOAT) {
				double val = (double)*p_args[i];
				MonoClass *cls = mono_get_double_class();
				if (cls && domain) {
					MonoObject *boxed = mono_value_box(domain, cls, &val);
					if (boxed) {
						gchandles[i] = mono_gchandle_new(boxed, 1); // pinned
						params[i] = mono_object_unbox(boxed);
					}
				}
			} else if (vt == Variant::INT) {
				int64_t val = (int64_t)*p_args[i];
				MonoClass *cls = mono_get_int64_class();
				if (cls && domain) {
					MonoObject *boxed = mono_value_box(domain, cls, &val);
					if (boxed) {
						gchandles[i] = mono_gchandle_new(boxed, 1);
						params[i] = mono_object_unbox(boxed);
					}
				}
			} else if (vt == Variant::BOOL) {
				int val = (bool)*p_args[i] ? 1 : 0;
				MonoClass *cls = mono_get_boolean_class();
				if (cls && domain) {
					MonoObject *boxed = mono_value_box(domain, cls, &val);
					if (boxed) {
						gchandles[i] = mono_gchandle_new(boxed, 1);
						params[i] = mono_object_unbox(boxed);
					}
				}
			} else if (domain) {
				params[i] = GDMonoInterop::variant_to_mono_object(domain, *p_args[i]);
			}
		}
	}

	MonoObject *exc = nullptr;
	MonoObject *result = mono_runtime_invoke(method, mono_object, params, &exc);

	// Free pinned GC handles for boxed value types
	if (gchandles) {
		for (int i = 0; i < p_argcount; i++) {
			if (gchandles[i]) {
				mono_gchandle_free(gchandles[i]);
			}
		}
		memfree(gchandles);
	}
	if (params) {
		memfree(params);
	}

	if (exc) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		// Use C API only to get exception class name (avoid calling ToString() which may crash)
		MonoClass *exc_class = mono_object_get_class(exc);
		const char *exc_name = exc_class ? mono_class_get_name(exc_class) : "unknown";
		const char *exc_ns = exc_class ? mono_class_get_namespace(exc_class) : "";
		// Try to extract Message property via mono_property_get_value (safer than ToString)
		char *exc_msg_utf8 = nullptr;
		if (exc_class) {
			MonoProperty *msg_prop = mono_class_get_property_from_name(exc_class, "Message");
			if (msg_prop) {
				MonoObject *msg_exc2 = nullptr;
				MonoObject *msg_obj = mono_property_get_value(msg_prop, exc, nullptr, &msg_exc2);
				if (msg_obj && !msg_exc2) {
					exc_msg_utf8 = mono_string_to_utf8((MonoString*)msg_obj);
				}
			}
		}
		// M6 修复: 删除临时 [DIAG] printf 诊断（已由 MonoLogger::log_error 统一记录）。
		MonoLogger::log_error(vformat("Exception in C# method call '%s': %s.%s %s",
			String(p_method), exc_ns, exc_name, exc_msg_utf8 ? exc_msg_utf8 : ""));
		if (exc_msg_utf8) mono_free(exc_msg_utf8);
		return Variant();
	}

	if (result) {
		return GDMonoInterop::mono_object_to_variant(result);
	}
	return Variant();
}

void CSharpInstance::notification(int p_notification, bool p_reversed) {
	// GDVIRTUAL system in Node::_notification() handles _Ready, _Process, _PhysicsProcess
	// by calling callp() which uses snake_to_pascal_case() to find C# methods.
	// Node::_call_unhandled_input/_call_input handle _UnhandledInput/_Input.
	// Safety: ensure processing flags are set on READY (idempotent with engine's GDVIRTUAL_IS_OVERRIDDEN check).
	if (!mono_object || !mono_class) {
		return;
	}

	if (p_notification == Node::NOTIFICATION_READY) {
		if (ready_called) {
			return;
		}
		ready_called = true;
		Node *node = Object::cast_to<Node>(owner);
		if (node) {
			node->set_process(true);
			node->set_physics_process(true);
			node->set_process_unhandled_input(true);
			node->set_process_input(true);
		}
	}
}

ScriptLanguage *CSharpInstance::get_language() {
	return CSharpLanguage::get_singleton();
}

String CSharpInstance::to_string(bool *r_valid) {
	if (r_valid) {
		*r_valid = false;
	}
	return "<CSharpInstance>";
}

void CSharpInstance::refcount_incremented() {
}

bool CSharpInstance::refcount_decremented() {
	return true;
}

const Variant CSharpInstance::get_rpc_config() const {
	return Variant();
}

bool CSharpInstance::initialize(Object *p_owner) {
	owner = p_owner;

	if (script.is_null() || !script->is_valid()) {
		return false;
	}

	mono_class = script->get_mono_class();
	if (!mono_class || !mono_class->is_valid()) {
		return false;
	}

	MonoDomain *domain = GDMono::get_singleton() ? GDMono::get_singleton()->get_scripts_domain() : nullptr;
	if (!domain) {
		return false;
	}
	MonoClass *raw_class = mono_class->get_raw_class();
	if (!raw_class) {
		return false;
	}

	mono_object = mono_object_new(domain, raw_class);
	if (!mono_object) {
		return false;
	}

	// S1 修复: 创建 pinned gchandle 防止 SGen GC 移动/回收 mono_object
	// pinned=1 钉住对象地址，GC 不会移动它；所有后续 mono_object 使用都安全
	// 注意: 精简版 mono-publib.h 只声明了 mono_gchandle_new(obj, pinned)，无独立 _pinned 后缀 API
	mono_object_gchandle = mono_gchandle_new(mono_object, /*pinned=*/1);

	// Set nativeInstance BEFORE calling the constructor, so C# constructors
	// can check `if (nativeInstance == IntPtr.Zero)` and skip native object
	// creation for scene-loaded nodes (which already have a native object).
	// This prevents native object leaks when constructors also call
	// godot_icall_CreateObject.
	MonoClass *base_class = mono_class->get_raw_class();
	while (base_class) {
		const char *ns = mono_class_get_namespace(base_class);
		const char *name = mono_class_get_name(base_class);
		if (ns && strcmp(ns, "Godot") == 0 && name && strcmp(name, "GodotObject") == 0) {
			break;
		}
		base_class = mono_class_get_parent(base_class);
	}

	if (base_class) {
		MonoClassField *native_field = mono_class_get_field_from_name(base_class, "nativeInstance");
		if (native_field) {
			int64_t value = (int64_t)(intptr_t)p_owner;
			mono_field_set_value(mono_object, native_field, &value);
		}
	}

	mono_runtime_object_init(mono_object);

	// 信号系统: 将脚本 [Signal] 声明注册到 owner，使 emit/connect 可用。
	// 信号名已在 CSharpScript::_parse_signal_declarations 中解析为 snake_case。
	if (p_owner) {
		for (const StringName &sig : script->script_signals) {
			MethodInfo mi;
			mi.name = sig;
			p_owner->add_user_signal(mi);
		}
	}

	return true;
}

CSharpInstance::CSharpInstance() {
}

CSharpInstance::~CSharpInstance() {
	// S1 修复: 析构时释放 pinned gchandle，允许 GC 回收 mono_object
	if (mono_object_gchandle != 0) {
		mono_gchandle_free(mono_object_gchandle);
		mono_object_gchandle = 0;
		mono_object = nullptr;
	}
}

String CSharpLanguage::get_name() const {
	return "C#";
}

void CSharpLanguage::init() {
}

String CSharpLanguage::get_type() const {
	return "CSharpScript";
}

String CSharpLanguage::get_extension() const {
	return "cs";
}

void CSharpLanguage::finish() {
}

bool CSharpLanguage::is_using_templates() {
	return true;
}

Ref<Script> CSharpLanguage::make_template(const String &p_template, const String &p_class_name, const String &p_base_class_name) const {
	Ref<CSharpScript> scr;
	scr.instantiate();

	String processed = p_template;
	String class_name = p_class_name.replace(" ", "_");
	String base_name = p_base_class_name;

	// Map common Godot types to their C# wrappers in GodotSharp.
	// Unknown types fall back to GodotObject so the file still compiles.
	static const char *known_types[] = {
		"Node", "Node2D", "Node3D", "Resource", "GodotObject",
		"Control", "Sprite2D", "Camera2D", "CharacterBody2D",
		"RigidBody2D", "Area2D", "CanvasItem", "Window",
		nullptr
	};
	bool base_known = false;
	for (int i = 0; known_types[i]; i++) {
		if (base_name == known_types[i]) {
			base_known = true;
			break;
		}
	}
	if (!base_known) {
		base_name = "GodotObject";
	}

	processed = processed.replace("_BASE_", base_name)
						.replace("_CLASS_", class_name)
						.replace("_TS_", "\t");

	scr->set_source_code(processed);
	return scr;
}

Vector<ScriptLanguage::ScriptTemplate> CSharpLanguage::get_built_in_templates(const StringName &p_object) {
	Vector<ScriptTemplate> templates;
	ScriptTemplate t;
	t.inherit = String(p_object);
	t.origin = TemplateLocation::TEMPLATE_BUILT_IN;

	// Provide a Node-oriented template (most common case for C# scripts).
	if (p_object == StringName("Node") ||
		p_object == StringName("Node2D") ||
		p_object == StringName("Node3D") ||
		p_object == StringName("Control") ||
		p_object == StringName("Window")) {
		t.name = "Default";
		t.description = "C# script with _Ready and _Process overrides";
		t.content =
				"using Godot;\n"
				"using System;\n"
				"\n"
				"public partial class _CLASS_ : _BASE_\n"
				"{\n"
				"_TS_// Called when the node enters the scene tree for the first time.\n"
				"_TS_public override void _Ready()\n"
				"_TS_{\n"
				"_TS_}\n"
				"\n"
				"_TS_// Called every frame. 'delta' is the elapsed time since the previous frame.\n"
				"_TS_public override void _Process(double delta)\n"
				"_TS_{\n"
				"_TS_}\n"
				"}\n";
		templates.append(t);
	} else {
		// Generic fallback template
		t.name = "Default";
		t.description = "C# script template";
		t.content =
				"using Godot;\n"
				"using System;\n"
				"\n"
				"public partial class _CLASS_ : _BASE_\n"
				"{\n"
				"_TS_// Called when the node enters the scene tree for the first time.\n"
				"_TS_public override void _Ready()\n"
				"_TS_{\n"
				"_TS_}\n"
				"}\n";
		templates.append(t);
	}

	return templates;
}

Vector<String> CSharpLanguage::get_reserved_words() const {
	static const char *_reserved[] = {
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

	Vector<String> words;
	for (int i = 0; _reserved[i]; i++) {
		words.push_back(_reserved[i]);
	}
	return words;
}

bool CSharpLanguage::is_control_flow_keyword(const String &p_string) const {
	return p_string == "break" || p_string == "case" || p_string == "continue" ||
		   p_string == "default" || p_string == "do" || p_string == "else" ||
		   p_string == "for" || p_string == "foreach" || p_string == "goto" ||
		   p_string == "if" || p_string == "return" || p_string == "switch" ||
		   p_string == "throw" || p_string == "try" || p_string == "while";
}

Vector<String> CSharpLanguage::get_comment_delimiters() const {
	Vector<String> delimiters;
	delimiters.push_back("//");
	delimiters.push_back("/* */");
	return delimiters;
}

Vector<String> CSharpLanguage::get_doc_comment_delimiters() const {
	Vector<String> delimiters;
	delimiters.push_back("///");
	delimiters.push_back("/** */");
	return delimiters;
}

Vector<String> CSharpLanguage::get_string_delimiters() const {
	Vector<String> delimiters;
	delimiters.push_back("' '");
	delimiters.push_back("\" \"");
	return delimiters;
}

bool CSharpLanguage::validate(const String &p_script, const String &p_path, List<String> *r_functions, List<ScriptError> *r_errors, List<Warning> *r_warnings, HashSet<int> *r_safe_lines) const {
	return true;
}

bool CSharpLanguage::supports_builtin_mode() const {
	return false;
}

int CSharpLanguage::find_function(const String &p_function, const String &p_code) const {
	return -1;
}

String CSharpLanguage::make_function(const String &p_class, const String &p_name, const PackedStringArray &p_args) const {
	// C# doesn't use class name for method generation, but Godot passes it
	String s = "public override void " + p_name + "(";
	for (int i = 0; i < p_args.size(); i++) {
		String arg = p_args[i];
		if (i > 0) s += ", ";
		// Godot passes args as "type name" - keep as-is for C#
		s += arg;
	}
	s += ") {\n    \n}\n";
	return s;
}

void CSharpLanguage::auto_indent_code(String &p_code, int p_from_line, int p_to_line) const {
}

void CSharpLanguage::add_global_constant(const StringName &p_variable, const Variant &p_value) {
}

String CSharpLanguage::debug_get_error() const {
	// Return the last unhandled C# exception message (captured by the
	// CSharpDebugger::on_unhandled_exception hook). When an IDE is attached
	// via SDB, breakpoints/stepping are handled directly by VS Code; this
	// hook only surfaces runtime exceptions to the Godot debugger panel.
	CSharpDebugger::ExceptionInfo ex = CSharpDebugger::get_last_exception();
	if (ex.valid) {
		return ex.message;
	}
	return "";
}

int CSharpLanguage::debug_get_stack_level_count() const {
	CSharpDebugger::ExceptionInfo ex = CSharpDebugger::get_last_exception();
	return ex.valid ? ex.frames.size() : 0;
}

int CSharpLanguage::debug_get_stack_level_line(int p_level) const {
	CSharpDebugger::ExceptionInfo ex = CSharpDebugger::get_last_exception();
	if (!ex.valid || p_level < 0 || p_level >= ex.frames.size()) return -1;
	return ex.frames[p_level].line;
}

String CSharpLanguage::debug_get_stack_level_function(int p_level) const {
	CSharpDebugger::ExceptionInfo ex = CSharpDebugger::get_last_exception();
	if (!ex.valid || p_level < 0 || p_level >= ex.frames.size()) return "";
	return ex.frames[p_level].function;
}

String CSharpLanguage::debug_get_stack_level_source(int p_level) const {
	CSharpDebugger::ExceptionInfo ex = CSharpDebugger::get_last_exception();
	if (!ex.valid || p_level < 0 || p_level >= ex.frames.size()) return "";
	return ex.frames[p_level].source;
}

void CSharpLanguage::debug_get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	// Local variable values are only available through the SDB protocol
	// (queried by the attached IDE, e.g. VS Code). Godot cannot read them
	// without implementing its own SDB client, which is out of scope.
}

void CSharpLanguage::debug_get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	// See comment in debug_get_stack_level_locals().
}

void CSharpLanguage::debug_get_globals(List<String> *p_globals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
}

String CSharpLanguage::debug_parse_stack_level_expression(int p_level, const String &p_expression, int p_max_subitems, int p_max_depth) {
	// Expression evaluation requires an active SDB session context, which
	// Godot does not host. Return empty so the editor falls back gracefully.
	return "";
}

void CSharpLanguage::reload_all_scripts() {
	// Iterate over all known CSharpScript instances and reload them so they
	// pick up newly compiled classes from the freshly loaded assembly.
	SelfList<CSharpScript> *elem = scripts_list.first();
	while (elem) {
		CSharpScript *script = elem->self();
		if (script && !script->script_path.is_empty()) {
			MonoLogger::log(vformat("Reloading C# script: %s", script->script_path));
			script->reload();
		}
		elem = elem->next();
	}
}

void CSharpLanguage::reload_scripts(const Array &p_scripts, bool p_soft_reload) {
	for (int i = 0; i < p_scripts.size(); i++) {
		Ref<CSharpScript> script = p_scripts[i];
		if (script.is_valid() && !script->script_path.is_empty()) {
			script->reload(!p_soft_reload);
		}
	}
}

void CSharpLanguage::reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) {
}

void CSharpLanguage::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("cs");
}

void CSharpLanguage::get_public_functions(List<MethodInfo> *p_functions) const {
}

void CSharpLanguage::get_public_constants(List<Pair<String, Variant>> *p_constants) const {
}

void CSharpLanguage::get_public_annotations(List<MethodInfo> *p_annotations) const {
}

void CSharpLanguage::profiling_start() {
}

void CSharpLanguage::profiling_stop() {
}

void CSharpLanguage::profiling_set_save_native_calls(bool p_enable) {
}

int CSharpLanguage::profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) {
	return 0;
}

int CSharpLanguage::profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) {
	return 0;
}

void CSharpLanguage::frame() {
	GDMono *gdmono = GDMono::get_singleton();
	if (gdmono) {
		gdmono->on_frame_tick();
	}
}

ScriptLanguage::ScriptNameCasing CSharpLanguage::preferred_file_name_casing() const {
	return SCRIPT_NAME_CASING_PASCAL_CASE;
}

CSharpLanguage::CSharpLanguage() {
	singleton = this;
}

CSharpLanguage::~CSharpLanguage() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

Error ResourceFormatSaverCSharpScript::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	Ref<CSharpScript> script = p_resource;
	ERR_FAIL_COND_V(script.is_null(), ERR_INVALID_PARAMETER);

	String source = script->get_source_code();
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot save C# script: " + p_path);

	f->store_string(source);
	if (source.size() > 0 && source[source.size() - 1] != '\n') {
		f->store_8('\n'); // Ensure file ends with newline
	}
	f->close();

#ifdef TOOLS_ENABLED
	// Notify the editor integration so it can ensure the .csproj exists
	// and trigger compilation of the C# project.
	{
		extern void csharp_editor_on_script_saved(const String &p_path);
		csharp_editor_on_script_saved(p_path);
	}
#endif

	return OK;
}

void ResourceFormatSaverCSharpScript::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	Ref<CSharpScript> script = p_resource;
	if (script.is_valid()) {
		p_extensions->push_back("cs");
	}
}

bool ResourceFormatSaverCSharpScript::recognize(const Ref<Resource> &p_resource) const {
	Ref<CSharpScript> script = p_resource;
	return script.is_valid();
}
