#ifndef GD_MONO_H
#define GD_MONO_H

#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/list.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/object/object_id.h"

#include <mono/mono-publib.h>

class GDMono {
	bool initialized = false;
	MonoDomain *root_domain = nullptr;
	MonoDomain *scripts_domain = nullptr;
	String assemblies_path;
	MonoImage *godotsharp_image = nullptr;
	MonoAssembly *godotsharp_assembly = nullptr;
	struct UserAssembly {
		MonoAssembly *assembly = nullptr;
		MonoImage *image = nullptr;
		String name;
	};
	Vector<UserAssembly> user_assemblies;

	List<void (*)()> pending_sync_callbacks;
	List<uint32_t> pending_delegate_handles; // GC handles of pinned Action delegates
	uint32_t sync_context_gchandle = 0;
	HashMap<ObjectID, uint32_t> object_gchandles;

	// Hot-reload support: track loaded user assembly paths for re-compilation
	List<String> loaded_assembly_paths;

	// Note: Mono SDB debugger state is managed by the CSharpDebugger namespace
	// (see mono_runtime/csharp_debugger.h). The agent must be configured before
	// mono_jit_init_version() and cannot be hot-toggled at runtime.

public:
	static GDMono *get_singleton();

	_FORCE_INLINE_ MonoDomain *get_root_domain() { return root_domain; }
	_FORCE_INLINE_ MonoDomain *get_scripts_domain() { return scripts_domain; }
	_FORCE_INLINE_ const String &get_assemblies_path() { return assemblies_path; }
	_FORCE_INLINE_ MonoImage *get_godotsharp_image() { return godotsharp_image; }
	_FORCE_INLINE_ MonoAssembly *get_godotsharp_assembly() { return godotsharp_assembly; }
	_FORCE_INLINE_ const Vector<UserAssembly> &get_user_assemblies() const { return user_assemblies; }

	void cache_managed_object(ObjectID p_native_id, MonoObject *p_mono_obj);
	MonoObject *get_cached_managed_object(ObjectID p_native_id) const;
	void remove_cached_managed_object(ObjectID p_native_id);

	// P1.1: 属性系统引用类型字段支持——双向查找 Godot Object* 与 MonoObject*
	MonoObject *get_mono_object_for_godot_object(Object *p_obj) const;
	Object *get_godot_object_for_mono_object(MonoObject *p_mono_obj) const;

	void post_sync_callback(void (*p_callback)());
	void post_sync_delegate(MonoObject *p_delegate);
	void process_sync_callbacks();
	void install_synchronization_context();

	void on_frame_tick();

	bool initialize();
	void cleanup();

#if defined(ANDROID_ENABLED) && !defined(TOOLS_ENABLED)
	// Android: 安装程序集 preload hook，从 APK 内 res:// 路径加载 .dll。
	// 在 initialize() 内部调用，无需外部主动调用（声明为 public 便于测试 mock）。
	void install_android_assembly_preload_hook();
#endif

	bool load_assembly(const String &p_path, bool p_is_proj_assembly = false);
	void clear_user_assemblies();

	// Hot reload: unload scripts domain and recreate (desktop only).
	// On WASM (DISABLE_APPDOMAINS), falls back to clear_user_assemblies + reload.
	bool reload_domain();
	// Reload a single assembly (unload old, load new). Returns true if domain reload was used.
	bool reload_assembly(const String &p_path);

	// Debugger state queries — delegate to CSharpDebugger namespace.
	// Use --mono-debugger=PORT at engine startup to enable the SDB agent.
	bool is_debugger_active() const; // true if SDB agent was requested via cmdline
	MonoClass *get_class(const String &p_namespace, const String &p_class_name);
	MonoClass *find_class(const String &p_class_name);
	MonoMethod *get_method(MonoClass *p_class, const String &p_name, int p_param_count = 0);

	GDMono();
	~GDMono();
};

#endif // GD_MONO_H