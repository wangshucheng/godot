/**************************************************************************/
/*  mono_host_bridge.h                                                    */
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
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "../static_link/mono_gc_static.h"
#include "../static_link/mono_threads_static.h"

#include <mono/metadata/mono-config.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/mono-runtime.h>
#include <mono/metadata/domain.h>

namespace gdmono {

/**
 * Godot Host Bridge Configuration
 *
 * Configuration structure passed to the embedded Mono runtime
 * to initialize the host environment.
 */
struct GodotHostConfig {
	/** Assembly search paths */
	const char **assembly_search_paths;
	int assembly_search_paths_count;

	/** Configuration directory path */
	const char *config_dir;

	/** Application domain name */
	const char *domain_name;

	/** Whether to enable debug mode */
	bool enable_debug;

	/** Profiler options */
	const char *profiler_options;

	/** GC configuration */
	const char *gc_configuration;

	/** Callback for native assert failures in managed code */
	void (*assertion_callback)(const char *message);
};

/**
 * Host Bridge API for Static Mono Linking
 *
 * This provides a hostfxr-like interface for the statically linked Mono runtime.
 * It handles:
 * - Runtime initialization
 * - Assembly loading
 * - Function pointer resolution (similar to hostfxr's load_assembly_and_get_function_pointer)
 * - Thread management
 * - GC callbacks
 */
namespace HostBridge {

/**
 * Initialize the embedded Mono runtime with Godot host.
 *
 * This is the main entry point for static Mono initialization.
 * It initializes GC, threading, and the JIT compiler.
 *
 * @param config Host configuration (can be nullptr for defaults)
 * @return true if initialization succeeded
 */
bool initialize_host(const GodotHostConfig *config);

/**
 * Check if the host runtime is initialized.
 * @return true if initialized
 */
bool is_host_initialized();

/**
 * Shutdown the embedded Mono runtime.
 * This should be called during Godot shutdown.
 */
void shutdown_host();

/**
 * Load an assembly from a file path.
 *
 * @param assembly_path Full path to the assembly file
 * @return The loaded MonoAssembly, or nullptr on failure
 */
MonoAssembly *load_assembly(const char *assembly_path);

/**
 * Load an assembly from raw bytes.
 *
 * Useful when assemblies are bundled in the binary or loaded from PCK.
 *
 * @param data Pointer to assembly bytes
 * @param size Size of the data
 * @param name Assembly name (for debugging)
 * @param ref_only If true, load for reflection only (not execution)
 * @return The loaded MonoImage, or nullptr on failure
 */
MonoImage *load_assembly_from_data(const uint8_t *data, size_t size,
		const char *name, bool ref_only);

/**
 * Get a function pointer from an assembly.
 *
 * This is the static linking equivalent of hostfxr's
 * load_assembly_and_get_function_pointer.
 *
 * @param assembly_path Path to the assembly
 * @param type_name Full type name (namespace.TypeName)
 * @param method_name Method name (must be annotated with [UnmanagedCallersOnly])
 * @param delegate_type Optional delegate type for the function (can be nullptr)
 * @param reserved Reserved for future use (pass nullptr)
 * @param delegate Pointer to store the function pointer
 * @return 0 on success, error code otherwise
 */
int get_function_pointer(const char *assembly_path,
		const char *type_name,
		const char *method_name,
		const char *delegate_type,
		void *reserved,
		void **delegate);

/**
 * Get a function pointer from a loaded assembly image.
 *
 * @param image The MonoImage to get the function from
 * @param type_name Full type name
 * @param method_name Method name
 * @param delegate_type Optional delegate type
 * @param delegate Pointer to store the function pointer
 * @return 0 on success
 */
int get_function_pointer_from_image(MonoImage *image,
		const char *type_name,
		const char *method_name,
		const char *delegate_type,
		void **delegate);

/**
 * Assembly resolution callback type.
 */
typedef MonoAssembly *(*AssemblyResolveFn)(
		MonoAssemblyName *assembly_name,
		void *user_data);

/**
 * Set a custom assembly resolution callback.
 *
 * This allows Godot to provide assemblies from custom locations
 * (e.g., PCK files, embedded resources).
 *
 * @param callback The resolution callback
 * @param user_data User data passed to the callback
 */
void set_assembly_resolve_callback(AssemblyResolveFn callback, void *user_data);

/**
 * Set the unhandled exception handler callback.
 *
 * @param callback The exception handler callback
 */
void set_unhandled_exception_callback(void (*callback)(void *));

/**
 * Thread attach callback type.
 */
typedef void (*ThreadAttachFn)(MonoThread *thread);
typedef void (*ThreadDetachFn)(MonoThread *thread);

/**
 * Set callbacks for thread lifecycle events.
 *
 * @param on_attach Called when a thread attaches to Mono
 * @param on_detach Called when a thread detaches from Mono
 */
void set_thread_callbacks(ThreadAttachFn on_attach, ThreadDetachFn on_detach);

/**
 * GC collection callback type.
 */
typedef void (*GCCollectFn)(int generation);
typedef int (*GCGetMaxGenerationFn)(void);

/**
 * Set callbacks for GC operations.
 *
 * @param collect Callback to perform GC collection
 * @param get_max_generation Callback to get max GC generation
 */
void set_gc_callbacks(GCCollectFn collect, GCGetMaxGenerationFn get_max_generation);

/**
 * Register all embedded assemblies with Mono.
 *
 * For WASM builds, assemblies can be embedded directly in the binary.
 * This function registers them so Mono can find them.
 *
 * @return true if all assemblies registered successfully
 */
bool register_embedded_assemblies();

/**
 * Get the Mono root domain.
 * @return The root MonoDomain
 */
MonoDomain *get_root_domain();

/**
 * Create a new application domain.
 *
 * @param friendly_name Name for the domain
 * @param configuration_file Optional configuration file
 * @return The new domain, or nullptr on failure
 */
MonoDomain *create_domain(const char *friendly_name, const char *configuration_file);

/**
 * Set the current domain.
 *
 * @param domain The domain to make current
 */
void set_current_domain(MonoDomain *domain);

/**
 * Get a runtime build information string.
 * @return Runtime version or build info
 */
const char *get_runtime_build_info();

/} // namespace HostBridge

} // namespace gdmono
