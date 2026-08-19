# ============================================================
# Godot 分类分趟执行 + 自动二分驱动（Mono 运行时崩溃自愈）
#
# 原理: CSharpBenchRunner 支持断点续跑（已完成单元携带不重测）+
#       manifest 清单 + CSBENCH_SKIP 跳过标注。本脚本:
#   1. 跑一趟该类目（进程崩溃则已测数据已增量落盘）
#   2. 对比 manifest 与实际 JSON 行，找出缺失（=崩溃时正在测的负载）
#   3. 把首个缺失负载加入 CSBENCH_SKIP，重跑（resume 只测剩余）
#   4. 重复直至行数收敛（崩溃项全部诚实标注为 skipped）
# ============================================================
param(
    [string[]]$Categories = @("Primitives","Collections","Linq","Async","Reflection","Delegates","SpanMemory","GcAlloc","Scenarios"),
    [switch]$Fresh
)
$godotExe = Resolve-Path "..\bin\editor\windows\godot.windows.editor.x86_64.mono.console.exe"
$R = "..\csharp_bench\BenchResults"
$workDir = (Get-Location).Path
$env:GODOT_LOG_PATH = "."
$env:CSBENCH_MAXSIZE = "10000"

foreach ($cat in $Categories) {
    Write-Host "`n=== [$(Get-Date -Format HH:mm:ss)] Godot pass: $cat ===" -ForegroundColor Cyan
    $env:CSBENCH_CATEGORIES = $cat
    $env:CSBENCH_PASS = $cat
    if ($Fresh) { $env:CSBENCH_FRESH = "1" }
    $skipList = @()

    for ($attempt = 1; $attempt -le 8; $attempt++) {
        $env:CSBENCH_SKIP = ($skipList -join ",")
        # 超时保护：引擎退出阶段崩溃偶发挂起（驱动/崩溃处理器死锁）。
        # 数据已增量落盘，超时强杀不丢数据。轮询 HasExited 规避 WaitForExit 的怪异行为。
        # timeoutSec 需覆盖最慢单元（Reflection/Expression_Compile@10k 单次 ~30s × 8 次迭代）。
        $timeoutSec = 900
        $deadline = [DateTime]::UtcNow.AddSeconds($timeoutSec)
        $p = Start-Process -FilePath $godotExe `
            -ArgumentList "--headless","--log-file","csbench_engine_$cat.log","--path",".","res://csbench_test.tscn" `
            -WorkingDirectory $workDir `
            -RedirectStandardOutput "$R\godot_stdout_$cat.txt" `
            -RedirectStandardError "$R\godot_stderr_$cat.txt" `
            -NoNewWindow -PassThru
        while (-not $p.HasExited -and [DateTime]::UtcNow -lt $deadline) {
            Start-Sleep -Milliseconds 500
        }
        if ($p.HasExited) {
            $rc = "$($p.ExitCode)"
        } else {
            try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch {}
            $rc = "timeout-${timeoutSec}s-killed (data already flushed)"
        }
        Remove-Item Env:CSBENCH_FRESH -ErrorAction SilentlyContinue

        $manifest = "csbench_godot_${cat}_manifest.txt"
        $warmJson = "csbench_godot_$cat.json"
        $coldJson = "csbench_godot_${cat}_cold.json"

        $expected = @()
        if (Test-Path $manifest) { $expected = @(Get-Content $manifest) }
        if ($expected.Count -eq 0) {
            Write-Host "  attempt ${attempt}: rc=$rc — manifest 缺失（引擎未完成初始化即被杀/崩溃），重试" -ForegroundColor Yellow
            if ($attempt -ge 3) { Write-Host "  $cat ABORT: manifest 无法生成" -ForegroundColor Red; break }
            continue
        }

        $haveWarm = @(); $haveCold = @()
        if (Test-Path $warmJson) { $haveWarm = @((Get-Content $warmJson -Raw | ConvertFrom-Json).results | ForEach-Object { "$($_.name)|$($_.size)" }) }
        if (Test-Path $coldJson) { $haveCold = @((Get-Content $coldJson -Raw | ConvertFrom-Json).results | ForEach-Object { "$($_.name)|$($_.size)" }) }

        $expWarm = @($expected | Where-Object { $_ -like "warm|*" } | ForEach-Object { $_.Substring(5) })
        $expCold = @($expected | Where-Object { $_ -like "cold|*" } | ForEach-Object { $_.Substring(5) })
        $missingWarm = @($expWarm | Where-Object { $haveWarm -notcontains $_ })
        $missingCold = @($expCold | Where-Object { $haveCold -notcontains $_ })

        Write-Host ("  attempt {0}: rc={1} warm={2}/{3} cold={4}/{5}" -f $attempt, $rc, $haveWarm.Count, $expWarm.Count, $haveCold.Count, $expCold.Count)

        if ($missingWarm.Count -eq 0 -and $missingCold.Count -eq 0) {
            $skipped = @((Get-Content $warmJson -Raw | ConvertFrom-Json).results | Where-Object skipped).Count
            Write-Host "  $cat COMPLETE (warm=$($haveWarm.Count) cold=$($haveCold.Count) skipped=$skipped)" -ForegroundColor Green
            break
        }
        # 冷/热首个缺失（同一负载名；冷崩则热必崩）
        $firstMissing = if ($missingCold.Count -gt 0) { $missingCold[0].Split('|')[0] } else { $missingWarm[0].Split('|')[0] }
        if ($skipList -contains "$cat/$firstMissing") {
            Write-Host "  WARN: $firstMissing already skipped but still missing; stop bisecting $cat" -ForegroundColor Yellow
            break
        }
        $skipList += "$cat/$firstMissing"
        Write-Host "  bisect: skip [$cat/$firstMissing] (crash suspect), resume re-run" -ForegroundColor Yellow
    }
}
Remove-Item Env:CSBENCH_CATEGORIES, Env:CSBENCH_PASS, Env:CSBENCH_MAXSIZE, Env:CSBENCH_SKIP, Env:CSBENCH_FRESH -ErrorAction SilentlyContinue
Write-Host "`nGODOT_BISECT_ALL_DONE"
