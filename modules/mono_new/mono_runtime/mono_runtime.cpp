// M7: 本文件为历史残留空壳。实际的 Mono 运行时初始化/关闭逻辑在 gd_mono.cpp
// (GDMono::initialize / GDMono::cleanup) 中实现。这两个空函数无任何调用方，
// 保留仅为避免 SCsub 通配编译 mono_runtime/*.cpp 时缺少文件的警告。
// 可在下次大版本整理时安全删除本文件及 mono_runtime.h。
#include "mono_runtime.h"

void mono_runtime_init() {
	// 已废弃: 见 GDMono::initialize()
}

void mono_runtime_shutdown() {
	// 已废弃: 见 GDMono::cleanup()
}
