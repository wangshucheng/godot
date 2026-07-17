#pragma once

#include "core/string/ustring.h"
#include "core/error/error_list.h"

#include <mono/metadata/object.h>
#include <mono/metadata/environment.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/class.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/jit/jit.h>

class MonoHost {
public:
	MonoHost();
	~MonoHost();

	Error initialize();
	void shutdown();
	void cleanup_partial_init(); // M2: cleanup on partial init failure

	bool load_assembly_and_run(const String &p_assembly_path);
	MonoAssembly *load_assembly(const String &p_path);
	bool load_godotsharp();

	void pump_sync_context();

	// Called from C# via icall when GodotSynchronizationContext is installed.
	// This registers the instance so pump_sync_context() can invoke
	// PumpInstance() as an instance method (avoiding static method dispatch
	// which triggers WASM interpreter signature mismatch).
	void register_sync_context(MonoObject *p_instance);

	static MonoHost *get_singleton() { return singleton; }

	MonoDomain *get_domain() const { return domain; }
	MonoAssembly *get_godotsharp_assembly() const { return godotsharp_assembly; }

private:
	bool is_initialized = false;
	MonoDomain *domain = nullptr;
	MonoAssembly *corlib_assembly = nullptr;
	MonoAssembly *godotsharp_assembly = nullptr;
	// Instance-based sync context pumping: avoids mono_runtime_invoke on
	// static methods, which triggers signature mismatch in WASM interpreter.
	MonoMethod *sync_context_pump_method = nullptr; // PumpInstance (instance method)
	MonoObject *sync_context_instance = nullptr;     // GodotSynchronizationContext._instance
	uint32_t sync_context_gchandle = 0;              // Strong GCHandle pinning sync_context_instance (prevents GC)
	bool sync_context_lazy_attempted = false;         // Avoid repeated lazy cache attempts

	static MonoHost *singleton;

	bool load_corlib();
	bool register_internal_calls();
	void cache_sync_context_method();
};
