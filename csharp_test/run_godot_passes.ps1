# Godot 分类分趟执行（每类一个进程，隔离 Mono 偶发运行时崩溃）
$cats = @("Primitives","Collections","Linq","Async","Reflection","Delegates","SpanMemory","GcAlloc","Scenarios")
foreach ($cat in $cats) {
    Write-Host "=== [$(Get-Date -Format HH:mm:ss)] Godot pass: $cat ==="
    $env:CSBENCH_CATEGORIES = $cat
    $env:CSBENCH_PASS = $cat
    $env:CSBENCH_MAXSIZE = "10000"   # Mono 大规模路径规避
    & "..\bin\editor\windows\godot.windows.editor.x86_64.mono.console.exe" --headless --log-file "csbench_engine_$cat.log" --path . res://csbench_test.tscn *> "..\csharp_bench\BenchResults\log_godot_$cat.txt"
    Write-Host "  $cat rc=$LASTEXITCODE"
    if (Test-Path "csbench_godot_$cat.json") {
        $j = Get-Content "csbench_godot_$cat.json" -Raw | ConvertFrom-Json
        Write-Host "  warm rows: $($j.results.Count)"
    } else { Write-Host "  WARM JSON MISSING" }
    if (Test-Path "csbench_godot_${cat}_cold.json") {
        Copy-Item "csbench_godot_${cat}_cold.json" "..\csharp_bench\BenchResults\csbench_godot_${cat}_cold.json" -Force
    }
}
Remove-Item Env:CSBENCH_CATEGORIES, Env:CSBENCH_PASS, Env:CSBENCH_MAXSIZE -ErrorAction SilentlyContinue
Write-Host "GODOT_ALL_PASSES_DONE"
