# run_csharp_test.ps1
# Desktop runner for the csharp_test 24-scenario C# workflow suite.
#
# The suite runs res://main.tscn headlessly. Since the 2026-07-26 test
# hardening, Test.cs quits by itself on desktop with exit code 0 (all
# scenarios passed) or 1 (any scenario failed). This script additionally
# parses the log to verify:
#   1. Exactly 24 "[TEST RESULT]" lines are present (no scenario was skipped
#      or crashed mid-run).
#   2. No "[TEST FAIL]" line is present.
#   3. Process exit code is 0.
#
# Usage:
#   powershell -File run_csharp_test.ps1 [-Editor <path-to-godot-console-exe>]
#
# Exit code: 0 = PASS, 1 = FAIL.

param(
    [string]$Editor = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$project = Join-Path $root "csharp_test"
$logFile = Join-Path $project "csharp_test_output.log"
$errFile = Join-Path $project "csharp_test_output.err"

# Auto-pick the newest available editor console binary.
if (-not $Editor) {
    $candidates = @(
        (Join-Path $root "bin\godot.windows.editor.x86_64.mono.console.exe"),
        (Join-Path $root "bin\editor\windows\godot.windows.editor.x86_64.mono.console.exe")
    ) | Where-Object { Test-Path $_ }
    if ($candidates.Count -eq 0) {
        Write-Host "FAIL: no editor binary found in bin/ or bin/editor/windows/"
        exit 1
    }
    $Editor = $candidates | Sort-Object { (Get-Item $_).LastWriteTime } -Descending | Select-Object -First 1
}

Write-Host "=== C# Workflow Test Runner (desktop, 24 scenarios) ==="
Write-Host "Editor: $Editor"
Write-Host "Project: $project"
Write-Host ""

Remove-Item -Path $logFile -ErrorAction SilentlyContinue
Remove-Item -Path $errFile -ErrorAction SilentlyContinue

$argString = "--path `"$project`" --headless `"res://main.tscn`""
Write-Host "Args: $argString"

$proc = Start-Process -FilePath $Editor `
    -ArgumentList $argString `
    -PassThru -NoNewWindow `
    -RedirectStandardOutput $logFile `
    -RedirectStandardError $errFile

# The suite quits by itself; 120s is generous for a cold start + 24 scenarios.
$proc | Wait-Process -Timeout 120 -ErrorAction SilentlyContinue

if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Write-Host "FAIL: process killed after 120s timeout (suite did not quit by itself)"
    exit 1
}

$exitCode = $proc.ExitCode
Write-Host "Exit code: $exitCode"
Write-Host ""

$allLog = ""
if (Test-Path $logFile) { $allLog += Get-Content $logFile -Raw }
if (Test-Path $errFile) { $allLog += Get-Content $errFile -Raw }

Write-Host "=== [TEST RESULT] lines ==="
$resultLines = [regex]::Matches($allLog, "\[TEST RESULT\].*")
foreach ($line in $resultLines) {
    Write-Host $line.Value
}
Write-Host ""

Write-Host "=== Verification ==="
$failed = 0

# 1. Exactly 25 scenario results (scenarios 0 through 24).
if ($resultLines.Count -eq 25) {
    Write-Host "PASS: 25/25 scenarios reported"
} else {
    Write-Host "FAIL: expected 25 scenario results, got $($resultLines.Count)"
    $failed++
}

# 2. No individual assertion failure.
$failLines = [regex]::Matches($allLog, "\[TEST FAIL\]")
if ($failLines.Count -eq 0) {
    Write-Host "PASS: no [TEST FAIL] assertions"
} else {
    Write-Host "FAIL: $($failLines.Count) failed assertions:"
    [regex]::Matches($allLog, "\[TEST FAIL\].*") | ForEach-Object { Write-Host "  " $_.Value }
    $failed++
}

# 3. No scenario-level FAIL verdict.
if ($allLog -notmatch ": FAIL") {
    Write-Host "PASS: no scenario FAIL verdict"
} else {
    Write-Host "FAIL: at least one scenario reported FAIL"
    $failed++
}

# 4. Exit code.
if ($exitCode -eq 0) {
    Write-Host "PASS: exit code 0"
} else {
    Write-Host "FAIL: exit code $exitCode"
    $failed++
}

Write-Host ""
if ($failed -eq 0) {
    Write-Host "=== RESULT: PASS ==="
    exit 0
} else {
    Write-Host "=== RESULT: FAIL ($failed checks failed) ==="
    exit 1
}
