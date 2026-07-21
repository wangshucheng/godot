Write-Host "===== Python processes ====="
Get-Process python -ErrorAction SilentlyContinue | Format-Table Id, ProcessName, CPU, StartTime
Write-Host ""
Write-Host "===== Latest WeappLog ====="
$logRoot = "C:\Users\Administrator\AppData\Local\微信开发者工具\User Data\24d38d8a9569239c3e8419c9a8c32be3\WeappLog"
Get-ChildItem $logRoot -Filter "*.log" -Recurse | Sort-Object LastWriteTime -Descending | Select-Object -First 5 | Format-Table LastWriteTime, Length, FullName
Write-Host ""
Write-Host "===== IDE processes ====="
Get-Process wechatdevtools -ErrorAction SilentlyContinue | Select-Object Id, ProcessName, MainWindowTitle | Format-Table -AutoSize
