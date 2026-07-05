#ifndef GD_MONO_H
#define GD_MONO_H

#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"

#include <mono/mono-publib.h>

class GDMono {
	bool initialized = false;
	MonoDomain *root_domain = nullptr;
	MonoDomain *scripts_domain = nullptr;
	String assemblies_path;

public:
	static GDMono *get_singleton();

	_FORCE_INLINE_ MonoDomain *get_root_domain() { return root_domain; }
	_FORCE_INLINE_ MonoDomain *get_scripts_domain() { return scripts_domain; }
	_FORCE_INLINE_ const String &get_assemblies_path() { return assemblies_path; }

	bool initialize();
	void cleanup();

	bool load_assembly(const String &p_path, bool p_is_proj_assembly = false);
	MonoClass *get_class(const String &p_namespace, const String &p_class_name);
	MonoMethod *get_method(MonoClass *p_class, const String &p_name, int p_param_count = 0);

	GDMono();
	~GDMono();
};

#endif // GD_MONO_H
