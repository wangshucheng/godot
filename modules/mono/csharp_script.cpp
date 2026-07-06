#include "csharp_script.h"
#include "mono_host.h"
#include "mono_bridge.h"
#include "mono_gc_bridge.h"
#include "mono_variant.h"
#include "core/object/object.h"
#include "core/os/os.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/resource.h"
#include "core/config/project_settings.h"
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

class ResourceFormatLoaderCSharpScript : public ResourceFormatLoader {
	GDSOFTCLASS(ResourceFormatLoaderCSharpScript, ResourceFormatLoader);

public:
	Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override {
		if (p_path.is_empty()) {
			if (r_error) *r_error = ERR_INVALID_PARAMETER;
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

	if (class_name.is_empty()) return;

	MonoDomain *domain = MonoHost::get_singleton() ? MonoHost::get_singleton()->get_domain() : nullptr;
	if (!domain) {
		return;
	}

	if (CSharpLanguage::get_singleton()) {
		CSharpLanguage::get_singleton()->load_scripts_assembly();
	}

	String cname_str = class_name;
	const char *cname = cname_str.utf8().get_data();
	String ns = _parse_namespace();

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
			if (mono_class) break;
		}
		if (mono_class) {
			mono_image = img;
			break;
		}
	}

	if (mono_class) {
		if (mono_class_get_flags(mono_class) & MONO_TYPE_ATTR_ABSTRACT) {
			mono_class = nullptr;
		} else {
			mono_class_valid = true;
		}
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
	if (source_valid && !mono_class && MonoHost::get_singleton() && MonoHost::get_singleton()->get_domain()) {
		resolve_mono_class();
	}
	return memnew(CSharpInstance(Ref<CSharpScript>(this), p_this));
}

CSharpInstance::CSharpInstance(const Ref<CSharpScript> &p_script, Object *p_owner) {
	script = p_script;
	owner = p_owner;
	mono_object = nullptr;
	gchandle = 0;

	if (!script.is_valid() || !owner) return;

	MonoDomain *domain = MonoHost::get_singleton() ? MonoHost::get_singleton()->get_domain() : nullptr;
	if (!domain) {
		return;
	}

	MonoClass *klass = script->mono_class;
	if (!klass) {
		return;
	}

	MonoObject *existing = mono_gc_bridge::get_managed(owner);
	if (existing) {
		mono_object = existing;
		gchandle = mono_gchandle_new(mono_object, false);
		return;
	}

	MonoObject *cs_obj = mono_object_new(domain, klass);
	if (!cs_obj) {
		return;
	}

	MonoClassField *native_ptr_field = nullptr;
	for (MonoClass *k = klass; k && !native_ptr_field; k = mono_class_get_parent(k)) {
		native_ptr_field = mono_class_get_field_from_name(k, "NativePtr");
	}

	if (native_ptr_field) {
		intptr_t ptr_val = (intptr_t)owner;
		mono_field_set_value(cs_obj, native_ptr_field, &ptr_val);
	}

	MonoObject *exc = nullptr;
	mono_runtime_object_init(cs_obj);

	if (exc) {
		return;
	}

	mono_bridge::tie_native_ptr(cs_obj, owner);

	mono_object = cs_obj;
	gchandle = mono_gchandle_new(mono_object, false);
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

	const char *mname_cstr = method_name.utf8().get_data();
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

		for (int i = 0; i < copy_count; i++) {
			MonoObject *mo = variant_to_mono_object(domain, *p_args[i]);
			arg_refs.write[i] = mo;
			args[i] = mo;
		}
		for (int i = copy_count; i < param_count; i++) {
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
	const char *pname_cstr = prop_name.utf8().get_data();
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
	const char *pname_cstr = prop_name.utf8().get_data();
	for (MonoClass *k = klass; k && !field; k = mono_class_get_parent(k)) {
		field = mono_class_get_field_from_name(k, pname_cstr);
	}
	if (field) {
		MonoObject *val = nullptr;
		mono_field_get_value(mono_object, field, &val);
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
	const char *pname_cstr = prop_name.utf8().get_data();
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
		Variant arg = p_notification;
		const Variant *args[1] = { &arg };
		Variant result;
		Callable::CallError err;
		invoke_method(on_notification, args, 1, result, err);
	}
}

String CSharpInstance::to_string(bool *r_valid) {
	if (r_valid) *r_valid = false;
	if (!mono_object) return "<CSharpInstance>";

	MonoMethod *to_string = find_method("ToString", 0);
	if (to_string) {
		Variant result;
		Callable::CallError err;
		invoke_method(to_string, nullptr, 0, result, err);
		if (err.error == Callable::CallError::CALL_OK && result.get_type() == Variant::STRING) {
			if (r_valid) *r_valid = true;
			return (String)result;
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

	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	String cwd = OS::get_singleton()->get_cwd();

	Vector<String> search_paths;
	search_paths.push_back(exe_dir.path_join(".mono/assemblies/ProjectScripts.dll"));
	search_paths.push_back(exe_dir.path_join("ProjectScripts.dll"));
	search_paths.push_back(cwd.path_join(".mono/assemblies/ProjectScripts.dll"));
	search_paths.push_back(cwd.path_join("ProjectScripts.dll"));

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
		search_paths.push_back(project_path.path_join(".mono/assemblies/ProjectScripts.dll"));
		search_paths.push_back(project_path.path_join("ProjectScripts.dll"));
	}

	for (int i = 0; i < search_paths.size(); i++) {
		const String &path = search_paths[i];
		if (FileAccess::exists(path)) {
			scripts_assembly = mono_domain_assembly_open(MonoHost::get_singleton()->get_domain(), path.utf8().get_data());
			if (scripts_assembly) {
				return scripts_assembly;
			}
		}
	}

	return nullptr;
}

void CSharpLanguage::init() {
	load_scripts_assembly();
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
	for (const Ref<Resource> &res : resources) {
		Ref<CSharpScript> cs_script = res;
		if (cs_script.is_valid() && !cs_script->is_valid() && cs_script->get_path().get_extension().to_lower() == "cs") {
			cs_script->reload();
		}
	}
}

void CSharpLanguage::frame() {
	if (MonoHost::get_singleton()) {
		MonoHost::get_singleton()->pump_sync_context();
	}
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
	String tmpl;
	if (p_template.contains("Empty")) {
		tmpl = "using Godot;\n\npublic class " + p_class_name + " : " + p_base_class_name + "\n{\n}\n";
	} else {
		tmpl = "using Godot;\n\npublic class " + p_class_name + " : " + p_base_class_name + "\n{\n\tpublic override void _Ready()\n\t{\n\t\tGD.Print(\"Hello from C#!\");\n\t}\n}\n";
	}
	script->set_source_code(tmpl);
	return script;
}

Vector<ScriptLanguage::ScriptTemplate> CSharpLanguage::get_built_in_templates(const StringName &p_object) {
	Vector<ScriptLanguage::ScriptTemplate> templates;

	{
		ScriptTemplate t;
		t.inherit = p_object;
		t.name = "Empty";
		t.description = "An empty C# script.";
		t.content = "using Godot;\n\npublic class _CLASS_ : _BASE_\n{\n}\n";
		t.id = 0;
		t.origin = ScriptLanguage::TEMPLATE_BUILT_IN;
		templates.push_back(t);
	}

	{
		ScriptTemplate t;
		t.inherit = p_object;
		t.name = "C# Script";
		t.description = "A C# script with _Ready() method.";
		t.content = "using Godot;\n\npublic class _CLASS_ : _BASE_\n{\n\tpublic override void _Ready()\n\t{\n\t\tGD.Print(\"Hello from C#!\");\n\t}\n}\n";
		t.id = 1;
		t.origin = ScriptLanguage::TEMPLATE_BUILT_IN;
		templates.push_back(t);
	}

	return templates;
}

String CSharpLanguage::validate_path(const String &p_path) const {
	return p_path;
}
