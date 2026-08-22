// Mono WASM P/Invoke fallback for the corefx System.Native layer.
//
// [GodotExt] 2026-08-20 fix (root cause of WASM "cant resolve internal call"):
//   The Mono 6.12 wasm BCL (mcs/class/lib/wasm) compiles corlib directly from
//   external/corefx FileSystem.Unix.cs, so System.IO.File.Exists (and friends)
//   P/Invoke "System.Native" → Interop+Sys → SystemNative_* C functions.
//   Those live in the corefx PAL sources which were never built for our WASM
//   static libs (configure has no wasm MONO_NATIVE_PLATFORM), so every
//   Sys..cctor call died with DllNotFoundException("System.Native").
//
// Fix follows the official mono-wasm approach (sdks/wasm/src/driver.c +
// pinvoke-tables-default.h): build the corefx PAL objects (tools/
// build_mono_native_wasm.ps1 → mono/libs/web/wasm/libmono-native.a) and
// register a dl-fallback that resolves "System.Native" through an explicit
// name→function table. mono_dl_fallback_register makes Mono route P/Invoke
// resolution through wasm_dl_load/wasm_dl_symbol before failing.
//
// The 27-entry table mirrors sdks/wasm/src/pinvoke-tables-default.h (classic
// wasm profile — the same BCL we embed).

#include "mono_wasm_pinvoke.h"

#include <cstdint>
#include <string.h>

extern "C" {

// === corefx PAL exports (libmono-native.a, built from external/corefx) ===
// 声明逐字对照官方 sdks/wasm/src/pininvoke-tables-default.h：WASM 间接调用
// 按签名匹配（function signature mismatch 会 trap），表内声明即真实调用
// 约定（指针/结构体在 wasm32 全部坍缩为 i32，UTime/UTimes 的 byval 结构体
// 以两个 i32 传入 —— 保持官方原样，勿"修正"）。
int SystemNative_ConvertErrorPlatformToPal(int);
int SystemNative_ConvertErrorPalToPlatform(int);
int SystemNative_StrErrorR(int, int, int);
void SystemNative_GetNonCryptographicallySecureRandomBytes(int, int);
int SystemNative_OpenDir(int);
int SystemNative_GetReadDirRBufferSize();
int SystemNative_ReadDirR(int, int, int, int);
int SystemNative_CloseDir(int);
int SystemNative_ReadLink(int, int, int);
int SystemNative_FStat2(int, int);
int SystemNative_Stat2(int, int);
int SystemNative_LStat2(int, int);
int SystemNative_Symlink(int, int);
int SystemNative_ChMod(int, int);
int SystemNative_CopyFile(int, int);
int SystemNative_GetEGid();
int SystemNative_GetEUid();
int SystemNative_LChflags(int, int);
int SystemNative_LChflagsCanSetHiddenFlag();
int SystemNative_Link(int, int);
int SystemNative_MkDir(int, int);
int SystemNative_Rename(int, int);
int SystemNative_RmDir(int);
int SystemNative_UTime(int, int);
int SystemNative_UTimes(int, int);
int SystemNative_Unlink(int);

// System.Core.dll 引用（UnixDomainSocketEndPoint）。emscripten musl 提供
// sockaddr_un，直接真实现（官方该函数在 pal_networking.c，属 netcore 表）。
#include <sys/un.h>
#include <stddef.h>
void SystemNative_GetDomainSocketSizes(int32_t *pathOffset, int32_t *pathSize, int32_t *addressSize) {
	struct sockaddr_un domainSocket;
	*pathOffset = (int32_t)offsetof(struct sockaddr_un, sun_path);
	*pathSize = (int32_t)sizeof(domainSocket.sun_path);
	*addressSize = (int32_t)sizeof(domainSocket);
}

// === name→function table (official pinvoke-tables-default.h ordering) ===
typedef struct {
	const char *name;
	void *func;
} PinvokeImport;

static PinvokeImport System_Native_imports[] = {
	{ "SystemNative_ConvertErrorPlatformToPal", (void *)SystemNative_ConvertErrorPlatformToPal },
	{ "SystemNative_ConvertErrorPalToPlatform", (void *)SystemNative_ConvertErrorPalToPlatform },
	{ "SystemNative_StrErrorR", (void *)SystemNative_StrErrorR },
	{ "SystemNative_GetNonCryptographicallySecureRandomBytes", (void *)SystemNative_GetNonCryptographicallySecureRandomBytes },
	{ "SystemNative_OpenDir", (void *)SystemNative_OpenDir },
	{ "SystemNative_GetReadDirRBufferSize", (void *)SystemNative_GetReadDirRBufferSize },
	{ "SystemNative_ReadDirR", (void *)SystemNative_ReadDirR },
	{ "SystemNative_CloseDir", (void *)SystemNative_CloseDir },
	{ "SystemNative_ReadLink", (void *)SystemNative_ReadLink },
	{ "SystemNative_FStat2", (void *)SystemNative_FStat2 },
	{ "SystemNative_Stat2", (void *)SystemNative_Stat2 },
	{ "SystemNative_LStat2", (void *)SystemNative_LStat2 },
	{ "SystemNative_Symlink", (void *)SystemNative_Symlink },
	{ "SystemNative_ChMod", (void *)SystemNative_ChMod },
	{ "SystemNative_CopyFile", (void *)SystemNative_CopyFile },
	{ "SystemNative_GetEGid", (void *)SystemNative_GetEGid },
	{ "SystemNative_GetEUid", (void *)SystemNative_GetEUid },
	{ "SystemNative_LChflags", (void *)SystemNative_LChflags },
	{ "SystemNative_LChflagsCanSetHiddenFlag", (void *)SystemNative_LChflagsCanSetHiddenFlag },
	{ "SystemNative_Link", (void *)SystemNative_Link },
	{ "SystemNative_MkDir", (void *)SystemNative_MkDir },
	{ "SystemNative_Rename", (void *)SystemNative_Rename },
	{ "SystemNative_RmDir", (void *)SystemNative_RmDir },
	{ "SystemNative_Stat2", (void *)SystemNative_Stat2 },
	{ "SystemNative_LStat2", (void *)SystemNative_LStat2 },
	{ "SystemNative_UTime", (void *)SystemNative_UTime },
	{ "SystemNative_UTimes", (void *)SystemNative_UTimes },
	{ "SystemNative_Unlink", (void *)SystemNative_Unlink },
	{ "SystemNative_GetDomainSocketSizes", (void *)SystemNative_GetDomainSocketSizes },
	{ NULL, NULL },
};

static void *pinvoke_tables[] = { System_Native_imports };
static const char *pinvoke_names[] = { "System.Native" };

// mono_dl_fallback_register (mono/utils/mono-dl.h, Mono 6.12 签名)
// MonoDlFallbackHandler *mono_dl_fallback_register (MonoDlFallbackLoad load_func,
//     MonoDlFallbackSymbol symbol_func, MonoDlFallbackClose close_func, void *user_data);
// 注意：返回值是 handler 指针（i32→i32）—— 签名不符会触发 wasm-ld
// function signature mismatch（间接调用 trap 风险）。
typedef void *(*mono_dl_fallback_load_func)(const char *name, int flags, char **err, void *user_data);
typedef void *(*mono_dl_fallback_symbol_func)(void *handle, const char *name, char **err, void *user_data);
typedef void (*mono_dl_fallback_close_func)(void *handle);
extern void *mono_dl_fallback_register(mono_dl_fallback_load_func load_func,
		mono_dl_fallback_symbol_func symbol_func,
		mono_dl_fallback_close_func close_func,
		void *user_data);

static void *wasm_dl_load(const char *name, int flags, char **err, void *user_data) {
	(void)flags;
	(void)err;
	(void)user_data;
	for (int i = 0; i < (int)(sizeof(pinvoke_tables) / sizeof(void *)); ++i) {
		if (!strcmp(name, pinvoke_names[i])) {
			return pinvoke_tables[i];
		}
	}
	return NULL;
}

static void *wasm_dl_symbol(void *handle, const char *name, char **err, void *user_data) {
	(void)err;
	(void)user_data;
	PinvokeImport *table = (PinvokeImport *)handle;
	for (int i = 0; table[i].name; ++i) {
		if (!strcmp(table[i].name, name)) {
			return table[i].func;
		}
	}
	return NULL;
}

void mono_wasm_pinvoke_init(void) {
	mono_dl_fallback_register(wasm_dl_load, wasm_dl_symbol, NULL, NULL);
}

} // extern "C"
