using System;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace CSharpBench.Core
{
    /// <summary>一个基准工作负载：签名统一为 Action&lt;int size&gt;，一次调用执行 size 个基本操作（或处理 size 元数据集）。</summary>
    public sealed class Workload
    {
        public readonly string Category;
        public readonly string Name;
        public readonly Action<int> Run;
        public readonly int[] Sizes;
        /// <summary>
        /// 可选非阻塞异步驱动（与 Run 同语义）。WASM 单线程宿主无法执行阻塞等待
        /// （GetResult() 自旋卡死唯一 JS 线程，事件循环停止 → 死锁），宿主改为
        /// 帧轮询驱动此 Task。null 表示该负载无阻塞语义，同步执行即可。
        /// </summary>
        public readonly Func<int, Task> RunAsync;
        public Workload(string category, string name, Action<int> run, int[] sizes, Func<int, Task> runAsync = null)
        {
            Category = category; Name = name; Run = run; Sizes = sizes; RunAsync = runAsync;
        }
    }

    public static class Workloads
    {
        /// <summary>三档数据规模（小/中/大）。</summary>
        public static readonly int[] Sizes3 = { 100, 10_000, 1_000_000 };
        /// <summary>两档（重型类别：异步/反射/GC 分配，避免 1M 规模失控）。</summary>
        public static readonly int[] Sizes2 = { 100, 10_000 };

        /// <summary>结果 sink，防止 JIT 死代码消除。</summary>
        public static volatile object Sink;
        public static long SinkLong;
        public static double SinkDouble;

        private static Workload[] _all;
        public static Workload[] All
        {
            get
            {
                if (_all == null) _all = Build();
                return _all;
            }
        }

        /// <summary>类目加载失败时记录（如 Mono 缺 System.Memory → Span 类标 skipped）。</summary>
        public static readonly List<Tuple<string, string>> LoadErrors = new List<Tuple<string, string>>();

        private static Workload[] Build()
        {
            var l = new List<Workload>();
            AddCategory(l, "Primitives",  Categories.PrimitivesW.Add);
            AddCategory(l, "Collections", Categories.CollectionsW.Add);
            AddCategory(l, "Linq",        Categories.LinqW.Add);
            AddCategory(l, "Async",       Categories.AsyncW.Add);
            AddCategory(l, "Reflection",  Categories.ReflectionW.Add);
            AddCategory(l, "Delegates",   Categories.DelegatesW.Add);
            AddCategory(l, "SpanMemory",  Categories.SpanW.Add);
            AddCategory(l, "GcAlloc",     Categories.GcAllocW.Add);
            AddCategory(l, "Scenarios",   Categories.ScenariosW.Add);
            return l.ToArray();
        }

        private static void AddCategory(List<Workload> l, string cat, Action<List<Workload>> add)
        {
            int before = l.Count;
            try { add(l); }
            catch (Exception ex)
            {
                // 回滚该类目已加入项，全部标记为 skipped 占位
                for (int i = l.Count - 1; i >= before; i--) l.RemoveAt(i);
                LoadErrors.Add(Tuple.Create(cat, ex.GetType().Name + ": " + ex.Message));
                foreach (int size in Sizes3)
                    l.Add(new Workload(cat, "(category unavailable)", null, new[] { size }));
            }
        }
    }

    /// <summary>注册辅助扩展。</summary>
    public static class WorkloadListExt
    {
        public static void Add(this List<Workload> l, string cat, string name, Action<int> run)
        {
            l.Add(new Workload(cat, name, run, Workloads.Sizes3));
        }
        public static void AddSmall(this List<Workload> l, string cat, string name, Action<int> run)
        {
            l.Add(new Workload(cat, name, run, Workloads.Sizes2));
        }
        /// <summary>带非阻塞异步驱动的注册（WASM 单线程宿主用 RunAsync 帧轮询驱动）。</summary>
        public static void AddSmallAsync(this List<Workload> l, string cat, string name, Action<int> run, Func<int, Task> runAsync)
        {
            l.Add(new Workload(cat, name, run, Workloads.Sizes2, runAsync));
        }
    }
}
