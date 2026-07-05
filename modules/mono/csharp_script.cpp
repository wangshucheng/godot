#include "csharp_script.h"
#include "mono_host.h"
#include "mono_bridge.h"
#include "mono_gc_bridge.h"
#include "mono_variant.h"
#include "core/object/object.h"
#include "core/os/os.h"
#include "core/io/file_access.h"
#include "scene/main/node.h"
#include <mono/metadata/object.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/class.h>
#include <mono/metadata/property.h>
#include <mono/metadata/attrdefs.h>
#include <cstdio>
#include <cstring>

using namespace mono_variant;
using namespace mono_bridge;

CSharpLanguage *CSharpLanguage::singleton = nullptr;

CSharpScript::CSharpScript() {}

ScriptLanguage *CSharpScript::get_language() const {
	return CSharpLanguage::get_singleton();
}

bool CSharpScript::has_method(const StringName &p_method) const {
	if (!mono_class) return false;
	MonoMethod *m = mono_class_get_method_from_name(mono_class, p_method.utf8().get_data(), -1);
	return m != nullptr;
}

int CSharpScript::get_script_method_argument_count(const StringName &p_method, bool *r_is_valid) const {
	if (!mono_class) {
		if (r_is_valid) *r_is_valid = false;
		return 0;
	}
	MonoMethod *m = mono_class_get_method_from_name(mono_class, p_method.utf8().get_data(), -1);
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
		const char *name = mono_method_get_name(m);
		if (name && name[0] != '.' && strncmp(name, "get_", 4) != 0 && strncmp(name, "set_", 4) != 0) {
			MethodInfo mi;
			mi.name = String::utf8(name);
			r_list->push_back(mi);
		}
	}
}

Variant CSharpScript::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
	return Variant();
}

void CSharpScript::resolve_mono_class() {
	mono_class = nullptr;
	mono_image = nullptr;
	method_cache.clear();

	if (class_name.is_empty()) return;

	MonoDomain *domain = MonoHost::get_singleton() ? MonoHost::get_singleton()->get_domain() : nullptr;
	if (!domain) return;

	const char *cname = class_name.utf8().get_data();

	const char *namespaces[] = {"", "Godot", nullptr};

	for (int ni = 0; namespaces[ni] != nullptr; ni++) {
		mono_class = mono_class_from_name(mono_get_corlib(), namespaces[ni], cname);
		if (mono_class) break;
	}

	if (!mono_class && MonoHost::get_singleton()) {
		MonoAssembly *gs = MonoHost::get_singleton()->get_godotsharp_assembly();
		if (gs) {
			mono_image = mono_assembly_get_image(gs);
			for (int ni = 0; namespaces[ni] != nullptr; ni++) {
				mono_class = mono_class_from_name(mono_image, namespaces[ni], cname);
				if (mono_class) break;
			}
		}
	}

	if (!mono_class && CSharpLanguage::get_singleton()) {
		MonoAssembly *scripts_asm = CSharpLanguage::get_singleton()->get_scripts_assembly();
		if (scripts_asm) {
			mono_image = mono_assembly_get_image(scripts_asm);
			for (int ni = 0; namespaces[ni] != nullptr; ni++) {
				mono_class = mono_class_from_name(mono_image, namespaces[ni], cname);
				if (mono_class) break;
			}
		}
	}

	if (mono_class) {
		if (mono_class_is_abstract(mono_class)) {
			printf("[Mono] WARNING: Class '%s' is abstract, cannot instantiate.\n", cname);
			mono_class = nullptr;
		} else {
			valid = true;
			printf("[Mono] CSharpScript: Resolved class '%s' for script.\n", cname);
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

	const char *mname = p_method.utf8().get_data();
	MonoMethod *m = mono_class_get_method_from_name(mono_class, mname, p_argcount);

	if (m) {
		method_cache[key] = m;
	}
	return m;
}

Error CSharpScript::reload(bool p_keep_state) {
	String path = get_path();
	if (path.is_empty()) {
		valid = true;
		class_name = "Node";
		native_base_name = "Node";
		return OK;
	}

	class_name = path.get_basename().get_file();
	native_base_name = "Node";

	if (MonoHost::get_singleton() && MonoHost::get_singleton()->get_domain()) {
		resolve_mono_class();
	} else {
		valid = true;
	}

	return OK;
}

ScriptInstance *CSharpScript::instance_create(Object *p_this) {
	if (!valid) {
		CSharpScript *mutable_this = const_cast<CSharpScript *>(this);
		if (MonoHost::get_singleton() && MonoHost::get_singleton()->get_domain() && !mono_class) {
			mutable_this->resolve_mono_class();
		}
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
		printf("[Mono] WARNING: CSharpInstance created before Mono domain initialized.\n");
		return;
	}

	MonoClass *klass = script->mono_class;
	if (!klass) {
		printf("[Mono] WARNING: CSharpInstance: class '%s' not resolved, instance will be non-functional.\n",
			  script->class_name.utf8().get_data());
		return;
	}

	MonoObject *existing = mono_gc_bridge::get_managed(owner);
	if (existing) {
		mono_object = existing;
		gchandle = mono_gchandle_new(mono_object, false);
		printf("[Mono] CSharpInstance: Reusing existing managed object for '%s'.\n", script->class_name.utf8().get_data());
		return;
	}

	MonoObject *cs_obj = mono_object_new(domain, klass);
	if (!cs_obj) {
		printf("[Mono] ERROR: Failed to create MonoObject for class '%s'.\n", script->class_name.utf8().get_data());
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
		MonoClass *exc_class = mono_object_get_class(exc);
		const char *exc_name = exc_class ? mono_class_get_name(exc_class) : "(unknown)";
		printf("[Mono] ERROR: Exception in script constructor: %s\n", exc_name ? exc_name : "?");
		return;
	}

	mono_bridge::tie_native_ptr(cs_obj, owner);

	mono_object = cs_obj;
	gchandle = mono_gchandle_new(mono_object, false);

	printf("[Mono] CSharpInstance created for '%s' (owner=%s).\n",
		   script->class_name.utf8().get_data(), String(owner->get_class()).utf8().get_data());
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

	const char *mname = method_name.utf8().get_data();
	for (MonoClass *k = klass; k; k = mono_class_get_parent(k)) {
		MonoMethod *m = mono_class_get_method_from_name(k, mname, p_argcount);
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

	const char *prop_name = p_name.utf8().get_data();
	String setter_name = "set_" + String(p_name);
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
	for (MonoClass *k = klass; k && !field; k = mono_class_get_parent(k)) {
		field = mono_class_get_field_from_name(k, prop_name);
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
		MonoProperty *prop = mono_class_get_property_from_name(k, prop_name);
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

	const char *prop_name = p_name.utf8().get_data();

	String getter_name = "get_" + String(p_name);
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
	for (MonoClass *k = klass; k && !field; k = mono_class_get_parent(k)) {
		field = mono_class_get_field_from_name(k, prop_name);
	}
	if (field) {
		MonoObject *val = nullptr;
		mono_field_get_value(mono_object, field, &val);
		r_ret = mono_object_to_variant(val);
		return true;
	}

	for (MonoClass *k = klass; k; k = mono_class_get_parent(k)) {
		MonoProperty *prop = mono_class_get_property_from_name(k, prop_name);
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

	const char *prop_name = p_name.utf8().get_data();
	MonoClass *klass = mono_object_get_class(mono_object);
	MonoProperty *prop = mono_class_get_property_from_name(klass, prop_name);
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

	String search_paths[] = {
		exe_dir.path_join(".mono/assemblies/GodotSharp.dll"),
		exe_dir.path_join(".mono/assemblies/ProjectScripts.dll"),
		exe_dir.path_join("ProjectScripts.dll"),
	};

	for (int i = 0; i < 3; i++) {
		if (FileAccess::exists(search_paths[i])) {
			scripts_assembly = mono_domain_assembly_open(MonoHost::get_singleton()->get_domain(), search_paths[i].utf8().get_data());
			if (scripts_assembly) {
				const char *img_name = mono_image_get_name(mono_assembly_get_image(scripts_assembly));
				printf("[Mono] Loaded project scripts assembly: %s\n", img_name ? img_name : "(unknown)");
				return scripts_assembly;
			}
		}
	}

	return nullptr;
}

void CSharpLanguage::init() {
	printf("[Mono] CSharpLanguage initialized.\n");
}

void CSharpLanguage::finish() {
	scripts_assembly = nullptr;
	loaded_assemblies.clear();
	printf("[Mono] CSharpLanguage finished.\n");
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
	String tmpl = "using Godot;\n\npublic partial class " + p_class_name + " : " + p_base_class_name + "\n{\n    public override void _Ready()\n    {\n        GD.Print(\"Hello from C#!\");\n    }\n}\n";
	script->set_source_code(tmpl);
	return script;
}

Vector<ScriptLanguage::ScriptTemplate> CSharpLanguage::get_built_in_templates(const StringName &p_object) {
	Vector<ScriptTemplate> templates;
	ScriptTemplate t;
	t.inherit = p_object;
	t.name = "C# Node";
	t.description = "C# Script";
	t.content = "using Godot;\n\npublic partial class _CLASS_ : _BASE_\n{\n    public override void _Ready()\n    {\n        GD.Print(\"Hello from C#!\");\n    }\n}\n";
	templates.push_back(t);
	return templates;
}

String CSharpLanguage::validate_path(const String &p_path) const {
	return p_path;
}
