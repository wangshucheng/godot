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

	bool load_assembly_and_run(const String &p_assembly_path);
	MonoAssembly *load_assembly(const String &p_path);
	bool load_godotsharp();

	void pump_sync_context();

	static MonoHost *get_singleton() { return singleton; }

	MonoDomain *get_domain() const { return domain; }
	MonoAssembly *get_godotsharp_assembly() const { return godotsharp_assembly; }

private:
	bool is_initialized = false;
	MonoDomain *domain = nullptr;
	MonoAssembly *corlib_assembly = nullptr;
	MonoAssembly *godotsharp_assembly = nullptr;
	MonoMethod *sync_context_pump_method = nullptr;

	static MonoHost *singleton;

	bool load_corlib();
	bool register_internal_calls();
	void cache_sync_context_method();
};
