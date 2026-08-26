Get-Process godot.windows.editor.x86_64.mono -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2
$count = (Get-Process godot* -ErrorAction SilentlyContinue | Measure-Object).Count
"killed, remaining=$count" | Out-File -FilePath C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47fad25801ac16b9570799\godot4.7_mono\kill_result.txt -Encoding ascii
