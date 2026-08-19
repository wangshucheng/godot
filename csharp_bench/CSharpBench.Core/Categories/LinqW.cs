using System;
using System.Collections.Generic;
using System.Linq;

namespace CSharpBench.Core.Categories
{
    /// <summary>LINQ 查询（14 项，含手写循环基线对照）。一次调用处理 size 元数据集。</summary>
    public static class LinqW
    {
        static readonly Dictionary<int, int[]> _src = new Dictionary<int, int[]>();
        static readonly Dictionary<int, int[]> _dup = new Dictionary<int, int[]>();

        static int[] Src(int n) { lock (_src) { int[] a; if (!_src.TryGetValue(n, out a)) { a = new int[n]; for (int i = 0; i < n; i++) a[i] = i; _src[n] = a; } return a; } }
        static int[] Dup(int n) { lock (_dup) { int[] a; if (!_dup.TryGetValue(n, out a)) { a = new int[n]; for (int i = 0; i < n; i++) a[i] = i % 100; _dup[n] = a; } return a; } }

        public static void Add(List<Workload> l)
        {
            l.Add("Linq", "Where_Filter_Count", Where_Filter_Count);
            l.Add("Linq", "Select_Project_Last", Select_Project_Last);
            l.Add("Linq", "WhereSelect_Chain_Sum", WhereSelect_Chain_Sum);
            l.Add("Linq", "Handwritten_Sum_Loop", Handwritten_Sum_Loop);
            l.Add("Linq", "OrderBy_First", OrderBy_First);
            l.Add("Linq", "OrderBy_Take10", OrderBy_Take10);
            l.Add("Linq", "GroupBy_Count", GroupBy_Count);
            l.Add("Linq", "Aggregate_Max", Aggregate_Max);
            l.Add("Linq", "Any_Miss_FullScan", Any_Miss_FullScan);
            l.Add("Linq", "FirstOrDefault_Miss", FirstOrDefault_Miss);
            l.Add("Linq", "ToList_Materialize", ToList_Materialize);
            l.Add("Linq", "ToArray_Materialize", ToArray_Materialize);
            l.Add("Linq", "Count_Predicate", Count_Predicate);
            l.Add("Linq", "Distinct_Count", Distinct_Count);
        }

        public static void Where_Filter_Count(int n) { Workloads.SinkLong = Src(n).Where(x => (x & 1) == 0).LongCount(); }
        public static void Select_Project_Last(int n) { Workloads.SinkLong = Src(n).Select(x => x * 2).Last(); }
        public static void WhereSelect_Chain_Sum(int n) { Workloads.SinkLong = Src(n).Where(x => (x & 1) == 0).Select(x => x * 3).Sum(); }
        public static void Handwritten_Sum_Loop(int n) { var a = Src(n); long s = 0; for (int i = 0; i < n; i++) { if ((a[i] & 1) == 0) s += a[i] * 3; } Workloads.SinkLong = s; }
        public static void OrderBy_First(int n) { Workloads.SinkLong = Src(n).OrderBy(x => -x).First(); }
        public static void OrderBy_Take10(int n) { Workloads.SinkLong = Src(n).OrderBy(x => -x).Take(10).Last(); }
        public static void GroupBy_Count(int n) { long c = 0; foreach (var g in Dup(n).GroupBy(x => x)) c += g.Count(); Workloads.SinkLong = c; }
        public static void Aggregate_Max(int n) { Workloads.SinkLong = Src(n).Aggregate(Math.Min); }
        public static void Any_Miss_FullScan(int n) { Workloads.Sink = Src(n).Any(x => x < 0); }
        public static void FirstOrDefault_Miss(int n) { Workloads.SinkLong = Src(n).FirstOrDefault(x => x < 0); }
        public static void ToList_Materialize(int n) { Workloads.Sink = Src(n).Where(x => (x & 1) == 0).ToList(); }
        public static void ToArray_Materialize(int n) { Workloads.Sink = Src(n).Where(x => (x & 1) == 0).ToArray(); }
        public static void Count_Predicate(int n) { Workloads.SinkLong = Src(n).Count(x => (x & 3) == 0); }
        public static void Distinct_Count(int n) { Workloads.SinkLong = Dup(n).Distinct().Count(); }
    }
}
