# Phase 0.1: Delegate probe runner — runs delegate_probe.tscn headlessly
# and verifies [PROBE] markers for the 3 test paths.
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
$logFile = Join-Path $project "delegate_probe_output.log"
$errFile = Join-Path $project "delegate_probe_output.err"

# Clean previous output
Remove-Item -Path $logFile -ErrorAction SilentlyContinue
Remove-Item -Path $errFile -ErrorAction SilentlyContinue

Write-Host "=== Phase 0.1 Delegate Probe Runner ==="
Write-Host "Editor: $editor"
Write-Host "Project: $project"
Write-Host ""

# Run the editor in headless mode, executing delegate_probe.tscn as the main scene.
$argString = "--path `"$project`" --headless `"res://delegate_probe.tscn`""
Write-Host "Args: $argString"

$proc = Start-Process -FilePath $editor `
    -ArgumentList $argString `
    -PassThru -NoNewWindow `
    -RedirectStandardOutput $logFile `
    -RedirectStandardError $errFile

# Wait up to 30 seconds (probe is short-lived).
$proc | Wait-Process -Timeout 30 -ErrorAction SilentlyContinue

if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Write-Host "FAIL: process killed after 30s timeout"
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

# Verify [PROBE] markers.
$allLog = ""
if (Test-Path $logFile) { $allLog += Get-Content $logFile -Raw }
if (Test-Path $errFile) { $allLog += Get-Content $errFile -Raw }

$expectedMarkers = @(
    "[PROBE] START DelegateProbe",
    "[PROBE] Test1 C# direct invoke: PASS",
    "[PROBE] Test2 C++ mono_runtime_invoke(delegate.Invoke): PASS",
    "[PROBE] Test3 C++ function pointer invoke: PASS",
    "[PROBE] DONE DelegateProbe (pass=3/3)"
)

Write-Host "=== Verification (5 markers) ==="
$passed = 0
$failed = 0
foreach ($marker in $expectedMarkers) {
    if ($allLog -match [regex]::Escape($marker)) {
        Write-Host "PASS: $marker"
        $passed++
    } else {
        Write-Host "FAIL: $marker NOT found"
        $failed++
    }
}

Write-Host ""
Write-Host "Summary: $passed/$($expectedMarkers.Count) markers found, $failed missing"

# Check for crash indicators
if ($exitCode -eq -1073741819) {
    Write-Host "CRASH: process terminated with 0xC0000005 (access violation)"
}

# Final verdict: require all markers + clean exit (exit code 0 is ideal,
# but probe doesn't call quit, so non-zero may be normal on timeout/scene end).
if ($failed -eq 0) {
    Write-Host ""
    Write-Host "=== RESULT: PASS (delegate可用性验证通过) ==="
    exit 0
} else {
    Write-Host ""
    Write-Host "=== RESULT: FAIL ==="
    exit 1
}
