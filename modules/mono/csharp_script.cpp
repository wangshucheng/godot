#include "csharp_script.h"
#include "mono_host.h"
#include "mono_bridge.h"
#include "mono_gc_bridge.h"
#include "mono_variant.h"
#include "core/object/object.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "core/io/file_access.h"
#include "core/io/dir_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/resource.h"
#include "core/config/project_settings.h"
#include "core/config/engine.h"
#include <cstring>
#include "scene/main/node.h"
#include <mono/metadata/object.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/class.h>
#include <mono/metadata/attrdefs.h>
#include <cstdio>
#include <cstring>

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

static String sanitize_project_name(const String &p_name) {
	String name = p_name;
	if (name.is_empty()) {
		name = "GodotProject";
	}
	String result;
	for (int i = 0; i < name.length(); i++) {
		char32_t c = name[i];
		if (c < 128) {
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
				result += c;
			} else if (c == ' ' || c == '-' || c == '#' || c == '.' || c == '(' || c == ')') {
				result += '_';
			}
		}
	}
	if (result.is_empty()) {
		result = "GodotProject";
	}
	if (result[0] >= '0' && result[0] <= '9') {
		result = "_" + result;
	}
	return result;
}

static String get_safe_project_name() {
	String project_name;
	if (ProjectSettings::get_singleton()) {
		if (ProjectSettings::get_singleton()->has_setting("dotnet/project/assembly_name")) {
			project_name = ProjectSettings::get_singleton()->get("dotnet/project/assembly_name");
		}
		if (project_name.is_empty()) {
			project_name = ProjectSettings::get_singleton()->get("application/config/name");
		}
	}
	return sanitize_project_name(project_name);
}

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

	MonoClass *klass = mono_object_get_class(mono_object);
	StringName key = StringName(method_name);
	if (p_argcount >= 0) {
		key = StringName(method_name + ":" + itos(p_argcount));
	}

	if (script.is_valid() && script->mono_class) {
		MonoMethod *m = script->get_method(StringName(method_name), p_argcount);
		if (m) return m;
	}

	CharString mname_utf8 = method_name.utf8();
	const char *mname_cstr = mname_utf8.get_data();
	for (MonoClass *k = klass; k; k = mono_class_get_parent(k)) {
		MonoMethod *m = mono_class_get_method_from_name(k, mname_cstr, p_argcount);
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
		MonoDomain *domain = mono_domain_get();
		MonoObject *val = variant_to_mono_object(domain, p_value);
		MonoType *ftype = mono_field_get_type(field);
		MonoClass *field_class = mono_class_from_mono_type(ftype);
		if (val && field_class && mono_class_is_valuetype(field_class)) {
			mono_field_set_value(mono_object, field, mono_object_unbox(val));
		} else {
			mono_field_set_value(mono_object, field, val);
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

	struct NotificationMap {
		int notification;
		const char *method_name;
		int arg_count;
	};

	static const NotificationMap notif_map[] = {
		{Node::NOTIFICATION_READY, "_Ready", 0},
		{Node::NOTIFICATION_ENTER_TREE, "_EnterTree", 0},
		{Node::NOTIFICATION_EXIT_TREE, "_ExitTree", 0},
		{Node::NOTIFICATION_PROCESS, "_Process", 1},
		{Node::NOTIFICATION_PHYSICS_PROCESS, "_PhysicsProcess", 1},
		{-1, nullptr, 0}
	};

	for (int i = 0; notif_map[i].method_name != nullptr; i++) {
		if (notif_map[i].notification == p_notification) {
			MonoMethod *m = find_method(notif_map[i].method_name, notif_map[i].arg_count);
			if (m) {
				// Only call if the method is actually overridden by the script class.
				// The Mono WASM interpreter has a bug with virtual dispatch for
				// inherited (non-overridden) methods that causes "function signature
				// mismatch". Since base class implementations are empty, skipping
				// them is safe and correct.
				MonoClass *method_declaring_class = mono_method_get_class(m);
				if (script.is_valid() && script->mono_class &&
					method_declaring_class != script->mono_class) {
					break;
				}

				MONO_LOG("[Mono] notification: calling %s for '%s'\n", notif_map[i].method_name, script->class_name.utf8().get_data());
				Variant result;
				Callable::CallError err;
				if (notif_map[i].arg_count == 1) {
					double delta = 0.0;
					if (owner->is_class("Node")) {
						Node *node = Object::cast_to<Node>(owner);
						if (p_notification == Node::NOTIFICATION_PROCESS) {
							delta = (double)node->get_process_delta_time();
						} else if (p_notification == Node::NOTIFICATION_PHYSICS_PROCESS) {
							delta = (double)node->get_physics_process_delta_time();
						}
					}
					Variant arg = delta;
					const Variant *args[1] = { &arg };
					invoke_method(m, args, 1, result, err);
				} else {
					invoke_method(m, nullptr, 0, result, err);
				}
			}
			break;
		}
	}

	MonoMethod *on_notification = find_method("_Notification", 1);
	if (on_notification) {
		// Skip if _Notification is not overridden (inherited from Godot.Node).
		MonoClass *notif_declaring_class = mono_method_get_class(on_notification);
		bool notif_overridden = !(script.is_valid() && script->mono_class &&
								  notif_declaring_class != script->mono_class);
		if (notif_overridden) {
			MONO_LOG("[Mono] notification: calling _Notification(%d) for '%s'\n", p_notification, script->class_name.utf8().get_data());
			Variant arg = p_notification;
			const Variant *args[1] = { &arg };
			Variant result;
			Callable::CallError err;
			invoke_method(on_notification, args, 1, result, err);
		}
	}
}

String CSharpInstance::to_string(bool *r_valid) {
	if (r_valid) *r_valid = false;
	if (!mono_object) return "<CSharpInstance>";

	MonoMethod *to_string = find_method("ToString", 0);
	if (to_string) {
		// Skip if ToString is not overridden by the script class.
		// System.Object.ToString() virtual dispatch triggers signature mismatch
		// in WASM interpreter mode.
		MonoClass *decl_class = mono_method_get_class(to_string);
		if (script.is_valid() && script->mono_class &&
			decl_class != script->mono_class) {
			// Not overridden - use C++ fallback.
		} else {
			Variant result;
			Callable::CallError err;
			invoke_method(to_string, nullptr, 0, result, err);
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

MonoAssembly *CSharpLanguage::load_scripts_assembly() {
	if (scripts_assembly) return scripts_assembly;
	if (!MonoHost::get_singleton() || !MonoHost::get_singleton()->get_domain()) return nullptr;

	String project_name = get_safe_project_name();
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
			scripts_assembly = mono_domain_assembly_open(MonoHost::get_singleton()->get_domain(), path.utf8().get_data());
			if (scripts_assembly) {
				printf("[Mono] Loaded scripts assembly: %s\n", path.utf8().get_data());
				fflush(stdout);
				return scripts_assembly;
			}
		}
	}

	return nullptr;
}

void CSharpLanguage::init() {
	ensure_project_file();
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
	scripts_assembly = nullptr;
	loaded_assemblies.clear();
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
			if (!cs_script->mono_class_valid || !cs_script->mono_class) {
				printf("[Mono] Reloading script: %s (mono_class_valid=%d, mono_class=%p)\n",
					   cs_script->get_path().utf8().get_data(), (int)cs_script->mono_class_valid, cs_script->mono_class);
				fflush(stdout);
				cs_script->reload();
				reloaded_count++;
			}
		}
	}
	if (reloaded_count > 0) {
		printf("[Mono] Reloaded %d pending scripts.\n", reloaded_count);
		fflush(stdout);
	}
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
	if (build_pending && Engine::get_singleton() && Engine::get_singleton()->is_editor_hint()) {
		build_pending = false;
		build_project();
	}
#endif
}

void CSharpLanguage::reload_all_scripts() {
	scripts_assembly = nullptr;
	load_scripts_assembly();
}

void CSharpLanguage::reload_scripts(const Array &p_scripts, bool p_soft_reload) {
	reload_all_scripts();
}

void CSharpLanguage::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("cs");
}

Ref<Script> CSharpLanguage::make_template(const String &p_template, const String &p_class_name, const String &p_base_class_name) const {
	Ref<CSharpScript> script;
	script.instantiate();
	String processed = p_template;
	processed = processed.replace("_CLASS_", p_class_name.to_pascal_case().validate_unicode_identifier())
	                     .replace("_BASE_", p_base_class_name);
	script->set_source_code(processed);
	return script;
}

Vector<ScriptLanguage::ScriptTemplate> CSharpLanguage::get_built_in_templates(const StringName &p_object) {
	Vector<ScriptLanguage::ScriptTemplate> templates;

	if (String(p_object) != "Object") {
		return templates;
	}

	{
		ScriptTemplate t;
		t.inherit = p_object;
		t.name = "Empty";
		t.description = "An empty C# script.";
		t.content = "using Godot;\n\npublic partial class _CLASS_ : _BASE_\n{\n}\n";
		t.id = 0;
		t.origin = ScriptLanguage::TEMPLATE_BUILT_IN;
		templates.push_back(t);
	}

	{
		ScriptTemplate t;
		t.inherit = p_object;
		t.name = "C# Script";
		t.description = "A C# script with _Ready() method.";
		t.content = "using Godot;\n\npublic partial class _CLASS_ : _BASE_\n{\n\tpublic override void _Ready()\n\t{\n\t\tGD.Print(\"Hello from C#!\");\n\t}\n}\n";
		t.id = 1;
		t.origin = ScriptLanguage::TEMPLATE_BUILT_IN;
		templates.push_back(t);
	}

	return templates;
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
	String project_name = get_safe_project_name();
	return project_path.path_join(project_name + ".csproj");
}

String CSharpLanguage::get_project_sln_path() const {
	String project_path = ProjectSettings::get_singleton()->get_resource_path();
	if (project_path.is_empty()) {
		project_path = OS::get_singleton()->get_cwd();
	}
	String project_name = get_safe_project_name();
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

	ensure_project_file();

	String csproj_path = get_project_csproj_path();
	if (!FileAccess::exists(csproj_path)) {
		ERR_PRINT("[Mono] Cannot build: .csproj not found.");
		return false;
	}

	String project_dir = csproj_path.get_base_dir();

	List<String> args;
	args.push_back("build");
	args.push_back(csproj_path);
	args.push_back("-c");
	args.push_back("Debug");
	args.push_back("-v:minimal");

	String dotnet_cmd = "dotnet";

	printf("[Mono] Building C# project: %s\n", csproj_path.utf8().get_data());
	fflush(stdout);

	String pipe_output;
	int exit_code = -1;
	Error err = OS::get_singleton()->execute(dotnet_cmd, args, &pipe_output, &exit_code, true, nullptr, false);

	if (err != OK) {
		printf("[Mono] WARNING: Failed to execute dotnet build. Is .NET SDK installed?\n");
		fflush(stdout);
		return false;
	}

	if (!pipe_output.is_empty()) {
		printf("%s\n", pipe_output.utf8().get_data());
		fflush(stdout);
	}

	if (exit_code != 0) {
		printf("[Mono] C# build failed with exit code: %d\n", exit_code);
		fflush(stdout);
		return false;
	}

	printf("[Mono] C# build succeeded.\n");
	fflush(stdout);

	String assemblies_dir = get_mono_assemblies_dir();
	String output_dll = assemblies_dir.path_join(get_project_csproj_path().get_file().get_basename() + ".dll");

	if (FileAccess::exists(output_dll)) {
		scripts_assembly = nullptr;
		scripts_assembly = mono_domain_assembly_open(MonoHost::get_singleton()->get_domain(), output_dll.utf8().get_data());
		if (scripts_assembly) {
			printf("[Mono] Loaded project scripts assembly: %s\n", output_dll.utf8().get_data());
			fflush(stdout);

			reload_all_pending_scripts();
		} else {
			printf("[Mono] Failed to load compiled scripts assembly.\n");
			fflush(stdout);
		}
	}

	return exit_code == 0;
#else
	return true;
#endif
}
