#include "csharp_script.h"
#include "mono_host.h"
#include "mono_bridge.h"
#include "mono_gc_bridge.h"
#include "mono_variant.h"
#include "utils/path_utils.h"
#include "utils/mono_script_metadata.h"
#include "core/object/object.h"
#include "core/object/script_language.h"
#include "core/config/engine.h"
#include "core/io/file_access.h"
#include "core/io/dir_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/resource.h"
#include "core/config/project_settings.h"
// P6: Time::get_ticks_msec() for build request cooldown.
#include "core/os/time.h"
#include "core/os/os.h"
#include "core/string/print_string.h"
#include <cstring>
#include "scene/main/node.h"
#include <mono/metadata/object.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/class.h>
#include <mono/metadata/attrdefs.h>
// P2 v2: mono_image_get_table_rows + MONO_TABLE_TYPEDEF for typedef iteration.
#include <mono/metadata/image.h>
#include <mono/metadata/blob.h>
#include <mono/metadata/mono-debug.h>
#include <cstdio>
#include <cstring>

// A1: 脚本模板（editor-only）。templates.gen.h 由 script_templates/SCsub 在
// editor_build 时生成，含 TEMPLATES[] 数组与 TEMPLATES_ARRAY_SIZE。
// EDITOR_GET 宏来自 editor/settings/editor_settings.h（Godot 4.7 路径），
// 供 _get_indentation() 读 text_editor/behavior/indent 设置。
#ifdef TOOLS_ENABLED
#include "editor/settings/editor_settings.h"
#include "editor/script_templates/templates.gen.h"
// P4: MonoBuildPanel — bottom dock for dotnet build output. build_project()
// routes stdout/stderr here via MonoBuildPanel::append_output().
#include "editor/mono_build_panel.h"
#endif

using namespace mono_variant;
using namespace mono_bridge;

// Verbose Mono-side logging. In release templates (target=template_release)
// the per-frame notification prints below would flood stdout and add real
// overhead (printf + fflush every frame for every script instance), so they
// are compiled out via DEBUG_ENABLED. Editor / debug builds still emit them
// for diagnostics. (M13)
#ifdef DEBUG_ENABLED
#define MONO_LOG(...) do { printf(__VA_ARGS__); fflush(stdout); } while (0)
#else
#define MONO_LOG(...) do {} while (0)
#endif

class ResourceFormatLoaderCSharpScript : public ResourceFormatLoader {
	GDSOFTCLASS(ResourceFormatLoaderCSharpScript, ResourceFormatLoader);

public:
	Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override {
		if (p_path.is_empty()) {
			if (r_error) *r_error = ERR_INVALID_PARAMETER;
			return Ref<Resource>();
		}

		// Filter out auto-generated .cs files from build directories
		String path_lower = p_path.to_lower();
		if (path_lower.contains("/obj/") || path_lower.contains("\\obj\\") ||
			path_lower.contains("/bin/") || path_lower.contains("\\bin\\") ||
			path_lower.contains("/.mono/") || path_lower.contains("\\.mono\\")) {
			if (r_error) *r_error = ERR_FILE_UNRECOGNIZED;
			return Ref<Resource>();
		}

		Ref<CSharpScript> scr;
		scr.instantiate();
		String load_path = p_original_path.is_empty() ? p_path : p_original_path;
		scr->set_path(load_path);

		String class_name = p_path.get_file().get_basename();
		scr->set_class_name(class_name);

		Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
		if (f.is_valid()) {
			String source = f->get_as_utf8_string();
			scr->set_source_code(source);
		}

		Error err = scr->reload();
		if (r_error) {
			*r_error = err;
		}
		return scr;
	}

	void get_recognized_extensions(List<String> *p_extensions) const override {
		p_extensions->push_back("cs");
	}

	bool handles_type(const String &p_type) const override {
		return p_type == "Script" || p_type == "CSharpScript";
	}

	String get_resource_type(const String &p_path) const override {
		if (p_path.get_extension().to_lower() == "cs") {
			// Filter out auto-generated .cs files from build directories
			String path_lower = p_path.to_lower();
			if (path_lower.contains("/obj/") || path_lower.contains("\\obj\\") ||
				path_lower.contains("/bin/") || path_lower.contains("\\bin\\") ||
				path_lower.contains("/.mono/") || path_lower.contains("\\.mono\\")) {
				return "";
			}
			return "CSharpScript";
		}
		return "";
	}

	void get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types = false) override {}
};

class ResourceFormatSaverCSharpScript : public ResourceFormatSaver {
	GDSOFTCLASS(ResourceFormatSaverCSharpScript, ResourceFormatSaver);

public:
	Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) override {
		Ref<CSharpScript> cs_script = p_resource;
		ERR_FAIL_COND_V(cs_script.is_null(), ERR_INVALID_PARAMETER);

		String source = cs_script->get_source_code();

		{
			Error err;
			Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
			ERR_FAIL_COND_V_MSG(err, err, "Cannot save C# script file '" + p_path + "'.");

			file->store_string(source);
			if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
				return ERR_CANT_CREATE;
			}
		}

		if (CSharpLanguage::get_singleton()) {
			CSharpLanguage::get_singleton()->request_build();
		}

		return OK;
	}

	void get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const override {
		if (Object::cast_to<CSharpScript>(*p_resource)) {
			p_extensions->push_back("cs");
		}
	}

	bool recognize(const Ref<Resource> &p_resource) const override {
		return Object::cast_to<CSharpScript>(*p_resource) != nullptr;
	}
};

static Ref<ResourceFormatLoaderCSharpScript> resource_loader_csharp;
static Ref<ResourceFormatSaverCSharpScript> resource_saver_csharp;

CSharpLanguage *CSharpLanguage::singleton = nullptr;

// A3: sanitize_project_name / get_safe_project_name migrated to utils/path_utils.{h,cpp}
// as Path::sanitize_project_name / Path::get_csharp_project_name (single source of truth,
// previously duplicated in mono_export_plugin.cpp as sanitize_assembly_name).

CSharpScript::CSharpScript() {}

ScriptLanguage *CSharpScript::get_language() const {
	return CSharpLanguage::get_singleton();
}

bool CSharpScript::has_method(const StringName &p_method) const {
	if (!mono_class) return false;
	String mname = String(p_method);
	MonoMethod *m = mono_class_get_method_from_name(mono_class, mname.utf8().get_data(), -1);
	return m != nullptr;
}

int CSharpScript::get_script_method_argument_count(const StringName &p_method, bool *r_is_valid) const {
	if (!mono_class) {
		if (r_is_valid) *r_is_valid = false;
		return 0;
	}
	String mname = String(p_method);
	MonoMethod *m = mono_class_get_method_from_name(mono_class, mname.utf8().get_data(), -1);
	if (!m) {
		if (r_is_valid) *r_is_valid = false;
		return 0;
	}
	MonoMethodSignature *sig = mono_method_signature(m);
	int count = mono_signature_get_param_count(sig);
	if (r_is_valid) *r_is_valid = true;
	return count;
}

void CSharpScript::get_script_method_list(List<MethodInfo> *r_list) const {
	if (!mono_class) return;
	void *iter = nullptr;
	while (MonoMethod *m = mono_class_get_methods(mono_class, &iter)) {
		const char *mname_str = mono_method_get_name(m);
		if (mname_str && mname_str[0] != '.' && strncmp(mname_str, "get_", 4) != 0 && strncmp(mname_str, "set_", 4) != 0) {
			MethodInfo mi;
			mi.name = String::utf8(mname_str);
			r_list->push_back(mi);
		}
	}
}

Variant CSharpScript::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
	return Variant();
}

// P1: Return [Export]-marked members as PropertyInfo list for Inspector display.
// Names are kept as C# PascalCase (matching how set/get handle them).
// Usage flags: PROPERTY_USAGE_EDITOR (visible in Inspector) | PROPERTY_USAGE_STORAGE (serialized).
void CSharpScript::get_script_property_list(List<PropertyInfo> *r_list) const {
	if (!exported_members_valid) {
		return;
	}
	for (const PropertyInfo &pi : exported_properties) {
		r_list->push_back(pi);
	}
}

// P1: Return the default value for an exported member.
// Constructs the type's default Variant (0 for INT, 0.0 for FLOAT, "" for STRING, etc.).
// This is used by Inspector "Revert" and scene serialization for first-time save.
bool CSharpScript::get_property_default_value(const StringName &p_property, Variant &r_value) const {
	if (!exported_members_valid) {
		return false;
	}
	for (const PropertyInfo &pi : exported_properties) {
		if (pi.name == p_property) {
			// Construct default Variant for this type (0 args = default constructor).
			Callable::CallError ce;
			Variant::construct(pi.type, r_value, nullptr, 0, ce);
			return ce.error == Callable::CallError::CALL_OK;
		}
	}
	return false;
}

// P3: Check if the script declares a signal with the given name.
// Iterates signal_cache (populated in resolve_mono_class from [Signal] delegates).
bool CSharpScript::has_script_signal(const StringName &p_signal) const {
	if (!signals_valid) {
		return false;
	}
	for (const MethodInfo &mi : signal_cache) {
		if (mi.name == p_signal) {
			return true;
		}
	}
	return false;
}

// P3: Return all [Signal]-marked delegates as MethodInfo list.
// Used by the editor signal panel to list available signals for connection.
void CSharpScript::get_script_signal_list(List<MethodInfo> *r_signals) const {
	if (!signals_valid) {
		return;
	}
	for (const MethodInfo &mi : signal_cache) {
		r_signals->push_back(mi);
	}
}

String CSharpScript::_parse_base_class() const {
	if (source.is_empty()) return "Node";

	Vector<String> lines = source.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i].strip_edges();
		int comment_pos = line.find("//");
		if (comment_pos >= 0) line = line.substr(0, comment_pos).strip_edges();
		if (line.is_empty()) continue;

		int colon_pos = line.find(":");
		if (colon_pos < 0) continue;

		String after_colon = line.substr(colon_pos + 1).strip_edges();
		if (after_colon.is_empty()) continue;

		Vector<String> parts = after_colon.split(",", false);
		if (parts.size() > 0) {
			String base = parts[0].strip_edges();
			if (base.is_empty()) continue;

			int space_pos = base.find(" ");
			if (space_pos > 0) base = base.substr(0, space_pos).strip_edges();

			int angle_pos = base.find("<");
			if (angle_pos > 0) base = base.substr(0, angle_pos).strip_edges();

			int dot_pos = base.rfind(".");
			if (dot_pos >= 0) base = base.substr(dot_pos + 1).strip_edges();

			return base;
		}
	}

	return "Node";
}

String CSharpScript::_parse_namespace() const {
	if (source.is_empty()) return "";

	Vector<String> lines = source.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i].strip_edges();
		if (line.begins_with("namespace ")) {
			String ns = line.substr(10).strip_edges();
			int brace_pos = ns.find("{");
			if (brace_pos > 0) ns = ns.substr(0, brace_pos).strip_edges();
			int semicolon_pos = ns.find(";");
			if (semicolon_pos > 0) ns = ns.substr(0, semicolon_pos).strip_edges();
			return ns;
		}
	}
	return "";
}

// P2: Parse a .cs file to extract class name, base type, and attributes.
// Used by the editor to scan res:// for global classes without loading the assembly.
// Returns the class name (or "" if not a valid C# script).
// r_base_type: the Godot native base class (Node, Control, etc.)
// r_icon_path: currently empty (IconPath attribute parsing not implemented)
// r_is_abstract: true if class has "abstract" modifier
// r_is_tool: true if class has [Tool] attribute (text-based scan)
//
// P2 v2: When the scripts assembly is loaded and global_class_cache is valid,
// prefer the AOT-accurate IL metadata cache over the text-based .cs file scan.
// The text scan is retained as a fallback for the window before the assembly
// is loaded (e.g., editor startup scan of res:// before first build).
String CSharpLanguage::get_global_class_name(const String &p_path, String *r_base_type, String *r_icon_path, bool *r_is_abstract, bool *r_is_tool) const {
	if (p_path.get_extension().to_lower() != "cs") {
		return "";
	}

	// Filter out build directories (same logic as ResourceFormatLoaderCSharpScript).
	String path_lower = p_path.to_lower();
	if (path_lower.contains("/obj/") || path_lower.contains("\\obj\\") ||
		path_lower.contains("/bin/") || path_lower.contains("\\bin\\") ||
		path_lower.contains("/.mono/") || path_lower.contains("\\.mono\\")) {
		return "";
	}

	// P2 v2 [REV-#06]: prefer .pdb reverse-lookup when valid (desktop editor).
	// The source map uses source file path as key (normalized to res://),
	// completely lifting the file_name==class_name constraint — a class
	// declared in res://scripts/actor/player.cs is correctly matched
	// regardless of the .cs filename.
	//
	// On WASM (WEB_ENABLED), the source map is always empty (.pdb unavailable),
	// so we fall through to the file_name==class_name convention below.
	// See spike_2026-07-26_p5_pdb.md for details.
	if (global_classes_valid) {
#if defined(TOOLS_ENABLED) && !defined(WEB_ENABLED)
		// Normalize p_path to res:// for lookup (source map keys are normalized).
		String lookup_path = p_path.replace("\\", "/");
		String res_path = ProjectSettings::get_singleton() ?
				ProjectSettings::get_singleton()->get_resource_path() : "";
		if (!res_path.is_empty()) {
			String norm_res = res_path.replace("\\", "/");
			if (lookup_path.begins_with(norm_res)) {
				lookup_path = "res://" + lookup_path.substr(norm_res.length()).lstrip("/");
			}
		}
		const String *src_match = global_class_source_map.getptr(lookup_path);
		if (src_match) {
			const GlobalClassInfo *info = global_class_cache.getptr(*src_match);
			if (info) {
				if (r_base_type) *r_base_type = info->base_type;
				if (r_icon_path) *r_icon_path = "";
				if (r_is_abstract) *r_is_abstract = info->is_abstract;
				if (r_is_tool) *r_is_tool = info->is_tool;
				return *src_match;
			}
		}
#endif // TOOLS_ENABLED && !WEB_ENABLED

		// Fallback: file_name==class_name convention (WASM, or desktop when
		// .pdb lookup missed — e.g., newly added script not yet built).
		// Under this convention, the cache key is the class name, which
		// equals the .cs basename without extension (spec §0.2.5).
		String candidate = p_path.get_file().get_basename();
		const GlobalClassInfo *info = global_class_cache.getptr(candidate);
		if (info) {
			if (r_base_type) *r_base_type = info->base_type;
			if (r_icon_path) *r_icon_path = "";
			if (r_is_abstract) *r_is_abstract = info->is_abstract;
			if (r_is_tool) *r_is_tool = info->is_tool;
			return candidate;
		}
		// P2 v2 fix (report §四): cache-valid-miss must NOT return "" directly.
		// A newly added [GlobalClass] script that hasn't been built yet would be
		// invisible in Add Node until the next successful build refreshes the
		// cache. Fall through to text-based scan so the editor sees the new
		// class immediately (text scan is the source of truth pre-build).
	}

	// Fallback: text-based scan (assembly not yet loaded, OR cache miss for
	// a not-yet-built [GlobalClass] script — report §四 cache-valid-miss case).
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return "";
	}

	String source = f->get_as_utf8_string();
	Vector<String> lines = source.split("\n");

	bool has_global_class = false;
	bool has_tool = false;
	bool is_abstract = false;
	String class_name_str;
	String base_type;

	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i].strip_edges();

		// Strip comments.
		int comment_pos = line.find("//");
		if (comment_pos >= 0) line = line.substr(0, comment_pos).strip_edges();
		if (line.is_empty()) continue;

		// Check for [GlobalClass] attribute (text-based scan).
		if (line.begins_with("[GlobalClass")) {
			has_global_class = true;
			continue;
		}
		// Check for [Tool] attribute.
		if (line.begins_with("[Tool")) {
			has_tool = true;
			continue;
		}

		// Look for class declaration: "public partial class Foo : Bar"
		// or "public abstract class Foo : Bar"
		int class_pos = line.find("class ");
		if (class_pos < 0) continue;

		// Check for "abstract" modifier before "class".
		String before_class = line.substr(0, class_pos);
		if (before_class.find("abstract") >= 0) {
			is_abstract = true;
		}

		// Extract class name.
		String after_class = line.substr(class_pos + 6).strip_edges();
		// Class name ends at space, colon, or brace.
		int name_end = after_class.length();
		for (int c = 0; c < after_class.length(); c++) {
			char32_t ch = after_class[c];
			if (ch == ' ' || ch == ':' || ch == '{' || ch == '\t') {
				name_end = c;
				break;
			}
		}
		class_name_str = after_class.substr(0, name_end).strip_edges();

		// Extract base type (same logic as _parse_base_class).
		int colon_pos = line.find(":");
		if (colon_pos >= 0) {
			String after_colon = line.substr(colon_pos + 1).strip_edges();
			if (!after_colon.is_empty()) {
				Vector<String> parts = after_colon.split(",", false);
				if (parts.size() > 0) {
					String base = parts[0].strip_edges();
					int space_pos = base.find(" ");
					if (space_pos > 0) base = base.substr(0, space_pos).strip_edges();
					int angle_pos = base.find("<");
					if (angle_pos > 0) base = base.substr(0, angle_pos).strip_edges();
					int dot_pos = base.rfind(".");
					if (dot_pos >= 0) base = base.substr(dot_pos + 1).strip_edges();
					base_type = base;
				}
			}
		}
		break; // Only parse first class declaration.
	}

	if (class_name_str.is_empty() || !has_global_class) {
		return "";
	}

	if (r_base_type) *r_base_type = base_type.is_empty() ? String("Node") : base_type;
	if (r_icon_path) *r_icon_path = String();
	if (r_is_abstract) *r_is_abstract = is_abstract;
	if (r_is_tool) *r_is_tool = has_tool;

	return class_name_str;
}

void CSharpScript::resolve_mono_class() {
	mono_class = nullptr;
	mono_image = nullptr;
	method_cache.clear();
	mono_class_valid = false;

	if (class_name.is_empty()) {
		printf("[Mono] resolve_mono_class: class_name is empty!\n");
		fflush(stdout);
		return;
	}

	MonoDomain *domain = MonoHost::get_singleton() ? MonoHost::get_singleton()->get_domain() : nullptr;
	if (!domain) {
		printf("[Mono] resolve_mono_class: no domain for '%s'\n", class_name.utf8().get_data());
		fflush(stdout);
		return;
	}

	if (CSharpLanguage::get_singleton()) {
		CSharpLanguage::get_singleton()->load_scripts_assembly();
	}

	if (!CSharpLanguage::get_singleton() || !CSharpLanguage::get_singleton()->get_scripts_assembly()) {
		printf("[Mono] resolve_mono_class: no scripts assembly for '%s'\n", class_name.utf8().get_data());
		fflush(stdout);
		return;
	}

	CharString cname_utf8 = class_name.utf8();
	const char *cname = cname_utf8.get_data();
	String ns = _parse_namespace();
	CharString ns_utf8_str = ns.utf8();

	printf("[Mono] resolve_mono_class: looking for class '%s' (ns='%s')\n", cname, ns_utf8_str.get_data());
	fflush(stdout);

	Vector<MonoImage *> search_images;
	if (CSharpLanguage::get_singleton() && CSharpLanguage::get_singleton()->get_scripts_assembly()) {
		search_images.push_back(mono_assembly_get_image(CSharpLanguage::get_singleton()->get_scripts_assembly()));
	}
	if (MonoHost::get_singleton() && MonoHost::get_singleton()->get_godotsharp_assembly()) {
		search_images.push_back(mono_assembly_get_image(MonoHost::get_singleton()->get_godotsharp_assembly()));
	}
	search_images.push_back(mono_get_corlib());

	Vector<String> namespaces_to_search;
	namespaces_to_search.push_back(ns);
	namespaces_to_search.push_back("");
	namespaces_to_search.push_back("Godot");

	Vector<CharString> ns_utf8;
	for (int ni = 0; ni < namespaces_to_search.size(); ni++) {
		ns_utf8.push_back(namespaces_to_search[ni].utf8());
	}

	for (int img_idx = 0; img_idx < search_images.size(); img_idx++) {
		MonoImage *img = search_images[img_idx];
		if (!img) continue;
		for (int ni = 0; ni < ns_utf8.size(); ni++) {
			mono_class = mono_class_from_name(img, ns_utf8[ni].get_data(), cname);
			if (mono_class) {
				printf("[Mono] resolve_mono_class: FOUND class '%s' in ns='%s' (image %d)\n", cname, ns_utf8[ni].get_data(), img_idx);
				fflush(stdout);
				break;
			}
		}
		if (mono_class) {
			mono_image = img;
			break;
		}
	}

	if (mono_class) {
		if (mono_class_get_flags(mono_class) & MONO_TYPE_ATTR_ABSTRACT) {
			printf("[Mono] resolve_mono_class: class '%s' is abstract, skipping\n", cname);
			fflush(stdout);
			mono_class = nullptr;
		} else {
			mono_class_valid = true;
			printf("[Mono] resolve_mono_class: class '%s' resolved successfully\n", cname);
			fflush(stdout);

			// P1: Collect [Export] members and check [Tool] attribute.
			// Convert to PropertyInfo immediately to avoid Mono header dependency in csharp_script.h.
			List<mono_script_meta::ExportedMember> members;
			mono_script_meta::collect_exported_members(mono_class, members);
			exported_properties.clear();
			for (const mono_script_meta::ExportedMember &m : members) {
				PropertyInfo pi;
				pi.type = m.type;
				pi.name = m.name;
				pi.hint = PROPERTY_HINT_NONE;
				pi.usage = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE;
				exported_properties.push_back(pi);
			}
			exported_members_valid = true;
			is_tool_class = mono_script_meta::class_has_attribute(mono_class, "ToolAttribute");
			is_global_class = mono_script_meta::class_has_attribute(mono_class, "GlobalClassAttribute");
			// P3: Collect [Signal]-marked nested delegates into signal_cache.
			signal_cache.clear();
			mono_script_meta::collect_signals(mono_class, signal_cache);
			signals_valid = true;
			printf("[Mono] resolve_mono_class: class '%s' has %d exported members, %d signals, is_tool=%d, is_global=%d\n",
					cname, exported_properties.size(), signal_cache.size(), is_tool_class ? 1 : 0, is_global_class ? 1 : 0);
			fflush(stdout);
		}
	} else {
		printf("[Mono] resolve_mono_class: class '%s' NOT FOUND in any assembly\n", cname);
		fflush(stdout);
	}
}

MonoMethod *CSharpScript::get_method(const StringName &p_method, int p_argcount) {
	if (!mono_class) return nullptr;

	StringName key = p_method;
	if (p_argcount >= 0) {
		key = StringName(String(p_method) + ":" + itos(p_argcount));
	}

	MonoMethod **cached = method_cache.getptr(key);
	if (cached) return *cached;

	String mname = String(p_method);
	MonoMethod *m = mono_class_get_method_from_name(mono_class, mname.utf8().get_data(), p_argcount);

	if (m) {
		method_cache[key] = m;
	}
	return m;
}

Error CSharpScript::reload(bool p_keep_state) {
	String path = get_path();
	source_valid = false;
	mono_class_valid = false;
	mono_class = nullptr;
	mono_image = nullptr;
	method_cache.clear();
	exported_properties.clear();
	exported_members_valid = false;
	is_tool_class = false;
	is_global_class = false;
	signal_cache.clear();
	signals_valid = false;

	if (path.is_empty()) {
		source_valid = true;
		class_name = "Node";
		native_base_name = "Node";
		return OK;
	}

	class_name = path.get_file().get_basename();

	String base_from_source = _parse_base_class();
	native_base_name = StringName(base_from_source);

	if (!source.is_empty()) {
		source_valid = true;
	} else {
		source_valid = false;
	}

	if (MonoHost::get_singleton() && MonoHost::get_singleton()->get_domain() && !class_name.is_empty()) {
		resolve_mono_class();
	}

	return OK;
}

ScriptInstance *CSharpScript::instance_create(Object *p_this) {
	printf("[Mono] instance_create called for '%s' (mono_class=%p, source_valid=%d)\n",
		   class_name.utf8().get_data(), mono_class, (int)source_valid);
	fflush(stdout);

	if (source_valid && !mono_class && MonoHost::get_singleton() && MonoHost::get_singleton()->get_domain()) {
		resolve_mono_class();
	}

	if (!mono_class) {
		printf("[Mono] instance_create: no mono_class for '%s', cannot create instance\n", class_name.utf8().get_data());
		fflush(stdout);
		return nullptr;
	}

	printf("[Mono] instance_create: creating CSharpInstance for '%s'\n", class_name.utf8().get_data());
	fflush(stdout);
	return memnew(CSharpInstance(Ref<CSharpScript>(this), p_this));
}

bool CSharpScript::can_instantiate() const {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton() && Engine::get_singleton()->is_editor_hint()) {
		return mono_class != nullptr && !Engine::get_singleton()->is_recovery_mode_hint() && ScriptServer::is_scripting_enabled();
	}
#endif
	return mono_class != nullptr;
}

PlaceHolderScriptInstance *CSharpScript::placeholder_instance_create(Object *p_this) {
#ifdef TOOLS_ENABLED
	PlaceHolderScriptInstance *si = memnew(PlaceHolderScriptInstance(CSharpLanguage::get_singleton(), Ref<Script>(this), p_this));
	return si;
#else
	return nullptr;
#endif
}

CSharpInstance::CSharpInstance(const Ref<CSharpScript> &p_script, Object *p_owner) {
	script = p_script;
	owner = p_owner;
	mono_object = nullptr;
	gchandle = 0;

	if (!script.is_valid() || !owner) {
		printf("[Mono] CSharpInstance: null script or owner\n");
		fflush(stdout);
		return;
	}

	MonoDomain *domain = MonoHost::get_singleton() ? MonoHost::get_singleton()->get_domain() : nullptr;
	if (!domain) {
		printf("[Mono] CSharpInstance: no domain\n");
		fflush(stdout);
		return;
	}

	MonoClass *klass = script->mono_class;
	if (!klass) {
		printf("[Mono] CSharpInstance: no mono_class for script '%s'\n", script->class_name.utf8().get_data());
		fflush(stdout);
		return;
	}

	MonoObject *existing = mono_gc_bridge::get_managed(owner);
	if (existing) {
		mono_object = existing;
		gchandle = mono_gchandle_new(mono_object, false);
		printf("[Mono] CSharpInstance: reused existing managed object for '%s'\n", script->class_name.utf8().get_data());
		fflush(stdout);
		return;
	}

	printf("[Mono] CSharpInstance: creating new managed object for '%s'\n", script->class_name.utf8().get_data());
	fflush(stdout);

	// Try to initialize the class first (this will reveal init failures)
	printf("[Mono] CSharpInstance: calling mono_class_init for '%s'...\n", script->class_name.utf8().get_data());
	fflush(stdout);
	mono_bool init_ok = mono_class_init(klass);
	printf("[Mono] CSharpInstance: mono_class_init returned: %d\n", (int)init_ok);
	fflush(stdout);

	// Print class hierarchy for debugging
	{
		MonoClass *k = klass;
		int depth = 0;
		while (k && depth < 10) {
			const char *kname = mono_class_get_name(k);
			const char *kns = mono_class_get_namespace(k);
			MONO_LOG("[Mono]   class[%d]: '%s' (ns='%s')\n", depth, kname ? kname : "?", kns ? kns : "?");
			k = mono_class_get_parent(k);
			depth++;
		}
	}

	MonoObject *cs_obj = mono_object_new(domain, klass);
	if (!cs_obj) {
		printf("[Mono] CSharpInstance: mono_object_new failed for '%s'\n", script->class_name.utf8().get_data());
		fflush(stdout);
		return;
	}

	printf("[Mono] CSharpInstance: managed object created, setting NativePtr\n");
	fflush(stdout);

	MonoClassField *native_ptr_field = nullptr;
	for (MonoClass *k = klass; k && !native_ptr_field; k = mono_class_get_parent(k)) {
		native_ptr_field = mono_class_get_field_from_name(k, "NativePtr");
	}

	if (native_ptr_field) {
		intptr_t ptr_val = (intptr_t)owner;
		mono_field_set_value(cs_obj, native_ptr_field, &ptr_val);
	}

	MonoObject *exc = nullptr;
	// mono_runtime_object_init_checked is not available in Mono 6.12.
	// Use mono_runtime_invoke on the parameterless .ctor() to get exception
	// output support. (Prior code used the non-existent _checked variant.)
	MonoMethod *ctor = mono_class_get_method_from_name(klass, ".ctor", 0);
	if (ctor) {
		mono_runtime_invoke(ctor, cs_obj, nullptr, &exc);
	} else {
		// No parameterless constructor — fall back to the plain init.
		mono_runtime_object_init(cs_obj);
	}

	if (exc) {
		MonoClass *exc_class = mono_object_get_class(exc);
		const char *exc_name = exc_class ? mono_class_get_name(exc_class) : "(unknown)";
		MonoString *msg_str = (MonoString *)mono_object_to_string(exc, nullptr);
		char *msg_utf8 = msg_str ? mono_string_to_utf8(msg_str) : nullptr;
		printf("[Mono] CSharpInstance: constructor exception for '%s': %s: %s\n",
			   script->class_name.utf8().get_data(),
			   exc_name ? exc_name : "?",
			   msg_utf8 ? msg_utf8 : "?");
		fflush(stdout);
		if (msg_utf8) mono_free(msg_utf8);
		return;
	}

	mono_bridge::tie_native_ptr(cs_obj, owner);

	mono_object = cs_obj;
	gchandle = mono_gchandle_new(mono_object, false);

	printf("[Mono] CSharpInstance: successfully created for '%s' (obj=%p, handle=%u)\n",
		   script->class_name.utf8().get_data(), mono_object, gchandle);
	fflush(stdout);
}

CSharpInstance::~CSharpInstance() {
	if (gchandle != 0) {
		mono_gchandle_free(gchandle);
		gchandle = 0;
	}
	mono_object = nullptr;
}

MonoMethod *CSharpInstance::find_method(const StringName &p_method, int p_argcount) {
	if (!mono_object) return nullptr;

	String method_name = String(p_method);

	struct VMethodMap {
		const char *godot_name;
		const char *cs_name;
	};
	static const VMethodMap vmap[] = {
		{"_ready", "_Ready"},
		{"_enter_tree", "_EnterTree"},
		{"_exit_tree", "_ExitTree"},
		{"_process", "_Process"},
		{"_physics_process", "_PhysicsProcess"},
		{"_input", "_Input"},
		{"_unhandled_input", "_UnhandledInput"},
		{"_notification", "_Notification"},
		{"_init", "_Init"},
		{nullptr, nullptr}
	};
	for (int i = 0; vmap[i].godot_name; i++) {
		if (method_name == vmap[i].godot_name) {
			method_name = vmap[i].cs_name;
			break;
		}
	}

	// N3 fix: resolve against the OBJECT's own class hierarchy FIRST.
	// After a hot reload (P0-1 versioned assembly), CSharpScript::mono_class
	// points at the NEW image while a pre-existing instance's mono_object is
	// still an instance of the OLD image's class. mono_runtime_invoke() with
	// a MonoMethod* from a different image than the object's class throws or
	// crashes on the class-identity check. Resolving on the object's own
	// class keeps old instances running their old (consistent) code; new
	// instances created after the reload have objects of the new class and
	// therefore resolve the new methods. script->get_method() remains as a
	// fallback for the rare case the method isn't found on the object class.
	MonoClass *klass = mono_object_get_class(mono_object);
	StringName key = StringName(method_name);
	if (p_argcount >= 0) {
		key = StringName(method_name + ":" + itos(p_argcount));
	}

	CharString mname_utf8 = method_name.utf8();
	const char *mname_cstr = mname_utf8.get_data();
	for (MonoClass *k = klass; k; k = mono_class_get_parent(k)) {
		MonoMethod *m = mono_class_get_method_from_name(k, mname_cstr, p_argcount);
		if (m) return m;
	}

	if (script.is_valid() && script->mono_class) {
		MonoMethod *m = script->get_method(StringName(method_name), p_argcount);
		if (m) return m;
	}

	return nullptr;
}

MonoObject *CSharpInstance::invoke_method(MonoMethod *p_method, const Variant **p_args, int p_argcount, Variant &r_result, Callable::CallError &r_error) {
	r_error.error = Callable::CallError::CALL_OK;
	if (!p_method || !mono_object) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return nullptr;
	}

	MonoDomain *domain = mono_domain_get();
	MonoMethodSignature *sig = mono_method_signature(p_method);
	int param_count = mono_signature_get_param_count(sig);

	void **args = nullptr;
	Vector<MonoObject *> arg_refs;

	if (param_count > 0) {
		arg_refs.resize(param_count);
		args = (void **)alloca(sizeof(void *) * param_count);
		int copy_count = param_count < p_argcount ? param_count : p_argcount;

		// Get parameter types so we can properly handle value types.
		// mono_runtime_invoke expects pointers to raw values for valuetype
		// parameters, not MonoObject* (boxed). Passing a boxed object for a
		// value type parameter causes "RuntimeError: function signature
		// mismatch" in WASM interpreter mode.
		MonoType *param_types[16];
		int pt_count = 0;
		{
			void *sig_iter = nullptr;
			while (MonoType *pt = mono_signature_get_params(sig, &sig_iter)) {
				if (pt_count < 16) {
					param_types[pt_count++] = pt;
				}
			}
		}

		for (int i = 0; i < copy_count; i++) {
			MonoObject *mo = variant_to_mono_object(domain, *p_args[i]);
			arg_refs.write[i] = mo;

			// For value type parameters, pass a pointer to the unboxed data.
			if (i < pt_count && param_types[i]) {
				MonoClass *pc = mono_class_from_mono_type(param_types[i]);
				if (pc && mono_class_is_valuetype(pc)) {
					if (mo) {
						args[i] = mono_object_unbox(mo);
					} else {
						// NULL Variant → default (zeroed) value type buffer.
						// Passing nullptr would crash mono_runtime_invoke when it
						// dereferences the parameter pointer.
						size_t val_size = mono_class_value_size(pc, nullptr);
						if (val_size > 0) {
							void *buf = alloca(val_size);
							memset(buf, 0, val_size);
							args[i] = buf;
						} else {
							args[i] = nullptr;
						}
					}
					continue;
				}
			}
			args[i] = mo;
		}
		for (int i = copy_count; i < param_count; i++) {
			// Trailing missing args: pass default (zeroed) value type buffer
			// for value type params, nullptr for ref type params.
			if (i < pt_count && param_types[i]) {
				MonoClass *pc = mono_class_from_mono_type(param_types[i]);
				if (pc && mono_class_is_valuetype(pc)) {
					size_t val_size = mono_class_value_size(pc, nullptr);
					if (val_size > 0) {
						void *buf = alloca(val_size);
						memset(buf, 0, val_size);
						args[i] = buf;
						continue;
					}
				}
			}
			args[i] = nullptr;
		}
	}

	MonoObject *exc = nullptr;
	MonoObject *ret = mono_runtime_invoke(p_method, mono_object, args, &exc);

	if (exc) {
		MonoClass *exc_class = mono_object_get_class(exc);
		const char *exc_name = exc_class ? mono_class_get_name(exc_class) : "(unknown)";
		MonoString *msg_str = (MonoString *)mono_object_to_string(exc, nullptr);
		char *msg_utf8 = msg_str ? mono_string_to_utf8(msg_str) : nullptr;
		printf("[Mono] Exception in C# method '%s': %s: %s\n",
			   mono_method_get_name(p_method),
			   exc_name ? exc_name : "?",
			   msg_utf8 ? msg_utf8 : "?");
		if (msg_utf8) mono_free(msg_utf8);
		// P5 [REV-#11]: Clear pending exception state to prevent downstream
		// mono_runtime_invoke calls from observing a stale exception (which
		// can cascade into editor instability under [Tool] script fuzz).
		// P1-#4 fix: overwrite MUST be true. Mono 6.12 semantics:
		// set_pending_exception(ex, false) only writes if no exception is
		// currently pending — i.e., in the exact case we're trying to clear
		// (an exception IS pending) it's a no-op. Passing true forces the
		// nullptr to overwrite the pending exception regardless of state.
		mono_runtime_set_pending_exception(nullptr, true);
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		r_result = Variant();
		return nullptr;
	}

	MonoType *ret_type = mono_signature_get_return_type(sig);
	if (ret_type && mono_type_get_type(ret_type) != MONO_TYPE_VOID) {
		r_result = mono_object_to_variant(ret);
	} else {
		r_result = Variant();
	}

	return ret;
}

bool CSharpInstance::set(const StringName &p_name, const Variant &p_value) {
	if (!mono_object) return false;

	String prop_name = String(p_name);
	String setter_name = "set_" + prop_name;
	MonoMethod *setter = find_method(setter_name, 1);
	if (setter) {
		Variant result;
		Callable::CallError err;
		const Variant *args[1] = { &p_value };
		invoke_method(setter, args, 1, result, err);
		return err.error == Callable::CallError::CALL_OK;
	}

	MonoClass *klass = mono_object_get_class(mono_object);
	MonoClassField *field = nullptr;
	CharString pname_utf8 = prop_name.utf8();
	const char *pname_cstr = pname_utf8.get_data();
	for (MonoClass *k = klass; k && !field; k = mono_class_get_parent(k)) {
		field = mono_class_get_field_from_name(k, pname_cstr);
	}
	if (field) {
		// Set-fix (found by csharp_test scenario 24): Variant::INT boxes as
		// Int64 and Variant::FLOAT as double, but C# fields may be int/float
		// (4 bytes). mono_field_set_value with the boxed 8-byte unbox would
		// clobber the adjacent field. Convert to the field's ACTUAL type
		// before writing.
		MonoType *ftype = mono_field_get_type(field);
		switch (mono_type_get_type(ftype)) {
			case MONO_TYPE_BOOLEAN: { bool v = (bool)p_value; mono_field_set_value(mono_object, field, &v); break; }
			case MONO_TYPE_I1: { int8_t v = (int8_t)(int64_t)p_value; mono_field_set_value(mono_object, field, &v); break; }
			case MONO_TYPE_U1: { uint8_t v = (uint8_t)(int64_t)p_value; mono_field_set_value(mono_object, field, &v); break; }
			case MONO_TYPE_I2: { int16_t v = (int16_t)(int64_t)p_value; mono_field_set_value(mono_object, field, &v); break; }
			case MONO_TYPE_U2: { uint16_t v = (uint16_t)(int64_t)p_value; mono_field_set_value(mono_object, field, &v); break; }
			case MONO_TYPE_I4: { int32_t v = (int32_t)(int64_t)p_value; mono_field_set_value(mono_object, field, &v); break; }
			case MONO_TYPE_U4: { uint32_t v = (uint32_t)(int64_t)p_value; mono_field_set_value(mono_object, field, &v); break; }
			case MONO_TYPE_I8: { int64_t v = (int64_t)p_value; mono_field_set_value(mono_object, field, &v); break; }
			case MONO_TYPE_U8: { uint64_t v = (uint64_t)(int64_t)p_value; mono_field_set_value(mono_object, field, &v); break; }
			case MONO_TYPE_R4: { float v = (float)(double)p_value; mono_field_set_value(mono_object, field, &v); break; }
			case MONO_TYPE_R8: { double v = (double)p_value; mono_field_set_value(mono_object, field, &v); break; }
			default: {
				MonoDomain *domain = mono_domain_get();
				MonoObject *val = variant_to_mono_object(domain, p_value);
				MonoClass *field_class = mono_class_from_mono_type(ftype);
				if (val && field_class && mono_class_is_valuetype(field_class)) {
					mono_field_set_value(mono_object, field, mono_object_unbox(val));
				} else {
					mono_field_set_value(mono_object, field, val);
				}
				break;
			}
		}
		return true;
	}

	for (MonoClass *k = klass; k; k = mono_class_get_parent(k)) {
		MonoProperty *prop = mono_class_get_property_from_name(k, pname_cstr);
		if (prop) {
			MonoMethod *pset = mono_property_get_set_method(prop);
			if (pset) {
				Variant result;
				Callable::CallError err;
				const Variant *args[1] = { &p_value };
				invoke_method(pset, args, 1, result, err);
				return err.error == Callable::CallError::CALL_OK;
			}
			break;
		}
	}

	return false;
}

bool CSharpInstance::get(const StringName &p_name, Variant &r_ret) const {
	if (!mono_object) return false;

	String prop_name = String(p_name);

	String getter_name = "get_" + prop_name;
	MonoMethod *getter = nullptr;
	if (script.is_valid() && script->mono_class) {
		CSharpInstance *nc = const_cast<CSharpInstance *>(this);
		getter = nc->find_method(getter_name, 0);
	}
	if (getter) {
		Variant result;
		Callable::CallError err;
		CSharpInstance *nc = const_cast<CSharpInstance *>(this);
		nc->invoke_method(getter, nullptr, 0, result, err);
		if (err.error == Callable::CallError::CALL_OK) {
			r_ret = result;
			return true;
		}
	}

	MonoClass *klass = mono_object_get_class(mono_object);
	MonoClassField *field = nullptr;
	CharString pname_utf8 = prop_name.utf8();
	const char *pname_cstr = pname_utf8.get_data();
	for (MonoClass *k = klass; k && !field; k = mono_class_get_parent(k)) {
		field = mono_class_get_field_from_name(k, pname_cstr);
	}
	if (field) {
		// mono_field_get_value reads sizeof(field) bytes into the destination
		// buffer. For value-type fields (int, float, Vector3, etc.) this is
		// wrong when the destination is a MonoObject* variable: it reads raw
		// value bytes into a pointer-sized slot — for fields larger than 8
		// bytes (e.g. Vector3 = 12 bytes) this is a stack overwrite, and for
		// smaller fields the resulting "pointer" is garbage that crashes
		// mono_object_to_variant. Use mono_field_get_value_object instead,
		// which boxes value-type fields into a fresh MonoObject and returns
		// the stored reference for reference-type fields. (H2)
		MonoObject *val = mono_field_get_value_object(mono_domain_get(), field, mono_object);
		r_ret = mono_object_to_variant(val);
		return true;
	}

	for (MonoClass *k = klass; k; k = mono_class_get_parent(k)) {
		MonoProperty *prop = mono_class_get_property_from_name(k, pname_cstr);
		if (prop) {
			MonoMethod *pget = mono_property_get_get_method(prop);
			if (pget) {
				Variant result;
				Callable::CallError err;
				CSharpInstance *nc = const_cast<CSharpInstance *>(this);
				nc->invoke_method(pget, nullptr, 0, result, err);
				if (err.error == Callable::CallError::CALL_OK) {
					r_ret = result;
					return true;
				}
			}
			break;
		}
	}

	return false;
}

Variant::Type CSharpInstance::get_property_type(const StringName &p_name, bool *r_is_valid) const {
	if (r_is_valid) *r_is_valid = false;
	if (!mono_object) return Variant::NIL;

	String prop_name = String(p_name);
	CharString pname_utf8 = prop_name.utf8();
	const char *pname_cstr = pname_utf8.get_data();
	MonoClass *klass = mono_object_get_class(mono_object);
	MonoProperty *prop = mono_class_get_property_from_name(klass, pname_cstr);
	if (prop) {
		MonoMethod *getter = mono_property_get_get_method(prop);
		if (getter) {
			MonoMethodSignature *sig = mono_method_signature(getter);
			MonoType *ret = mono_signature_get_return_type(sig);
			if (r_is_valid) *r_is_valid = true;
			MonoTypeEnum t = (MonoTypeEnum)mono_type_get_type(ret);
			switch (t) {
				case MONO_TYPE_BOOLEAN: return Variant::BOOL;
				case MONO_TYPE_I4:
				case MONO_TYPE_I8:
				case MONO_TYPE_U4:
				case MONO_TYPE_U8: return Variant::INT;
				case MONO_TYPE_R4:
				case MONO_TYPE_R8: return Variant::FLOAT;
				case MONO_TYPE_STRING: return Variant::STRING;
				case MONO_TYPE_OBJECT:
				case MONO_TYPE_CLASS:
				case MONO_TYPE_SZARRAY: return Variant::OBJECT;
				default: return Variant::NIL;
			}
		}
	}
	return Variant::NIL;
}

// P1: Forward to script's exported member list so Inspector can display [Export] properties.
void CSharpInstance::get_property_list(List<PropertyInfo> *p_properties) const {
	if (script.is_valid()) {
		script->get_script_property_list(p_properties);
	}
}

void CSharpInstance::get_method_list(List<MethodInfo> *r_list) const {
	if (script.is_valid()) {
		script->get_script_method_list(r_list);
	}
}

bool CSharpInstance::has_method(const StringName &p_method) const {
	CSharpInstance *nc = const_cast<CSharpInstance *>(this);
	return nc->find_method(p_method, -1) != nullptr;
}

int CSharpInstance::get_method_argument_count(const StringName &p_method, bool *r_is_valid) const {
	CSharpInstance *nc = const_cast<CSharpInstance *>(this);
	MonoMethod *m = nc->find_method(p_method, -1);
	if (!m) {
		if (r_is_valid) *r_is_valid = false;
		return 0;
	}
	MonoMethodSignature *sig = mono_method_signature(m);
	if (r_is_valid) *r_is_valid = true;
	return mono_signature_get_param_count(sig);
}

Variant CSharpInstance::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
	r_error.argument = 0;
	r_error.expected = 0;

	MonoMethod *m = find_method(p_method, p_argcount);
	if (!m) {
		m = find_method(p_method, -1);
	}
	if (!m) {
		return Variant();
	}

	// Skip non-overridden virtual methods to avoid WASM interpreter
	// function signature mismatch bug during virtual dispatch.
	MonoClass *decl_class = mono_method_get_class(m);
	if (script.is_valid() && script->mono_class &&
		decl_class != script->mono_class) {
		uint32_t flags = mono_method_get_flags(m, nullptr);
		if (flags & MONO_METHOD_ATTR_VIRTUAL) {
			// Virtual method not overridden by script class - skip.
			return Variant();
		}
	}

	Variant result;
	invoke_method(m, p_args, p_argcount, result, r_error);
	return result;
}

void CSharpInstance::notification(int p_notification, bool p_reversed) {
	if (p_notification == Object::NOTIFICATION_PREDELETE) {
		if (owner) {
			mono_gc_bridge::notify_native_destroyed(owner);
		}
		if (gchandle != 0) {
			mono_gchandle_free(gchandle);
			gchandle = 0;
		}
		mono_object = nullptr;
		return;
	}

	if (!mono_object) return;

	MONO_LOG("[Mono] notification(id=%d) for '%s'\n", p_notification, script->class_name.utf8().get_data());

	// Phase 0.2: dispatch via cached entry table.
	// See docs/spike_2026-07-28_phase0.2_notify_dispatch.md.
	//
	// High-frequency notifications (READY/ENTER_TREE/EXIT_TREE/PROCESS/
	// PHYSICS_PROCESS) have dedicated entries resolved lazily on first use.
	// Other IDs (e.g., NOTIFICATION_DRAW, NOTIFICATION_INTERNAL_PROCESS) fall
	// through to the generic _Notification(int) path below.
	const NotifySpec *spec = csharp_notify_find_spec(p_notification);
	if (spec) {
		NotifyEntryIndex idx = csharp_notify_spec_entry_index(spec);
		NotifyEntry &entry = notify_dispatch_.get_entry(idx);
		if (!entry.resolved) {
			resolve_notify_entry(idx);
		}
		if (entry.method) {
			invoke_cached_notify(idx, p_notification);
		}
	}

	// _Notification(int) is always called for any notification ID, regardless
	// of whether a dedicated entry above handled it. This mirrors Godot's
	// C++ notification flow: virtual _Ready() + virtual _Notification(int).
	NotifyEntry &notif_entry = notify_dispatch_.get_entry(NOTIFY_ENTRY_NOTIFICATION);
	if (!notif_entry.resolved) {
		resolve_notify_entry(NOTIFY_ENTRY_NOTIFICATION);
	}
	if (notif_entry.method) {
		// Skip if _Notification is not overridden (inherited from Godot.Node).
		// Same WASM signature-mismatch workaround as dedicated entries.
		bool notif_overridden = !(script.is_valid() && script->mono_class &&
								  notif_entry.declaring_class != script->mono_class);
		if (notif_overridden) {
			MONO_LOG("[Mono] notification: calling _Notification(%d) for '%s'\n", p_notification, script->class_name.utf8().get_data());
			Variant arg = p_notification;
			const Variant *args[1] = { &arg };
			Variant result;
			Callable::CallError err;
			invoke_method(notif_entry.method, args, 1, result, err);
		}
	}
}

// Phase 0.2: resolve a notify entry by looking up the method on the
// instance's class hierarchy. Cached for the lifetime of the instance
// (unless cleared by hot reload).
void CSharpInstance::resolve_notify_entry(NotifyEntryIndex p_index) {
	NotifyEntry &entry = notify_dispatch_.get_entry(p_index);
	entry.method = nullptr;
	entry.declaring_class = nullptr;
	entry.resolved = true; // mark resolved even if not found (avoids re-lookup)

	if (!mono_object) return;

	const NotifySpec *spec = csharp_notify_find_spec_by_entry(p_index);
	if (!spec) return;

	const char *method_name = csharp_notify_spec_method_name(spec);
	int arg_count = csharp_notify_spec_arg_count(spec);

	MonoMethod *m = find_method(StringName(method_name), arg_count);
	if (!m) return;

	entry.method = m;
	entry.declaring_class = mono_method_get_class(m);
}

// Phase 0.2: invoke a cached notify entry via mono_runtime_invoke.
// Skips the call if the method is inherited (not overridden by the script class).
void CSharpInstance::invoke_cached_notify(NotifyEntryIndex p_index, int p_notification) {
	NotifyEntry &entry = notify_dispatch_.get_entry(p_index);
	if (!entry.method) return;

	// Only call if the method is actually overridden by the script class.
	// The Mono WASM interpreter has a bug with virtual dispatch for
	// inherited (non-overridden) methods that causes "function signature
	// mismatch". Since base class implementations are empty, skipping
	// them is safe and correct.
	if (script.is_valid() && script->mono_class &&
		entry.declaring_class != script->mono_class) {
		return;
	}

	const NotifySpec *spec = csharp_notify_find_spec_by_entry(p_index);
	if (!spec) return;

	int arg_count = csharp_notify_spec_arg_count(spec);
	int arg_provider = csharp_notify_spec_arg_provider(spec);

	MONO_LOG("[Mono] notification: calling %s for '%s'\n",
		csharp_notify_spec_method_name(spec),
		script->class_name.utf8().get_data());

	Variant result;
	Callable::CallError err;

	if (arg_count == 0) {
		invoke_method(entry.method, nullptr, 0, result, err);
	} else if (arg_count == 1) {
		Variant arg;
		switch (arg_provider) {
			case 1: { // DELTA_PROCESS
				if (owner && owner->is_class("Node")) {
					Node *node = Object::cast_to<Node>(owner);
					arg = (double)node->get_process_delta_time();
				} else {
					arg = 0.0;
				}
				break;
			}
			case 2: { // DELTA_PHYSICS
				if (owner && owner->is_class("Node")) {
					Node *node = Object::cast_to<Node>(owner);
					arg = (double)node->get_physics_process_delta_time();
				} else {
					arg = 0.0;
				}
				break;
			}
			case 3: { // NOTIFICATION_ID
				arg = p_notification;
				break;
			}
			default:
				arg = Variant();
				break;
		}
		const Variant *args[1] = { &arg };
		invoke_method(entry.method, args, 1, result, err);
	}
}

String CSharpInstance::to_string(bool *r_valid) {
	if (r_valid) *r_valid = false;
	if (!mono_object) return "<CSharpInstance>";

	// Phase 0.2: use cached dispatch entry for ToString.
	NotifyEntry &ts_entry = notify_dispatch_.get_entry(NOTIFY_ENTRY_TOSTRING);
	if (!ts_entry.resolved) {
		resolve_notify_entry(NOTIFY_ENTRY_TOSTRING);
	}
	if (ts_entry.method) {
		// Skip if ToString is not overridden by the script class.
		// System.Object.ToString() virtual dispatch triggers signature mismatch
		// in WASM interpreter mode.
		if (script.is_valid() && script->mono_class &&
			ts_entry.declaring_class != script->mono_class) {
			// Not overridden - use C++ fallback.
		} else {
			Variant result;
			Callable::CallError err;
			invoke_method(ts_entry.method, nullptr, 0, result, err);
			if (err.error == Callable::CallError::CALL_OK && result.get_type() == Variant::STRING) {
				if (r_valid) *r_valid = true;
				return (String)result;
			}
		}
	}
	return "<CSharpInstance:" + script->class_name + ">";
}

ScriptLanguage *CSharpInstance::get_language() {
	return CSharpLanguage::get_singleton();
}

CSharpLanguage::CSharpLanguage() { singleton = this; }
CSharpLanguage::~CSharpLanguage() { singleton = nullptr; }

// P0-1 fix: open the scripts dll via a versioned temp copy.
// Mono 6.12 caches opened assemblies by file path. If we open the same path
// twice (e.g., before and after `dotnet build` rewrites the dll), the second
// open() returns the cached MonoImage — containing the OLD IL — even though
// the file on disk has changed. The previous fix (close + reopen) caused UAF
// because close() releases the image while CSharpScript/CSharpInstance still
// hold raw MonoClass*/MonoObject* pointers into it.
//
// This helper copies `p_dll_path` to `<p_dll_path>.rev{N}.dll` (a fresh path
// that has never been opened, so no cache entry exists) and opens the copy.
// The original dll on disk is never touched by Mono, so `dotnet build` can
// safely overwrite it on the next hot reload.
//
// N5 fix: the versioned copy is EDITOR-ONLY (TOOLS_ENABLED). Exported games
// load the scripts assembly exactly once, so Mono's path-keyed cache can
// never go stale — they open the original path directly.
// N1 fix: temp copies are tracked in `opened_rev_paths` and deleted at the
// NEXT editor startup by cleanup_stale_rev_files() — the files stay locked
// by Mono for the whole session, so they cannot be deleted at shutdown.
MonoAssembly *CSharpLanguage::open_versioned_assembly(const String &p_dll_path) {
	if (!MonoHost::get_singleton() || !MonoHost::get_singleton()->get_domain()) {
		return nullptr;
	}

#ifndef TOOLS_ENABLED
	return mono_domain_assembly_open(MonoHost::get_singleton()->get_domain(), p_dll_path.utf8().get_data());
#else
	assembly_rev++;
	String rev_path = p_dll_path + ".rev" + itos(assembly_rev) + ".dll";

	Error err = DirAccess::copy_absolute(p_dll_path, rev_path);
	if (err != OK) {
		// Fallback: open the original path directly. For cold start (first
		// load_scripts_assembly call) there is no cache yet, so this is safe.
		// For hot reload this risks loading stale IL, but it's better than
		// crashing — the user will be prompted to restart the editor.
		WARN_PRINT("[Mono] Versioned assembly copy failed (err=" + itos((int)err) + "), falling back to direct open: " + p_dll_path);
		return mono_domain_assembly_open(MonoHost::get_singleton()->get_domain(), p_dll_path.utf8().get_data());
	}

	MonoAssembly *asm_ptr = mono_domain_assembly_open(MonoHost::get_singleton()->get_domain(), rev_path.utf8().get_data());
	if (asm_ptr) {
		opened_rev_paths.push_back(rev_path);
		print_verbose("[Mono] Opened versioned assembly rev=" + itos(assembly_rev) + " (" + rev_path + ")");
	}
	return asm_ptr;
#endif
}

// N1 fix: delete `.rev{N}.dll` temp copies left behind by previous editor
// sessions. Must run at STARTUP (before Mono opens anything): the files are
// locked by Mono for the entire session that created them, so they cannot be
// deleted at shutdown — least of all on Windows.
void CSharpLanguage::cleanup_stale_rev_files() {
#ifdef TOOLS_ENABLED
	String assemblies_dir = get_mono_assemblies_dir();
	Ref<DirAccess> da = DirAccess::open(assemblies_dir);
	if (da.is_null()) {
		return;
	}
	da->list_dir_begin();
	String fname = da->get_next();
	while (!fname.is_empty()) {
		if (!da->current_is_dir() && fname.contains(".rev") && fname.ends_with(".dll")) {
			da->remove(fname);
		}
		fname = da->get_next();
	}
	da->list_dir_end();
	opened_rev_paths.clear();
#endif
}

MonoAssembly *CSharpLanguage::load_scripts_assembly() {
	if (scripts_assembly) return scripts_assembly;
	if (!MonoHost::get_singleton() || !MonoHost::get_singleton()->get_domain()) return nullptr;

	String project_name = Path::get_csharp_project_name();
	String assemblies_dir = get_mono_assemblies_dir();

	Vector<String> search_paths;

	if (!assemblies_dir.is_empty()) {
		search_paths.push_back(assemblies_dir.path_join(project_name + ".dll"));
		search_paths.push_back(assemblies_dir.path_join("ProjectScripts.dll"));
	}

	String project_res_path = ProjectSettings::get_singleton() ? ProjectSettings::get_singleton()->get_resource_path() : "";
	if (!project_res_path.is_empty()) {
		search_paths.push_back(project_res_path.path_join(".mono/assemblies").path_join(project_name + ".dll"));
		search_paths.push_back(project_res_path.path_join(".mono/assemblies/ProjectScripts.dll"));
		search_paths.push_back(project_res_path.path_join(project_name + ".dll"));
		search_paths.push_back(project_res_path.path_join("ProjectScripts.dll"));
	}

	String cwd = OS::get_singleton()->get_cwd();
	search_paths.push_back(cwd.path_join(".mono/assemblies").path_join(project_name + ".dll"));
	search_paths.push_back(cwd.path_join(".mono/assemblies/ProjectScripts.dll"));
	search_paths.push_back(cwd.path_join(project_name + ".dll"));
	search_paths.push_back(cwd.path_join("ProjectScripts.dll"));

#ifdef WEB_ENABLED
	// On Web, BCL and project assemblies are extracted to MEMFS at .mono/assemblies/
	search_paths.push_back(String(".mono/assemblies/").path_join(project_name + ".dll"));
	search_paths.push_back(String(".mono/assemblies/ProjectScripts.dll"));
	search_paths.push_back(String(project_name + ".dll"));
	search_paths.push_back(String("ProjectScripts.dll"));
#endif

#ifdef ANDROID_ENABLED
	// Android: assemblies are extracted from PCK to user_data_dir at runtime
	// (see mono_host.cpp initialize()). res:// paths are virtual inside PCK
	// and mono_domain_assembly_open() needs real filesystem paths.
	{
		String user_data_dir = OS::get_singleton()->get_user_data_dir();
		search_paths.push_back(user_data_dir.path_join(".mono").path_join("assemblies").path_join(project_name + ".dll"));
		search_paths.push_back(user_data_dir.path_join(".mono").path_join("assemblies").path_join("ProjectScripts.dll"));
	}
#endif

	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	search_paths.push_back(exe_dir.path_join(".mono/assemblies").path_join(project_name + ".dll"));
	search_paths.push_back(exe_dir.path_join(".mono/assemblies/ProjectScripts.dll"));
	search_paths.push_back(exe_dir.path_join(project_name + ".dll"));
	search_paths.push_back(exe_dir.path_join("ProjectScripts.dll"));

	List<String> cmdline_args = OS::get_singleton()->get_cmdline_args();
	String project_path;
	bool next_is_path = false;
	for (const String &arg : cmdline_args) {
		if (next_is_path) {
			project_path = arg;
			break;
		}
		if (arg == "--path" || arg == "-p") {
			next_is_path = true;
		}
	}
	if (!project_path.is_empty()) {
		if (project_path.is_relative_path()) {
			project_path = OS::get_singleton()->get_cwd().path_join(project_path);
		}
		search_paths.push_back(project_path.path_join(".mono/assemblies").path_join(project_name + ".dll"));
		search_paths.push_back(project_path.path_join(".mono/assemblies/ProjectScripts.dll"));
		search_paths.push_back(project_path.path_join(project_name + ".dll"));
		search_paths.push_back(project_path.path_join("ProjectScripts.dll"));
	}

	for (int i = 0; i < search_paths.size(); i++) {
		const String &path = search_paths[i];
		if (FileAccess::exists(path)) {
			// P0-1 fix: open via versioned temp copy to bypass Mono's path-keyed
			// image cache. Old assembly (if any) is NOT closed — see open_versioned_assembly().
			scripts_assembly = open_versioned_assembly(path);
			if (scripts_assembly) {
				opened_assemblies.push_back(scripts_assembly);
				printf("[Mono] Loaded scripts assembly: %s\n", path.utf8().get_data());
				fflush(stdout);
				// P2 v2: rebuild global class cache from the freshly loaded
				// assembly's TypeDef table. Subsequent get_global_class_name()
				// calls will hit the cache instead of doing text-based .cs scan.
				refresh_global_classes();
				return scripts_assembly;
			}
		}
	}

	return nullptr;
}

void CSharpLanguage::init() {
	ensure_project_file();
	// N1 fix: delete .rev{N}.dll temp copies left by previous editor sessions
	// (files are locked by Mono all session, so cleanup happens at startup).
	cleanup_stale_rev_files();
	load_scripts_assembly();

#ifdef MONO_AOT_MODE
	// Register generic roots for Full AOT mode.
	// This forces the runtime to instantiate generic types that may be
	// accessed via reflection but were not statically detected by the AOT compiler.
	printf("[Mono] AOT: Registering generic roots...\n");
	fflush(stdout);
	{
		MonoImage *roots_image = nullptr;
		if (scripts_assembly) {
			roots_image = mono_assembly_get_image(scripts_assembly);
		}
		MonoClass *roots_class = nullptr;
		if (roots_image) {
			roots_class = mono_class_from_name(roots_image, "", "GenericRoots");
		}
		if (!roots_class && MonoHost::get_singleton() && MonoHost::get_singleton()->get_godotsharp_assembly()) {
			roots_class = mono_class_from_name(
				mono_assembly_get_image(MonoHost::get_singleton()->get_godotsharp_assembly()),
				"", "GenericRoots");
		}
		if (roots_class) {
			MonoMethod *register_method = mono_class_get_method_from_name(
				roots_class, "Register", 0);
			if (register_method) {
				MonoObject *exc = nullptr;
				mono_runtime_invoke(register_method, nullptr, nullptr, &exc);
				if (exc) {
					ERR_PRINT("[Mono] AOT: Exception during generic root registration");
				} else {
					printf("[Mono] AOT: Generic roots registered.\n");
					fflush(stdout);
				}
			} else {
				printf("[Mono] AOT: WARNING: GenericRoots.Register method not found\n");
				fflush(stdout);
			}
		} else {
			printf("[Mono] AOT: WARNING: GenericRoots class not found in any assembly\n");
			fflush(stdout);
		}
	}
#endif

#ifdef TOOLS_ENABLED
	// P7: Register debugger settings so they appear in Project Settings → Dotnet.
	// Read back in mono_host.cpp before mono_jit_init_version.
	GLOBAL_DEF("dotnet/debugger/enabled", false);
	GLOBAL_DEF("dotnet/debugger/port", 55555);

	if (Engine::get_singleton() && Engine::get_singleton()->is_editor_hint()) {
		if (!scripts_assembly) {
			printf("[Mono] No scripts assembly loaded, triggering initial build...\n");
			fflush(stdout);
			build_project();
		}
	}
#endif
}

void CSharpLanguage::finish() {
#ifdef TOOLS_ENABLED
	// P4 v2 (W3): if an async build is still in flight, block until the
	// worker returns (OS::execute completes or fails) — prevents destroying
	// the language while the thread reads its members (UAF guard).
	if (build_thread_started) {
		build_thread.wait_to_finish();
		build_thread_started = false;
	}
	build_state = BuildState::IDLE;
#endif

	// N2 fix: do NOT mono_assembly_close() the opened assemblies here. The
	// whole point of the P0-1 "never close" strategy is that CSharpScript /
	// CSharpInstance may still hold raw MonoClass*/MonoObject* pointers into
	// those images during teardown, and the sgen heap may still contain
	// managed objects of those classes — closing at finish() would
	// re-introduce the exact UAF we set out to remove, for zero benefit:
	// the process is exiting and the OS reclaims everything anyway.
	opened_assemblies.clear();
	opened_rev_paths.clear();
	scripts_assembly = nullptr;
	loaded_assemblies.clear();
	global_class_cache.clear();
	global_classes_valid = false;
}

void register_csharp_resource_loader() {
	if (!resource_loader_csharp.is_valid()) {
		resource_loader_csharp.instantiate();
		ResourceLoader::add_resource_format_loader(resource_loader_csharp, true);
	}
	if (!resource_saver_csharp.is_valid()) {
		resource_saver_csharp.instantiate();
		ResourceSaver::add_resource_format_saver(resource_saver_csharp, true);
	}
}

void unregister_csharp_resource_loader() {
	if (resource_saver_csharp.is_valid()) {
		ResourceSaver::remove_resource_format_saver(resource_saver_csharp);
		resource_saver_csharp.unref();
	}
	if (resource_loader_csharp.is_valid()) {
		ResourceLoader::remove_resource_format_loader(resource_loader_csharp);
		resource_loader_csharp.unref();
	}
}

void CSharpLanguage::reload_all_pending_scripts() {
	if (!scripts_assembly) return;

	List<Ref<Resource>> resources;
	ResourceCache::get_cached_resources(&resources);
	int reloaded_count = 0;
	for (const Ref<Resource> &res : resources) {
		Ref<CSharpScript> cs_script = res;
		if (cs_script.is_valid() && cs_script->get_path().get_extension().to_lower() == "cs") {
			// P0-1 fix: previously only reloaded scripts where
			// !mono_class_valid || !mono_class — i.e., scripts that had
			// never been resolved. After P0-1 fix the old assembly is NOT
			// closed, so previously-resolved scripts still have valid
			// mono_class pointers — but those pointers reference the OLD
			// image's metadata. We must force-reload every C# script so
			// resolve_mono_class() re-runs against the freshly opened
			// assembly's image and picks up new IL (changed method bodies,
			// new [Export] members, removed signals, etc.).
			print_verbose("[Mono] Hot-reloading script: " + cs_script->get_path());
			cs_script->reload();
			reloaded_count++;
		}
	}
	if (reloaded_count > 0) {
		printf("[Mono] Reloaded %d pending scripts (P0-1 force-reload all).\n", reloaded_count);
		fflush(stdout);
	}
}

// W5 SG PoC (v3 pre-research): receive one compile-time [GlobalClass] entry
// pushed from the C# module initializer generated by
// GodotSharp.SourceGenerators. icon_path is accepted per the eval §6.2 API
// design but not yet consumed — GlobalClassInfo has no icon field until icon
// support lands in a later SG phase.
void CSharpLanguage::sg_register_global_class(const String &p_class_name, const String &p_base_type, bool p_is_tool, bool p_is_abstract, const String &p_icon_path) {
	GlobalClassInfo info;
	info.base_type = p_base_type.is_empty() ? "Node" : p_base_type;
	info.is_tool = p_is_tool;
	info.is_abstract = p_is_abstract;
	// source_path stays empty: the SG registry carries no source mapping.
	// Desktop .pdb reverse-lookup is unavailable on this path (known PoC
	// limitation — get_global_class_name() falls back to the filename
	// convention, same as the WASM path).
	sg_global_class_cache[p_class_name] = info;
	sg_registry_populated = true;
	(void)p_icon_path;
}

// P2 v2: Rebuild global_class_cache by iterating the scripts assembly TypeDef table.
// Replaces v1 text-based .cs file scanning with AOT-accurate IL metadata reads.
// Filter rules:
//   - Skip non-public types (Godot global classes are always public top-level types).
//   - Skip compiler-generated types (name starts with '<' — closures/async state machines).
//   - Require [GlobalClass] attribute (mono_script_meta::class_has_attribute, AOT-safe).
// Metadata extracted per class:
//   - base_type = mono_class_get_name(mono_class_get_parent(klass)); falls back to "Node".
//   - is_abstract = MONO_TYPE_ATTR_ABSTRACT flag.
//   - is_tool = [Tool] attribute presence.
//   - source_path = via mono_debug_lookup_source_location(.ctor) (desktop only, [REV-#06]).
// Spec ref: §4.P2.2 (typedef iteration + path lookup HashMap).
void CSharpLanguage::refresh_global_classes() {
	global_class_cache.clear();
	global_class_source_map.clear();
	global_classes_valid = false;

	if (!scripts_assembly) {
		return;
	}
	MonoImage *image = mono_assembly_get_image(scripts_assembly);
	if (!image) {
		return;
	}

	// W5 SG PoC: prefer the compile-time registry. Running <Module>'s class
	// initializer executes the [ModuleInitializer] generated by
	// GodotSharp.SourceGenerators, which pushes all [GlobalClass] entries
	// back into sg_global_class_cache via icall (eval §5.3 方式 A: C# pushes,
	// C++ never invokes managed code — no mono_runtime_invoke, WASM-safe).
	// mono_runtime_class_init is idempotent: on non-SG (or already-run)
	// assemblies the cctor is a no-op and sg_registry_populated stays false,
	// preserving the reflection-scan fallback below for pre-SG assemblies.
	MonoClass *module_klass = mono_class_from_name(image, "", "<Module>");
	if (module_klass) {
		// Modern Mono (5.x+/6.x+) requires a MonoVTable* for
		// mono_runtime_class_init; get one via the domain so the
		// <Module> .cctor actually fires (SG registry bootstrap).
		// If no domain exists yet the cctor is a no-op anyway.
		MonoDomain *init_domain = mono_domain_get();
		if (init_domain) {
			MonoVTable *vtable = mono_class_vtable(init_domain, module_klass);
			if (vtable) {
				mono_runtime_class_init(vtable);
			}
		}
	}
	if (sg_registry_populated) {
		global_class_cache = sg_global_class_cache;
		global_classes_valid = true;
		printf("[Mono] P2 refresh_global_classes: SG registry path, %d global classes registered (0 typedefs scanned)\n",
				sg_global_class_cache.size());
		fflush(stdout);
		return;
	}

	MonoDomain *domain = MonoHost::get_singleton() ? MonoHost::get_singleton()->get_domain() : nullptr;

	int num_typedefs = mono_image_get_table_rows(image, MONO_TABLE_TYPEDEF);
	int registered = 0;
	int pdb_hits = 0;

	for (int i = 1; i <= num_typedefs; i++) {
		uint32_t token = (MONO_TABLE_TYPEDEF << 24) | (uint32_t)i;
		MonoClass *klass = mono_class_get(image, token);
		if (!klass) {
			continue;
		}

		// Skip non-public types — Godot global classes are always public.
		uint32_t flags = mono_class_get_flags(klass);
		if (!(flags & MONO_TYPE_ATTR_PUBLIC)) {
			continue;
		}

		const char *cname = mono_class_get_name(klass);
		if (!cname || !cname[0] || cname[0] == '<') {
			// Skip anonymous/compiler-generated types (closures, async state machines).
			continue;
		}

		// Require [GlobalClass] attribute (AOT-safe via mono_custom_attrs_from_class).
		if (!mono_script_meta::class_has_attribute(klass, "GlobalClassAttribute")) {
			continue;
		}

		GlobalClassInfo info;
		info.is_tool = mono_script_meta::class_has_attribute(klass, "ToolAttribute");
		info.is_abstract = (flags & MONO_TYPE_ATTR_ABSTRACT) != 0;

		MonoClass *parent = mono_class_get_parent(klass);
		if (parent) {
			const char *pname = mono_class_get_name(parent);
			if (pname && pname[0]) {
				info.base_type = String(pname);
			}
		}
		if (info.base_type.is_empty()) {
			// Godot default base when no : Base is declared (mirrors _parse_base_class).
			info.base_type = "Node";
		}

		// P2 v2 [REV-#06]: .pdb reverse-lookup to map class_name → source_path.
		// Uses .ctor method (always exists for instantiable classes) to get
		// a MonoMethod*, then mono_debug_lookup_source_location to get source file.
		// Desktop only — WASM has no .pdb (see spike_2026-07-26_p5_pdb.md).
		// Path is normalized to res:// when it falls under the project resource path.
#if defined(TOOLS_ENABLED) && !defined(WEB_ENABLED)
		if (domain) {
			MonoMethod *ctor_method = mono_class_get_method_from_name(klass, ".ctor", 0);
			if (ctor_method) {
				MonoDebugSourceLocation *loc = mono_debug_lookup_source_location(ctor_method, 0, domain);
				if (loc && loc->source_file) {
					String src_path = String(loc->source_file).replace("\\", "/");
					// Normalize to res:// if under the project resource path.
					String res_path = ProjectSettings::get_singleton() ?
							ProjectSettings::get_singleton()->get_resource_path() : "";
					if (!res_path.is_empty()) {
						String norm_res = res_path.replace("\\", "/");
						if (src_path.begins_with(norm_res)) {
							src_path = "res://" + src_path.substr(norm_res.length()).lstrip("/");
						}
					}
					info.source_path = src_path;
					global_class_source_map[src_path] = String(cname);
					pdb_hits++;
				}
				if (loc) {
					mono_debug_free_source_location(loc);
				}
			}
		}
#endif // TOOLS_ENABLED && !WEB_ENABLED

		global_class_cache[String(cname)] = info;
		registered++;
	}

	global_classes_valid = true;
	printf("[Mono] P2 refresh_global_classes: %d typedefs scanned, %d global classes registered",
			num_typedefs, registered);
#if defined(TOOLS_ENABLED) && !defined(WEB_ENABLED)
	printf(", %d .pdb source lookups", pdb_hits);
#endif
	printf("\n");
	fflush(stdout);
}

void CSharpLanguage::frame() {
	if (MonoHost::get_singleton()) {
		MonoHost::get_singleton()->pump_sync_context();
	}

	// H8: drain the deferred free queue on the main thread. C# finalizers
	// run on the GC thread and may have enqueued engine objects for release;
	// this is the safe point to actually free them.
	mono_gc_bridge::flush_deferred_free();

#ifdef TOOLS_ENABLED
	// P4 v2 (W3): poll the async build mailbox; on completion this joins the
	// worker and runs the Mono reload sequence right here on the main thread.
	if (build_state == BuildState::BUILDING) {
		_poll_async_build();
	}

	if (build_pending && Engine::get_singleton() && Engine::get_singleton()->is_editor_hint()) {
		if (build_state == BuildState::IDLE) {
			print_verbose("[Mono] frame() consuming build_pending, calling build_project_async()");
			build_pending = false;
			// Async path; falls back to the synchronous build internally when
			// the worker thread cannot start.
			build_project_async();
		}
		// else: a build is in flight — keep build_pending queued; it is
		// consumed on a later frame after the current build completes
		// (request merging, W3 D2-D3 requirement).
	}
#endif
}

// P6: EditorFileSystem::filesystem_changed handler.
// Simplified strategy (spec §4.P6.2): any filesystem change in a project
// that contains .cs files triggers request_build(). The build_pending flag
// already deduplicates concurrent requests (setting it twice is a no-op),
// and dotnet build's own incremental compilation makes empty-change builds
// cheap (<1s). The 500ms cooldown below guards against signal bursts where
// EditorFileSystem fires filesystem_changed multiple times in rapid
// succession (write + stat + rename from external IDEs).
// Phase2 verification: force scons rebuild by content change.
void CSharpLanguage::_on_filesystem_changed() {
#ifdef TOOLS_ENABLED
	if (!Engine::get_singleton() || !Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	uint64_t now = Time::get_singleton()->get_ticks_msec();
	if (now - last_build_request_ms < BUILD_COOLDOWN_MS) {
		return;
	}
	last_build_request_ms = now;
	print_verbose("[Mono] filesystem_changed → request_build()");
	request_build();
#endif
}

void CSharpLanguage::reload_all_scripts() {
	// P0-1 fix: do NOT call mono_assembly_close() on the old assembly.
	// close() releases the image while CSharpScript/CSharpInstance still hold
	// raw MonoClass*/MonoObject* pointers into it → UAF on next access or
	// sgen GC scan. Instead, clear the pointer so load_scripts_assembly() does
	// not early-return, and open a fresh versioned copy (bypasses Mono's
	// path-keyed image cache without releasing the old image). Old assemblies
	// accumulate in `opened_assemblies` and are NEVER closed (see N2 fix).
	if (scripts_assembly) {
		print_verbose("[Mono] Hot reload: reloading scripts assembly (old image kept alive)");
		scripts_assembly = nullptr;
		global_classes_valid = false;
	}
	load_scripts_assembly();
}

void CSharpLanguage::reload_scripts(const Array &p_scripts, bool p_soft_reload) {
	reload_all_scripts();
}

// P5: Reload a single [Tool] script in-place. Called by the editor when
// the user explicitly reloads a tool script. We do NOT implement GDScript's
// StateBackup mechanism — instance state is lost on reload (v1 tradeoff
// documented in spec §4.P5.2).
void CSharpLanguage::reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) {
#ifdef TOOLS_ENABLED
	if (p_script.is_null()) {
		return;
	}
	// P1-#5 fix: trigger an actual rebuild FIRST. reload() below only
	// re-resolves the class against the ALREADY LOADED (old) assembly —
	// without a dotnet build + assembly reload, source changes never take
	// effect and "reload tool script" was a no-op. request_build() is
	// consumed by frame() → build_project(), which reloads the assembly and
	// then reloads every script (reload_all_pending_scripts).
	request_build();
	p_script->reload(p_soft_reload);
	// After reload, tool script instances need their method/property caches
	// rebuilt. reload_all_pending_scripts() iterates the script cache and
	// re-resolves mono_class for any script flagged dirty.
	reload_all_pending_scripts();
	// P5 [REV-#11]: Clear pending exception state in case reload() triggered
	// mono_runtime_invoke (e.g., static constructor re-execution) and left
	// an unobserved exception behind. Without this, the next mono_runtime_invoke
	// call may observe a stale exception and cascade into editor instability.
	// P1-#4 fix: overwrite MUST be true (see invoke_method comment above).
	mono_runtime_set_pending_exception(nullptr, true);
#endif
}

void CSharpLanguage::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("cs");
}

Ref<Script> CSharpLanguage::make_template(const String &p_template, const String &p_class_name, const String &p_base_class_name) const {
	Ref<CSharpScript> script;
	script.instantiate();
	String processed = p_template;
	// A1: 占位符替换。_BINDINGS_NAMESPACE_ 硬编码为 "Godot"（当前 glue 命名空间，
	// 与旧 mono BINDINGS_NAMESPACE 宏一致）；_TS_ 由 _get_indentation() 提供
	// （读 text_editor/behavior/indent 设置，非 editor 回退 "\t"）。
	processed = processed.replace("_CLASS_", p_class_name.to_pascal_case().validate_unicode_identifier())
	                     .replace("_BASE_", p_base_class_name)
	                     .replace("_BINDINGS_NAMESPACE_", "Godot")
	                     .replace("_TS_", _get_indentation());
	script->set_source_code(processed);
	return script;
}

Vector<ScriptLanguage::ScriptTemplate> CSharpLanguage::get_built_in_templates(const StringName &p_object) {
	Vector<ScriptLanguage::ScriptTemplate> templates;
#ifdef TOOLS_ENABLED
	// A1: 遍历 templates.gen.h 中的 TEMPLATES[]，按 inherit 匹配返回。
	// 照搬旧 mono 1963b2f 实现（csharp_script.cpp:381-391）。
	for (int i = 0; i < TEMPLATES_ARRAY_SIZE; i++) {
		if (TEMPLATES[i].inherit == p_object) {
			templates.append(TEMPLATES[i]);
		}
	}
	// 兜底：若 TEMPLATES[] 无匹配且 p_object=="Object"，返回内联 Empty 模板。
	// spec §A1.3：保留现有 id=0 Empty 作为 Object 兜底（正常情况下 Object/empty
	// 模板会匹配，此分支仅在 templates.gen.h 异常缺失时触发）。
	if (templates.is_empty() && String(p_object) == "Object") {
		ScriptTemplate t;
		t.inherit = p_object;
		t.name = "Empty";
		t.description = "An empty C# script.";
		t.content = "using Godot;\n\npublic partial class _CLASS_ : _BASE_\n{\n}\n";
		t.id = 0;
		t.origin = ScriptLanguage::TEMPLATE_BUILT_IN;
		templates.push_back(t);
	}
#endif
	return templates;
}

// A1: 读编辑器缩进设置。非 editor 或非 TOOLS 构建回退为 "\t"。
// 照搬旧 mono 1963b2f 实现（csharp_script.cpp:427-440）。
String CSharpLanguage::_get_indentation() const {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		bool use_space_indentation = EDITOR_GET("text_editor/behavior/indent/type");
		if (use_space_indentation) {
			int indent_size = EDITOR_GET("text_editor/behavior/indent/size");
			return String(" ").repeat(indent_size);
		}
	}
#endif
	return "\t";
}

Vector<String> CSharpLanguage::get_reserved_words() const {
	Vector<String> words;
	static const char *_reserved_words[] = {
		"abstract", "as", "base", "bool", "break", "byte", "case", "catch", "char",
		"checked", "class", "const", "continue", "decimal", "default", "delegate", "do",
		"double", "else", "enum", "event", "explicit", "extern", "false", "finally",
		"fixed", "float", "for", "foreach", "goto", "if", "implicit", "in", "int",
		"interface", "internal", "is", "lock", "long", "namespace", "new", "null",
		"object", "operator", "out", "override", "params", "private", "protected",
		"public", "readonly", "ref", "return", "sbyte", "sealed", "short", "sizeof",
		"stackalloc", "static", "string", "struct", "switch", "this", "throw", "true",
		"try", "typeof", "uint", "ulong", "unchecked", "unsafe", "ushort", "using",
		"virtual", "void", "volatile", "while",
		"add", "alias", "ascending", "async", "await", "by", "descending", "dynamic",
		"equals", "from", "get", "global", "group", "into", "join", "let", "nameof",
		"on", "orderby", "partial", "remove", "select", "set", "value", "var", "when",
		"where", "yield",
		nullptr
	};
	for (int i = 0; _reserved_words[i]; i++) {
		words.push_back(_reserved_words[i]);
	}
	return words;
}

bool CSharpLanguage::is_control_flow_keyword(const String &p_keyword) const {
	return p_keyword == "break" || p_keyword == "case" || p_keyword == "catch" ||
	       p_keyword == "continue" || p_keyword == "default" || p_keyword == "do" ||
	       p_keyword == "else" || p_keyword == "finally" || p_keyword == "for" ||
	       p_keyword == "foreach" || p_keyword == "goto" || p_keyword == "if" ||
	       p_keyword == "return" || p_keyword == "switch" || p_keyword == "throw" ||
	       p_keyword == "try" || p_keyword == "while";
}

Vector<String> CSharpLanguage::get_comment_delimiters() const {
	return {"//", "/* */"};
}

Vector<String> CSharpLanguage::get_doc_comment_delimiters() const {
	return {"///", "/** */"};
}

Vector<String> CSharpLanguage::get_string_delimiters() const {
	return {"' '", "\" \""};
}

String CSharpLanguage::validate_path(const String &p_path) const {
	String class_name = p_path.get_file().get_basename();
	Vector<String> keywords = get_reserved_words();
	for (int i = 0; i < keywords.size(); i++) {
		if (keywords[i] == class_name) {
			return "Class name can't be a reserved keyword";
		}
	}
	return "";
}

String CSharpLanguage::get_project_csproj_path() const {
	String project_path = ProjectSettings::get_singleton()->get_resource_path();
	if (project_path.is_empty()) {
		project_path = OS::get_singleton()->get_cwd();
	}
	String project_name = Path::get_csharp_project_name();
	return project_path.path_join(project_name + ".csproj");
}

String CSharpLanguage::get_project_sln_path() const {
	String project_path = ProjectSettings::get_singleton()->get_resource_path();
	if (project_path.is_empty()) {
		project_path = OS::get_singleton()->get_cwd();
	}
	String project_name = Path::get_csharp_project_name();
	return project_path.path_join(project_name + ".sln");
}

String CSharpLanguage::get_mono_assemblies_dir() const {
	String project_path = ProjectSettings::get_singleton()->get_resource_path();
	if (project_path.is_empty()) {
		project_path = OS::get_singleton()->get_cwd();
	}
	return project_path.path_join(".mono").path_join("assemblies");
}

void CSharpLanguage::ensure_project_file() {
#ifdef TOOLS_ENABLED
	if (!Engine::get_singleton() || !Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	String csproj_path = get_project_csproj_path();
	if (FileAccess::exists(csproj_path)) {
		return;
	}

	String project_path = csproj_path.get_base_dir();
	String project_name = csproj_path.get_file().get_basename();
	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (da.is_valid()) {
		String mono_dir = project_path.path_join(".mono");
		if (!da->dir_exists(mono_dir)) {
			da->make_dir_recursive(mono_dir.path_join("assemblies"));
		}
	}

	String godotsharp_dll_path = exe_dir.path_join("GodotSharp").path_join("Api").path_join("Debug").path_join("GodotSharp.dll");
	String godotsharp_hint_path = exe_dir.path_join("GodotSharp").path_join("Api").path_join("Debug");

	String csproj_content = String() +
		"<Project Sdk=\"Microsoft.NET.Sdk\">\n" +
		"  <PropertyGroup>\n" +
		"    <TargetFramework>netstandard2.0</TargetFramework>\n" +
		"    <AssemblyName>" + project_name + "</AssemblyName>\n" +
		"    <RootNamespace>" + project_name + "</RootNamespace>\n" +
		"    <LangVersion>latest</LangVersion>\n" +
		"    <OutputPath>.mono/assemblies/</OutputPath>\n" +
		"    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>\n" +
		"    <DebugType>portable</DebugType>\n" +
		"    <GenerateDocumentationFile>false</GenerateDocumentationFile>\n" +
		"  </PropertyGroup>\n" +
		"  <ItemGroup>\n" +
		"    <Reference Include=\"GodotSharp\">\n" +
		"      <HintPath>" + godotsharp_hint_path.replace("\\", "/") + "/GodotSharp.dll</HintPath>\n" +
		"      <Private>false</Private>\n" +
		"    </Reference>\n" +
		"  </ItemGroup>\n" +
		"</Project>\n";

	{
		Error err;
		Ref<FileAccess> f = FileAccess::open(csproj_path, FileAccess::WRITE, &err);
		if (err == OK) {
			f->store_string(csproj_content);
			printf("[Mono] Generated C# project file: %s\n", csproj_path.utf8().get_data());
			fflush(stdout);
		}
	}

	String sln_path = get_project_sln_path();
	if (!FileAccess::exists(sln_path)) {
		String guid_a = "{" + project_name.to_upper().md5_text().substr(0, 8) + "-" +
						project_name.to_upper().md5_text().substr(8, 4) + "-" +
						project_name.to_upper().md5_text().substr(12, 4) + "-" +
						project_name.to_upper().md5_text().substr(16, 4) + "-" +
						project_name.to_upper().md5_text().substr(20, 12) + "}";
		String guid_b = "{1A2B3C4D-5E6F-7A8B-9C0D-1E2F3A4B5C6D}";

		String sln_content = String() +
			"Microsoft Visual Studio Solution File, Format Version 12.00\n" +
			"# Visual Studio Version 17\n" +
			"VisualStudioVersion = 17.0.31903.59\n" +
			"MinimumVisualStudioVersion = 10.0.40219.1\n" +
			"Project(\"{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}\") = \"" + project_name + "\", \"" + project_name + ".csproj\", \"" + guid_a + "\"\n" +
			"EndProject\n" +
			"Global\n" +
			"\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n" +
			"\t\tDebug|Any CPU = Debug|Any CPU\n" +
			"\t\tRelease|Any CPU = Release|Any CPU\n" +
			"\tEndGlobalSection\n" +
			"\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n" +
			"\t\t" + guid_a + ".Debug|Any CPU.ActiveCfg = Debug|Any CPU\n" +
			"\t\t" + guid_a + ".Debug|Any CPU.Build.0 = Debug|Any CPU\n" +
			"\t\t" + guid_a + ".Release|Any CPU.ActiveCfg = Release|Any CPU\n" +
			"\t\t" + guid_a + ".Release|Any CPU.Build.0 = Release|Any CPU\n" +
			"\tEndGlobalSection\n" +
			"EndGlobal\n";

		{
			Error err;
			Ref<FileAccess> f = FileAccess::open(sln_path, FileAccess::WRITE, &err);
			if (err == OK) {
				f->store_string(sln_content);
				printf("[Mono] Generated C# solution file: %s\n", sln_path.utf8().get_data());
				fflush(stdout);
			}
		}
	}
#endif
}

bool CSharpLanguage::build_project() {
#ifdef TOOLS_ENABLED
	if (!Engine::get_singleton() || !Engine::get_singleton()->is_editor_hint()) {
		return true;
	}

	// P4: route build output to the Mono bottom panel when available.
	// Falls back to printf when the panel isn't instantiated (e.g. running
	// headless tests or before the editor dock manager has created it).
	MonoBuildPanel *panel = MonoBuildPanel::get_singleton();

	ensure_project_file();

	String csproj_path = get_project_csproj_path();
	if (!FileAccess::exists(csproj_path)) {
		String msg = "[Mono] Cannot build: .csproj not found: " + csproj_path;
		ERR_PRINT(msg);
		if (panel) {
			panel->append_output(msg);
			panel->set_status("Build failed", true);
		}
		return false;
	}

	List<String> args;
	args.push_back("build");
	args.push_back(csproj_path);
	args.push_back("-c");
	args.push_back("Debug");
	args.push_back("-v:minimal");

	String dotnet_cmd = "dotnet";

	String header = "[Mono] Building C# project: " + csproj_path;
	printf("%s\n", header.utf8().get_data());
	fflush(stdout);

	if (panel) {
		panel->clear_output();
		panel->append_output(header);
		panel->set_status("Building...");
	}

	String pipe_output;
	int exit_code = -1;
	Error err = OS::get_singleton()->execute(dotnet_cmd, args, &pipe_output, &exit_code, true, nullptr, false);

	String exec_err = (err != OK) ? pipe_output : String();
	return _complete_build(err == OK && exit_code == 0, pipe_output, exec_err);
#else
	return true;
#endif
}

#ifdef TOOLS_ENABLED
// P4 v2 (W3): shared post-build path — output routing + Mono reload sequence.
// Called ONLY on the main thread (by the synchronous build_project() or by
// _poll_async_build() when the async worker finishes). All Mono API calls
// live here, keeping the worker thread Mono-free.
bool CSharpLanguage::_complete_build(bool p_ok, const String &p_output, const String &p_exec_err) {
	MonoBuildPanel *panel = MonoBuildPanel::get_singleton();

	if (!p_exec_err.is_empty()) {
		String msg = "[Mono] WARNING: Failed to execute dotnet build. Is .NET SDK installed?";
		printf("%s\n", msg.utf8().get_data());
		fflush(stdout);
		if (panel) {
			panel->append_output(msg);
			if (!p_output.is_empty()) {
				panel->append_output(p_output);
			}
			panel->set_status("Build failed", true);
		}
		return false;
	}

	if (!p_output.is_empty()) {
		printf("%s\n", p_output.utf8().get_data());
		fflush(stdout);
		if (panel) {
			panel->append_output(p_output);
		}
	}

	if (!p_ok) {
		String msg = "[Mono] C# build failed.";
		printf("%s\n", msg.utf8().get_data());
		fflush(stdout);
		if (panel) {
			panel->append_output(msg);
			panel->set_status("Build failed", true);
		}
		return false;
	}

	String ok_msg = "[Mono] C# build succeeded.";
	printf("%s\n", ok_msg.utf8().get_data());
	fflush(stdout);
	if (panel) {
		panel->append_output(ok_msg);
	}

	// P0-1 fix: do NOT call mono_assembly_close() on the old assembly.
	// close() releases the image while CSharpScript/CSharpInstance still hold
	// raw MonoClass*/MonoObject* pointers into it → UAF (report P0-1).
	// Instead, clear the pointer and open a versioned copy of the new dll
	// (bypasses Mono's path-keyed image cache without releasing the old image).
	if (scripts_assembly) {
		printf("[Mono] Build reload: clearing scripts_assembly pointer (old image kept alive)\n");
		fflush(stdout);
		scripts_assembly = nullptr;
		global_classes_valid = false;
	}

	String assemblies_dir = get_mono_assemblies_dir();
	String output_dll = assemblies_dir.path_join(get_project_csproj_path().get_file().get_basename() + ".dll");

	if (FileAccess::exists(output_dll)) {
		// P0-1 fix: open via versioned temp copy (see open_versioned_assembly).
		scripts_assembly = open_versioned_assembly(output_dll);
		if (scripts_assembly) {
			opened_assemblies.push_back(scripts_assembly);
			String load_msg = "[Mono] Loaded project scripts assembly: " + output_dll;
			printf("%s\n", load_msg.utf8().get_data());
			fflush(stdout);
			if (panel) {
				panel->append_output(load_msg);
			}

			// P2 v2: rebuild global class cache from the new assembly's TypeDef
			// table. Subsequent editor scans of res:// for global classes will
			// hit the cache instead of re-parsing every .cs file.
			refresh_global_classes();

			reload_all_pending_scripts();
			if (panel) {
				panel->set_status("Build succeeded");
			}
		} else {
			String fail_msg = "[Mono] Failed to load compiled scripts assembly.";
			printf("%s\n", fail_msg.utf8().get_data());
			fflush(stdout);
			if (panel) {
				panel->append_output(fail_msg);
				panel->set_status("Assembly load failed", true);
			}
		}
	} else if (panel) {
		panel->set_status("Build succeeded");
	}

	return true;
}

// P4 v2 (W3): thread thunk — keeps the csproj path alive across the thread
// boundary (String is COW; copied by value into the lambda-equivalent struct).
void CSharpLanguage::_build_thread_func(void *p_ud) {
	String *csproj = static_cast<String *>(p_ud);
	CSharpLanguage *self = CSharpLanguage::get_singleton();
	if (self && csproj) {
		self->_build_worker(*csproj);
	}
	memdelete(csproj);
}

// Worker: runs the blocking dotnet build WITHOUT touching Mono, the panel,
// or any engine Object — the result is marshalled through the mailbox and
// consumed by _poll_async_build() on the main thread.
void CSharpLanguage::_build_worker(const String &p_csproj_path) {
	List<String> args;
	args.push_back("build");
	args.push_back(p_csproj_path);
	args.push_back("-c");
	args.push_back("Debug");
	args.push_back("-v:minimal");

	String pipe_output;
	int exit_code = -1;
	Error err = OS::get_singleton()->execute("dotnet", args, &pipe_output, &exit_code, true, nullptr, false);

	MutexLock lock(build_result_lock);
	build_result_ok = (err == OK && exit_code == 0);
	build_result_output = pipe_output;
	build_result_exec_err = (err != OK) ? pipe_output : String();
	build_result_ready = true;
}

// P4 v2 (W3): launch a non-blocking build. Returns false when a build is
// already running (the caller's build_pending stays queued and will be
// consumed by frame() after completion — request merging) or prerequisites
// are missing. Falls back to the synchronous path if the thread cannot start.
bool CSharpLanguage::build_project_async() {
	if (!Engine::get_singleton() || !Engine::get_singleton()->is_editor_hint()) {
		return true;
	}
	if (build_state == BuildState::BUILDING) {
		print_verbose("[Mono] build_project_async: build already in progress, request merged");
		return false;
	}

	MonoBuildPanel *panel = MonoBuildPanel::get_singleton();

	ensure_project_file();

	String csproj_path = get_project_csproj_path();
	if (!FileAccess::exists(csproj_path)) {
		String msg = "[Mono] Cannot build: .csproj not found: " + csproj_path;
		ERR_PRINT(msg);
		if (panel) {
			panel->append_output(msg);
			panel->set_status("Build failed", true);
		}
		return false;
	}

	{
		MutexLock lock(build_result_lock);
		build_result_ready = false;
		build_result_ok = false;
		build_result_output = String();
		build_result_exec_err = String();
	}

	build_state = BuildState::BUILDING;

	String header = "[Mono] Building C# project (async): " + csproj_path;
	printf("%s\n", header.utf8().get_data());
	fflush(stdout);
	if (panel) {
		panel->clear_output();
		panel->append_output(header);
		panel->set_status("Building...");
		panel->set_building(true);
	}

	String *ud = memnew(String(csproj_path));
	Thread::ID tid = build_thread.start(_build_thread_func, ud);
	Error thr_err = (tid != 0) ? OK : ERR_CANT_CREATE;
	if (thr_err != OK) {
		// Thread start failed — degrade gracefully to the synchronous path.
		memdelete(ud);
		build_state = BuildState::IDLE;
		if (panel) {
			panel->append_output("[Mono] WARNING: worker thread unavailable, falling back to synchronous build.");
			panel->set_building(false);
		}
		print_verbose("[Mono] build_project_async: thread start failed, fallback to sync");
		return build_project();
	}
	build_thread_started = true;
	return true;
}

// Main-thread poll: called from frame() while BUILDING. On completion, joins
// the worker, runs the Mono reload sequence, and re-enables the panel button.
void CSharpLanguage::_poll_async_build() {
	if (build_state != BuildState::BUILDING) {
		return;
	}

	bool ready = false;
	bool ok = false;
	String output;
	String exec_err;
	{
		MutexLock lock(build_result_lock);
		ready = build_result_ready;
		if (ready) {
			ok = build_result_ok;
			output = build_result_output;
			exec_err = build_result_exec_err;
			build_result_ready = false;
		}
	}
	if (!ready) {
		return;
	}

	if (build_thread_started) {
		build_thread.wait_to_finish();
		build_thread_started = false;
	}
	build_state = BuildState::IDLE;

	MonoBuildPanel *panel = MonoBuildPanel::get_singleton();
	if (panel) {
		panel->set_building(false);
	}

	_complete_build(ok, output, exec_err);
}
#endif // TOOLS_ENABLED
