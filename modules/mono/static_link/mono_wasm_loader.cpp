/**************************************************************************/
/*  mono_wasm_loader.cpp                                                  */
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

#include "mono_wasm_loader.h"

#ifdef WEB_ENABLED

#include <emscripten/html5.h>
#include <emscripten/val.h>

#include <mono/metadata/assembly.h>
#include <mono/metadata/image.h>
#include <mono/metadata/loader.h>

#include "core/io/file_access.h"
#include "core/string/print_string.h"

namespace gdmono {

namespace WasmLoader {

namespace {

// Embedded assembly data
struct EmbeddedAssembly {
	String name;
	Vector<uint8_t> data;
};

Vector<EmbeddedAssembly> s_embedded_assemblies;
String s_fallback_url_prefix = "./";
bool s_thread_support_enabled = true;
bool s_initialized = false;

// Mutex for thread-safe assembly loading
emscripten::Val s_assembly_mutex;

} // anonymous namespace

bool initialize() {
	if (s_initialized) {
		return true;
	}

	print_verbose(".NET: Initializing WASM assembly loader...");

	// Install the assembly preload hook for Mono
	mono_install_assembly_preload_hook(assembly_preload_hook, nullptr);

	// Initialize thread support if available
#ifdef USE_PTHREADS
	s_thread_support_enabled = EM_ASM_INT({
		return typeof SharedArrayBuffer !== 'undefined';
	});
#endif

	s_initialized = true;

	print_verbose(".NET: WASM assembly loader initialized");
	return true;
}

void shutdown() {
	if (!s_initialized) {
		return;
	}

	print_verbose(".NET: Shutting down WASM assembly loader...");

	// Clear embedded assemblies
	s_embedded_assemblies.clear();

	s_initialized = false;

	print_verbose(".NET: WASM assembly loader shut down");
}

LoadResult load_assembly_sync(const char *assembly_name) {
	LoadResult result;

	if (!assembly_name || !s_initialized) {
		result.error_message = "Invalid parameters or not initialized";
		return result;
	}

	print_verbose(".NET: Loading assembly synchronously: " + String(assembly_name));

	// First, check embedded assemblies
	for (const EmbeddedAssembly &asm_data : s_embedded_assemblies) {
		if (asm_data.name == assembly_name) {
			// Load from embedded data
			MonoImageOpenStatus status = MONO_IMAGE_OK;

			MonoImage *image = mono_image_open_from_data_full(
					reinterpret_cast<char *>(const_cast<uint8_t *>(asm_data.data.ptr())),
					asm_data.data.size(),
					/* need_copy */ true,
					&status,
					/* ref_only */ false);

			if (status != MONO_IMAGE_OK || !image) {
				result.error_message = "Failed to open assembly image";
				return result;
			}

			// Load the assembly from the image
			MonoAssembly *assembly = mono_assembly_load_from_full(
					image, assembly_name, &status, /* refonly */ false);

			if (status != MONO_IMAGE_OK || !assembly) {
				mono_image_close(image);
				result.error_message = "Failed to load assembly from image";
				return result;
			}

			result.image = mono_assembly_get_image(assembly);
			result.success = true;
			return result;
		}
	}

	// Try to load from fallback URL
	String url = s_fallback_url_prefix + String(assembly_name);

	print_verbose(".NET: Trying to fetch assembly from: " + url);

	// For synchronous loading in WASM, we need to use fetch with emscripten
	// Note: This is a blocking operation which may not work in all browsers
	// Consider using the async version for production

	result.error_message = "Assembly not found in embedded or fallback locations";
	return result;
}

emscripten::Val load_assembly_async(const char *assembly_name, const char *url) {
	// Create a promise that loads the assembly
	emscripten::Val promise = emscripten::val::module_property("fetchAssemblyPromise");

	if (!url) {
		url = s_fallback_url_prefix.c_str();
	}

	String full_url = String(url) + String(assembly_name);

	// Use JavaScript's fetch API to load the assembly
	emscripten::Val fetch_params = emscripten::val::object();
	fetch_params.set("method", "GET");
	fetch_params.set("responseType", "arraybuffer");

	emscripten::Val window = emscripten::val::global("window");
	emscripten::Val fetch_promise = window["fetch"](full_url.utf8().get_data(), fetch_params);

	// Chain then to convert response to ArrayBuffer then to Uint8Array
	emscripten::Val array_buffer_promise = fetch_promise.call<emscripten::Val>("then",
			emscripten::val::module_property("responseToArrayBuffer"));

	return array_buffer_promise;
}

bool register_embedded_assembly(const char *name, const uint8_t *data, size_t size) {
	if (!name || !data || size == 0) {
		return false;
	}

	EmbeddedAssembly asm_data;
	asm_data.name = name;
	asm_data.data.resize(size);
	memcpy(asm_data.data.ptrw(), data, size);

	s_embedded_assemblies.push_back(asm_data);

	print_verbose(".NET: Registered embedded assembly: " + String(name));

	return true;
}

bool is_assembly_available(const char *assembly_name) {
	if (!assembly_name) {
		return false;
	}

	// Check embedded assemblies
	for (const EmbeddedAssembly &asm_data : s_embedded_assemblies) {
		if (asm_data.name == assembly_name) {
			return true;
		}
	}

	// Check if assembly is already loaded by Mono
	MonoAssemblyName aname;
	memset(&aname, 0, sizeof(aname));
	// Note: We would need to set the assembly name fields properly

	return false;
}

int get_embedded_assembly_count() {
	return s_embedded_assemblies.size();
}

const char *get_embedded_assembly_name(int index) {
	if (index < 0 || index >= s_embedded_assemblies.size()) {
		return nullptr;
	}
	return s_embedded_assemblies[index].name.c_str();
}

void set_fallback_url_prefix(const char *url_prefix) {
	if (url_prefix) {
		s_fallback_url_prefix = url_prefix;
	}
}

void set_thread_support_enabled(bool enabled) {
	s_thread_support_enabled = enabled;
}

bool is_thread_support_enabled() {
	return s_thread_support_enabled;
}

MonoAssembly *assembly_preload_hook(MonoAssemblyName *name, char **path, void *user_data) {
	if (!name) {
		return nullptr;
	}

	const char *name_str = mono_assembly_name_get_name(name);
	const char *culture = mono_assembly_name_get_culture(name);

	String assembly_name;
	if (culture && strcmp(culture, "")) {
		assembly_name = culture;
		assembly_name += "/";
	}
	assembly_name += name_str;
	if (!assembly_name.ends_with(".dll")) {
		assembly_name += ".dll";
	}

	print_verbose(".NET: Assembly preload hook called for: " + assembly_name);

	// Try to load from our WASM loader
	LoadResult result = load_assembly_sync(assembly_name.utf8().get_data());

	if (result.success && result.image) {
		// The assembly is already loaded, return it
		return mono_assembly_load_full(result.image, nullptr, nullptr, /* refonly */ false);
	}

	// Return nullptr to let Mono try its default loading mechanism
	return nullptr;
}

} // namespace WasmLoader

} // namespace gdmono

#endif // WEB_ENABLED
