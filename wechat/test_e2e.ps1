# ============================================================
# test_e2e.ps1 - 微信小游戏端到端自动化测试
#
# 流程:
#   1. 杀掉微信开发者工具 + 旧 HTTP 服务器
#   2. 清理 IDE workspace 缓存（防止项目类型识别错误）
#   3. 启动本地 HTTP 服务器（提供 .data 文件，端口 8000）
#   4. 用 CLI 打开 IDE 加载项目
#   5. 监控 WeappLog 目录最新日志文件
#   6. 检测关键标志判断成功/失败
#   7. 自动退出并打印诊断结果
#
# 成功条件 (满足任一):
#   - 看到 "[WeChat] Game started successfully!" 且后续 rAF canvas 尺寸 > 100x100
#   - 看到 "[C#] Main._Ready() called!"
#
# 失败条件:
#   - 看到 "Game failed to start" 或 "WASM instantiation failed"
#   - 看到 canvas 尺寸 <= 10x10 持续超过 60 秒
#   - 超时（默认 5 分钟）未达成成功条件
# ============================================================

param(
    [string]$ProjectPath = "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\minigame",
    [string]$CdnRoot = "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\exports\web_2048",
    [int]$CdnPort = 8000,
    [string]$CliPath = "D:\software\Tencent\微信web开发者工具\cli.bat",
    [int]$TimeoutSec = 300,
    [switch]$KeepOpen
)

$ErrorActionPreference = "Continue"
$OutputEncoding = [System.Text.Encoding]::UTF8
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$LogDir = "$env:LOCALAPPDATA\微信开发者工具\User Data\Default\WeappLog\logs"
$HttpSrvPid = $null
$IdeStarted = $false
$StartTime = Get-Date

function Write-Step($msg) {
    Write-Host ""
    Write-Host "=== $msg ===" -ForegroundColor Cyan
}

function Write-OK($msg) {
    Write-Host "[OK] $msg" -ForegroundColor Green
}

function Write-Err($msg) {
    Write-Host "[ERR] $msg" -ForegroundColor Red
}

function Write-Info($msg) {
    Write-Host "[i] $msg" -ForegroundColor Yellow
}

# ============================================================
# Step 1: 杀掉旧进程
# ============================================================
Write-Step "Killing existing DevTools and HTTP server"

try {
    $devtoolsProcs = Get-Process -Name "wechatdevtools","wechatdevtools-helper","WeAppExe","WeappPlayer" -ErrorAction SilentlyContinue
    if ($devtoolsProcs) {
        $devtoolsProcs | ForEach-Object {
            Write-Info "Killing $($_.ProcessName) (PID=$($_.Id))"
            Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
        }
        Start-Sleep -Seconds 3
    } else {
        Write-Info "No DevTools process running"
    }
} catch {
    Write-Info "Process kill error: $_"
}

# 杀掉占用 8000 端口的进程
try {
    $portProcs = Get-NetTCPConnection -LocalPort $CdnPort -State Listen -ErrorAction SilentlyContinue
    if ($portProcs) {
        $portProcs | ForEach-Object {
            Write-Info "Killing PID=$($_.OwningProcess) on port $CdnPort"
            Stop-Process -Id $_.OwningProcess -Force -ErrorAction SilentlyContinue
        }
        Start-Sleep -Seconds 2
    }
} catch {
    Write-Info "Port kill error: $_"
}

# ============================================================
# Step 2: 清理 IDE workspace 缓存
# ============================================================
Write-Step "Clearing IDE workspace cache"

$workspaceDir = "$env:LOCALAPPDATA\微信开发者工具\User Data\Default\Editor\1.78\user-data\User\workspaceStorage"
if (Test-Path $workspaceDir) {
    try {
        Remove-Item -Path $workspaceDir -Recurse -Force -ErrorAction SilentlyContinue
        Write-OK "Cleared workspaceStorage"
    } catch {
        Write-Info "workspaceStorage clear failed: $_"
    }
} else {
    Write-Info "No workspaceStorage dir"
}

# ============================================================
# Step 3: 启动本地 HTTP 服务器（CDN）
# ============================================================
Write-Step "Starting HTTP CDN server on port $CdnPort"

if (-not (Test-Path $CdnRoot)) {
    Write-Err "CDN root not found: $CdnRoot"
    exit 1
}

# 用 Python 起 SimpleHTTPServer（最稳定）
$pyCmd = "import http.server, socketserver, sys; " +
         "handler = http.server.SimpleHTTPRequestHandler; " +
         "socketserver.TCPServer.allow_reuse_address = True; " +
         "httpd = socketserver.TCPServer(('0.0.0.0', $CdnPort), handler); " +
         "print('HTTP CDN serving $CdnRoot on port $CdnPort'); " +
         "httpd.serve_forever()"

$HttpSrvProcess = Start-Process -FilePath "python" `
    -ArgumentList "-c", $pyCmd `
    -WorkingDirectory $CdnRoot `
    -WindowStyle Hidden `
    -PassThru

$HttpSrvPid = $HttpSrvProcess.Id
Write-OK "HTTP server PID=$HttpSrvPid"

# 等 HTTP 服务器就绪
Start-Sleep -Seconds 2
try {
    $resp = Invoke-WebRequest -Uri "http://localhost:$CdnPort/" -UseBasicParsing -TimeoutSec 5
    Write-OK "HTTP server responding (status=$($resp.StatusCode))"
} catch {
    Write-Err "HTTP server not responding: $_"
    exit 1
}

# ============================================================
# Step 4: 用 CLI 打开 IDE 加载项目
# ============================================================
Write-Step "Opening WeChat DevTools with project"

if (-not (Test-Path $CliPath)) {
    Write-Err "CLI not found: $CliPath"
    exit 1
}

if (-not (Test-Path $ProjectPath)) {
    Write-Err "Project path not found: $ProjectPath"
    exit 1
}

# cli.bat 启动后会立即返回（IDE 异步启动）
$ideProcess = Start-Process -FilePath "cmd.exe" `
    -ArgumentList "/c", "`"$CliPath`" open --project `"$ProjectPath`"" `
    -WindowStyle Hidden `
    -PassThru

$IdeStarted = $true
Write-OK "IDE launch dispatched (launcher PID=$($ideProcess.Id))"
Write-Info "Waiting 30s for IDE to fully start and auto-compile..."
Start-Sleep -Seconds 30

# ============================================================
# Step 5: 监控日志文件
# ============================================================
Write-Step "Monitoring WeappLog directory for test result"

if (-not (Test-Path $LogDir)) {
    Write-Err "Log directory not found: $LogDir"
    Write-Info "Available user data dirs:"
    Get-ChildItem "$env:LOCALAPPDATA\微信开发者工具\User Data\Default" -Directory -ErrorAction SilentlyContinue | ForEach-Object { Write-Info "  $($_.Name)" }
    exit 1
}

# 记录启动时间点，只看此时间后的日志
$MonitorStart = Get-Date
$Deadline = $MonitorStart.AddSeconds($TimeoutSec)

$LatestLog = $null
$LastLogPos = 0
$CanvasSizePattern = 'canvas=(\d+)x(\d+)'
$LastCanvasSize = ""

$Result = "TIMEOUT"
$FailReason = ""
$SuccessEvidences = @()

Write-Info "Monitoring until $Deadline (timeout ${TimeoutSec}s)"

while ((Get-Date) -lt $Deadline) {
    # 找最新的日志文件
    $logFiles = Get-ChildItem $LogDir -Filter "*.log" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending
    if (-not $logFiles) {
        Start-Sleep -Seconds 3
        continue
    }

    $currentLog = $logFiles[0].FullName
    if ($currentLog -ne $LatestLog) {
        # 切换到新日志文件
        $LatestLog = $currentLog
        $LastLogPos = 0
        Write-Info "Following: $LatestLog"
    }

    # 读取新增内容
    try {
        $content = Get-Content $LatestLog -Raw -ErrorAction SilentlyContinue
        if (-not $content) { Start-Sleep -Seconds 3; continue }

        $newContent = $content.Substring([Math]::Min($LastLogPos, $content.Length))
        $LastLogPos = $content.Length

        if ($newContent.Length -gt 0) {
            # 实时显示新日志（仅关键字）
            $newLines = $newContent -split "`n"
            foreach ($line in $newLines) {
                $line = $line.Trim()
                if (-not $line) { continue }
                if ($line -match '\[WeChat\]|\[Mono\]|\[C#\]|Game started|Game failed|WASM instantiation|canvas=|ERROR|Failed|TypeError|ReferenceError|CompileError') {
                    Write-Host "  LOG: $line" -ForegroundColor DarkGray
                }
            }

            # 判断成功条件
            if ($newContent -match '\[WeChat\] Game started successfully') {
                $SuccessEvidences += "Game started successfully"
            }
            if ($newContent -match '\[C#\] Main\._Ready\(\) called') {
                $SuccessEvidences += "C# Main._Ready() called"
            }

            # 判断 canvas 尺寸
            $canvasMatches = [regex]::Matches($newContent, $CanvasSizePattern)
            foreach ($m in $canvasMatches) {
                $w = [int]$m.Groups[1].Value
                $h = [int]$m.Groups[2].Value
                $LastCanvasSize = "${w}x${h}"
                if ($w -gt 100 -and $h -gt 100) {
                    $SuccessEvidences += "Canvas size OK: ${w}x${h}"
                } elseif ($w -le 10 -and $h -le 10) {
                    $FailReason = "Canvas shrunk to ${w}x${h} (3x3 bug not fixed)"
                    $Result = "FAIL"
                    break
                }
            }

            # 判断失败条件
            if ($newContent -match 'Game failed to start|WASM instantiation failed|FATAL|RuntimeError') {
                $FailReason = "Engine startup failed"
                $Result = "FAIL"
                break
            }

            # 成功条件: 看到 Game started + canvas 尺寸正常
            if (($SuccessEvidences -contains "Game started successfully" -or $SuccessEvidences -contains "C# Main._Ready() called") -and
                ($SuccessEvidences -match "Canvas size OK")) {
                $Result = "PASS"
                break
            }
        }
    } catch {
        # 文件被占用是正常的
    }

    Start-Sleep -Seconds 3
}

# ============================================================
# Step 6: 输出诊断结果
# ============================================================
Write-Step "Test result: $Result"

$Elapsed = ((Get-Date) - $StartTime).TotalSeconds
Write-Info "Elapsed: $([Math]::Round($Elapsed, 1))s"
Write-Info "Latest canvas size: $LastCanvasSize"
Write-Info "Success evidences: $($SuccessEvidences -join ', ')"

if ($Result -eq "FAIL") {
    Write-Err "Fail reason: $FailReason"
}

# 打印最新日志的关键内容（用于诊断）
if ($LatestLog -and (Test-Path $LatestLog)) {
    Write-Step "Latest log tail (key lines)"
    try {
        $logContent = Get-Content $LatestLog -Raw -ErrorAction SilentlyContinue
        if ($logContent) {
            $keyPatterns = '\[WeChat\]|\[Mono\]|\[C#\]|Godot Engine|WASM|Game started|Game failed|canvas=|ERROR|Failed|TypeError|ReferenceError|CompileError|RuntimeError|instantiateWasm|WXWebAssembly|_Ready'
            $keyLines = ($logContent -split "`n") | Where-Object { $_ -match $keyPatterns } | Select-Object -Last 60
            foreach ($line in $keyLines) {
                Write-Host "  $line" -ForegroundColor Gray
            }
        }
    } catch {}
}

# ============================================================
# Step 7: 清理
# ============================================================
if (-not $KeepOpen) {
    Write-Step "Cleanup"
    if ($HttpSrvPid) {
        try {
            Stop-Process -Id $HttpSrvPid -Force -ErrorAction SilentlyContinue
            Write-OK "HTTP server killed"
        } catch {}
    }
    # 不杀 IDE（用户可能想看）— 加 -KeepOpen:$false 时也不杀，让用户主动关
    Write-Info "IDE kept open for inspection. Close it manually if needed."
} else {
    Write-Info "KeepOpen=true: HTTP server ($HttpSrvPid) and IDE left running"
}

if ($Result -eq "PASS") {
    Write-Host ""
    Write-Host "==========================================" -ForegroundColor Green
    Write-Host "  TEST PASSED - Game runs successfully!" -ForegroundColor Green
    Write-Host "==========================================" -ForegroundColor Green
    exit 0
} else {
    Write-Host ""
    Write-Host "==========================================" -ForegroundColor Red
    Write-Host "  TEST FAILED: $Result" -ForegroundColor Red
    if ($FailReason) { Write-Host "  Reason: $FailReason" -ForegroundColor Red }
    Write-Host "==========================================" -ForegroundColor Red
    exit 1
}
