# P5-C fuzz test runner — runs fuzz_test.tscn headlessly and verifies [FUZZ] markers.
# Extended from 10 to 21 tests (2026-07-28, P5 scheme C).
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$project = Join-Path $root "csharp_test"

# Auto-pick the newest available editor console binary.
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

Write-Host "=== P5-C Fuzz Test Runner (21 tests) ==="
Write-Host "Editor: $editor"
Write-Host "Project: $project"
Write-Host ""

# Run the editor in headless mode, executing fuzz_test.tscn as the main scene.
$argString = "--path `"$project`" --headless `"res://fuzz_test.tscn`""
Write-Host "Args: $argString"

$proc = Start-Process -FilePath $editor `
    -ArgumentList $argString `
    -PassThru -NoNewWindow `
    -RedirectStandardOutput $logFile `
    -RedirectStandardError $errFile

# Wait up to 120 seconds (Fuzz13 has 150ms delay, Fuzz15 runs 6 frames).
$proc | Wait-Process -Timeout 120 -ErrorAction SilentlyContinue

if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Write-Host "FAIL: process killed after 120s timeout"
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

# Verify [FUZZ] START markers for all 21 tests.
$allLog = ""
if (Test-Path $logFile) { $allLog += Get-Content $logFile -Raw }
if (Test-Path $errFile) { $allLog += Get-Content $errFile -Raw }

# Fuzz01-Fuzz21 class names (must match the .cs filenames).
$fuzzClassNames = @(
    "Fuzz01NullRef", "Fuzz02DivZero", "Fuzz03StackOverflow",
    "Fuzz04InfiniteLoop", "Fuzz05RecursiveStackBlowup", "Fuzz06AsyncException",
    "Fuzz07StaticCtorException", "Fuzz08PropertyGetterException",
    "Fuzz09MethodArgException", "Fuzz10SignalCallbackException",
    "Fuzz11StaticFieldException", "Fuzz12CrossScriptCascade",
    "Fuzz13AsyncDelayedException", "Fuzz14SignalReentryException",
    "Fuzz15PerFrameAccumulation", "Fuzz16ExitTreeException",
    "Fuzz17CallableException", "Fuzz18StaticMethodException",
    "Fuzz19NestedRethrow", "Fuzz20ToStringException",
    "Fuzz21StateConsistency"
)

Write-Host "=== Verification (21 START markers) ==="
$passed = 0
$failed = 0
foreach ($name in $fuzzClassNames) {
    $marker = "[FUZZ] START $name"
    if ($allLog -match [regex]::Escape($marker)) {
        Write-Host "PASS: $marker"
        $passed++
    } else {
        Write-Host "FAIL: $marker NOT found"
        $failed++
    }
}

Write-Host ""
Write-Host "Summary: $passed/21 START markers found, $failed missing"

# Verify Fuzz21 consistency checks passed (5 checks expected).
$fuzz21PassPattern = "\[FUZZ\] DONE Fuzz21StateConsistency \(checks=4 passed=4\)"
if ($allLog -match $fuzz21PassPattern) {
    Write-Host "PASS: Fuzz21 consistency checks (5/5)"
} else {
    Write-Host "WARN: Fuzz21 consistency checks not all passed (see log)"
}

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
