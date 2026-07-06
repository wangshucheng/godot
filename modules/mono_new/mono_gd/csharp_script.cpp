#include "csharp_script.h"

#include "gd_mono_class.h"
#include "../mono_runtime/gd_mono.h"
#include "../utils/mono_logger.h"
#include "core/io/file_access.h"
#include "core/object/object.h"
#include "scene/main/node.h"

extern "C" {
MonoClassField *mono_class_get_field_from_name(MonoClass *klass, const char *name);
void mono_field_set_value(MonoObject *obj, MonoClassField *field, void *value);
MonoClass *mono_class_get_parent(MonoClass *klass);
const char *mono_class_get_namespace(MonoClass *klass);
const char *mono_class_get_name(MonoClass *klass);
MonoObject *mono_runtime_invoke(MonoMethod *method, void *obj, void **params, MonoObject **exc);
MonoObject *mono_object_new(MonoDomain *domain, MonoClass *klass);
void mono_runtime_object_init(MonoObject *obj);
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
	return "";
}

void CSharpScript::set_source_code(const String &p_code) {
}

Error CSharpScript::reload(bool p_keep_state) {
	if (script_path.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}

	if (!GDMono::get_singleton()) {
		return ERR_UNCONFIGURED;
	}

	valid = false;

	String file = script_path.get_file();
	class_name = file.get_basename();

	MonoLogger::log(vformat("Loading C# script: %s", script_path));

	MonoClass *klass = GDMono::get_singleton()->find_class(class_name);
	if (!klass) {
		MonoLogger::log_error(vformat("Failed to find C# class: %s", class_name));
		return ERR_FILE_CANT_OPEN;
	}

	if (mono_class) {
		delete mono_class;
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
	return false;
}

void CSharpScript::get_script_signal_list(List<MethodInfo> *r_signals) const {
}

bool CSharpScript::get_property_default_value(const StringName &p_property, Variant &r_value) const {
	return false;
}

void CSharpScript::get_script_method_list(List<MethodInfo> *p_list) const {
}

void CSharpScript::get_script_property_list(List<PropertyInfo> *p_list) const {
}

const Variant CSharpScript::get_rpc_config() const {
	return Variant();
}

Error CSharpScript::load_source_code(const String &p_path) {
	script_path = p_path;
	return reload();
}

CSharpScript::CSharpScript() {
}

CSharpScript::~CSharpScript() {
}

bool CSharpInstance::set(const StringName &p_name, const Variant &p_value) {
	return false;
}

bool CSharpInstance::get(const StringName &p_name, Variant &r_ret) const {
	return false;
}

void CSharpInstance::get_property_list(List<PropertyInfo> *p_properties) const {
}

Variant::Type CSharpInstance::get_property_type(const StringName &p_name, bool *r_is_valid) const {
	if (r_is_valid) {
		*r_is_valid = false;
	}
	return Variant::NIL;
}

void CSharpInstance::validate_property(PropertyInfo &p_property) const {
}

bool CSharpInstance::property_can_revert(const StringName &p_name) const {
	return false;
}

bool CSharpInstance::property_get_revert(const StringName &p_name, Variant &r_ret) const {
	return false;
}

void CSharpInstance::get_method_list(List<MethodInfo> *p_list) const {
}

bool CSharpInstance::has_method(const StringName &p_method) const {
	if (!mono_class) {
		return false;
	}
	return mono_class->has_method(p_method);
}

Variant CSharpInstance::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	if (!mono_object || !mono_class) {
		r_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return Variant();
	}

	MonoMethod *method = mono_class->get_method(p_method);
	if (!method) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}

	r_error.error = Callable::CallError::CALL_OK;

	MonoObject *exc = nullptr;
	MonoObject *result = mono_runtime_invoke(method, mono_object, nullptr, &exc);

	if (exc) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}

	return Variant();
}

void CSharpInstance::notification(int p_notification, bool p_reversed) {
	if (!mono_object || !mono_class) {
		return;
	}

	if (p_notification == Node::NOTIFICATION_READY) {
		if (ready_called) {
			return;
		}
		ready_called = true;
		MonoMethod *ready_method = mono_class->get_method("_Ready");
		if (ready_method) {
			MonoObject *exc = nullptr;
			mono_runtime_invoke(ready_method, mono_object, nullptr, &exc);
			if (exc) {
				MonoLogger::log_error("Exception calling _Ready()");
			}
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

	MonoObject *exc = nullptr;
	mono_object = mono_object_new(GDMono::get_singleton()->get_scripts_domain(), mono_class->get_raw_class());
	if (!mono_object) {
		return false;
	}

	mono_runtime_object_init(mono_object);

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
			void *value = p_owner;
			mono_field_set_value(mono_object, native_field, &value);
		}
	}

	return true;
}

CSharpInstance::CSharpInstance() {
}

CSharpInstance::~CSharpInstance() {
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
	return "";
}

void CSharpLanguage::auto_indent_code(String &p_code, int p_from_line, int p_to_line) const {
}

void CSharpLanguage::add_global_constant(const StringName &p_variable, const Variant &p_value) {
}

String CSharpLanguage::debug_get_error() const {
	return "";
}

int CSharpLanguage::debug_get_stack_level_count() const {
	return 0;
}

int CSharpLanguage::debug_get_stack_level_line(int p_level) const {
	return -1;
}

String CSharpLanguage::debug_get_stack_level_function(int p_level) const {
	return "";
}

String CSharpLanguage::debug_get_stack_level_source(int p_level) const {
	return "";
}

void CSharpLanguage::debug_get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
}

void CSharpLanguage::debug_get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
}

void CSharpLanguage::debug_get_globals(List<String> *p_globals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
}

String CSharpLanguage::debug_parse_stack_level_expression(int p_level, const String &p_expression, int p_max_subitems, int p_max_depth) {
	return "";
}

void CSharpLanguage::reload_all_scripts() {
}

void CSharpLanguage::reload_scripts(const Array &p_scripts, bool p_soft_reload) {
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

CSharpLanguage::CSharpLanguage() {
	singleton = this;
}

CSharpLanguage::~CSharpLanguage() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
