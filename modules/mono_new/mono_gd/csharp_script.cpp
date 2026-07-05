#include "csharp_script.h"

#include "gd_mono_class.h"
#include "../mono_runtime/gd_mono.h"
#include "../utils/mono_logger.h"

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
		return StringName("RefCounted");
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
	script_namespace = "Godot";

	GDMono::get_singleton()->load_assembly(script_path);

	mono_class = new GDMonoClass(script_namespace, class_name);
	if (!mono_class->is_valid()) {
		delete mono_class;
		mono_class = nullptr;
		return ERR_FILE_CANT_OPEN;
	}

	valid = true;
	MonoLogger::log(vformat("Loaded C# script: %s.%s", script_namespace, class_name));

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
	return nullptr;
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
}

ScriptLanguage *CSharpInstance::get_language() {
	return script.is_valid() ? script->get_language() : nullptr;
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

CSharpLanguage::CSharpLanguage() {
}

CSharpLanguage::~CSharpLanguage() {
}
