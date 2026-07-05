#include "mono_logger.h"

#include "core/os/os.h"
#include "core/string/print_string.h"

void MonoLogger::log(const String &p_message) {
	print_line(vformat("[Mono] %s", p_message));
}

void MonoLogger::log_warning(const String &p_message) {
	WARN_PRINT(vformat("[Mono] %s", p_message));
}

void MonoLogger::log_error(const String &p_message) {
	ERR_PRINT(vformat("[Mono] %s", p_message));
}
