#include "mono_icalls.h"
#include "core/os/os.h"

#include <cstdio>
#include <cstdlib>

static void godot_icall_Console_WriteLine_raw(MonoString *message) {
	if (message == nullptr) {
		printf("\n");
		fflush(stdout);
		return;
	}

	char *utf8_str = mono_string_to_utf8(message);
	if (utf8_str != nullptr) {
		printf("%s\n", utf8_str);
		mono_free(utf8_str);
	} else {
		printf("\n");
	}
	fflush(stdout);
}

void godot_register_icalls() {
	mono_add_internal_call(
			"HelloWorld.ConsoleBridge::godot_icall_Console_WriteLine",
			(const void *)godot_icall_Console_WriteLine_raw);

	printf("[Mono] Registered internal calls.\n");
}
