$dir = "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\minigame"
Write-Host "===== All files in minigame ====="
Get-ChildItem $dir -Force | Format-Table Name, Length, LastWriteTime
Write-Host ""
Write-Host "===== Check for app.json ====="
if (Test-Path "$dir\app.json") { Write-Host "app.json EXISTS" } else { Write-Host "No app.json (correct for minigame)" }
Write-Host ""
Write-Host "===== game.json ====="
Get-Content "$dir\game.json"
Write-Host ""
Write-Host "===== project.config.json ====="
Get-Content "$dir\project.config.json"
