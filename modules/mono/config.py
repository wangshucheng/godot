def can_build(env, platform):
    # P2-1: 本模块是上游 Godot 4 官方 .NET 模块（Godot.NET.Sdk + SourceGenerators，
    # 基于 .NET 6+/CoreCLR），不属于本项目的自研 Mono 静态链接方案。
    # 本项目使用 modules/mono_new/（Mono 6.12 sgen 静态链接 + .NET Framework 4.8 BCL）。
    # 为避免两套模块并存导致的混淆与潜在类名冲突，此处直接返回 False 禁用编译。
    # 如需实验性启用上游模块，临时改为 `return True` 并加 `module_mono_enabled=yes`。
    return False


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
