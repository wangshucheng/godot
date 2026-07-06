#include "gd_mono.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "core/os/main_loop.h"
#include "utils/mono_logger.h"
#include "../mono_gd/interop/gd_mono_interop_variant.h"
#include "../mono_gd/interop/gd_mono_callable.h"
#include "../glue/mono_glue.h"

static GDMono *singleton = nullptr;

GDMono *GDMono::get_singleton() {
	return singleton;
}

GDMono::GDMono() {
	singleton = this;
}

GDMono::~GDMono() {
	cleanup();
	if (singleton == this)
		singleton = nullptr;
}

void GDMono::cache_managed_object(ObjectID p_native_id, MonoObject *p_mono_obj) {
	if (!p_mono_obj) return;
	uint32_t gchandle = mono_gchandle_new(p_mono_obj, true);
	if (object_gchandles.has(p_native_id)) {
		mono_gchandle_free(object_gchandles[p_native_id]);
	}
	object_gchandles[p_native_id] = gchandle;
}

MonoObject *GDMono::get_cached_managed_object(ObjectID p_native_id) const {
	const uint32_t *gchandle = object_gchandles.getptr(p_native_id);
	if (!gchandle || *gchandle == 0) return nullptr;
	return mono_gchandle_get_target(*gchandle);
}

void GDMono::remove_cached_managed_object(ObjectID p_native_id) {
	uint32_t *gchandle = object_gchandles.getptr(p_native_id);
	if (gchandle && *gchandle != 0) {
		mono_gchandle_free(*gchandle);
		*gchandle = 0;
	}
	object_gchandles.erase(p_native_id);
}

void GDMono::post_sync_callback(void (*p_callback)()) {
	if (p_callback) {
		pending_sync_callbacks.push_back(p_callback);
	}
}

void GDMono::process_sync_callbacks() {
	while (!pending_sync_callbacks.is_empty()) {
		void (*cb)() = pending_sync_callbacks.front()->get();
		pending_sync_callbacks.pop_front();
		if (cb) cb();
	}
}

void GDMono::install_synchronization_context() {
	if (!scripts_domain) return;
	MonoLogger::log("Installing Godot synchronization context...");
}

void GDMono::on_frame_tick() {
	process_sync_callbacks();
}

bool GDMono::initialize() {
	if (initialized)
		return true;

	MonoLogger::log("Initializing Mono runtime (static linkage mode)...");

	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();

	String mono_lib_dir = exe_dir.path_join("mono").path_join("lib");
	String mono_etc_dir = exe_dir.path_join("mono").path_join("etc");
	mono_set_dirs(mono_lib_dir.utf8().get_data(), mono_etc_dir.utf8().get_data());

	mono_config_parse(nullptr);

	assemblies_path = exe_dir.path_join(".mono").path_join("assemblies");
	if (!DirAccess::exists(assemblies_path)) {
		DirAccess::make_dir_recursive_absolute(assemblies_path);
	}

	String mono_bcl_dir = mono_lib_dir.path_join("mono").path_join("4.5");

	const char *runtime_version = "v4.0.30319";

	root_domain = mono_jit_init_version("GodotEngine", runtime_version);
	if (!root_domain) {
		MonoLogger::log_warning(vformat("Mono JIT init reported issues (BCL not found at %s). Managed code execution will be unavailable until BCL assemblies are deployed.", mono_bcl_dir));
		MonoLogger::log_warning("Place mscorlib.dll and BCL assemblies in: <exe_dir>/mono/lib/mono/4.5/");
	} else {
		MonoLogger::log(vformat("Mono BCL path: %s", mono_bcl_dir));
	}

	if (root_domain) {
		scripts_domain = mono_domain_create_appdomain(const_cast<char *>("GodotScripts"), nullptr);
		if (!scripts_domain) {
			MonoLogger::log_error("Failed to create scripts app domain");
		} else {
			mono_domain_set(scripts_domain, true);
		}
	}

	GDMonoInterop::variant_register_icalls();
	GDMonoCallable::register_icalls();

	mono_glue_init();

	if (scripts_domain) {
		Vector<String> search_paths;
		search_paths.push_back(exe_dir.path_join("GodotSharp.dll"));
		search_paths.push_back(exe_dir.path_join(".mono").path_join("assemblies").path_join("GodotSharp.dll"));
		search_paths.push_back(assemblies_path.path_join("GodotSharp.dll"));

		for (const String &path : search_paths) {
			if (FileAccess::exists(path)) {
				MonoLogger::log(vformat("Loading GodotSharp from: %s", path));
				godotsharp_assembly = mono_domain_assembly_open(scripts_domain, path.utf8().get_data());
				if (godotsharp_assembly) {
					godotsharp_image = mono_assembly_get_image(godotsharp_assembly);
					if (godotsharp_image) {
						MonoLogger::log("GodotSharp loaded successfully");
						break;
					}
				}
			}
		}
	}

	if (scripts_domain) {
		Vector<String> search_dirs;
		search_dirs.push_back(assemblies_path);
		search_dirs.push_back(OS::get_singleton()->get_user_data_dir().path_join(".mono").path_join("assemblies"));
		search_dirs.push_back(String("res://.mono/assemblies"));

		Vector<String> user_dll_paths;

		for (const String &search_dir : search_dirs) {
			String abs_dir = ProjectSettings::get_singleton() ? ProjectSettings::get_singleton()->globalize_path(search_dir) : search_dir;
			MonoLogger::log(vformat("Scanning for user assemblies in: %s", abs_dir));

			if (FileAccess::exists(abs_dir) || DirAccess::exists(abs_dir)) {
				Ref<DirAccess> dir = DirAccess::open(abs_dir);
				if (dir.is_valid()) {
					dir->list_dir_begin();
					String fname = dir->get_next();
					while (!fname.is_empty()) {
						if (!dir->current_is_dir() && fname.ends_with(".dll") && fname != "GodotSharp.dll") {
							String full_path = abs_dir.path_join(fname);
							if (!user_dll_paths.has(full_path)) {
								user_dll_paths.push_back(full_path);
							}
						}
						fname = dir->get_next();
					}
					dir->list_dir_end();
				}
			}
		}

		MonoLogger::log(vformat("Found %d candidate user assemblies", user_dll_paths.size()));

		for (const String &path : user_dll_paths) {
			MonoLogger::log(vformat("Loading user assembly from: %s", path));
			MonoAssembly *assy = mono_domain_assembly_open(scripts_domain, path.utf8().get_data());
			if (assy) {
				MonoImage *img = mono_assembly_get_image(assy);
				if (img) {
					UserAssembly ua;
					ua.assembly = assy;
					ua.image = img;
					ua.name = path.get_file().get_basename();
					user_assemblies.push_back(ua);
					MonoLogger::log(vformat("User assembly loaded: %s", ua.name));
				}
			} else {
				MonoLogger::log_warning(vformat("Failed to load assembly: %s", path));
			}
		}
	}

	initialized = true;

	if (root_domain && scripts_domain) {
		install_synchronization_context();
		MonoLogger::log("Mono runtime initialized successfully");
		MonoLogger::log(vformat("Runtime build info: %s", mono_get_runtime_build_info()));
	} else {
		MonoLogger::log("Mono runtime initialized in limited mode (no BCL/assemblies)");
	}
	MonoLogger::log(vformat("User assemblies path: %s", assemblies_path));

	return true;
}

void GDMono::cleanup() {
	if (!initialized)
		return;

	for (KeyValue<ObjectID, uint32_t> &E : object_gchandles) {
		if (E.value != 0) {
			mono_gchandle_free(E.value);
		}
	}
	object_gchandles.clear();

	if (sync_context_gchandle != 0) {
		mono_gchandle_free(sync_context_gchandle);
		sync_context_gchandle = 0;
	}

	user_assemblies.clear();
	godotsharp_assembly = nullptr;
	godotsharp_image = nullptr;

	if (scripts_domain) {
		mono_domain_set(root_domain, true);
		mono_domain_unload(scripts_domain);
		scripts_domain = nullptr;
	}

	if (root_domain) {
		mono_jit_cleanup(root_domain);
		root_domain = nullptr;
	}

	initialized = false;
	MonoLogger::log("Mono runtime cleaned up");
}

bool GDMono::load_assembly(const String &p_path, bool p_is_proj_assembly) {
	if (!scripts_domain) {
		MonoLogger::log_error(vformat("Cannot load assembly (scripts domain not initialized): %s", p_path));
		return false;
	}

	MonoAssembly *assembly = mono_domain_assembly_open(scripts_domain, p_path.utf8().get_data());
	if (!assembly) {
		MonoLogger::log_error(vformat("Failed to load assembly: %s", p_path));
		return false;
	}

	MonoImage *image = mono_assembly_get_image(assembly);
	if (!image) {
		MonoLogger::log_error(vformat("Failed to get image for assembly: %s", p_path));
		return false;
	}

	const char *name = mono_image_get_name(image);
	MonoLogger::log(vformat("Loaded assembly: %s", name));

	return true;
}

MonoClass *GDMono::get_class(const String &p_namespace, const String &p_class_name) {
	if (!root_domain) {
		return nullptr;
	}

	MonoClass *klass = nullptr;

	for (const UserAssembly &ua : user_assemblies) {
		if (ua.image) {
			klass = mono_class_from_name(ua.image,
					p_namespace.utf8().get_data(),
					p_class_name.utf8().get_data());
			if (klass) return klass;
		}
	}

	if (godotsharp_image) {
		klass = mono_class_from_name(godotsharp_image,
				p_namespace.utf8().get_data(),
				p_class_name.utf8().get_data());
		if (klass) return klass;
	}

	const char *namespaces[] = { p_namespace.utf8().get_data(), "Godot", "System", nullptr };
	for (int i = 0; namespaces[i] != nullptr; i++) {
		klass = mono_class_from_name(mono_get_corlib(), namespaces[i], p_class_name.utf8().get_data());
		if (klass) return klass;
	}

	if (p_namespace.is_empty()) {
		klass = mono_class_from_name(mono_get_corlib(), "", p_class_name.utf8().get_data());
	}

	return klass;
}

MonoClass *GDMono::find_class(const String &p_class_name) {
	if (!root_domain) {
		return nullptr;
	}

	MonoClass *klass = nullptr;

	const char *namespaces_to_try[] = { "", "Godot", "TestProject", nullptr };
	for (int i = 0; namespaces_to_try[i] != nullptr; i++) {
		klass = get_class(namespaces_to_try[i], p_class_name);
		if (klass) return klass;
	}

	return nullptr;
}

MonoMethod *GDMono::get_method(MonoClass *p_class, const String &p_name, int p_param_count) {
	return mono_class_get_method_from_name(p_class, p_name.utf8().get_data(), p_param_count);
}