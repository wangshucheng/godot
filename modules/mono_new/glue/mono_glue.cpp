#include "mono_glue.h"

// This file previously contained stub icall implementations that shadowed the
// real icalls registered in gd_mono_interop_variant.cpp and gd_mono_callable.cpp.
// The stubs and mono_glue_init()/mono_glue_register_icalls() have been removed
// to prevent accidental override of production icalls.
//
// Real icall registration happens in:
//   - GDMonoInterop::variant_register_icalls() (gd_mono_interop_variant.cpp)
//   - GDMonoCallable::register_icalls() (gd_mono_callable.cpp)
//   - GDSignalAwaiter::register_icalls() (signal_awaiter_utils.cpp)
//   - GDMonoInterop::register_node_icalls() / register_node2d_icalls() (glue_cpp/)
//
// This file is intentionally empty; the SCsub glob (glue/*.cpp) still compiles it
// but there are no symbols defined here.
