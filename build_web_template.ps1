# build_web_template.ps1
# Builds the Godot Web (WASM) export template with Mono (hybrid AOT).
#
# Usage:
#   powershell -File build_web_template.ps1 [-Clean] [-Jobs 8]
#
#   -Clean   Delete bin/obj/modules/mono and bin/templates/web first.
#            REQUIRED when switching AOT <-> Interpreter modes: scons does
#            not invalidate cached .o files on CPPDEFINES changes and would
#            otherwise link stale objects (H9 P0-1, 2026-07-26).
#
# Known workaround (integrated in scene/resources/SCsub since 2026-07-27):
#   clang++ 23 crashes compiling scene/resources/style_box_flat.cpp at -Os
#   (emcc frontend bug, see docs/review_2026-07-26_h9_wasm.md). The SCsub now
#   builds that single file at -O0 on web automatically.
#   NOTE: use -j4 or lower — with -j8, clang++ OOM-crashes on other large TUs
#   (observed: servers/rendering/rendering_server.cpp).

param(
    [switch]$Clean,
    [int]$Jobs = 4
)

$ErrorActionPreference = "Continue"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $root

# Activate emsdk environment.
$emsdkEnv = "C:\emsdk\emsdk_env.ps1"
if (Test-Path $emsdkEnv) { . $emsdkEnv | Out-Null }

if ($Clean) {
    Write-Host "[web-build] cleaning bin/obj/modules/mono and bin/templates/web ..."
    Remove-Item -Recurse -Force "bin\obj\modules\mono" -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force "bin\templates\web" -ErrorAction SilentlyContinue
}

$sconsArgs = @("platform=web", "target=template_release", "mono_wasm=yes", "threads=no", "-j$Jobs")

Write-Host "[web-build] scons $($sconsArgs -join ' ')"
scons @sconsArgs
$code1 = $LASTEXITCODE

if ($code1 -ne 0) {
    $obj = "bin\obj\scene\resources\style_box_flat.web.template_release.wasm32.o"
    Write-Host "[web-build] scons failed (exit $code1) — applying style_box_flat -O0 workaround ..."
    & "C:\emsdk\upstream\bin\clang++.exe" --driver-mode=g++ -target wasm32-unknown-emscripten `
        -fignore-exceptions -pthread -mllvm -combiner-global-alias-analysis=false `
        -mllvm -enable-emscripten-sjlj -mllvm -disable-lsr `
        --sysroot=C:/emsdk/upstream/emscripten/cache/sysroot -D __EMSCRIPTEN_SHARED_MEMORY__=1 `
        -Xclang -iwithsysroot/include/fakesdl -Xclang -iwithsysroot/include/compat `
        -o $obj -c -std=gnu++17 -fno-exceptions -msimd128 -fno-color-diagnostics `
        -fansi-escape-codes -O0 -Wall -Wshadow-field-in-constructor -Wshadow-uncaptured-local `
        -Wno-ordered-compare-function-pointers -Wenum-conversion `
        -D NDEBUG -D WEB_ENABLED -D UNIX_ENABLED -D UNIX_SOCKET_UNAVAILABLE -D GLES3_ENABLED `
        -D JAVASCRIPT_EVAL_ENABLED -D PTHREAD_NO_RENAME -D GDSCRIPT_NO_LSP `
        -D _LIBCPP_REMOVE_TRANSITIVE_INCLUDES -D MINIZIP_ENABLED -D BROTLI_ENABLED `
        -D OVERRIDE_ENABLED -D THREADS_ENABLED -D CLIPPER2_ENABLED -D ZSTD_STATIC_LINKING_ONLY `
        -I thirdparty/freetype/include -I thirdparty/libpng -I thirdparty/zstd -I thirdparty/zlib `
        -I thirdparty/clipper2/include -I thirdparty/brotli/include -I platform/web -I . `
        -I C:/emsdk/upstream/emscripten/cache/sysroot/include `
        scene/resources/style_box_flat.cpp
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[web-build] FAIL: -O0 workaround compile failed"
        Pop-Location
        exit 1
    }
    Write-Host "[web-build] workaround applied, resuming scons ..."
    scons @sconsArgs
    $code1 = $LASTEXITCODE
}

Pop-Location
if ($code1 -eq 0) {
    Write-Host "[web-build] SUCCESS"
} else {
    Write-Host "[web-build] FAIL (exit $code1)"
}
exit $code1
