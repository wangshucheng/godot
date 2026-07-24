$WS = "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6"
Write-Host "bin/windows/GodotSharp.dll:"
Get-ChildItem "$WS\godot4_7_mono\bin\windows\GodotSharp.dll" -ErrorAction SilentlyContinue | Select-Object Length, LastWriteTime
Write-Host "bin/GodotSharp.dll:"
Get-ChildItem "$WS\godot4_7_mono\bin\GodotSharp.dll" -ErrorAction SilentlyContinue | Select-Object Length, LastWriteTime
Write-Host "BCL GodotSharp.dll:"
Get-ChildItem "$WS\godot4_7_mono\bin\mono\lib\mono\4.5\GodotSharp.dll" | Select-Object Length, LastWriteTime
