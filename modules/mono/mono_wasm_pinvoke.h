// Mono WASM P/Invoke fallback registration (see mono_wasm_pinvoke.cpp).
// Only used on the web platform.

#pragma once

#ifdef WEB_ENABLED

#ifdef __cplusplus
extern "C" {
#endif

// Registers the "System.Native" dl-fallback table with the Mono runtime.
// Must be called before mono_jit_init_version so P/Invoke resolution is
// routed through the fallback from the very first Interop+Sys call.
void mono_wasm_pinvoke_init(void);

#ifdef __cplusplus
}
#endif

#endif // WEB_ENABLED
