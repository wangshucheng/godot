$WS = "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6"
Write-Host "=== bin/windows/ GodotSharp ==="
Get-ChildItem "$WS\godot4_7_mono\bin\windows\GodotSharp*" -ErrorAction SilentlyContinue | Select-Object Name, Length, LastWriteTime
Write-Host "=== bin/ GodotSharp ==="
Get-ChildItem "$WS\godot4_7_mono\bin\GodotSharp*" -ErrorAction SilentlyContinue | Select-Object Name, Length, LastWriteTime
