// mono_static_compat.c
//
// Bridge between the Mono static library (libmono-static-sgen.lib) and the
// static UCRT (libucrt.lib).
//
// The Mono static lib was compiled with /MD (dynamic CRT) and uses
// __declspec(dllimport) for CRT functions. This causes the compiler to
// generate indirect calls through __imp_ prefixed function pointer variables.
// When linking with the static UCRT, these __imp_ variables don't exist -
// the functions are directly available as _lseek, _read, etc.
//
// This file defines each __imp_ variable as a global function pointer
// initialized to the address of the corresponding CRT function. The linker
// resolves the Mono lib's references to __imp_xxx using these definitions.
//
// Note: /ALTERNATENAME linker directive does NOT work for __imp_ prefixed
// symbols because MSVC's linker treats them as import thunks, not regular
// symbols. Defining them as actual variables is the only reliable approach.

#ifdef _MSC_VER

#include <io.h>
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <malloc.h>
#include <math.h>
#include <sys/utime.h>
#include <sys/timeb.h>
#include <process.h>
#include <time.h>
#include <wchar.h>
#include <string.h>

// Each __imp_xxx is a function pointer that the Mono static lib calls
// indirectly. We define it as a void* initialized to the CRT function address.
// On x64, function pointers and data pointers are both 8 bytes, so this is safe.

// POSIX file I/O functions
void* __imp_lseek       = (void*)_lseek;
void* __imp_read        = (void*)_read;
void* __imp_close       = (void*)_close;
void* __imp_open        = (void*)_open;
void* __imp_getcwd      = (void*)_getcwd;
void* __imp_unlink      = (void*)_unlink;
void* __imp_rename      = (void*)rename;
void* __imp_fdopen      = (void*)_fdopen;
void* __imp__fdopen     = (void*)_fdopen;   // oldnames.lib double-decorated variant

// CRT functions
void* __imp__ecvt_s     = (void*)_ecvt_s;
void* __imp__access     = (void*)_access;
void* __imp__wmkdir     = (void*)_wmkdir;
void* __imp__wmktemp    = (void*)_wmktemp;
void* __imp__localtime64 = (void*)_localtime64;
void* __imp__ftime64    = (void*)_ftime64;
void* __imp__utime64    = (void*)_utime64;
void* __imp_system      = (void*)system;
void* __imp__wsystem    = (void*)_wsystem;
void* __imp__getch      = (void*)_getch;
void* __imp__resetstkoflw = (void*)_resetstkoflw;

// String functions (referenced by sgen-bridge.obj / sgen-new-bridge.obj and
// oldnames.lib(strdup.obi) — without these the final link fails with
// LNK2019 unresolved __imp_strdup).
void* __imp_strdup      = (void*)_strdup;
void* __imp__strdup     = (void*)_strdup;   // oldnames.lib double-decorated variant

// C99 math functions (float versions may be missing from static UCRT)
// For doubles that exist in static UCRT, the __imp_ thunk still needs defining.
void* __imp_cbrt        = (void*)cbrt;
void* __imp_acoshf      = (void*)acoshf;
void* __imp_asinhf      = (void*)asinhf;
void* __imp_atanhf      = (void*)atanhf;
void* __imp_modff       = (void*)modff;

#endif // _MSC_VER
