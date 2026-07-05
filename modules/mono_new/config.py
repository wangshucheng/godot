def can_build(env, platform):
    return True


def configure(env):
    pass


def get_opts(platform):
    from SCons.Variables import BoolVariable, PathVariable

    return [
        BoolVariable("mono_new_static", "Statically link Mono runtime", True),
        PathVariable("mono_new_prefix", "Path to Mono installation prefix", "", PathVariable.PathAccept),
        PathVariable("mono_new_bcl", "Path to Mono BCL directory", "", PathVariable.PathAccept),
        BoolVariable("mono_new_stub", "Use Mono stub implementations (for build verification)", False),
        BoolVariable("mono_new_web_support", "Enable WebAssembly support", False),
    ]


def get_doc_classes():
    return [
        "CSharpScript",
    ]


def get_doc_path():
    return "doc_classes"


def get_icons_path():
    return "icons"
