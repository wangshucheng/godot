#include "csharp_script.h"
#include "mono_host.h"
#include "mono_bridge.h"
#include "mono_variant.h"
#include <mono/metadata/object.h>
#include <mono/metadata/assembly.h>
#include <cstdio>

using namespace mono_variant;
using namespace mono_bridge;

CSharpLanguage *CSharpLanguage::singleton = nullptr;

CSharpScript::CSharpScript() {}

ScriptLanguage *CSharpScript::get_language() const {
	return CSharpLanguage::get_singleton();
}

bool CSharpScript::has_method(const StringName &p_method) const {
	return false;
}

ScriptInstance *CSharpScript::instance_create(Object *p_this) {
	return memnew(CSharpInstance(Ref<CSharpScript>(this), p_this));
}

Error CSharpScript::reload(bool p_keep_state) {
	valid = true;
	class_name = get_path().get_basename().get_file();
	native_base_name = "Node";
	return OK;
}

CSharpInstance::CSharpInstance(const Ref<CSharpScript> &p_script, Object *p_owner) {
	script = p_script;
	owner = p_owner;
	printf("[Mono] CSharpInstance created for %s\n", String(owner->get_class()).utf8().get_data());
}

CSharpInstance::~CSharpInstance() {}

bool CSharpInstance::set(const StringName &p_name, const Variant &p_value) { return false; }
bool CSharpInstance::get(const StringName &p_name, Variant &r_ret) const { return false; }
bool CSharpInstance::has_method(const StringName &p_method) const { return false; }

Variant CSharpInstance::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
	return Variant();
}

void CSharpInstance::notification(int p_notification, bool p_reversed) {}
ScriptLanguage *CSharpInstance::get_language() { return CSharpLanguage::get_singleton(); }

CSharpLanguage::CSharpLanguage() { singleton = this; }
CSharpLanguage::~CSharpLanguage() { singleton = nullptr; }

void CSharpLanguage::init() {
	printf("[Mono] CSharpLanguage initialized.\n");
}

void CSharpLanguage::finish() {
	printf("[Mono] CSharpLanguage finished.\n");
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
