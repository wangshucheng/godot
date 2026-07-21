$root = "C:\Users\Administrator\AppData\Local\微信开发者工具\User Data\24d38d8a9569239c3e8419c9a8c32be3"
$cutoff = (Get-Date).AddMinutes(-15)
Write-Host "===== All files modified in last 15 min ====="
$files = Get-ChildItem $root -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $cutoff }
$files | Sort-Object LastWriteTime -Descending | Select-Object -First 30 | ForEach-Object {
    Write-Host "$($_.LastWriteTime) $($_.Length) $($_.FullName)"
}
Write-Host ""
Write-Host "Total: $($files.Count) files"
