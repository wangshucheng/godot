$WS = "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6"
# Copy GodotSharp.dll to engine bin dir (where _deploy_mono_web looks for it)
Copy-Item "$WS\godot4_7_mono\bin\mono\lib\mono\4.5\GodotSharp.dll" "$WS\godot4_7_mono\bin\GodotSharp.dll" -Force
Write-Host "GodotSharp.dll copied to engine bin/"
Get-ChildItem "$WS\godot4_7_mono\bin\GodotSharp.dll" | Select-Object Name, Length
