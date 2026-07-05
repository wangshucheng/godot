#ifndef MONO_AOT_H
#define MONO_AOT_H

#include "core/typedefs.h"

struct AotModuleEntry {
	const char *name;
	void **info;
};

void mono_aot_init();
void mono_aot_register_modules();
void mono_aot_shutdown();

const AotModuleEntry *mono_aot_get_module_table();

#endif // MONO_AOT_H
