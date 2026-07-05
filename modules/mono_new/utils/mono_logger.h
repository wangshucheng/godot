#ifndef MONO_LOGGER_H
#define MONO_LOGGER_H

#include "core/string/ustring.h"

class MonoLogger {
public:
	static void log(const String &p_message);
	static void log_warning(const String &p_message);
	static void log_error(const String &p_message);
};

#endif // MONO_LOGGER_H
