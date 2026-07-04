/**************************************************************************/
/*  mono_host_bridge.cpp                                                  */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "mono_host_bridge.h"

#include <mono/metadata/assembly.h>
#include <mono/metadata/mono-config.h>
#include <mono/metadata/reflection.h>
#include <mono/metadata/runtime.h>
#include <mono/metadata/object.h>
#include <mono/metadata/image.h>
#include <mono/metadata/class.h>
#include <mono/metadata/icall.h>

#include "core/os/os.h"
#include "core/string/string.h"

namespace {

// Internal state
bool s_host_initialized = false;
gdmono::HostBridge::AssemblyResolveFn s_assembly_resolve_callback = nullptr;
void *s_assembly_resolve_user_data = nullptr;
void (*s_unhandled_exception_callback)(void *) = nullptr;

// Thread callbacks
gdmono::HostBridge::ThreadAttachFn s_thread_attach_callback = nullptr;
gdmono::HostBridge::ThreadDetachFn s_thread_detach_callback = nullptr;

// GC callbacks
gdmono::HostBridge::GCCollectFn s_gc_collect_callback = nullptr;
gdmono::HostBridge::GCGetMaxGenerationFn s_gc_get_max_generation_callback = nullptr;

} // anonymous namespace

namespace gdmono {

// Forward declarations
extern "C" void mono_static_init(void);
extern "C" void mono_static_cleanup(void);

namespace HostBridge {

bool initialize_host(const GodotHostConfig *config) {
	if (s_host_initialized) {
		print_verbose(".NET: Host bridge already initialized");
		return true;
	}

	print_verbose(".NET: Initializing host bridge for static Mono...");

	// Initialize GC first
	if (!gdmono::gc_init_static()) {
		ERR_PRINT(".NET: Failed to initialize GC");
		return false;
	}

	// Initialize threading
	gdmono::threads_init_static();

	// Set default GC callbacks
	s_gc_collect_callback = gdmono::gc_collect_static;
	s_gc_get_max_generation_callback = gdmono::gc_get_max_generation_static;

	// Set default thread callbacks
	s_thread_attach_callback = nullptr;
	s_thread_detach_callback = nullptr;

	// Initialize Mono runtime
	const char *runtime_version = "v4.0.30319";
	const char *domain_name = config ? config->domain_name : "GodotEngine.RootDomain";

	print_verbose(".NET: Initializing Mono JIT with domain: " + String(domain_name));

	// Initialize the JIT compiler and create the root domain
	// mono_jit_init_version() initializes both the JIT and creates the initial AppDomain
	MonoDomain *domain = mono_jit_init_version(domain_name, runtime_version);

	if (!domain) {
		ERR_PRINT(".NET: Failed to initialize Mono JIT");
		gdmono::threads_cleanup_static();
		gdmono::gc_finalize_static();
		return false;
	}

	print_verbose(".NET: Mono JIT initialized successfully");

	// Set up assembly search paths if provided
	if (config && config->assembly_search_paths && config->assembly_search_paths_count > 0) {
		for (int i = 0; i < config->assembly_search_paths_count; i++) {
			mono_set_assemblies_path(config->assembly_search_paths[i]);
		}
	}

	// Set configuration directory
	if (config && config->config_dir) {
		mono_config_parse(config->config_dir);
	}

	// Install assembly preload hook for custom assembly loading
	if (s_assembly_resolve_callback) {
		mono_install_assembly_load_hook([](MonoAssembly *assembly, void *user_data) {
			// This is called when an assembly is loaded
			// The actual resolution is handled by the resolve callback
		}, nullptr);

		// Note: For full resolution support, we'd need to install a different hook
		// mono_install_assembly_preload_hook() handles pre-loading before the default search
	}

	// Install unhandled exception handler
	if (config && config->assertion_callback) {
		// This would be set via mono_install_unhandled_exception_hook()
	}

	s_host_initialized = true;

	print_verbose(".NET: Host bridge initialized successfully");
	return true;
}

bool is_host_initialized() {
	return s_host_initialized;
}

void shutdown_host() {
	if (!s_host_initialized) {
		return;
	}

	print_verbose(".NET: Shutting down host bridge...");

	// Detach all threads
	// This is critical - all threads must be detached before shutting down

	// Clean up threading
	gdmono::threads_cleanup_static();

	// Finalize GC (this will run finalizers)
	gdmono::gc_finalize_static();

	// Shutdown the Mono runtime
	// mono_jit_cleanup() shuts down the JIT and releases the root domain
	// Note: In static linking, we need to be careful about cleanup order

	s_host_initialized = false;

	print_verbose(".NET: Host bridge shut down");
}

MonoAssembly *load_assembly(const char *assembly_path) {
	if (!assembly_path) {
		return nullptr;
	}

	MonoAssemblyName aname;
	memset(&aname, 0, sizeof(aname));

	// Try to load the assembly using Mono's default search
	MonoImageOpenStatus status = MONO_IMAGE_OK;

	// First, try opening directly from file
	MonoImage *image = mono_image_open(assembly_path, &status);

	if (!image || status != MONO_IMAGE_OK) {
		if (image) {
			mono_image_close(image);
		}
		// Try assembly load with just the name
		return mono_assembly_load(&aname, nullptr, nullptr);
	}

	// Load the assembly from the image
	MonoAssembly *assembly = mono_assembly_load_from_full(image, nullptr, &status, false);

	if (!assembly || status != MONO_IMAGE_OK) {
		if (image) {
			mono_image_close(image);
		}
		return nullptr;
	}

	return assembly;
}

MonoImage *load_assembly_from_data(const uint8_t *data, size_t size,
		const char *name, bool ref_only) {
	if (!data || size == 0) {
		return nullptr;
	}

	MonoImageOpenStatus status = MONO_IMAGE_OK;

	MonoImage *image = mono_image_open_from_data_full(
			reinterpret_cast<char *>(const_cast<uint8_t *>(data)),
			size,
			/* need_copy */ true,
			&status,
			ref_only);

	if (!image || status != MONO_IMAGE_OK) {
		if (image) {
			mono_image_close(image);
		}
		return nullptr;
	}

	// If name is provided, set it
	if (name) {
		// mono_image_set_name(image, name); // Not available in all Mono versions
	}

	return image;
}

int get_function_pointer(const char *assembly_path,
		const char *type_name,
		const char *method_name,
		const char *delegate_type,
		void *reserved,
		void **delegate) {
	if (!assembly_path || !type_name || !method_name || !delegate) {
		return -1;
	}

	*delegate = nullptr;

	// Load the assembly
	MonoAssembly *assembly = load_assembly(assembly_path);
	if (!assembly) {
		ERR_PRINT(String(".NET: Failed to load assembly: ") + assembly_path);
		return -2;
	}

	MonoImage *image = mono_assembly_get_image(assembly);
	if (!image) {
		ERR_PRINT(".NET: Failed to get assembly image");
		return -3;
	}

	return get_function_pointer_from_image(image, type_name, method_name,
			delegate_type, delegate);
}

int get_function_pointer_from_image(MonoImage *image,
		const char *type_name,
		const char *method_name,
		const char *delegate_type,
		void **delegate) {
	if (!image || !type_name || !method_name || !delegate) {
		return -1;
	}

	*delegate = nullptr;

	// Look up the type
	MonoClass *klass = mono_class_from_name(image, "", type_name);
	if (!klass) {
		// Try with global namespace
		klass = mono_class_from_name(image, nullptr, type_name);
	}

	if (!klass) {
		// Try parsing namespace.TypeName format
		String full_name = type_name;
		int last_dot = full_name.rfind(".");
		if (last_dot > 0) {
			String ns = full_name.substr(0, last_dot);
			String class_name = full_name.substr(last_dot + 1);
			klass = mono_class_from_name(image, ns.utf8().get_data(), class_name.utf8().get_data());
		}
	}

	if (!klass) {
		ERR_PRINT(String(".NET: Failed to find type: ") + type_name);
		return -4;
	}

	// Look up the method
	// Method must be marked with [UnmanagedCallersOnly] for this to work
	MonoMethod *method = mono_class_get_method_from_name(klass, method_name, -1);

	if (!method) {
		ERR_PRINT(String(".NET: Failed to find method: ") + method_name + " in type " + type_name);
		return -5;
	}

	// Get the function pointer using mono_runtime_invoke()
	// For [UnmanagedCallersOnly] methods, we need a different approach
	//
	// The proper way is to use mono_runtime_delegate_invoke() but that requires
	// a managed delegate. For [UnmanagedCallersOnly], we need to get the
	// native function pointer directly.
	//
	// In Mono, [UnmanagedCallersOnly] methods can be called via:
	// mono_runtime_invoke() but that wraps in a delegate first.
	// For true function pointer access, we need to compile the method
	// and get its JITted code pointer.

	void *runtime_info = mono_compile_method(method);
	if (!runtime_info) {
		ERR_PRINT(".NET: Failed to compile method");
		return -6;
	}

	*delegate = runtime_info;

	return 0;
}

void set_assembly_resolve_callback(AssemblyResolveFn callback, void *user_data) {
	s_assembly_resolve_callback = callback;
	s_assembly_resolve_user_data = user_data;

	if (callback) {
		mono_install_assembly_load_hook([](MonoAssembly *assembly, void *user_data) {
			// Called when an assembly is being loaded
		}, nullptr);

		mono_install_assembly_preload_hook([](MonoAssemblyName *name, char **path, void *user_data) -> MonoAssembly * {
			if (s_assembly_resolve_callback) {
				return s_assembly_resolve_callback(name, s_assembly_resolve_user_data);
			}
			return nullptr;
		}, nullptr);
	}
}

void set_unhandled_exception_callback(void (*callback)(void *)) {
	s_unhandled_exception_callback = callback;

	if (callback) {
		mono_install_unhandled_exception_hook([](void *exception) {
			if (s_unhandled_exception_callback) {
				s_unhandled_exception_callback(exception);
			}
		});
	}
}

void set_thread_callbacks(ThreadAttachFn on_attach, ThreadDetachFn on_detach) {
	s_thread_attach_callback = on_attach;
	s_thread_detach_callback = on_detach;

	// Set up the threading callbacks
	gdmono::set_thread_callbacks_static(
			[](MonoThread *thread) {
				if (s_thread_attach_callback) {
					s_thread_attach_callback(thread);
				}
			},
			[](MonoThread *thread) {
				if (s_thread_detach_callback) {
					s_thread_detach_callback(thread);
				}
			});
}

void set_gc_callbacks(GCCollectFn collect, GCGetMaxGenerationFn get_max_generation) {
	s_gc_collect_callback = collect;
	s_gc_get_max_generation_callback = get_max_generation;
}

bool register_embedded_assemblies() {
	// This function is intended for WASM builds where assemblies
	// are embedded in the binary
	// The actual implementation depends on how assemblies are bundled
#ifdef WEB_ENABLED
	// For WASM, we'd register the embedded assembly data here
	// mono_wasm_register_embedded_assemblies();
#endif
	return true;
}

MonoDomain *get_root_domain() {
	return mono_get_root_domain();
}

MonoDomain *create_domain(const char *friendly_name, const char *configuration_file) {
	if (!s_host_initialized) {
		return nullptr;
	}

	return mono_domain_create_appdomain(
			const_cast<char *>(friendly_name),
			const_cast<char *>(configuration_file));
}

void set_current_domain(MonoDomain *domain) {
	if (!domain) {
		return;
	}

	mono_domain_set(domain, true);
}

const char *get_runtime_build_info() {
	return mono_get_runtime_build_info();
}

} // namespace HostBridge

} // namespace gdmono
