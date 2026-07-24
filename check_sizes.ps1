$WS = "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6"
Write-Host "=== Engine bin GodotSharp.dll ==="
Get-ChildItem "$WS\godot4_7_mono\bin\GodotSharp.dll" | Select-Object Name, Length, LastWriteTime
Write-Host "=== BCL GodotSharp.dll ==="
Get-ChildItem "$WS\godot4_7_mono\bin\mono\lib\mono\4.5\GodotSharp.dll" | Select-Object Name, Length, LastWriteTime
Write-Host "=== WASM BCL GodotSharp.dll ==="
Get-ChildItem "$WS\godot4_7_mono\modules\mono_new\preload\mono\lib\mono\4.5\GodotSharp.dll" -ErrorAction SilentlyContinue | Select-Object Name, Length, LastWriteTime
Write-Host "=== Project assemblies GodotSharp.dll ==="
Get-ChildItem "$WS\test_project\.mono\assemblies\GodotSharp.dll" | Select-Object Name, Length, LastWriteTime
Write-Host "=== Exported pck size ==="
Get-ChildItem "$WS\test_project\export\web\index.pck" | Select-Object Name, Length, LastWriteTime
