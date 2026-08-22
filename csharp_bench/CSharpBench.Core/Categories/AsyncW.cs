using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace CSharpBench.Core.Categories
{
    /// <summary>异步编程（12 项，规模上限 10k 避免 1M await 失控）。同步包装内部等待，保证与其它负载统一。</summary>
    public static class AsyncW
    {
        public static void Add(List<Workload> l)
        {
            l.AddSmall("Async", "Await_CompletedTask", Await_CompletedTask);
            l.AddSmall("Async", "AwaitChain_Depth4", AwaitChain_Depth4);
            l.AddSmall("Async", "AwaitChain_Depth16", AwaitChain_Depth16);
            // Await_TaskYield / TaskRun_Offload_Wait 的同步包装内部是阻塞等待：
            // - 桌面 Godot 宿主：await Task.Yield() 的 continuation 被 GodotSynchronizationContext
            //   捕获，需主线程泵送，而主线程正阻塞在 GetResult() → 死锁。宿主测量期间
            //   临时摘除 sync context 后 ThreadPool（真实线程）接管，可正常完成。
            // - WASM 单线程：GetResult() 自旋卡死唯一 JS 线程，emscripten 事件循环停止 →
            //   必然死锁且无 ASYNCIFY 可解。宿主改用 RunAsync 帧轮询驱动（非阻塞）。
            l.AddSmallAsync("Async", "Await_TaskYield", Await_TaskYield, YieldDriver);
            l.AddSmall("Async", "Await_FromResult", Await_FromResult);
            l.AddSmallAsync("Async", "TaskRun_Offload_Wait", TaskRun_Offload_Wait, TaskRunDriver);
            l.AddSmall("Async", "WhenAll_8", WhenAll_8);
            l.AddSmall("Async", "WhenAll_64", WhenAll_64);
            l.AddSmall("Async", "Tcs_SetResult_Await", Tcs_SetResult_Await);
            l.AddSmall("Async", "ConfigureAwaitFalse_Chain4", ConfigureAwaitFalse_Chain4);
            l.AddSmall("Async", "SemaphoreSlim_WaitRelease", SemaphoreSlim_WaitRelease);
            l.AddSmall("Async", "ProducerConsumer_Queue", ProducerConsumer_Queue);
        }

        // ---- 非阻塞异步驱动（WASM 单线程宿主帧轮询执行；与同步包装同语义，
        //      仅把"阻塞等待"换成宿主驱动的 await）----

        public static Task YieldDriver(int n) { return YieldAsync(n); }

        public static async Task TaskRunDriver(int n)
        {
            long v = 0;
            for (int i = 0; i < n; i++) v += await Task.Run(() => 1);
            Workloads.SinkLong = v;
        }

        public static void Await_CompletedTask(int n) { AwaitCompletedAsync(n).GetAwaiter().GetResult(); }
        static async Task AwaitCompletedAsync(int n) { for (int i = 0; i < n; i++) await Task.CompletedTask; }

        public static void AwaitChain_Depth4(int n) { ChainAsync(n, 4).GetAwaiter().GetResult(); }
        public static void AwaitChain_Depth16(int n) { ChainAsync(n, 16).GetAwaiter().GetResult(); }
        static async Task ChainAsync(int n, int depth)
        {
            for (int i = 0; i < n; i++)
                for (int d = 0; d < depth; d++)
                    await Task.CompletedTask;
        }

        public static void Await_TaskYield(int n) { YieldAsync(n).GetAwaiter().GetResult(); }
        static async Task YieldAsync(int n) { for (int i = 0; i < n; i++) await Task.Yield(); }

        public static void Await_FromResult(int n) { FromResultAsync(n).GetAwaiter().GetResult(); }
        static async Task FromResultAsync(int n) { long s = 0; for (int i = 0; i < n; i++) s += await Task.FromResult(i); Workloads.SinkLong = s; }

        public static void TaskRun_Offload_Wait(int n)
        {
            long v = 0;
            for (int i = 0; i < n; i++) v += Task.Run(() => 1).Result;
            Workloads.SinkLong = v;
        }

        public static void WhenAll_8(int n) { WhenAllAsync(n, 8).GetAwaiter().GetResult(); }
        public static void WhenAll_64(int n) { WhenAllAsync(n, 64).GetAwaiter().GetResult(); }
        static Task[] Pool(int k)
        {
            var t = new Task[k];
            for (int i = 0; i < k; i++) t[i] = Task.CompletedTask;
            return t;
        }
        static async Task WhenAllAsync(int n, int k)
        {
            var pool = Pool(k);
            for (int i = 0; i < n; i++) await Task.WhenAll(pool);
        }

        public static void Tcs_SetResult_Await(int n) { TcsAsync(n).GetAwaiter().GetResult(); }
        static async Task TcsAsync(int n)
        {
            long s = 0;
            for (int i = 0; i < n; i++)
            {
                var tcs = new TaskCompletionSource<int>();
                tcs.SetResult(i);
                s += await tcs.Task;
            }
            Workloads.SinkLong = s;
        }

        public static void ConfigureAwaitFalse_Chain4(int n) { CAFAsync(n).GetAwaiter().GetResult(); }
        static async Task CAFAsync(int n)
        {
            for (int i = 0; i < n; i++)
                for (int d = 0; d < 4; d++)
                    await Task.CompletedTask.ConfigureAwait(false);
        }

        public static void SemaphoreSlim_WaitRelease(int n)
        {
            var sem = new SemaphoreSlim(1, 1);
            for (int i = 0; i < n; i++) { sem.Wait(); sem.Release(); }
        }

        public static void ProducerConsumer_Queue(int n)
        {
            using (var q = new BlockingCollection<int>(8))
            {
                long s = 0;
                for (int i = 0; i < n; i++)
                {
                    q.Add(i);
                    s += q.Take();
                }
                Workloads.SinkLong = s;
            }
        }
    }
}
