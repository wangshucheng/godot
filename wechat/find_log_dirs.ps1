$root = "C:\Users\Administrator\AppData\Local\微信开发者工具\User Data\24d38d8a9569239c3e8419c9a8c32be3"

Write-Host "===== WeappSimulator ====="
Get-ChildItem "$root\WeappSimulator" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 5 | Format-Table FullName, Length, LastWriteTime

Write-Host ""
Write-Host "===== WeappApplication ====="
Get-ChildItem "$root\WeappApplication" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 5 | Format-Table FullName, Length, LastWriteTime

Write-Host ""
Write-Host "===== Default subfolder ====="
Get-ChildItem "$root\Default" -ErrorAction SilentlyContinue | Select-Object Name, LastWriteTime | Format-Table -AutoSize

Write-Host ""
Write-Host "===== Check if Default/WeappLog exists ====="
if (Test-Path "$root\Default\WeappLog") {
    Get-ChildItem "$root\Default\WeappLog" -Recurse | Sort-Object LastWriteTime -Descending | Select-Object -First 5 | Format-Table FullName, Length, LastWriteTime
} else {
    Write-Host "No Default/WeappLog"
}

Write-Host ""
Write-Host "===== ALL WeappLog dirs ====="
Get-ChildItem $root -Recurse -Directory -Filter "WeappLog" -ErrorAction SilentlyContinue | ForEach-Object {
    $logs = Get-ChildItem $_.FullName -Filter "*.log" -ErrorAction SilentlyContinue
    if ($logs) {
        Write-Host "$($_.FullName) - $($logs.Count) log files"
        $logs | Sort-Object LastWriteTime -Descending | Select-Object -First 2 | ForEach-Object {
            Write-Host "  $($_.LastWriteTime) $($_.Length) $($_.Name)"
        }
    }
}
