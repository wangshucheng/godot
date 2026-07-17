#include "csharp_debugger.h"

#include "../utils/mono_logger.h"

#include "core/error/error_macros.h"
#include "core/os/mutex.h"
#include "core/string/ustring.h"
#include "core/string/print_string.h"
#include "core/templates/list.h"
#include "core/templates/local_vector.h"
#include "core/variant/variant.h" // for vformat

#include <mono/mono-publib.h>

#include <cstdlib>
#include <cstring>

// Forward-declare Mono API functions from headers we cannot include directly
// (<mono/metadata/object.h>, <mono/metadata/exception.h>, <mono/jit/jit.h>)
// because they have missing transitive includes in our minimal Mono header set.
// These symbols are all exported from libmonosgen-2.0.a.
// Note: mono_string_to_utf8 and mono_free are already declared in
// <mono/mono-publib.h> (included via csharp_debugger.h), so we don't redeclare them.
extern "C" {
MonoString *mono_object_to_string(MonoObject *obj, MonoObject **exc);
void mono_install_unhandled_exception_hook(void (*hook)(MonoObject *exc, void *user_data), void *user_data);
mono_bool mono_is_debugger_attached(void);
void mono_jit_parse_options(int argc, char **argv);
}

namespace CSharpDebugger {

// ---------------------------------------------------------------------------
// Module state (file-static globals)
// ---------------------------------------------------------------------------

static int s_requested_port = 0; // 0 = not requested
static ExceptionInfo s_last_exception;
// Guards s_last_exception: the unhandled exception hook may fire on any Mono
// internal thread (finalizer, timer, etc.), while debug_get_* hooks read it
// from the main thread. Without this lock, Vector<StackFrame> replacement
// could race with reader iteration and cause use-after-free.
static Mutex s_exception_mutex;
// Ordering guard: configure_before_jit_init() must run BEFORE
// mono_jit_init_version(). Once mark_jit_initialized() is called, any later
// attempt to configure the SDB agent is rejected to avoid undefined behavior
// inside Mono (mono_jit_parse_options after JIT init typically aborts).
static bool s_jit_init_done = false;

// ---------------------------------------------------------------------------
// Command-line parsing
// ---------------------------------------------------------------------------

void parse_command_line(const List<String> &p_args) {
	s_requested_port = 0;

	// Parse --mono-debugger=PORT or --mono-debugger (uses default port).
	// We only support the command-line form (not project.godot) because
	// the SDB agent must be configured before mono_jit_init_version(), which
	// runs very early — before project.godot is reliably loaded in all paths.
	for (const String &arg : p_args) {
		if (arg == "--mono-debugger") {
			s_requested_port = DEFAULT_DEBUGGER_PORT;
		} else if (arg.begins_with("--mono-debugger=")) {
			String port_str = arg.substr(strlen("--mono-debugger="));
			int port = port_str.to_int();
			if (port > 0 && port <= 65535) {
				s_requested_port = port;
			} else {
				MonoLogger::log_warning(vformat("Invalid --mono-debugger port '%s', using default %d", port_str, DEFAULT_DEBUGGER_PORT));
				s_requested_port = DEFAULT_DEBUGGER_PORT;
			}
		}
	}
}

// ---------------------------------------------------------------------------
// SDB agent configuration (before JIT init)
// ---------------------------------------------------------------------------

void configure_before_jit_init() {
#ifdef WEB_ENABLED
	if (s_requested_port != 0) {
		MonoLogger::log_warning("Mono SDB debugger requested but not supported in WASM (interpreter mode)");
	}
	return;
#else
	// Ordering guard: mono_jit_parse_options() must be called BEFORE
	// mono_jit_init_version(). Calling it after JIT init is undefined
	// behavior (typically aborts the process).
	ERR_FAIL_COND_MSG(s_jit_init_done,
			"CSharpDebugger::configure_before_jit_init() must be called BEFORE "
			"mono_jit_init_version(). Call it from GDMono::initialize() before "
			"the JIT init call, and call mark_jit_initialized() right after.");

	if (s_requested_port == 0) {
		return;
	}

	// Mono 6.12 does NOT accept --debugger-agent via the MONO_DEBUG env var.
	// The SDB agent must be configured via mono_jit_parse_options(), which
	// parses the same command-line syntax that the `mono` launcher uses.
	// Format: --debugger-agent=transport=dt_socket,address=127.0.0.1:PORT,server=y,suspend=n
	// - server=y: engine listens, IDE connects (attach mode)
	// - suspend=n: don't pause on startup, run immediately
	String agent_option = vformat(
			"--debugger-agent=transport=dt_socket,address=127.0.0.1:%d,server=y,suspend=n",
			s_requested_port);

	CharString opt_utf8 = agent_option.utf8();
	// Copy the option into a writable buffer we own. mono_jit_parse_options
	// takes char **argv (non-const), and Mono may modify the string in place
	// (e.g. write '\0' delimiters). Using CharString::ptrw() directly risks
	// corrupting CharString's internal state. The buffer is null-terminated
	// by CharString contract (size() excludes the terminator).
	LocalVector<char> opt_buf;
	opt_buf.resize(opt_utf8.size() + 1);
	memcpy(opt_buf.ptr(), opt_utf8.get_data(), opt_utf8.size() + 1);
	char *argv[1] = { opt_buf.ptr() };
	mono_jit_parse_options(1, argv);

	MonoLogger::log(vformat("Mono SDB agent configured on port %d (attach IDE to 127.0.0.1:%d)", s_requested_port, s_requested_port));
#endif
}

void mark_jit_initialized() {
	s_jit_init_done = true;
}

// ---------------------------------------------------------------------------
// Unhandled exception hook
// ---------------------------------------------------------------------------

// Parse a .NET-formatted stack trace into StackFrame entries.
// Expected line format (Mono with debug symbols):
//   "  at Godot.TestScript._Process (System.Double delta) [0x0001c] in /path/TestScript.cs:42"
// Without debug symbols:
//   "  at Godot.TestScript._Process (System.Double delta) [0x0001c]"
static Vector<StackFrame> parse_stack_trace(const String &p_trace) {
	Vector<StackFrame> frames;
	if (p_trace.is_empty()) {
		return frames;
	}

	// Split by newlines; skip the first line (the exception type + message).
	Vector<String> lines = p_trace.split("\n", false);
	for (int i = 0; i < lines.size() && frames.size() < MAX_FRAMES; i++) {
		String line = lines[i].strip_edges();
		if (!line.begins_with("at ")) {
			continue;
		}

		StackFrame frame;

		// Mono frame format:
		//   at <function> [0xIL_OFFSET] in <source>:<line>
		// or:
		//   at <function> [0xIL_OFFSET]
		// We parse from the end backwards for the "in <file>:<line>" suffix.

		// Strip the leading "at ".
		String rest = line.substr(3); // after "at "

		// Look for " in " separator (between function and source location).
		int in_idx = rest.find(" in ");
		if (in_idx >= 0) {
			frame.function = rest.substr(0, in_idx).strip_edges();
			String location = rest.substr(in_idx + 4).strip_edges();

			// location is "<path>:<line>" — split at the LAST colon.
			int last_colon = location.rfind(":");
			if (last_colon >= 0) {
				frame.source = location.substr(0, last_colon).strip_edges();
				String line_str = location.substr(last_colon + 1).strip_edges();
				frame.line = line_str.to_int();
			} else {
				// No line number; keep entire location as source.
				frame.source = location;
				frame.line = 0;
			}
		} else {
			// No " in <file>:line" segment — debug symbols absent.
			// Strip the trailing "[0x...]" IL offset marker if present.
			int bracket_idx = rest.rfind(" [");
			if (bracket_idx >= 0) {
				frame.function = rest.substr(0, bracket_idx).strip_edges();
			} else {
				frame.function = rest;
			}
			frame.source = "";
			frame.line = 0;
		}

		frames.push_back(frame);
	}
	return frames;
}

static void on_unhandled_exception(MonoObject *p_exc, void *p_user_data) {
	(void)p_user_data;
	if (!p_exc) {
		MonoLogger::log_error("Unhandled C# exception: (null exception object)");
		return;
	}

	// Convert the exception to its .NET string representation, which includes
	// the type name, message, and formatted StackTrace.
	MonoObject *to_string_exc = nullptr;
	MonoString *exc_str = mono_object_to_string(p_exc, &to_string_exc);
	if (to_string_exc) {
		MonoLogger::log_error("Unhandled C# exception: (exception.ToString() threw)");
		return;
	}
	if (!exc_str) {
		MonoLogger::log_error("Unhandled C# exception: (ToString returned null)");
		return;
	}

	char *utf8 = mono_string_to_utf8(exc_str);
	if (!utf8) {
		MonoLogger::log_error("Unhandled C# exception: (UTF-8 conversion failed)");
		return;
	}

	String formatted = String::utf8(utf8);
	mono_free(utf8);

	// Split off the first line as the message; the rest is the stack trace.
	Vector<String> lines = formatted.split("\n", false);
	String message;
	Vector<StackFrame> frames;

	if (lines.size() > 0) {
		message = lines[0].strip_edges();
	}
	if (lines.size() > 1) {
		// Rejoin lines 1..N and parse as a stack trace.
		String trace;
		for (int i = 1; i < lines.size(); i++) {
			if (i > 1) trace += "\n";
			trace += lines[i];
		}
		frames = parse_stack_trace(trace);
	}

	// Store for the Godot debugger panel to query via debug_get_* hooks.
	// Lock: the hook may fire on any Mono internal thread (finalizer, timer),
	// while debug_get_* hooks read s_last_exception from the main thread.
	{
		MutexLock lock(s_exception_mutex);
		s_last_exception.message = message;
		s_last_exception.frames = frames;
		s_last_exception.valid = true;
	}

	MonoLogger::log_error("Unhandled C# exception:\n" + formatted);
}

void install_exception_hook() {
#ifdef WEB_ENABLED
	// SDB not supported in interpreter mode; skip hook installation to avoid
	// confusing the runtime. Unhandled exceptions still print via Mono's
	// default handler.
	return;
#else
	mono_install_unhandled_exception_hook(on_unhandled_exception, nullptr);
	MonoLogger::log("Mono unhandled exception hook installed");
#endif
}

// ---------------------------------------------------------------------------
// State queries
// ---------------------------------------------------------------------------

bool is_requested() {
	return s_requested_port != 0;
}

int get_requested_port() {
	return s_requested_port;
}

bool is_attached() {
#ifdef WEB_ENABLED
	return false;
#else
	return mono_is_debugger_attached() != 0;
#endif
}

// ---------------------------------------------------------------------------
// Exception info access
// ---------------------------------------------------------------------------

ExceptionInfo get_last_exception() {
	// Return a deep copy under the lock. The hook may overwrite s_last_exception
	// while the caller is still inspecting the returned value; a copy makes the
	// caller independent of any later capture or clear_last_exception() call.
	MutexLock lock(s_exception_mutex);
	return s_last_exception;
}

void clear_last_exception() {
	MutexLock lock(s_exception_mutex);
	s_last_exception.valid = false;
	s_last_exception.message = String();
	s_last_exception.frames.clear();
}

} // namespace CSharpDebugger
