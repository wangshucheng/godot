#ifndef CSHARP_SCRIPT_H
#define CSHARP_SCRIPT_H

#include "core/object/script_language.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/string/ustring.h"
#include "core/templates/self_list.h"
#include "core/templates/hash_set.h"

#include <mono/mono-publib.h>

class GDMonoClass;

class ResourceFormatLoaderCSharpScript : public ResourceFormatLoader {
public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;
};

class CSharpScript : public Script {
	GDCLASS(CSharpScript, Script);

	friend class CSharpLanguage;

	String script_path;
	String script_namespace;
	String class_name;
	GDMonoClass *mono_class = nullptr;
	bool valid = false;

	String source;
	bool source_changed_cache = false;

	// Signal names parsed from [Signal] attribute declarations in the C# source.
	// Populated by _parse_signal_declarations() during source load.
	HashSet<StringName> script_signals;

	SelfList<CSharpScript> script_list;

protected:
	static void _bind_methods();

public:
	virtual bool can_instantiate() const override;
	virtual Ref<Script> get_base_script() const override;
	virtual StringName get_global_name() const override;
	virtual bool inherits_script(const Ref<Script> &p_script) const override;
	virtual StringName get_instance_base_type() const override;
	virtual ScriptInstance *instance_create(Object *p_this) override;
	virtual bool has_source_code() const override;
	virtual String get_source_code() const override;
	virtual void set_source_code(const String &p_code) override;
	virtual Error reload(bool p_keep_state = false) override;

#ifdef TOOLS_ENABLED
	virtual StringName get_doc_class_name() const override;
	virtual Vector<DocData::ClassDoc> get_documentation() const override;
	virtual String get_class_icon_path() const override;
#endif

	virtual bool has_method(const StringName &p_method) const override;
	virtual MethodInfo get_method_info(const StringName &p_method) const override;
	virtual bool is_tool() const override;
	virtual bool is_valid() const override;
	virtual bool is_abstract() const override;
	virtual ScriptLanguage *get_language() const override;
	virtual bool has_script_signal(const StringName &p_signal) const override;
	virtual void get_script_signal_list(List<MethodInfo> *r_signals) const override;
	virtual bool get_property_default_value(const StringName &p_property, Variant &r_value) const override;
	virtual void get_script_method_list(List<MethodInfo> *p_list) const override;
	virtual void get_script_property_list(List<PropertyInfo> *p_list) const override;
	virtual const Variant get_rpc_config() const override;

	virtual Error load_source_code(const String &p_path);
	String get_script_path() const { return script_path; }
	void set_script_path(const String &p_path) { script_path = p_path; }
	GDMonoClass *get_mono_class() { return mono_class; }

	CSharpScript();
	~CSharpScript();

private:
	void _parse_signal_declarations();
};

class CSharpInstance : public ScriptInstance {
	friend class CSharpScript;

	Object *owner = nullptr;
	Ref<CSharpScript> script;
	MonoObject *mono_object = nullptr;
	uint32_t mono_object_gchandle = 0; // S1 修复: 钉住 MonoObject* 防止 SGen GC 移动/回收
	GDMonoClass *mono_class = nullptr;
	bool ready_called = false;

public:
	virtual bool set(const StringName &p_name, const Variant &p_value) override;
	virtual bool get(const StringName &p_name, Variant &r_ret) const override;
	virtual void get_property_list(List<PropertyInfo> *p_properties) const override;
	virtual Variant::Type get_property_type(const StringName &p_name, bool *r_is_valid = nullptr) const override;
	virtual void validate_property(PropertyInfo &p_property) const override;
	virtual bool property_can_revert(const StringName &p_name) const override;
	virtual bool property_get_revert(const StringName &p_name, Variant &r_ret) const override;
	virtual void get_method_list(List<MethodInfo> *p_list) const override;
	virtual bool has_method(const StringName &p_method) const override;
	virtual Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override;
	virtual void notification(int p_notification, bool p_reversed = false) override;
	virtual Object *get_owner() override { return owner; }
	virtual Ref<Script> get_script() const override { return script; }
	virtual ScriptLanguage *get_language() override;
	virtual String to_string(bool *r_valid) override;
	virtual void refcount_incremented() override;
	virtual bool refcount_decremented() override;
	virtual const Variant get_rpc_config() const override;

	bool initialize(Object *p_owner);

	CSharpInstance();
	~CSharpInstance();
};

class CSharpLanguage : public ScriptLanguage {
	StringName type_name = "C#";
	static CSharpLanguage *singleton;

public:
	static CSharpLanguage *get_singleton() { return singleton; }

	virtual String get_name() const override;
	virtual void init() override;
	virtual String get_type() const override;
	virtual String get_extension() const override;
	virtual void finish() override;
	virtual bool is_using_templates() override;
	virtual Ref<Script> make_template(const String &p_template, const String &p_class_name, const String &p_base_class_name) const override;
	virtual Vector<ScriptTemplate> get_built_in_templates(const StringName &p_object) override;
	virtual Vector<String> get_reserved_words() const override;
	virtual bool is_control_flow_keyword(const String &p_string) const override;
	virtual Vector<String> get_comment_delimiters() const override;
	virtual Vector<String> get_doc_comment_delimiters() const override;
	virtual Vector<String> get_string_delimiters() const override;
	virtual bool validate(const String &p_script, const String &p_path = "", List<String> *r_functions = nullptr, List<ScriptError> *r_errors = nullptr, List<Warning> *r_warnings = nullptr, HashSet<int> *r_safe_lines = nullptr) const override;
	virtual bool supports_builtin_mode() const override;
	virtual int find_function(const String &p_function, const String &p_code) const override;
	virtual String make_function(const String &p_class, const String &p_name, const PackedStringArray &p_args) const override;
	virtual void auto_indent_code(String &p_code, int p_from_line, int p_to_line) const override;
	virtual void add_global_constant(const StringName &p_variable, const Variant &p_value) override;
	virtual String debug_get_error() const override;
	virtual int debug_get_stack_level_count() const override;
	virtual int debug_get_stack_level_line(int p_level) const override;
	virtual String debug_get_stack_level_function(int p_level) const override;
	virtual String debug_get_stack_level_source(int p_level) const override;
	virtual void debug_get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) override;
	virtual void debug_get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) override;
	virtual void debug_get_globals(List<String> *p_globals, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) override;
	virtual String debug_parse_stack_level_expression(int p_level, const String &p_expression, int p_max_subitems = -1, int p_max_depth = -1) override;
	virtual void reload_all_scripts() override;
	virtual void reload_scripts(const Array &p_scripts, bool p_soft_reload) override;
	virtual void reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual void get_public_functions(List<MethodInfo> *p_functions) const override;
	virtual void get_public_constants(List<Pair<String, Variant>> *p_constants) const override;
	virtual void get_public_annotations(List<MethodInfo> *p_annotations) const override;
	virtual void profiling_start() override;
	virtual void profiling_stop() override;
	virtual void profiling_set_save_native_calls(bool p_enable) override;
	virtual int profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) override;
	virtual int profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) override;
	virtual void frame() override;
	virtual ScriptNameCasing preferred_file_name_casing() const override;

private:
	friend class CSharpScript;
	SelfList<CSharpScript>::List scripts_list;

public:
	CSharpLanguage();
	~CSharpLanguage();
};

class ResourceFormatSaverCSharpScript : public ResourceFormatSaver {
public:
	virtual Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) override;
	virtual void get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const override;
	virtual bool recognize(const Ref<Resource> &p_resource) const override;
};

#endif
