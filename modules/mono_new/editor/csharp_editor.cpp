#include "../mono_gd/csharp_script.h"
#include "../utils/mono_logger.h"

#ifdef TOOLS_ENABLED

void initialize_csharp_editor() {
	MonoLogger::log("Initializing C# editor integration...");
}

void uninitialize_csharp_editor() {
	MonoLogger::log("Cleaning up C# editor integration...");
}

#endif
