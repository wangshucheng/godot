$ErrorActionPreference = "Stop"
$baseDir = "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47fad25801ac16b9570799\godot4.7_mono"
$wasmOpt = "D:\tools\emsdk\upstream\bin\wasm-opt.exe"
Set-Location $baseDir

$wasmIn = "bin\godot.web.template_release.wasm32.nothreads.mono.wasm"
$wasmOut = "bin\godot.web.template_release.wasm32.nothreads.mono.noeh.wasm"
$wasmcodeDir = "bin\exports\wechat\wechat_build\minigame\wasmcode"

Write-Host "=== Step 2: Strip EH ==="
& $wasmOpt --strip-eh --enable-simd --enable-bulk-memory --enable-bulk-memory-opt --enable-sign-ext --enable-nontrapping-float-to-int --enable-tail-call --enable-reference-types --enable-exception-handling $wasmIn -o $wasmOut
if ($LASTEXITCODE -ne 0) { Write-Host "[ERROR] strip-eh failed"; exit 1 }
Write-Host "[OK] noeh.wasm: $([math]::Round((Get-Item $wasmOut).Length/1MB,2)) MB"

Write-Host ""
Write-Host "=== Step 3: Brotli compress ==="
& python "..\compress_wasm_brotli.py" $wasmOut $wasmcodeDir
if ($LASTEXITCODE -ne 0) { Write-Host "[ERROR] brotli failed"; exit 1 }

$newBr = Get-ChildItem "$wasmcodeDir\*.wasm.br" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
$newHash = $newBr.BaseName -replace '\.wasm$', ''
Write-Host "[OK] New WASM.br: $($newBr.Name) ($([math]::Round($newBr.Length/1MB,2)) MB)"
Write-Host "[OK] New hash: $newHash"

Write-Host ""
Write-Host "=== Step 4: Delete old wasm.br ==="
Get-ChildItem "$wasmcodeDir\*.wasm.br" | Where-Object { $_.Name -ne $newBr.Name } | ForEach-Object {
    Remove-Item $_.FullName -Force
    Write-Host "  Deleted: $($_.Name)"
}

Write-Host ""
Write-Host "=== Step 5: Sync JS + WASM to wechat_input ==="
Copy-Item "bin\godot.web.template_release.wasm32.nothreads.mono.js" "bin\exports\wechat\wechat_input\game.js" -Force
Copy-Item $wasmIn "bin\exports\wechat\wechat_input\game.wasm" -Force
Write-Host "[OK] Synced game.js + game.wasm"

Write-Host ""
Write-Host "=== Step 6: Update config.json fileMD5 ==="
$cfgPath = "bin\exports\wechat\wechat_build\minigame\wx-game-kit\config.json"
$cfg = Get-Content $cfgPath -Raw
$cfgNew = $cfg -replace '"fileMD5":\s*"[0-9a-f]+"', "`"fileMD5`": `"$newHash`""
if ($cfgNew -ne $cfg) {
    Set-Content $cfgPath $cfgNew -NoNewline
    Write-Host "[OK] config.json fileMD5 -> $newHash"
} else {
    Write-Host "[WARN] config.json not updated"
}

Write-Host ""
Write-Host "=== Step 7: Regenerate framework.js ==="
& python "..\godot-mono-wasm\tools\inject_wechat_extensions.py" 2>&1 | Select-Object -Last 5

Write-Host ""
Write-Host "=== DONE ==="
Write-Host "New WASM.br hash: $newHash"
Get-ChildItem "bin\exports\wechat\wechat_build\minigame\wasmcode\*.wasm.br" | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
