# verify_p7_debugger.ps1
# Verifies the P7 Mono sdb debugger agent (spec docs/mono_editor_spec.md section 4.P7).
#
# Prerequisites: start the Godot editor (or a debug game) with the sdb agent
# enabled, either via project setting `dotnet/debugger/enabled=true` or by
# setting the environment variable GODOT_MONO_DEBUGGER_PORT=<port>.
#
# The script checks:
#   1. A TCP listener exists on the debugger port (127.0.0.1).
#   2. A TCP client connection to the port succeeds (sdb handshake accept).
#
# Usage:
#   powershell -File verify_p7_debugger.ps1 [-Port 55555]
#
# Exit code: 0 = all checks PASS, 1 = any check FAIL.

param(
    [int]$Port = 0
)

$ErrorActionPreference = "Stop"

# Resolve port: explicit parameter > GODOT_MONO_DEBUGGER_PORT > default 55555.
if ($Port -le 0) {
    if ($env:GODOT_MONO_DEBUGGER_PORT) {
        $Port = [int]$env:GODOT_MONO_DEBUGGER_PORT
    } else {
        $Port = 55555
    }
}

Write-Host "[P7] Verifying Mono sdb debugger agent on 127.0.0.1:$Port ..."

$failed = $false

# Check 1: listener present.
$listener = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue
if ($listener) {
    Write-Host "[P7] PASS: port $Port is LISTENING (PID $($listener[0].OwningProcess))"
} else {
    Write-Host "[P7] FAIL: no listener on port $Port. Is the editor/game running with the sdb agent enabled?"
    $failed = $true
}

# Check 2: TCP connect succeeds.
try {
    $client = New-Object System.Net.Sockets.TcpClient
    $async = $client.BeginConnect("127.0.0.1", $Port, $null, $null)
    $ok = $async.AsyncWaitHandle.WaitOne(3000)
    if ($ok -and $client.Connected) {
        $client.EndConnect($async)
        Write-Host "[P7] PASS: TCP connect to 127.0.0.1:$Port succeeded"
    } else {
        Write-Host "[P7] FAIL: TCP connect to 127.0.0.1:$Port timed out"
        $failed = $true
    }
    $client.Close()
} catch {
    Write-Host "[P7] FAIL: TCP connect error: $($_.Exception.Message)"
    $failed = $true
}

if ($failed) {
    Write-Host "[P7] RESULT: FAIL"
    exit 1
}
Write-Host "[P7] RESULT: ALL PASS"
exit 0
