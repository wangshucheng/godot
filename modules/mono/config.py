def can_build(env, platform):
    return True


def configure(env):
    env.add_module_version_string("mono")


def get_doc_classes():
    return [
        "CSharpScript",
    ]


def get_doc_path():
    return "doc_classes"


def get_modules_path():
    return "modules"


def is_enabled():
    return True
