#pragma once

#include "core/object/script_language.h"
#include "core/object/script_instance.h"
#include "core/doc_data.h"
#include "core/templates/hash_map.h"

typedef struct _MonoClass MonoClass;
typedef struct _MonoObject MonoObject;
typedef struct _MonoImage MonoImage;
typedef struct _MonoAssembly MonoAssembly;
typedef struct _MonoMethod MonoMethod;

class CSharpLanguage;

class CSharpScript : public Script {
	GDCLASS(CSharpScript, Script);
	friend class CSharpLanguage;
	friend class CSharpInstance;

	String source;
	String class_name;
	StringName native_base_name;
	bool mono_class_valid = false;
	bool source_valid = false;

	MonoClass *mono_class = nullptr;
	MonoImage *mono_image = nullptr;
	HashMap<StringName, MonoMethod *> method_cache;

	// P1: [Export] member cache as PropertyInfo list (no Mono header dependency).
	// Collected in resolve_mono_class via mono_script_meta::collect_exported_members,
	// then converted to PropertyInfo for Inspector display.
	List<PropertyInfo> exported_properties;
	bool exported_members_valid = false;
	// P1: [Tool] attribute cache (true = script runs in editor).
	bool is_tool_class = false;
	// P2: [GlobalClass] attribute cache.
	// When true, get_global_name() returns the class_name so the script appears
	// in the "Add Node" dialog and class database.
	bool is_global_class = false;
	// P3: [Signal] cache — collected in resolve_mono_class via
	// mono_script_meta::collect_signals (nested delegates with [Signal] attribute).
	// MethodInfo.name is the signal name (EventHandler suffix stripped).
	List<MethodInfo> signal_cache;
	bool signals_valid = false;

	void resolve_mono_class();
	MonoMethod *get_method(const StringName &p_method, int p_argcount = -1);
	String _parse_base_class() const;
	String _parse_namespace() const;

public:
	void set_class_name(const String &p_name) { class_name = p_name; }
	bool can_instantiate() const override;
	Ref<Script> get_base_script() const override { return Ref<Script>(); }
	bool inherits_script(const Ref<Script> &p_script) const override { return false; }
	StringName get_instance_base_type() const override { return native_base_name; }
	ScriptInstance *instance_create(Object *p_this) override;
	PlaceHolderScriptInstance *placeholder_instance_create(Object *p_this) override;
	bool has_source_code() const override { return true; }
	String get_source_code() const override { return source; }
	void set_source_code(const String &p_code) override { source = p_code; mono_class = nullptr; mono_image = nullptr; method_cache.clear(); mono_class_valid = false; exported_properties.clear(); exported_members_valid = false; is_tool_class = false; is_global_class = false; signal_cache.clear(); signals_valid = false; }
	Error reload(bool p_keep_state = false) override;
	// P3: Check if the script declares a signal with the given name.
	bool has_script_signal(const StringName &p_signal) const override;
	// P3: Return all [Signal]-marked delegates as MethodInfo list for the editor signal panel.
	void get_script_signal_list(List<MethodInfo> *r_signals) const override;
	bool get_property_default_value(const StringName &p_property, Variant &r_value) const override;
	void get_script_method_list(List<MethodInfo> *r_list) const override;
	bool has_method(const StringName &p_method) const override;
	int get_script_method_argument_count(const StringName &p_method, bool *r_is_valid = nullptr) const override;
	MethodInfo get_method_info(const StringName &p_method) const override { return MethodInfo(); }
	Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override;
	void get_script_property_list(List<PropertyInfo> *r_list) const override;
	int get_member_line(const StringName &p_member) const override { return -1; }
	const Variant get_rpc_config() const override { return Variant(); }
	void get_members(HashSet<StringName> *p_members) override {}
	bool is_tool() const override { return is_tool_class; }
	// P2: Return class name when [GlobalClass] attribute is present.
	StringName get_global_name() const override { return is_global_class ? StringName(class_name) : StringName(); }
	bool is_valid() const override { return source_valid; }
	bool is_abstract() const override { return false; }
	ScriptLanguage *get_language() const override;

#ifdef TOOLS_ENABLED
	StringName get_doc_class_name() const override { return class_name; }
	Vector<DocData::ClassDoc> get_documentation() const override { return Vector<DocData::ClassDoc>(); }
	String get_class_icon_path() const override { return String(); }
#endif

	static void _bind_methods() {}
	CSharpScript();
};

class CSharpInstance : public ScriptInstance {
	friend class CSharpScript;

	Object *owner = nullptr;
	Ref<CSharpScript> script;
	MonoObject *mono_object = nullptr;
	uint32_t gchandle = 0;

	MonoObject *invoke_method(MonoMethod *p_method, const Variant **p_args, int p_argcount, Variant &r_result, Callable::CallError &r_error);
	MonoMethod *find_method(const StringName &p_method, int p_argcount = -1);

public:
	Object *get_owner() override { return owner; }
	bool set(const StringName &p_name, const Variant &p_value) override;
	bool get(const StringName &p_name, Variant &r_ret) const override;
	void get_property_list(List<PropertyInfo> *p_properties) const override;
	Variant::Type get_property_type(const StringName &p_name, bool *r_is_valid) const override;
	void validate_property(PropertyInfo &p_property) const override {}
	bool property_can_revert(const StringName &p_name) const override { return false; }
	bool property_get_revert(const StringName &p_name, Variant &r_ret) const override { return false; }
	void get_method_list(List<MethodInfo> *p_list) const override;
	bool has_method(const StringName &p_method) const override;
	int get_method_argument_count(const StringName &p_method, bool *r_is_valid = nullptr) const override;
	Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override;
	void notification(int p_notification, bool p_reversed = false) override;
	String to_string(bool *r_valid) override;
	Ref<Script> get_script() const override { return script; }
	ScriptLanguage *get_language() override;
	CSharpInstance(const Ref<CSharpScript> &p_script, Object *p_owner);
	~CSharpInstance();
};

class CSharpLanguage : public ScriptLanguage {
	static CSharpLanguage *singleton;
	int lang_idx = -1;
	HashMap<String, MonoAssembly *> loaded_assemblies;
	MonoAssembly *scripts_assembly = nullptr;
	bool build_pending = false;
	// P6: Cooldown timestamp (msec) to debounce filesystem_changed bursts.
	// EditorFileSystem may fire the signal several times in rapid succession
	// when an external IDE saves a .cs file (write + stat + rename). The
	// cooldown coalesces these into a single build request.
	uint64_t last_build_request_ms = 0;
	static constexpr uint64_t BUILD_COOLDOWN_MS = 500;

	// P0-1 fix: hot-reload use-after-free prevention.
	// Previously reload_all_scripts()/build_project() called
	// mono_assembly_close() on the old scripts_assembly to evict Mono's
	// path-keyed image cache (so the next open() would see the freshly
	// compiled IL). But close() releases the image while CSharpScript and
	// CSharpInstance still hold raw MonoClass*/MonoObject* pointers into it
	// → UAF on the next has_method/get_method/sgen GC scan.
	//
	// Fix (report P0-1 方案 A, "永不 close"): hot reload now copies the new
	// dll to a versioned temp path (ProjectScripts.rev{N}.dll) and opens
	// that, bypassing Mono's path-keyed cache without closing the old image.
	// Old assemblies accumulate in `opened_assemblies` and are released as a
	// batch in finish() (editor shutdown — safe, no live references by then).
	uint64_t assembly_rev = 0;
	List<MonoAssembly *> opened_assemblies;

	// P2 v2: Global class metadata cache (class_name → info).
	// Populated by refresh_global_classes() by iterating the scripts assembly
	// TypeDef table (spec §4.P2.2). Replaces the v1 text-based scan in
	// get_global_class_name() with AOT-accurate IL metadata; the text scan is
	// retained as a fallback for the window before the assembly is loaded
	// (e.g., editor startup scan of res:// before first build).
	struct GlobalClassInfo {
		String base_type;
		bool is_abstract = false;
		bool is_tool = false;
	};
	HashMap<String, GlobalClassInfo> global_class_cache;
	bool global_classes_valid = false;

public:
	static CSharpLanguage *get_singleton() { return singleton; }
	void set_language_index(int p_idx) { lang_idx = p_idx; }

	MonoAssembly *load_scripts_assembly();
	MonoAssembly *get_scripts_assembly() const { return scripts_assembly; }
	void reload_all_pending_scripts();
	// P2 v2: Rebuild global_class_cache from scripts_assembly TypeDef table.
	// Called after load_scripts_assembly() succeeds (covers init / hot reload /
	// build_project paths). Safe to call when scripts_assembly is null — clears
	// the cache and marks it invalid, forcing get_global_class_name fallback.
	void refresh_global_classes();
	// P0-1 fix: open `p_dll_path` via a versioned temp copy so Mono's path-keyed
	// image cache doesn't return the stale pre-rebuild image. The original path
	// is copied to `<path>.rev{N}.dll` and that copy is opened. Returns null on
	// copy failure (caller may fall back to opening the original path directly,
	// accepting the stale-cache risk for cold start where there is no cache yet).
	MonoAssembly *open_versioned_assembly(const String &p_dll_path);

	void ensure_project_file();
	bool build_project();
	void request_build() { build_pending = true; }
	// P6: EditorFileSystem::filesystem_changed handler — request a build
	// subject to a 500ms cooldown (spec §4.P6.3).
	void _on_filesystem_changed();
	String get_project_csproj_path() const;
	String get_project_sln_path() const;
	String get_mono_assemblies_dir() const;

	String get_name() const override { return "C#"; }
	String get_type() const override { return "CSharpScript"; }
	String get_extension() const override { return "cs"; }
	void init() override;
	void finish() override;
	void frame() override;
	Vector<String> get_reserved_words() const override;
	bool is_control_flow_keyword(const String &p_keyword) const override;
	Vector<String> get_comment_delimiters() const override;
	Vector<String> get_doc_comment_delimiters() const override;
	Vector<String> get_string_delimiters() const override;
	bool is_using_templates() override { return true; }
	Ref<Script> make_template(const String &p_template, const String &p_class_name, const String &p_base_class_name) const override;
	Vector<ScriptTemplate> get_built_in_templates(const StringName &p_object) override;
	bool validate(const String &p_script, const String &p_path, List<String> *r_functions, List<ScriptError> *r_errors, List<Warning> *r_warnings, HashSet<int> *r_safe_lines) const override { return true; }
	String validate_path(const String &p_path) const override;
	bool supports_builtin_mode() const override { return false; }
	int find_function(const String &p_function, const String &p_code) const override { return -1; }
	String make_function(const String &p_class, const String &p_name, const PackedStringArray &p_args) const override { return ""; }
	ScriptNameCasing preferred_file_name_casing() const override { return SCRIPT_NAME_CASING_PASCAL_CASE; }
	// A1: 读 text_editor/behavior/indent 设置，供 make_template 替换 _TS_ 占位符。
	// 非 editor 或非 TOOLS 构建回退为 "\t"（与旧 mono 1963b2f 一致）。
	String _get_indentation() const;
	bool handles_global_class_type(const String &p_type) const override { return p_type == "CSharpScript"; }
	String get_global_class_name(const String &p_path, String *r_base_type, String *r_icon_path, bool *r_is_abstract, bool *r_is_tool) const override;

	void auto_indent_code(String &p_code, int p_from_line, int p_to_line) const override {}
	void add_global_constant(const StringName &p_variable, const Variant &p_value) override {}

	String debug_get_error() const override { return ""; }
	int debug_get_stack_level_count() const override { return 0; }
	int debug_get_stack_level_line(int p_level) const override { return -1; }
	String debug_get_stack_level_function(int p_level) const override { return ""; }
	String debug_get_stack_level_source(int p_level) const override { return ""; }
	void debug_get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) override {}
	void debug_get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) override {}
	void debug_get_globals(List<String> *p_globals, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) override {}
	String debug_parse_stack_level_expression(int p_level, const String &p_expression, int p_max_subitems = -1, int p_max_depth = -1) override { return ""; }
	Vector<StackInfo> debug_get_current_stack_info() override { return {}; }

	void reload_all_scripts() override;
	void reload_scripts(const Array &p_scripts, bool p_soft_reload) override;
	// P5: Reload a single [Tool] script — implemented in csharp_script.cpp.
	void reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) override;
	void get_recognized_extensions(List<String> *p_extensions) const override;
	void get_public_functions(List<MethodInfo> *p_functions) const override {}
	void get_public_constants(List<Pair<String, Variant>> *p_constants) const override {}
	void get_public_annotations(List<MethodInfo> *p_annotations) const override {}

	void profiling_start() override {}
	void profiling_stop() override {}
	void profiling_set_save_native_calls(bool p_enable) override {}
	int profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) override { return 0; }
	int profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) override { return 0; }

	CSharpLanguage();
	~CSharpLanguage();
};

void register_csharp_resource_loader();
void unregister_csharp_resource_loader();
