/**************************************************************************/
/*  mono_wasm_loader.h                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining    */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
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
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#ifdef WEB_ENABLED

#include <emscripten/html5.h>
#include <emscripten/val.h>

#include <mono/metadata/assembly.h>
#include <mono/metadata/image.h>

namespace gdmono {

/**
 * Web/WASM Assembly Loader for Static Mono
 *
 * Handles loading .NET assemblies when running in WASM environment.
 * Supports loading from:
 * - Embedded data sections (for single-file WASM builds)
 * - Fetched .wasm files
 * - PCK-packed assemblies
 */
namespace WasmLoader {

/**
 * Assembly loading result
 */
struct LoadResult {
	bool success = false;
	MonoImage *image = nullptr;
	String error_message;

	LoadResult() = default;
	LoadResult(MonoImage *p_image) :
			success(p_image != nullptr), image(p_image) {}
};

/**
 * Initialize the WASM assembly loader.
 * Sets up the Mono runtime environment for WASM.
 * @return true if initialization succeeded
 */
bool initialize();

/**
 * Shutdown the WASM assembly loader.
 */
void shutdown();

/**
 * Load an assembly synchronously from embedded data.
 *
 * For single-file WASM exports, assemblies are embedded directly
 * in the WASM binary's data section. This function loads them.
 *
 * @param assembly_name Name of the assembly (e.g., "GodotSharp.dll")
 * @return LoadResult with the loaded MonoImage or error
 */
LoadResult load_assembly_sync(const char *assembly_name);

/**
 * Load an assembly asynchronously using WebAssembly promises.
 *
 * This is useful for assemblies stored separately from the main WASM.
 *
 * @param assembly_name Name of the assembly to load
 * @param url URL to fetch the assembly from (if not embedded)
 * @return Promise that resolves to LoadResult
 */
emscripten::Val load_assembly_async(const char *assembly_name, const char *url);

/**
 * Register an embedded assembly with Mono.
 *
 * Assemblies embedded in the WASM binary can be registered here.
 *
 * @param name Assembly name
 * @param data Pointer to assembly data
 * @param size Size of assembly data in bytes
 * @return true if registration succeeded
 */
bool register_embedded_assembly(const char *name, const uint8_t *data, size_t size);

/**
 * Check if an assembly is available (embedded or loaded).
 * @param assembly_name Name of the assembly
 * @return true if the assembly is available
 */
bool is_assembly_available(const char *assembly_name);

/**
 * Get the count of registered embedded assemblies.
 * @return Number of embedded assemblies
 */
int get_embedded_assembly_count();

/**
 * Get the name of an embedded assembly by index.
 * @param index Assembly index
 * @return Assembly name, or nullptr if index out of range
 */
const char *get_embedded_assembly_name(int index);

/**
 * Set the fallback URL prefix for assemblies.
 * When an assembly is not embedded, this URL is used to fetch it.
 * @param url_prefix URL prefix (e.g., "./" or "/wasm/")
 */
void set_fallback_url_prefix(const char *url_prefix);

/**
 * Enable or disable WASM threading support.
 * When enabled, Mono will use SharedArrayBuffer for thread synchronization.
 * @param enabled true to enable threading
 */
void set_thread_support_enabled(bool enabled);

/**
 * Get whether threading is enabled.
 * @return true if threading is enabled
 */
bool is_thread_support_enabled();

/**
 * Assembly preload hook for Mono's assembly loading system.
 * This allows Mono to use our custom assembly loading mechanism.
 */
MonoAssembly *assembly_preload_hook(MonoAssemblyName *name, char **path, void *user_data);

} // namespace WasmLoader

} // namespace gdmono

#endif // WEB_ENABLED
