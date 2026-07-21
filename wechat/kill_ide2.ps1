Get-Process wechatdevtools -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 5
$remaining = Get-Process wechatdevtools -ErrorAction SilentlyContinue
if ($remaining) {
    Write-Host "Still alive:"
    $remaining | Format-Table Id, ProcessName
    $remaining | ForEach-Object { 
        try { taskkill /F /PID $_.Id 2>&1 | Out-Null } catch {}
    }
    Start-Sleep -Seconds 3
}
$final = Get-Process wechatdevtools -ErrorAction SilentlyContinue
if ($final) {
    Write-Host "Cannot kill:"
    $final | Format-Table Id, ProcessName
} else {
    Write-Host "All wechatdevtools killed"
}
