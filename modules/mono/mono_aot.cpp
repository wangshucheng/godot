#include "mono_aot.h"
#include <mono/metadata/assembly.h>
#include <cstdio>

extern "C" {
	void mono_aot_register_module(void *aot_info);
}

static int aot_registered_count = 0;

void mono_aot_register_modules() {
	const AotModuleEntry *table = mono_aot_get_module_table();
	if (!table || !table->name) {
		printf("[Mono] AOT: No AOT modules registered (JIT mode or empty AOT table).\n");
		return;
	}

	printf("[Mono] AOT: Registering Full AOT modules...\n");
	aot_registered_count = 0;
	for (const AotModuleEntry *entry = table; entry->name != nullptr; ++entry) {
		if (entry->info != nullptr && *entry->info != nullptr) {
			mono_aot_register_module(*entry->info);
			aot_registered_count++;
			printf("[Mono] AOT:   Registered: %s\n", entry->name);
		} else {
			printf("[Mono] AOT:   WARNING: Module '%s' info is NULL (not linked or not AOT-compiled).\n", entry->name);
		}
	}
	printf("[Mono] AOT: %d modules registered.\n", aot_registered_count);
}

void mono_aot_init() {
#ifdef MONO_AOT_MODE
	printf("[Mono] AOT: Full AOT runtime initialized (no JIT compiler).\n");
#else
	printf("[Mono] AOT: JIT runtime initialized (AOT support available for hybrid mode).\n");
#endif
}

void mono_aot_shutdown() {
#ifdef MONO_AOT_MODE
	printf("[Mono] AOT: Full AOT runtime shutdown.\n");
#else
	printf("[Mono] AOT: JIT runtime shutdown.\n");
#endif
}
