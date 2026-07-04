def is_desktop(platform):
    return platform in ["windows", "macos", "linuxbsd"]


def is_unix_like(platform):
    return platform in ["macos", "linuxbsd", "android", "ios"]


def module_supports_tools_on(platform):
    return is_desktop(platform)


def configure(env, env_mono):
    # is_android = env["platform"] == "android"
    # is_web = env["platform"] == "web"
    # is_ios = env["platform"] == "ios"
    # is_ios_sim = is_ios and env["arch"] in ["x86_32", "x86_64"]

    if env.editor_build:
        if not module_supports_tools_on(env["platform"]):
            raise RuntimeError("This module does not currently support building for this platform for editor builds.")
        env_mono.Append(CPPDEFINES=["GD_MONO_HOT_RELOAD"])

    # Static linking configuration for embedded Mono runtime
    if env.get("mono_static", False):
        env_mono.Append(CPPDEFINES=["GD_MONO_STATIC_LINKING"])

        if env["platform"] == "windows":
            # Windows MSVC: Use /WHOLEARCHIVE to include all static archive symbols
            # Link against required system libraries for Mono static linking
            if env.msvc:
                env_mono.Append(LINKFLAGS=["/WHOLEARCHIVE:libmono-static-sgen.lib"])
                env_mono.Append(LINKFLAGS=["Mincore.lib"])
                env_mono.Append(LINKFLAGS=["msvcrt.lib"])
                env_mono.Append(LINKFLAGS=["LIBCMT.lib"])
                env_mono.Append(LINKFLAGS=["Psapi.lib"])
        elif env["platform"] in ["macos", "ios"]:
            # macOS/iOS: Use -force_load to include all symbols from archives
            env_mono.Append(LINKFLAGS=["-Wl,-force_load"])
            env_mono.Append(LINKFLAGS=["-Wl,-force_load_all"])
            # iOS doesn't support dynamic library loading for third-party code
            if env["platform"] == "ios":
                env_mono.Append(CPPDEFINES=["MONO_IOS=1"])
        elif env["platform"] == "linuxbsd":
            # Linux: Use -Wl,--whole-archive to preserve unused symbols
            env_mono.Append(LINKFLAGS=["-Wl,--whole-archive"])
            env_mono.Append(LINKFLAGS=["-Wl,--whole-archive,libmonosgen-2.0.a"])
            env_mono.Append(LINKFLAGS=["-Wl,--whole-archive,libmono-2.0.a"])
            env_mono.Append(LINKFLAGS=["-Wl,--no-whole-archive"])
            env_mono.Append(LIBS=["psapi", "version"])
        elif env["platform"] == "android":
            # Android: Similar to Linux, but with Android-specific considerations
            env_mono.Append(LINKFLAGS=["-Wl,--whole-archive"])
            env_mono.Append(LINKFLAGS=["-Wl,--whole-archive,libmonosgen-2.0.a"])
            env_mono.Append(LINKFLAGS=["-Wl,--no-whole-archive"])
        elif env["platform"] == "web":
            # Web/WASM: Enable Mono for web platform when static linking
            env_mono.Append(CPPDEFINES=["WEB_MONO_ENABLED=1"])
            env_mono.Append(CPPDEFINES=["USE_PTHREADS=1"])
            env_mono.Append(CPPDEFINES=["MONO_WASM=1"])
            # Emscripten-specific settings for Mono static linking
            env_mono.Append(LINKFLAGS=["-sINITIAL_MEMORY=256MB"])
            env_mono.Append(LINKFLAGS=["-sMAXIMUM_MEMORY=2GB"])
            env_mono.Append(LINKFLAGS=["-sALLOW_MEMORY_GROWTH=1"])
            env_mono.Append(LINKFLAGS=["-sSTACK_SIZE=1MB"])
            env_mono.Append(LINKFLAGS=["-sUSE_PTHREADS=1"])
            env_mono.Append(LINKFLAGS=["-sPTHREAD_POOL_SIZE=4"])
            env_mono.Append(LINKFLAGS=["-fexceptions"])
            env_mono.Append(LINKFLAGS=["-sERROR_ON_UNDEFINED_SYMBOLS=0"])
            env_mono.Append(CPPDEFINES=["JAVASCRIPT_ENABLED"])
            env_mono.Append(CPPDEFINES=["MONO_DLL_EXPORT_DISABLE"])
