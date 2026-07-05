#ifndef MONO_PLATFORM_WEB_H
#define MONO_PLATFORM_WEB_H

#ifdef WEB_ENABLED

#include "core/string/ustring.h"

namespace MonoWeb {
	void initialize();
	void cleanup();

	String locate_assembly(const String &p_name);
	void* wasm_malloc(int p_size);
	void wasm_free(void *p_ptr);

	void register_bcallbacks();
}

#endif

#endif // MONO_PLATFORM_WEB_H
