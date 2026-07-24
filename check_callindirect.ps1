$WS = "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6"
$env:EMSDK = "$WS\emsdk"
$env:EM_CACHE = "$WS\em_cache"
$env:PATH = "$WS\emsdk\upstream\emscripten;$WS\emsdk\node\22.16.0_64bit\bin;$WS\emsdk\python\3.13.3_64bit;$env:PATH"
wasm-dis C:\gdmono_build\templates\web\.web_zip\godot.wasm 2>&1 | Select-String "call_indirect" | Select-Object -First 5
