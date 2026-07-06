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
	uint32_t sync_context_gchandle = 0;
	HashMap<ObjectID, uint32_t> object_gchandles;

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

	void post_sync_callback(void (*p_callback)());
	void process_sync_callbacks();
	void install_synchronization_context();

	void on_frame_tick();

	bool initialize();
	void cleanup();

	bool load_assembly(const String &p_path, bool p_is_proj_assembly = false);
	MonoClass *get_class(const String &p_namespace, const String &p_class_name);
	MonoClass *find_class(const String &p_class_name);
	MonoMethod *get_method(MonoClass *p_class, const String &p_name, int p_param_count = 0);

	GDMono();
	~GDMono();
};

#endif // GD_MONO_H