#ifndef MONO_PLATFORM_IOS_H
#define MONO_PLATFORM_IOS_H

#ifdef IOS_ENABLED

#include "core/string/ustring.h"

namespace MonoiOS {
	void initialize();
	void cleanup();

	// 定位 NSBundle 内的程序集路径
	String locate_assembly(const String &p_name);
}

#endif  // IOS_ENABLED

#endif  // MONO_PLATFORM_IOS_H
