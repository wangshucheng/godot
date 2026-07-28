#!/bin/bash
# WASM template build script - run via Git Bash
set +e
WS="C:/Users/Administrator/AppData/Roaming/TRAE SOLO CN/ModularData/ai-agent/work-mode-projects/6a47e7225801ac16b95705a6"
LOG="$WS/build_wasm.log"
{
  echo "=== Starting WASM template build at $(date) ==="
  # Source emsdk_env.sh to activate emsdk properly
  source "$WS/emsdk/emsdk_env.sh"
  export EM_CACHE="$WS/em_cache"
  # emsdk_env.sh doesn't always add upstream/emscripten to PATH on Windows;
  # add it explicitly so bash can find emcc (unix shell script).
  export PATH="$WS/emsdk/upstream/emscripten:$WS/emsdk/node/22.16.0_64bit/bin:$WS/emsdk/python/3.13.3_64bit:$PATH"
  echo "which scons: $(command -v scons 2>&1)"
  echo "which emcc: $(command -v emcc 2>&1)"
  echo "emcc version: $(emcc --version 2>&1 | head -1)"
  cd /c/gdmono_build || { echo "ERROR: cannot cd to /c/gdmono_build"; exit 2; }
  echo "PWD: $(pwd)"
  scons -j4 platform=web target=template_release \
      mono_new_static=yes mono_new_web_support=yes \
      threads=no
  echo "=== Build exit code: $? at $(date) ==="
} > "$LOG" 2>&1
