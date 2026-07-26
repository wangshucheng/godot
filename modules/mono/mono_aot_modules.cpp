// AOT module registration table.
// This file is hand-maintained (NOT auto-generated) and tracked in git.
//
// Updated 2026-07-26 (H9 fix): Removed nullptr entries for facade/user assemblies.
// Previous table had nullptr entries for System.Runtime/System.Collections/
// System.Threading.Tasks/GodotSharp/ProjectScripts. In MONO_AOT_MODE_INTERP_LLVMONLY
// mode, Mono's module loader treats a nullptr info pointer as "module known but
// not AOT-compiled" and aborts with "Failed to load AOT module ... dependency
// cannot be found" when a registered AOT module (e.g. System) depends on one
// of these nullptr entries.
//
// Fix: Only register modules that actually have AOT info. Modules NOT in the
// table are loaded from MEMFS via the interpreter (Hybrid AOT fallback), which
// is the intended behavior for facades and user assemblies.
//
// History:
//   2026-07-17: Hybrid AOT - BCL assemblies use AOT, user assemblies interpreted.
//   mscorlib compiled with interp,llvmonly flags (has MONO_AOT_FILE_FLAG_INTERP).
//   Binary-patched mscorlib.o: changed fill_number_data import signature from
//     sig=3 ((i32,i32)->void) to sig=6 ((i32,i32)->i32) to fix Mono 6.12 AOT
//     compiler P/Invoke wrapper bug that encoded void return instead of byte*.
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
    // Facade DLLs (System.Runtime etc.) are loaded from MEMFS via interpreter.
}

// Only register modules with valid AOT info. nullptr entries cause Mono to
// abort in INTERP_LLVMONLY mode when a registered module's dependency is nullptr.
// Modules absent from this table fall back to MEMFS + interpreter (Hybrid AOT).
static const AotModuleEntry aot_module_table[] = {
    {"mscorlib", &mono_aot_module_mscorlib_info},
    {"System", &mono_aot_module_System_info},
    {"System.Core", &mono_aot_module_System_Core_info},
    // System.Runtime / System.Collections / System.Threading.Tasks:
    //   Loaded from MEMFS Facades/ dir (embedded by SCsub _embed_bcl_assemblies).
    // GodotSharp / ProjectScripts:
    //   Loaded from MEMFS (interpreter fallback, avoids mono_runtime_invoke
    //   signature mismatch on AOT-compiled static methods).
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
