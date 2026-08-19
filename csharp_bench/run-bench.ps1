# ============================================================
# CSharpBench 一键执行脚本
# 用法:
#   .\run-bench.ps1                          # 全流程: 构建 + 4 TFM + Godot + 报告
#   .\run-bench.ps1 -Profile accurate        # 完整档（时间长）
#   .\run-bench.ps1 -SkipGodot               # 跳过 Godot 宿主
#   .\run-bench.ps1 -SkipDotNet              # 跳过 .NET 多版本（只跑 Godot+报告）
#   .\run-bench.ps1 -Tfms net6.0,net7.0      # 只跑指定 TFM
#
# 已知运行时兼容性问题（2026-08-19 实测，已内置规避）:
#   1. [已修 2026-08-19] SpanMemory 在 net48/netcoreapp3.1/net6.0 的 fallback warm
#      循环栈溢出（System.Memory 循环路径，根因同第 7 条：StackAlloc_Fill128 循环内
#      stackalloc）→ SpanW.cs 已修复并四 TFM 双引擎验证，预防性跳过已解除
#   2. Collections 1M 规模触发 BDN InProcess "takes too long" → 已改为两档
#   3. Mono-in-Godot: await Task.Yield 死锁（SyncContext 主线程泵送）→ runner 内置跳过
#   4. Mono 大规模数组路径 "Array fill produced wrong size" → Godot 列限 10k
#   5. Mono 反射 warm 循环偶发进程级崩溃（非确定性；冷启动单次调用正常）→
#      Runner 断点续跑 + manifest + 自动二分（见 Godot 段），崩溃项诚实标注 skip
#   6. Godot headless 退出阶段崩溃或挂起 → 增量落盘 + 超时强杀，数据无损
#   7. [已修 2026-08-19] net7.0 BDN warm 在 SpanMemory.StackAlloc_Fill128(size=1M)
#      栈溢出（rc=-1073741571/0xC00000FD），拖垮 GcAlloc/Scenarios → 双重修复：
#      a) 根因：SpanW.StackAlloc_Fill128 的 stackalloc 提升到循环外（栈占用 O(1)）；
#      b) 验证：net7.0/net48 × BDN/fallback 双引擎冒烟通过（quick 30/30, rc=0），
#         accurate 档缺失数据当日补齐（log_*_fill.txt），跳过/拆趟已全部解除。
# ============================================================
param(
    [ValidateSet("quick", "accurate")][string]$Profile = "quick",
    [string[]]$Tfms = @("net48", "netcoreapp3.1", "net6.0", "net7.0"),
    [switch]$SkipDotNet,
    [switch]$SkipGodot,
    [switch]$SkipReport,
    [string]$OutDir = ""
)
$ErrorActionPreference = "Continue"
$root = $PSScriptRoot
if (-not $OutDir) { $OutDir = Join-Path $root "BenchResults" }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$godotExe = Join-Path $root "..\bin\editor\windows\godot.windows.editor.x86_64.mono.console.exe"
# [2026-08-19 已修] SpanMemory 栈溢出根因修复（SpanW.StackAlloc_Fill128 的 stackalloc
# 提升到循环外，见头部说明 1/7），四 TFM 双引擎验证通过 → 预防性跳过全部解除。
$skipForTfm = @{}
# warm 阶段进程隔离机制（备用）：若某分类再出现 BDN InProcessEmit 进程级崩溃，可将其
# 加入此表——BDN 主趟跳过该类，单独 fallback 进程补跑，报告按行键自动合并回同一列。
$splitWarmForTfm = @{}

function Step($msg) { Write-Host "`n===== [$(Get-Date -Format HH:mm:ss)] $msg =====" -ForegroundColor Cyan }

# ---- 0. 构建 ----
Step "Build (Release multi-TFM + Core + Report)"
dotnet build (Join-Path $root "CSharpBench.Console\CSharpBench.Console.csproj") -c Release | Select-String "个错误" | Select-Object -First 1
dotnet build (Join-Path $root "CSharpBench.Report\CSharpBench.Report.csproj") -c Release | Select-String "个错误" | Select-Object -First 1

# ---- 1. .NET 多 TFM ----
if (-not $SkipDotNet) {
    foreach ($tfm in $Tfms) {
        $dll = Join-Path $root "CSharpBench.Console\bin\Release\$tfm\CSharpBench.Console.dll"
        $exe = Join-Path $root "CSharpBench.Console\bin\Release\$tfm\CSharpBench.Console.exe"
        if (-not (Test-Path $dll) -and -not (Test-Path $exe)) { Write-Host "skip $tfm (not built)"; continue }
        $skip = $skipForTfm[$tfm]
        $split = $splitWarmForTfm[$tfm]
        # BDN 主趟跳过项 = 原有 skip + 需进程隔离的分类
        $mainSkip = (@($skip, $split) | Where-Object { $_ }) -join ','
        if ($mainSkip) { $env:CSBENCH_SKIP = $mainSkip } else { Remove-Item Env:CSBENCH_SKIP -ErrorAction SilentlyContinue }
        $runner = if (Test-Path $dll) { "dotnet $dll" } else { $exe }
        Step "Run warm ($Profile): $tfm (BDN auto, skip=[$mainSkip])"
        if (Test-Path $dll) { dotnet $dll --profile $Profile --mode warm --engine auto --out $OutDir *> (Join-Path $OutDir "log_${tfm}_warm.txt") }
        else { & $exe --profile $Profile --mode warm --engine auto --out $OutDir *> (Join-Path $OutDir "log_${tfm}_warm.txt") }
        Write-Host "  rc=$LASTEXITCODE"
        # 隔离趟：单独进程补跑被拆出的分类（fallback 引擎；需先清掉 CSBENCH_SKIP，
        # 否则 FallbackRunner 会把该类整类 skip 掉）。崩溃只损失本类，不连坐主趟。
        if ($split) {
            if ($skip) { $env:CSBENCH_SKIP = $skip } else { Remove-Item Env:CSBENCH_SKIP -ErrorAction SilentlyContinue }
            Step "Run warm isolated ($Profile): $tfm / $split (fallback, separate process)"
            if (Test-Path $dll) { dotnet $dll --profile $Profile --mode warm --engine fallback --category $split --out $OutDir *> (Join-Path $OutDir "log_${tfm}_warm_${split}.txt") }
            else { & $exe --profile $Profile --mode warm --engine fallback --category $split --out $OutDir *> (Join-Path $OutDir "log_${tfm}_warm_${split}.txt") }
            Write-Host "  rc=$LASTEXITCODE"
        }
        # cold 趟维持原有 skip 语义
        if ($skip) { $env:CSBENCH_SKIP = $skip } else { Remove-Item Env:CSBENCH_SKIP -ErrorAction SilentlyContinue }
        Step "Run cold: $tfm (fallback in-process first-call)"
        if (Test-Path $dll) { dotnet $dll --profile $Profile --mode cold --engine fallback --out $OutDir *> (Join-Path $OutDir "log_${tfm}_cold.txt") }
        else { & $exe --profile $Profile --mode cold --engine fallback --out $OutDir *> (Join-Path $OutDir "log_${tfm}_cold.txt") }
        Write-Host "  rc=$LASTEXITCODE"
        Remove-Item Env:CSBENCH_SKIP -ErrorAction SilentlyContinue
    }
}

# ---- 2. Godot 宿主（Mono 6.12，分类分趟 + 自动二分） ----
if (-not $SkipGodot) {
    Step "Build Godot project (csharp_test)"
    Push-Location (Join-Path $root "..\csharp_test")
    dotnet build CSharpTest.csproj -c Debug 2>&1 | Select-String "个错误" | Select-Object -First 1
    # System.Memory 及依赖需手动放入 .mono/assemblies（NuGet 包不随自定义 OutputPath 传递复制）
    $cache = "$env:USERPROFILE\.nuget\packages"
    foreach ($pkg in @("system.memory", "system.buffers", "system.runtime.compilerservices.unsafe")) {
        $dir = Get-ChildItem "$cache\$pkg" -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
        if ($dir) {
            $lib = Get-ChildItem "$($dir.FullName)\lib" -Directory | Where-Object Name -eq "net461" | Select-Object -First 1
            if (-not $lib) { $lib = Get-ChildItem "$($dir.FullName)\lib" -Directory | Sort-Object Name -Descending | Select-Object -First 1 }
            Copy-Item (Join-Path $lib.FullName "*.dll") ".mono\assemblies\" -Force -ErrorAction SilentlyContinue
        }
    }
    # 二分驱动脚本（自带断点续跑/manifest 对比/超时强杀/崩溃项自动标注），
    # 详见 run_godot_bisect.ps1 头部说明。
    & powershell -ExecutionPolicy Bypass -File "run_godot_bisect.ps1" *>&1 |
        Tee-Object -FilePath (Join-Path $OutDir "log_godot_bisect.txt")
    Copy-Item "csbench_godot_*.json" $OutDir -Force -ErrorAction SilentlyContinue
    Pop-Location
}

# ---- 3. 报告 ----
if (-not $SkipReport) {
    Step "Merge & generate report (HTML + MD)"
    dotnet (Join-Path $root "CSharpBench.Report\bin\Release\net7.0\CSharpBench.Report.dll") --indir $OutDir --out (Join-Path $OutDir "report_latest")
}

Step "DONE -> $OutDir"
