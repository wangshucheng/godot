def can_build(env, platform):
    if env.editor_build:
        env.module_add_dependencies("mono", ["regex"])

    return True


def get_opts(platform):
    from SCons.Variables import BoolVariable

    return [
        BoolVariable("mono_static", "Enable static linking of Mono runtime libraries (no external .NET runtime required)", False),
    ]


def configure(env):
    # Check if the platform has marked mono as supported.
    supported = env.get("supported", [])
    if "mono" not in supported:
        import sys

        print("The 'mono' module does not currently support building for this platform. Aborting.")
        sys.exit(255)

    env.add_module_version_string("mono")

    # Add mono_static build option for static linking Mono runtime
    if env.get("mono_static", False):
        env.Append(CPPDEFINES=["GD_MONO_STATIC_LINKING"])
        print("Mono: Static linking enabled")


def get_doc_classes():
    return [
        "CSharpScript",
        "GodotSharp",
    ]


def get_doc_path():
    return "doc_classes"


def is_enabled():
    # The module is disabled by default. Use module_mono_enabled=yes to enable it.
    return False
