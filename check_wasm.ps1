$bytes = [System.IO.File]::ReadAllBytes("C:\gdmono_build\templates\web\.web_zip\godot.wasm")
$count = 0
for ($i=0; $i -lt $bytes.Length; $i++) {
    if ($bytes[$i] -eq 0x11) { $count++ }
}
Write-Host "Total call_indirect opcodes (0x11): $count"
Write-Host "WASM file size: $([math]::Round($bytes.Length / 1MB, 2)) MB"
