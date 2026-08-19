using System;
using System.Collections.Generic;

namespace CSharpBench.Core.Categories
{
    /// <summary>集合操作（16 项）。数据集按 size 惰性缓存，避免把建数据时间计入测量。</summary>
    public static class CollectionsW
    {
        static readonly Dictionary<int, int[]> _arrays = new Dictionary<int, int[]>();
        static readonly Dictionary<int, List<int>> _lists = new Dictionary<int, List<int>>();
        static readonly Dictionary<int, Dictionary<int, int>> _dicts = new Dictionary<int, Dictionary<int, int>>();
        static readonly Dictionary<int, HashSet<int>> _sets = new Dictionary<int, HashSet<int>>();
        static readonly Dictionary<int, int[]> _reverseArrays = new Dictionary<int, int[]>();

        static int[] Arr(int n) { lock (_arrays) { int[] a; if (!_arrays.TryGetValue(n, out a)) { a = new int[n]; for (int i = 0; i < n; i++) a[i] = i; _arrays[n] = a; } return a; } }
        static int[] RevArr(int n) { lock (_reverseArrays) { int[] a; if (!_reverseArrays.TryGetValue(n, out a)) { a = new int[n]; for (int i = 0; i < n; i++) a[i] = n - i; _reverseArrays[n] = a; } return a; } }
        static List<int> Lst(int n) { lock (_lists) { List<int> a; if (!_lists.TryGetValue(n, out a)) { a = new List<int>(); for (int i = 0; i < n; i++) a.Add(i); _lists[n] = a; } return a; } }
        static Dictionary<int, int> Dct(int n) { lock (_dicts) { Dictionary<int, int> d; if (!_dicts.TryGetValue(n, out d)) { d = new Dictionary<int, int>(); for (int i = 0; i < n; i++) d[i] = i; _dicts[n] = d; } return d; } }
        static HashSet<int> Set(int n) { lock (_sets) { HashSet<int> s; if (!_sets.TryGetValue(n, out s)) { s = new HashSet<int>(); for (int i = 0; i < n; i++) s.Add(i); _sets[n] = s; } return s; } }

        public static void Add(List<Workload> l)
        {
            // 注: 集合类为两档规模(100/10k)。1M 规模下 List.Contains 等 O(n) 扫描
            // 单次调用达数十 ms，触发 BDN InProcess "takes too long" 限制，且
            // Mono 6.12 在部分大规模数组路径存在 "Array fill produced wrong size" 内部错误。
            l.AddSmall("Collections", "Array_ForIterate", Array_ForIterate);
            l.AddSmall("Collections", "Array_Clone_Sort", Array_Clone_Sort);
            l.AddSmall("Collections", "List_Add_Grow", List_Add_Grow);
            l.AddSmall("Collections", "List_Add_Prealloc", List_Add_Prealloc);
            l.AddSmall("Collections", "List_IndexGet", List_IndexGet);
            l.AddSmall("Collections", "List_Contains_Miss", List_Contains_Miss);
            l.AddSmall("Collections", "List_IterateFor", List_IterateFor);
            l.AddSmall("Collections", "List_IterateForeach", List_IterateForeach);
            l.AddSmall("Collections", "Dictionary_Add_Grow", Dictionary_Add_Grow);
            l.AddSmall("Collections", "Dictionary_Add_Prealloc", Dictionary_Add_Prealloc);
            l.AddSmall("Collections", "Dictionary_Lookup_Hit", Dictionary_Lookup_Hit);
            l.AddSmall("Collections", "Dictionary_Lookup_Miss", Dictionary_Lookup_Miss);
            l.AddSmall("Collections", "Dictionary_IterateForeach", Dictionary_IterateForeach);
            l.AddSmall("Collections", "HashSet_Add", HashSet_Add);
            l.AddSmall("Collections", "HashSet_Contains_Hit", HashSet_Contains_Hit);
            l.AddSmall("Collections", "Queue_Stack_PushPop", Queue_Stack_PushPop);
        }

        public static void Array_ForIterate(int n) { var a = Arr(n); long s = 0; for (int i = 0; i < n; i++) s += a[i]; Workloads.SinkLong = s; }
        public static void Array_Clone_Sort(int n) { var copy = (int[])RevArr(n).Clone(); Array.Sort(copy); Workloads.SinkLong = copy[0]; }
        public static void List_Add_Grow(int n) { var l = new List<int>(); for (int i = 0; i < n; i++) l.Add(i); Workloads.SinkLong = l.Count; }
        public static void List_Add_Prealloc(int n) { var l = new List<int>(n); for (int i = 0; i < n; i++) l.Add(i); Workloads.SinkLong = l.Count; }
        public static void List_IndexGet(int n) { var l = Lst(n); long s = 0; for (int i = 0; i < n; i++) s += l[i]; Workloads.SinkLong = s; }
        public static void List_Contains_Miss(int n) { var l = Lst(n); bool f = false; for (int i = 0; i < n; i++) f = l.Contains(-1); Workloads.Sink = f; }
        public static void List_IterateFor(int n) { var l = Lst(n); long s = 0; for (int i = 0; i < n; i++) s += l[i]; Workloads.SinkLong = s; }
        public static void List_IterateForeach(int n) { var l = Lst(n); long s = 0; foreach (var v in l) s += v; Workloads.SinkLong = s; }
        public static void Dictionary_Add_Grow(int n) { var d = new Dictionary<int, int>(); for (int i = 0; i < n; i++) d[i] = i; Workloads.SinkLong = d.Count; }
        public static void Dictionary_Add_Prealloc(int n) { var d = new Dictionary<int, int>(n); for (int i = 0; i < n; i++) d[i] = i; Workloads.SinkLong = d.Count; }
        public static void Dictionary_Lookup_Hit(int n) { var d = Dct(n); var k = Arr(n); long s = 0; for (int i = 0; i < n; i++) s += d[k[i]]; Workloads.SinkLong = s; }
        public static void Dictionary_Lookup_Miss(int n) { var d = Dct(n); int v; long s = 0; for (int i = 0; i < n; i++) { if (d.TryGetValue(i + n, out v)) s += v; } Workloads.SinkLong = s; }
        public static void Dictionary_IterateForeach(int n) { var d = Dct(n); long s = 0; foreach (var kv in d) s += kv.Value; Workloads.SinkLong = s; }
        public static void HashSet_Add(int n) { var s = new HashSet<int>(); for (int i = 0; i < n; i++) s.Add(i); Workloads.SinkLong = s.Count; }
        public static void HashSet_Contains_Hit(int n) { var s = Set(n); bool f = false; for (int i = 0; i < n; i++) f = s.Contains(i); Workloads.Sink = f; }
        public static void Queue_Stack_PushPop(int n)
        {
            var q = new Queue<int>();
            var st = new Stack<int>();
            for (int i = 0; i < n; i++) { q.Enqueue(i); st.Push(i); }
            long v = 0;
            for (int i = 0; i < n; i++) { v += q.Dequeue() + st.Pop(); }
            Workloads.SinkLong = v;
        }
    }
}
