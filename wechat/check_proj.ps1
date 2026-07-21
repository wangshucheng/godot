Write-Host "===== minigame directory contents ====="
$dir = "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\minigame"
Get-ChildItem $dir -Force | Sort-Object LastWriteTime -Descending | Select-Object LastWriteTime, Length, Name | Format-Table -AutoSize

Write-Host ""
Write-Host "===== WeappCache ====="
$weappCache = "C:\Users\Administrator\AppData\Local\微信开发者工具\User Data\24d38d8a9569239c3e8419c9a8c32be3\WeappCache"
Get-ChildItem $weappCache -ErrorAction SilentlyContinue | Format-Table Name, LastWriteTime

Write-Host ""
Write-Host "===== WeappEditor recent ====="
$weappEditor = "C:\Users\Administrator\AppData\Local\微信开发者工具\User Data\24d38d8a9569239c3e8419c9a8c32be3\WeappEditor"
Get-ChildItem $weappEditor -Recurse -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 10 LastWriteTime, Length, FullName | Format-Table -AutoSize
