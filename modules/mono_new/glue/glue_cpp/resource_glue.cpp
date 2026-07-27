// ICall bindings for Resource / RefCounted - direct C++ method calls.
// All float-returning icalls return int64 bit-patterns to comply with the WASM
// interpreter's do_icall signature constraints (no direct double returns).
// The C# side uses DoubleLongUnion to reinterpret the bits back to double.

#include "../../mono_gd/interop/gd_mono_interop_variant.h"
#include "../../utils/mono_logger.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#include "core/object/ref_counted.h"
#include "core/io/resource.h"
#include "core/io/resource_saver.h"
#include "core/variant/variant.h"
#include <mono/mono-publib.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" {
char *mono_string_to_utf8(MonoString *s);
void mono_free(void *ptr);
}

namespace {

// Helper: convert MonoString -> String (RAII over the utf8 buffer).
struct MonoStringHolder {
	char *utf8;
	MonoStringHolder(MonoString *s) : utf8(s ? mono_string_to_utf8(s) : nullptr) {}
	~MonoStringHolder() { if (utf8) mono_free(utf8); }
	operator bool() const { return utf8 != nullptr; }
	String to_string() const { return utf8 ? String::utf8(utf8) : String(); }
};

// RefCounted::init_ref() -> bool (returns false if refcount reached 0 during init)
static mono_bool icall_RefCounted_InitRef(int64_t p_obj) {
	RefCounted *rc = (RefCounted *)(intptr_t)p_obj;
	if (!rc) return false;
	return rc->init_ref();
}

// RefCounted::reference() -> bool
static mono_bool icall_RefCounted_Reference(int64_t p_obj) {
	RefCounted *rc = (RefCounted *)(intptr_t)p_obj;
	if (!rc) return false;
	return rc->reference();
}

// RefCounted::unreference() -> bool (returns true if refcount reached 0)
static mono_bool icall_RefCounted_Unreference(int64_t p_obj) {
	RefCounted *rc = (RefCounted *)(intptr_t)p_obj;
	if (!rc) return false;
	return rc->unreference();
}

// RefCounted::get_reference_count() -> int64
static int64_t icall_RefCounted_GetReferenceCount(int64_t p_obj) {
	RefCounted *rc = (RefCounted *)(intptr_t)p_obj;
	if (!rc) return 0;
	return (int64_t)rc->get_reference_count();
}

// Resource::set_path(path) - sets the resource_path. Wraps Resource::set_path().
static void icall_Resource_SetPath(int64_t p_res, MonoString *p_path) {
	Resource *res = (Resource *)(intptr_t)p_res;
	if (!res || !p_path) return;
	MonoStringHolder holder(p_path);
	if (!holder) return;
	res->set_path(holder.to_string());
}

// Resource::get_path() -> string
static MonoString *icall_Resource_GetPath(int64_t p_res) {
	Resource *res = (Resource *)(intptr_t)p_res;
	if (!res) return mono_string_new(mono_domain_get(), "");
	String path = res->get_path();
	CharString cs = path.utf8();
	return mono_string_new(mono_domain_get(), cs.get_data());
}

// Resource::set_name(name) - resource_name property
static void icall_Resource_SetName(int64_t p_res, MonoString *p_name) {
	Resource *res = (Resource *)(intptr_t)p_res;
	if (!res || !p_name) return;
	MonoStringHolder holder(p_name);
	if (!holder) return;
	res->set_name(holder.to_string());
}

// Resource::get_name() -> string
static MonoString *icall_Resource_GetName(int64_t p_res) {
	Resource *res = (Resource *)(intptr_t)p_res;
	if (!res) return mono_string_new(mono_domain_get(), "");
	String name = res->get_name();
	CharString cs = name.utf8();
	return mono_string_new(mono_domain_get(), cs.get_data());
}

// Object::set_meta(name, Variant) - generic meta storage. We accept a Variant
// encoded as a MonoObject* via the existing mono_object_to_variant bridge.
// To keep the icall signature WASM-safe, we route through the same path used
// by godot_icall_Object_CallStringObject: a string + an object pointer.
static void icall_Resource_SetMetaString(int64_t p_obj, MonoString *p_name, MonoString *p_value) {
	Object *obj = (Object *)(intptr_t)p_obj;
	if (!obj || !p_name) return;
	MonoStringHolder name_h(p_name);
	if (!name_h) return;
	MonoStringHolder value_h(p_value);
	String value_str = value_h ? value_h.to_string() : String();
	obj->set_meta(name_h.to_string(), value_str);
}

// Object::get_meta(name, default) -> string. Returns the default if missing.
static MonoString *icall_Resource_GetMetaString(int64_t p_obj, MonoString *p_name, MonoString *p_default) {
	Object *obj = (Object *)(intptr_t)p_obj;
	if (!obj || !p_name) return p_default ? p_default : mono_string_new(mono_domain_get(), "");
	MonoStringHolder name_h(p_name);
	if (!name_h) return p_default ? p_default : mono_string_new(mono_domain_get(), "");
	String key = name_h.to_string();
	if (!obj->has_meta(key)) {
		if (p_default) return p_default;
		return mono_string_new(mono_domain_get(), "");
	}
	Variant v = obj->get_meta(key);
	String s = v;
	CharString cs = s.utf8();
	return mono_string_new(mono_domain_get(), cs.get_data());
}

// Object::has_meta(name) -> bool
static mono_bool icall_Resource_HasMeta(int64_t p_obj, MonoString *p_name) {
	Object *obj = (Object *)(intptr_t)p_obj;
	if (!obj || !p_name) return false;
	MonoStringHolder name_h(p_name);
	if (!name_h) return false;
	return obj->has_meta(name_h.to_string());
}

// Object::remove_meta(name)
static void icall_Resource_RemoveMeta(int64_t p_obj, MonoString *p_name) {
	Object *obj = (Object *)(intptr_t)p_obj;
	if (!obj || !p_name) return;
	MonoStringHolder name_h(p_name);
	if (!name_h) return;
	obj->remove_meta(name_h.to_string());
}

// Object::get_meta_list() -> string[] (packed as semicolon-separated for WASM safety)
// Returns all meta keys joined by '\n' so the C# side can Split.
static MonoString *icall_Resource_GetMetaList(int64_t p_obj) {
	Object *obj = (Object *)(intptr_t)p_obj;
	if (!obj) return mono_string_new(mono_domain_get(), "");
	List<StringName> keys;
	obj->get_meta_list(&keys);
	String joined;
	for (const StringName &k : keys) {
		if (!joined.is_empty()) joined += "\n";
		joined += String(k);
	}
	CharString cs = joined.utf8();
	return mono_string_new(mono_domain_get(), cs.get_data());
}

// Resource::take_over_path(path) - useful for programmatic resource registration.
// In Godot 4.7, take_over_path() is private; the public API is set_path(path, true).
static void icall_Resource_TakeOverPath(int64_t p_res, MonoString *p_path) {
	Resource *res = (Resource *)(intptr_t)p_res;
	if (!res || !p_path) return;
	MonoStringHolder holder(p_path);
	if (!holder) return;
	res->set_path(holder.to_string(), true);
}

// H7 扩展: Resource::duplicate(flags) -> Resource*
// 通过 ClassDB 路径调用，避免依赖 Resource::duplicate 的具体签名
// flags 参考 Node::DuplicateFlags: 0=浅复制, SUBRESOURCES=1 深复制
static int64_t icall_Resource_Duplicate(int64_t p_res, int64_t p_flags) {
	Resource *res = (Resource *)(intptr_t)p_res;
	if (!res) return 0;
	Variant v_res = res;
	Variant v_flags = (int64_t)p_flags;
	const Variant *args[] = { &v_flags };
	Variant ret;
	Callable::CallError err;
	static const StringName method_name("duplicate");
	ret = res->callp(method_name, args, 1, err);
	if (err.error != Callable::CallError::CALL_OK) return 0;
	Resource *dup = Object::cast_to<Resource>(ret);
	if (!dup) return 0;
	dup->reference(); // C# 侧 ownsNative=true，析构时会 unreference
	return (int64_t)(intptr_t)dup;
}

// H7 扩展: ResourceSaver::save(resource, path, flags) -> int32
// 返回 OK(0) 或错误码（负值）；用 int32 透传以复用 cookie ILII
static int32_t icall_ResourceSaver_Save(int64_t p_res, MonoString *p_path, int32_t p_flags) {
	Resource *res = (Resource *)(intptr_t)p_res;
	if (!res || !p_path) return (int32_t)ERR_INVALID_PARAMETER;
	MonoStringHolder holder(p_path);
	if (!holder) return (int32_t)ERR_INVALID_PARAMETER;
	Error err = ResourceSaver::save(Ref<Resource>(res), holder.to_string(), (uint32_t)p_flags);
	return (int32_t)err;
}

// ClassDB validation: verify Resource / RefCounted have the methods we bind.
static void scan_and_validate_resource_methods() {
	const char *resource_methods[] = {
		"set_path", "get_path", "set_name", "get_name",
		"take_over_path", "set_meta", "get_meta", "has_meta",
		"remove_meta", "get_meta_list",
		nullptr
	};
	for (int i = 0; resource_methods[i] != nullptr; i++) {
		if (!ClassDB::has_method("Resource", resource_methods[i])) {
			MonoLogger::log_warning(vformat("ClassDB: Resource::%s not found", resource_methods[i]));
		}
	}
	const char *refcounted_methods[] = {
		"init_ref", "reference", "unreference", "get_reference_count",
		nullptr
	};
	for (int i = 0; refcounted_methods[i] != nullptr; i++) {
		if (!ClassDB::has_method("RefCounted", refcounted_methods[i])) {
			MonoLogger::log_warning(vformat("ClassDB: RefCounted::%s not found", refcounted_methods[i]));
		}
	}
}

} // anonymous namespace

namespace GDMonoInterop {

void register_resource_icalls() {
	MonoLogger::log("Registering Resource/RefCounted icalls...");
	scan_and_validate_resource_methods();

	// RefCounted - reference counting
	mono_add_internal_call("Godot.Resource::godot_icall_RefCounted_InitRef", (const void *)icall_RefCounted_InitRef);
	mono_add_internal_call("Godot.Resource::godot_icall_RefCounted_Reference", (const void *)icall_RefCounted_Reference);
	mono_add_internal_call("Godot.Resource::godot_icall_RefCounted_Unreference", (const void *)icall_RefCounted_Unreference);
	mono_add_internal_call("Godot.Resource::godot_icall_RefCounted_GetReferenceCount", (const void *)icall_RefCounted_GetReferenceCount);

	// Resource - path
	mono_add_internal_call("Godot.Resource::godot_icall_Resource_SetPath", (const void *)icall_Resource_SetPath);
	mono_add_internal_call("Godot.Resource::godot_icall_Resource_GetPath", (const void *)icall_Resource_GetPath);
	mono_add_internal_call("Godot.Resource::godot_icall_Resource_TakeOverPath", (const void *)icall_Resource_TakeOverPath);

	// Resource - name
	mono_add_internal_call("Godot.Resource::godot_icall_Resource_SetName", (const void *)icall_Resource_SetName);
	mono_add_internal_call("Godot.Resource::godot_icall_Resource_GetName", (const void *)icall_Resource_GetName);

	// Object-level metadata (also works for Resource since it inherits Object)
	mono_add_internal_call("Godot.Resource::godot_icall_Resource_SetMetaString", (const void *)icall_Resource_SetMetaString);
	mono_add_internal_call("Godot.Resource::godot_icall_Resource_GetMetaString", (const void *)icall_Resource_GetMetaString);
	mono_add_internal_call("Godot.Resource::godot_icall_Resource_HasMeta", (const void *)icall_Resource_HasMeta);
	mono_add_internal_call("Godot.Resource::godot_icall_Resource_RemoveMeta", (const void *)icall_Resource_RemoveMeta);
	mono_add_internal_call("Godot.Resource::godot_icall_Resource_GetMetaList", (const void *)icall_Resource_GetMetaList);

	// H7 扩展: Resource.Duplicate + ResourceSaver.Save
	mono_add_internal_call("Godot.Resource::godot_icall_Resource_Duplicate", (const void *)icall_Resource_Duplicate);
	mono_add_internal_call("Godot.ResourceSaver::godot_icall_ResourceSaver_Save", (const void *)icall_ResourceSaver_Save);

	MonoLogger::log("Resource/RefCounted icalls registered (15 methods)");
}

} // namespace GDMonoInterop
