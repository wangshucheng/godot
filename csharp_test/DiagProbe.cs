using Godot;
using System;

// WASM BCL icall 缺失诊断探针：逐项触碰候选 API，崩溃前最后一个
// [diag] 标记即触发点（Interlocked.CompareExchange<T> 泛型 icall 在
// WASM libmono 与嵌入 mscorlib 间失配 → 不可捕获的 wasm trap）。
public partial class DiagProbe : Node
{
    private static void Step(string s) { GD.Print("[diag] " + s + " OK"); }

    public override void _Ready()
    {
        GD.Print("[diag] ctor-ok");
        Step("env-var");
        var e = System.Environment.GetEnvironmentVariable("PATH");
        Step("stringcomparer-ordinalignorecase");
        var sc = StringComparer.OrdinalIgnoreCase;
        Step("hashset-ctor-with-comparer");
        var hs = new System.Collections.Generic.HashSet<string>(StringComparer.OrdinalIgnoreCase);
        hs.Add("x");
        Step("stopwatch-timestamp");
        var t = System.Diagnostics.Stopwatch.GetTimestamp();
        Step("gc-gettotalmemory");
        var m = System.GC.GetTotalMemory(false);
        Step("syncctx-current");
        var c = System.Threading.SynchronizationContext.Current;
        Step("process-getcurrentprocess");
        var p = System.Diagnostics.Process.GetCurrentProcess();
        Step("process-totalprocessortime");
        var cpu = p.TotalProcessorTime;
        Step("task-completed");
        var tsk = System.Threading.Tasks.Task.CompletedTask;
        Step("task-fromresult");
        var t2 = System.Threading.Tasks.Task.FromResult(1);
        Step("task-yield-awaiter");
        var y = System.Threading.Tasks.Task.Yield().GetAwaiter();
        Step("interlocked-compareexchange-int");
        int loc = 1;
        var r1 = System.Threading.Interlocked.CompareExchange(ref loc, 2, 1);
        Step("interlocked-compareexchange-generic-object");
        object o1 = new object(), o2 = new object(), o3 = new object();
        var r2 = System.Threading.Interlocked.CompareExchange(ref o1, o2, o3);
        Step("taskrun-simple");
        var t3 = System.Threading.Tasks.Task.Run(() => 1);
        Step("all-done");
    }
}
