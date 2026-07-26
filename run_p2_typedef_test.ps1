# P2 v2 typedef iteration verification — runs the editor headlessly, triggers
# scripts assembly load (which calls refresh_global_classes), and verifies:
#   1. refresh_global_classes() logged "P2 refresh_global_classes: ... typedefs scanned"
#   2. ExportTest is registered in global_script_class_cache.cfg
$ErrorActionPreference = "Stop"

$editor = "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47fad25801ac16b9570799\godot4.7_mono\bin\godot.windows.editor.x86_64.mono.exe"
$project = "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47fad25801ac16b9570799\godot4.7_mono\csharp_test"
$logFile = Join-Path $project "p2_typedef_test.log"
$errFile = Join-Path $project "p2_typedef_test.err"

Remove-Item -Path $logFile -ErrorAction SilentlyContinue
Remove-Item -Path $errFile -ErrorAction SilentlyContinue

Write-Host "=== P2 v2 Typedef Iteration Test ==="
Write-Host "Editor: $editor"
Write-Host "Project: $project"
Write-Host ""

# Headless + --editor forces the editor boot path (loads assembly + scans res:// for global classes).
$argString = "--path `"$project`" --headless --editor"
Write-Host "Args: $argString"

# Spawn editor with a short timeout — it will idle in the editor loop after init;
# we just need the boot sequence (init → load_scripts_assembly → refresh_global_classes).
$proc = Start-Process -FilePath $editor `
    -ArgumentList $argString `
    -PassThru -NoNewWindow `
    -RedirectStandardOutput $logFile `
    -RedirectStandardError $errFile

# 12s should be plenty for the assembly load + global class scan on a warm build.
Start-Sleep -Seconds 12

if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Write-Host "Note: killed editor process after 12s (expected — editor idles in main loop)"
}

Write-Host ""
Write-Host "=== STDOUT tail ==="
if (Test-Path $logFile) {
    Get-Content $logFile -Tail 60
}
Write-Host ""

# Verification 1: refresh_global_classes log present
$allLog = ""
if (Test-Path $logFile) { $allLog += Get-Content $logFile -Raw }
if (Test-Path $errFile) { $allLog += Get-Content $errFile -Raw }

Write-Host "=== Verification ==="
$passed = 0
$failed = 0

if ($allLog -match "P2 refresh_global_classes:\s*(\d+)\s*typedefs scanned,\s*(\d+)\s*global classes registered") {
    $typedefs = $matches[1]
    $registered = $matches[2]
    Write-Host "PASS: refresh_global_classes ran — $typedefs typedefs scanned, $registered global classes registered"
    $passed++
} else {
    Write-Host "FAIL: refresh_global_classes log not found"
    $failed++
}

# Verification 2: ExportTest mentioned in logs (resolve_mono_class should fire)
if ($allLog -match "resolve_mono_class.*ExportTest.*is_global=1") {
    Write-Host "PASS: ExportTest resolved with is_global=1"
    $passed++
} else {
    Write-Host "Note: ExportTest resolve log not found (may have been before tail)"
    $failed++
}

# Verification 3: Loaded scripts assembly
if ($allLog -match "Loaded scripts assembly") {
    Write-Host "PASS: scripts assembly loaded"
    $passed++
} else {
    Write-Host "FAIL: scripts assembly load log not found"
    $failed++
}

Write-Host ""
Write-Host "Summary: $passed passed, $failed failed"

if ($failed -eq 0) {
    Write-Host ""
    Write-Host "=== RESULT: PASS ==="
    exit 0
} else {
    Write-Host ""
    Write-Host "=== RESULT: FAIL ==="
    exit 1
}
