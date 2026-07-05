#include "mono_aot.h"

#ifdef MONO_AOT_MODE

extern "C" {
extern void *mono_aot_module_mscorlib_info;
extern void *mono_aot_module_System_info;
extern void *mono_aot_module_System_Core_info;
extern void *mono_aot_module_System_Runtime_info;
extern void *mono_aot_module_System_Collections_info;
extern void *mono_aot_module_System_Threading_Tasks_info;
extern void *mono_aot_module_GodotSharp_info;
}

static const AotModuleEntry aot_module_table[] = {
	{"mscorlib", &mono_aot_module_mscorlib_info},
	{"System", &mono_aot_module_System_info},
	{"System.Core", &mono_aot_module_System_Core_info},
	{"System.Runtime", &mono_aot_module_System_Runtime_info},
	{"System.Collections", &mono_aot_module_System_Collections_info},
	{"System.Threading.Tasks", &mono_aot_module_System_Threading_Tasks_info},
	{"GodotSharp", &mono_aot_module_GodotSharp_info},
	{nullptr, nullptr}
};

#else

static const AotModuleEntry aot_module_table[] = {
	{nullptr, nullptr}
};

#endif

const AotModuleEntry *mono_aot_get_module_table() {
	return aot_module_table;
}
