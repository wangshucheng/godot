using System;
using System.Collections.Generic;

namespace CSharpBench.Core.Categories
{
    public class SmallObj { public int A; public string B; }
    public class BigObj { public int A, B, C, D, E, F, G, H; public string S1, S2; public DateTime T; }
    public struct SmallStruct { public int A; public long B; }
    public class FinalizableObj { ~FinalizableObj() { } public int A; }

    /// <summary>GC/分配（10 项，规模上限 10k）。</summary>
    public static class GcAllocW
    {
        static readonly Stack<List<int>> _pool = new Stack<List<int>>();

        public static void Add(List<Workload> l)
        {
            l.AddSmall("GcAlloc", "Class_New_Small", Class_New_Small);
            l.AddSmall("GcAlloc", "Struct_CopyAssign_Baseline", Struct_CopyAssign_Baseline);
            l.AddSmall("GcAlloc", "Class_New_Big", Class_New_Big);
            l.AddSmall("GcAlloc", "Array_New_Int128", Array_New_Int128);
            l.AddSmall("GcAlloc", "Array_New_Class16", Array_New_Class16);
            l.AddSmall("GcAlloc", "ListPool_Reuse8", ListPool_Reuse8);
            l.AddSmall("GcAlloc", "Finalizable_New", Finalizable_New);
            l.AddSmall("GcAlloc", "String_New100", String_New100);
            l.AddSmall("GcAlloc", "Boxing_Plus_Equals", Boxing_Plus_Equals);
            l.AddSmall("GcAlloc", "GC_Collect_Gen0", GC_Collect_Gen0);
        }

        public static void Class_New_Small(int n) { SmallObj o = null; for (int i = 0; i < n; i++) o = new SmallObj { A = i }; Workloads.Sink = o; }
        public static void Struct_CopyAssign_Baseline(int n)
        {
            var arr = new SmallStruct[16];
            for (int i = 0; i < n; i++)
            {
                var s = new SmallStruct { A = i, B = i };
                arr[i & 15] = s;
            }
            Workloads.Sink = arr;
        }
        public static void Class_New_Big(int n) { BigObj o = null; for (int i = 0; i < n; i++) o = new BigObj { A = i, S1 = "s" }; Workloads.Sink = o; }
        public static void Array_New_Int128(int n) { int[] a = null; for (int i = 0; i < n; i++) a = new int[128]; Workloads.Sink = a; }
        public static void Array_New_Class16(int n) { SmallObj[] a = null; for (int i = 0; i < n; i++) { a = new SmallObj[16]; for (int k = 0; k < 16; k++) a[k] = new SmallObj(); } Workloads.Sink = a; }
        public static void ListPool_Reuse8(int n)
        {
            long v = 0;
            for (int i = 0; i < n; i++)
            {
                List<int> l;
                lock (_pool)
                {
                    l = _pool.Count > 0 ? _pool.Pop() : new List<int>(8);
                }
                for (int k = 0; k < 8; k++) l.Add(k);
                v += l.Count;
                l.Clear();
                lock (_pool) { _pool.Push(l); }
            }
            Workloads.SinkLong = v;
        }
        public static void Finalizable_New(int n)
        {
            FinalizableObj o = null;
            for (int i = 0; i < n; i++) { o = new FinalizableObj(); GC.SuppressFinalize(o); }
            Workloads.Sink = o;
        }
        public static void String_New100(int n) { string s = null; for (int i = 0; i < n; i++) s = new string('a', 100); Workloads.Sink = s; }
        public static void Boxing_Plus_Equals(int n)
        {
            bool b = false;
            for (int i = 0; i < n; i++) { object o = i; b = o.Equals(i); }
            Workloads.Sink = b;
        }
        public static void GC_Collect_Gen0(int n) { for (int i = 0; i < n; i++) GC.Collect(0, GCCollectionMode.Forced, false); }
    }
}
