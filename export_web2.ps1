$WS = "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6"
$exe = "$WS\godot4_7_mono\bin\windows\godot.windows.editor.dev.x86_64.console.exe"
& $exe --headless --path "$WS\test_project" --export-release "Web" "$WS\test_project\export\web\index.html" 2>&1 | Select-String "Export|deploy|assembl|Mono|user" | Select-Object -First 30
