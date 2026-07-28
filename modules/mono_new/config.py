def can_build(env, platform):
    return True


def configure(env):
    pass


def get_opts(platform):
    from SCons.Variables import BoolVariable, PathVariable

    opts = [
        BoolVariable("mono_new_static", "Statically link Mono runtime", True),
        PathVariable("mono_new_prefix", "Path to Mono installation prefix", "", PathVariable.PathAccept),
        PathVariable("mono_new_bcl", "Path to Mono BCL directory", "", PathVariable.PathAccept),
        BoolVariable("mono_new_stub", "Use Mono stub implementations (for build verification)", False),
        BoolVariable("mono_new_web_support", "Enable WebAssembly support", False),
    ]

    # iOS/Android 移动端选项（仅在对应平台构建时暴露）
    if platform == "ios":
        opts.append(BoolVariable("mono_new_ios_interp",
                                 "Use interpreter mode (required by App Store, no JIT). "
                                 "If disabled, Full AOT is used (requires monoaot toolchain).",
                                 True))
    if platform == "android":
        opts.append(BoolVariable("mono_new_android_interp",
                                 "Use interpreter mode (default, reuses WASM-validated path). "
                                 "If disabled, JIT is allowed (Android permits JIT).",
                                 True))

    return opts


def get_doc_classes():
    return [
        "CSharpScript",
    ]


def get_doc_path():
    return "doc_classes"


def get_icons_path():
    return "icons"
