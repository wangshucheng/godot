"""WeChat Mini Game platform detector.

This platform reuses the Emscripten-based toolchain and configuration of the
``web`` platform (CC=emcc, CXX=em++, WASM linker flags, ...). The
``wechat_minigame`` directory does not ship its own ``emscripten_helpers.py``,
so ``platform/web`` is prepended to ``sys.path`` to make it importable, and the
web platform's ``detect`` module is loaded directly so its logic can be reused
instead of duplicated.
"""

import importlib.util as _importlib_util
import os
import sys

# Make platform/web importable (for emscripten_helpers) and locate the web
# platform's detect module to reuse its configuration logic.
_web_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "web")
if _web_dir not in sys.path:
    sys.path.insert(0, _web_dir)

_web_spec = _importlib_util.spec_from_file_location(
    "_web_detect_for_wechat_minigame", os.path.join(_web_dir, "detect.py")
)
web_detect = _importlib_util.module_from_spec(_web_spec)
_web_spec.loader.exec_module(web_detect)


def get_name():
    return "WeChatMiniGame"


def can_build():
    # Reuse the web platform's check (requires emcc on PATH).
    return web_detect.can_build()


def get_tools(env):
    return web_detect.get_tools(env)


def get_opts():
    return web_detect.get_opts()


def get_doc_classes():
    # No editor export platform yet; the doc_classes directory is reserved
    # for future WeChatMiniGame-specific documentation classes.
    return []


def get_doc_path():
    return "doc_classes"


def get_flags():
    # Same baseline flags as web: arch=wasm32, vulkan=False, optimize="size".
    return web_detect.get_flags()


def configure(env):
    # Delegate to the web platform's configure() to set up the Emscripten
    # toolchain (CC=emcc, CXX=em++, WASM linker flags, JS helpers, ...).
    web_detect.configure(env)
    # Tag WeChatMiniGame builds with a dedicated define so platform code can
    # branch on it later (e.g. #ifdef WECHAT_MINIGAME_ENABLED).
    env.Append(CPPDEFINES=["WECHAT_MINIGAME_ENABLED"])
