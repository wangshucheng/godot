Get-Process wechatdevtools -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process python -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 5
Write-Host "Killed all processes"
$final = Get-Process wechatdevtools -ErrorAction SilentlyContinue
if ($final) { Write-Host "Still alive:"; $final | Format-Table Id, ProcessName }
else { Write-Host "All wechatdevtools killed" }
$py = Get-Process python -ErrorAction SilentlyContinue
if ($py) { Write-Host "Python still alive:"; $py | Format-Table Id, ProcessName }
else { Write-Host "All python killed" }
