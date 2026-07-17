#ifndef CSHARP_DEBUGGER_H
#define CSHARP_DEBUGGER_H

#include "core/string/ustring.h"
#include "core/templates/list.h"
#include "core/templates/vector.h"

// Note: We intentionally do NOT include <mono/mono-publib.h> here because
// other translation units (e.g. csharp_script.cpp) declare Mono API functions
// via their own extern "C" blocks with slightly different signatures (e.g.
// mono_gchandle_new returns uint32_t instead of mono_gchandle). Including
// mono-publib.h here would trigger redefinition errors in those TUs.
// Instead, we use C++ forward declarations matching Mono's typedef pattern.
// In mono-publib.h:  typedef struct _MonoObject MonoObject;
//                    typedef MonoObject MonoString;  (etc.)
typedef struct _MonoObject MonoObject;
typedef MonoObject MonoString;

// CSharpDebugger: encapsulates the Mono Soft Debugger (SDB) agent configuration
// and the unhandled exception capture used to populate the Godot debugger
// panel's call stack view.
//
// Lifecycle:
//   1. parse_command_line()   — called at CORE initialization level (very early)
//   2. configure_before_jit_init() — called from GDMono::initialize() before
//      mono_jit_init_version(); sets the MONO_DEBUG env var so the SDB agent
//      starts listening when the JIT is initialized.
//   3. install_exception_hook() — called after mono_jit_init_version() succeeds;
//      registers on_unhandled_exception() as the Mono unhandled exception hook.
//
// Why command-line + restart (no hot-swap):
//   Mono SDB agent must be configured BEFORE mono_jit_init_version(). After
//   JIT init the agent cannot be toggled. Toggling the debugger at runtime
//   therefore requires restarting the engine.
//
// WebAssembly note:
//   Mono interpreter mode (MONO_EE_MODE_INTERP) does not support SDB. All
//   methods are safe to call from WASM but the debugger will never activate.
namespace CSharpDebugger {

// Default port for the SDB agent if --mono-debugger is passed without a value.
constexpr int DEFAULT_DEBUGGER_PORT = 56000;

// --- Stack frame / exception info -------------------------------------------

struct StackFrame {
	String function; // e.g. "Godot.TestScript._Process (System.Double delta)"
	String source;   // e.g. "/path/TestScript.cs" (empty if no debug symbols)
	int line = 0;    // 1-based; 0 if unknown
};

struct ExceptionInfo {
	String message;        // .NET formatted "Type: Message"
	Vector<StackFrame> frames;
	bool valid = false;    // true if an unhandled exception has been captured
};

// --- Lifecycle (called from register_types.cpp + gd_mono.cpp) ---------------

// Parse --mono-debugger=PORT (or --mono-debugger, which uses default port)
// from the engine command-line arguments. Stores the requested port in an
// internal global; does not touch env vars or Mono state.
void parse_command_line(const List<String> &p_args);

// Set the MONO_DEBUG env var so the SDB agent starts listening when
// mono_jit_init_version() is called. No-op if no port was requested or
// on WASM (interpreter mode doesn't support SDB).
void configure_before_jit_init();

// Register on_unhandled_exception() as the Mono unhandled exception hook.
// Should be called after mono_jit_init_version() succeeds.
void install_exception_hook();

// --- State queries (for UI + debug hooks) -----------------------------------

// True if --mono-debugger was passed on the command line.
bool is_requested();

// Returns the port the SDB agent is configured on, or 0 if not requested.
int get_requested_port();

// True if a debugger frontend (e.g. VS Code) is currently attached via SDB.
// Always false on WASM.
bool is_attached();

// --- Exception info (consumed by CSharpLanguage::debug_get_* hooks) ---------

const ExceptionInfo &get_last_exception();
void clear_last_exception();

} // namespace CSharpDebugger

#endif // CSHARP_DEBUGGER_H
