#pragma once

#include "core/object/script_language.h"
#include "core/object/script.h"
#include "core/object/script_instance.h"
#include "core/templates/hash_map.h"

class CSharpLanguage;

class CSharpScript : public Script {
	GDCLASS(CSharpScript, Script);
	friend class CSharpLanguage;

	String source;
	String class_name;
	StringName native_base_name;
	bool valid = false;

public:
	bool can_instantiate() const override { return valid; }
	Ref<Script> get_base_script() const override { return Ref<Script>(); }
	StringName get_global_name() const override { return StringName(); }
	bool inherits_script(const Ref<Script> &p_script) const override { return false; }
	ScriptInstance *instance_create(Object *p_this) override;
	PlaceHolderScriptInstance *placeholder_instance_create(Object *p_this) override { return nullptr; }
	bool has_source_code() const override { return true; }
	String get_source_code() const override { return source; }
	void set_source_code(const String &p_code) override { source = p_code; }
	Error reload(bool p_keep_state = false) override;
	bool has_script_signal(const StringName &p_signal) const override { return false; }
	void get_script_signal_list(List<MethodInfo> *r_signals) const override {}
	bool get_property_default_value(const StringName &p_property, Variant &r_value) const override { return false; }
	void get_script_method_list(List<MethodInfo> *p_list) const override {}
	bool has_method(const StringName &p_method) const override;
	int get_script_method_argument_count(const StringName &p_method, bool *r_is_valid = nullptr) const override { if (r_is_valid) *r_is_valid = false; return 0; }
	MethodInfo get_method_info(const StringName &p_method) const override { return MethodInfo(); }
	Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override { r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD; return Variant(); }
	void get_script_property_list(List<PropertyInfo> *p_list) const override {}
	int get_member_line(const StringName &p_member) const override { return -1; }
	const Variant get_rpc_config() const override { return Variant(); }
	void get_members(HashSet<StringName> *p_members) override {}
	ScriptLanguage *get_language() const override;
	CSharpScript();
};

class CSharpInstance : public ScriptInstance {
	Object *owner = nullptr;
	Ref<CSharpScript> script;
	void *mono_gchandle = nullptr;

public:
	Object *get_owner() override { return owner; }
	bool set(const StringName &p_name, const Variant &p_value) override;
	bool get(const StringName &p_name, Variant &r_ret) const override;
	void get_property_list(List<PropertyInfo> *p_properties) const override {}
	Variant::Type get_property_type(const StringName &p_name, bool *r_is_valid) const override { if (r_is_valid) *r_is_valid = false; return Variant::NIL; }
	void get_method_list(List<MethodInfo> *p_list) const override {}
	bool has_method(const StringName &p_method) const override;
	int get_method_argument_count(const StringName &p_method, bool *r_is_valid = nullptr) const override { if (r_is_valid) *r_is_valid = false; return 0; }
	Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override;
	void notification(int p_notification, bool p_reversed = false) override;
	String to_string(bool *r_valid) override { if (r_valid) *r_valid = false; return "<CSharpScript>"; }
	Ref<Script> get_script() const override { return script; }
	ScriptLanguage *get_language() override;
	CSharpInstance(const Ref<CSharpScript> &p_script, Object *p_owner);
	~CSharpInstance();
};

class CSharpLanguage : public ScriptLanguage {
	static CSharpLanguage *singleton;
	int lang_idx = -1;
public:
	static CSharpLanguage *get_singleton() { return singleton; }
	void set_language_index(int p_idx) { lang_idx = p_idx; }

	String get_name() const override { return "C#"; }
	String get_type() const override { return "CSharpScript"; }
	String get_extension() const override { return "cs"; }
	void init() override;
	void finish() override;
	void frame() override {}
	Vector<String> get_reserved_words() const override { return {}; }
	bool is_control_flow_keyword(const String &p_keyword) const override { return false; }
	Vector<String> get_comment_delimiters() const override { return {"//", ""}; }
	Vector<String> get_doc_comment_delimiters() const override { return {"///", ""}; }
	Vector<String> get_string_delimiters() const override { return {"\"", "'"}; }
	bool is_using_templates() override { return true; }
	Ref<Script> make_template(const String &p_template, const String &p_class_name, const String &p_base_class_name) const override;
	Vector<ScriptTemplate> get_built_in_templates(const StringName &p_object) override;
	bool validate(const String &p_script, const String &p_path, List<String> *r_functions, List<ScriptError> *r_errors, List<Warning> *r_warnings, HashSet<int> *r_safe_lines) const override { return true; }
	String validate_path(const String &p_path) const override;
	bool supports_builtin_mode() const override { return false; }
	int find_function(const String &p_function, const String &p_code) const override { return -1; }
	String make_function(const String &p_class, const String &p_name, const PackedStringArray &p_args) const override { return ""; }
	ScriptNameCasing preferred_file_name_casing() const override { return SCRIPT_NAME_CASING_PASCAL_CASE; }
	bool handles_global_class_type(const String &p_type) const override { return false; }
	String get_global_class_name(const String &p_path, String *r_base_type, String *r_icon_path, bool *r_is_abstract, bool *r_is_tool) const override { return ""; }
	String debug_get_error() const override { return ""; }
	int debug_get_stack_level_count() const override { return 0; }
	int debug_get_stack_level_line(int p_level) const override { return -1; }
	String debug_get_stack_level_function(int p_level) const override { return ""; }
	String debug_get_stack_level_source(int p_level) const override { return ""; }
	Vector<StackInfo> debug_get_current_stack_info() override { return {}; }
	void reload_all_scripts() override {}
	void reload_scripts(const Array &p_scripts, bool p_soft_reload) override {}
	void reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) override {}
	void get_recognized_extensions(List<String> *p_extensions) const override;
	CSharpLanguage();
	~CSharpLanguage();
};
