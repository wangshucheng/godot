Get-Process wechatdevtools -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 3
Write-Host "Killed all wechatdevtools processes"
Get-Process wechatdevtools -ErrorAction SilentlyContinue | Format-Table Id, ProcessName
