#include "gd_mono.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/os/os.h"
#include "utils/mono_logger.h"
#include "../mono_gd/interop/gd_mono_interop_variant.h"
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
		mono_thread_set_main(mono_thread_current());
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

	mono_glue_init();

	initialized = true;

	if (root_domain && scripts_domain) {
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
	return mono_class_from_name(
			mono_get_corlib(),
			p_namespace.utf8().get_data(),
			p_class_name.utf8().get_data());
}

MonoMethod *GDMono::get_method(MonoClass *p_class, const String &p_name, int p_param_count) {
	return mono_class_get_method_from_name(p_class, p_name.utf8().get_data(), p_param_count);
}
