#ifndef MONO_PLATFORM_ANDROID_H
#define MONO_PLATFORM_ANDROID_H

#ifdef ANDROID_ENABLED

#include "core/string/ustring.h"

namespace MonoAndroid {
	void initialize();
	void cleanup();

	// 定位 APK 内的程序集路径（res://.godot/mono/publish/<arch>/<name>）
	String locate_assembly(const String &p_name);
}

#endif  // ANDROID_ENABLED

#endif  // MONO_PLATFORM_ANDROID_H
