$root = "C:\Users\Administrator\AppData\Local\微信开发者工具\User Data\24d38d8a9569239c3e8419c9a8c32be3"
Write-Host "===== All .log files modified in last 30 min ====="
$cutoff = (Get-Date).AddMinutes(-30)
Get-ChildItem $root -Recurse -Filter "*.log" -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $cutoff } | Sort-Object LastWriteTime -Descending | Select-Object LastWriteTime, Length, FullName | Format-Table -AutoSize

Write-Host ""
Write-Host "===== All files modified in last 5 min (top 20) ====="
Get-ChildItem $root -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $cutoff -and $_.Length -lt 1MB } | Sort-Object LastWriteTime -Descending | Select-Object -First 20 LastWriteTime, Length, FullName | Format-Table -AutoSize
