# P5 fuzz test runner — runs fuzz_test.tscn headlessly and verifies [FUZZ] markers.
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$project = Join-Path $root "csharp_test"

# Auto-pick the newest available editor console binary (previously hardcoded
# to the stale bin/editor/windows copy — which could test an OLD build).
$candidates = @(
    (Join-Path $root "bin\godot.windows.editor.x86_64.mono.console.exe"),
    (Join-Path $root "bin\editor\windows\godot.windows.editor.x86_64.mono.console.exe")
) | Where-Object { Test-Path $_ }
if ($candidates.Count -eq 0) {
    Write-Host "FAIL: no editor binary found"
    exit 1
}
$editor = $candidates | Sort-Object { (Get-Item $_).LastWriteTime } -Descending | Select-Object -First 1
$logFile = Join-Path $project "fuzz_test_output.log"
$errFile = Join-Path $project "fuzz_test_output.err"

# Clean previous output
Remove-Item -Path $logFile -ErrorAction SilentlyContinue
Remove-Item -Path $errFile -ErrorAction SilentlyContinue

Write-Host "=== P5 Fuzz Test Runner ==="
Write-Host "Editor: $editor"
Write-Host "Project: $project"
Write-Host ""

# Run the editor in headless mode, executing fuzz_test.tscn as the main scene.
# --headless: no GUI
# res://fuzz_test.tscn: scene to run (overrides project main_scene)
$argString = "--path `"$project`" --headless `"res://fuzz_test.tscn`""
Write-Host "Args: $argString"

$proc = Start-Process -FilePath $editor `
    -ArgumentList $argString `
    -PassThru -NoNewWindow `
    -RedirectStandardOutput $logFile `
    -RedirectStandardError $errFile

# Wait up to 90 seconds for the process to exit (fuzz tests should complete quickly).
$proc | Wait-Process -Timeout 90 -ErrorAction SilentlyContinue

if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Write-Host "FAIL: process killed after 90s timeout"
    exit 1
}

$exitCode = $proc.ExitCode
Write-Host "Exit code: $exitCode"
Write-Host ""

# Show log output
if (Test-Path $logFile) {
    Write-Host "=== STDOUT log ==="
    Get-Content $logFile
    Write-Host ""
}
if (Test-Path $errFile) {
    $errContent = Get-Content $errFile -ErrorAction SilentlyContinue
    if ($errContent) {
        Write-Host "=== STDERR log ==="
        $errContent
        Write-Host ""
    }
}

# Verify [FUZZ] markers
$allLog = ""
if (Test-Path $logFile) { $allLog += Get-Content $logFile -Raw }
if (Test-Path $errFile) { $allLog += Get-Content $errFile -Raw }

$expectedStarts = 1..10 | ForEach-Object { "Fuzz{0:D2}" -f $_ }
$expectedStartMarkers = $expectedStarts | ForEach-Object { "[FUZZ] START $_" }

Write-Host "=== Verification ==="
$passed = 0
$failed = 0
foreach ($marker in $expectedStartMarkers) {
    if ($allLog -match [regex]::Escape($marker)) {
        Write-Host "PASS: $marker found"
        $passed++
    } else {
        Write-Host "FAIL: $marker NOT found"
        $failed++
    }
}

Write-Host ""
Write-Host "Summary: $passed/10 START markers found, $failed missing"

# Check for crash indicators
if ($exitCode -ne 0 -and $exitCode -ne -1073741819) {
    Write-Host "Note: non-zero exit code $exitCode (may indicate uncaught exception)"
}

if ($exitCode -eq -1073741819) {
    Write-Host "CRASH: process terminated with 0xC0000005 (access violation)"
}

# Final verdict
if ($failed -eq 0 -and $exitCode -eq 0) {
    Write-Host ""
    Write-Host "=== RESULT: PASS ==="
    exit 0
} else {
    Write-Host ""
    Write-Host "=== RESULT: FAIL ==="
    exit 1
}
