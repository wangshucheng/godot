// AOT module registration table.
// This file is hand-maintained (NOT auto-generated) and tracked in git.
//
// Updated 2026-07-17: Hybrid AOT - BCL assemblies use AOT, user assemblies interpreted.
// mscorlib compiled with interp,llvmonly flags (has MONO_AOT_FILE_FLAG_INTERP).
// Binary-patched mscorlib.o: changed fill_number_data import signature from
//   sig=3 ((i32,i32)->void) to sig=6 ((i32,i32)->i32) to fix Mono 6.12 AOT
//   compiler P/Invoke wrapper bug that encoded void return instead of byte*.
// GodotSharp and ProjectScripts run via interpreter to avoid mono_runtime_invoke
//   signature mismatch when invoking AOT-compiled static methods (Runtime.Initialize).
// The 3 facade entries remain nullptr (no separate DLLs in WASM BCL).
//
// Note: When _run_aot_workflow runs (non-PRECOMPILED mode), aot_compile.py may
// generate mono_aot_modules.gen.cpp which overrides this file. That generated
// file is gitignored (*.gen.*). In MONO_AOT_PRECOMPILED=1 mode, this file is
// used directly.
#include "mono_aot.h"

#ifdef MONO_AOT_MODE

extern "C" {
    extern void *mono_aot_module_mscorlib_info;
    extern void *mono_aot_module_System_info;
    extern void *mono_aot_module_System_Core_info;
    // GodotSharp and ProjectScripts .o files are still linked (provide shared
    // aot_wrapper_* symbols) but NOT registered as AOT modules -> interpreted.
}

static const AotModuleEntry aot_module_table[] = {
    {"mscorlib", &mono_aot_module_mscorlib_info},
    {"System", &mono_aot_module_System_info},
    {"System.Core", &mono_aot_module_System_Core_info},
    {"System.Runtime", nullptr},
    {"System.Collections", nullptr},
    {"System.Threading.Tasks", nullptr},
    {"GodotSharp", nullptr},
    {"ProjectScripts", nullptr},
    {nullptr, nullptr}
};

const AotModuleEntry *mono_aot_get_module_table() {
    return aot_module_table;
}

#else

static const AotModuleEntry aot_module_table[] = {
    {nullptr, nullptr}
};

const AotModuleEntry *mono_aot_get_module_table() {
    return aot_module_table;
}

#endif
