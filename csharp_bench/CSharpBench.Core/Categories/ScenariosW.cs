using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Reflection;
using System.Text;

namespace CSharpBench.Core.Categories
{
    public class OrderPoco
    {
        public int Id; public string Customer; public double Amount; public bool Paid;
        public OrderPoco(int id, string c, double a, bool p) { Id = id; Customer = c; Amount = a; Paid = p; }
    }

    /// <summary>综合真实场景（8 项，批量语义：一次调用处理 size 元数据集）。</summary>
    public static class ScenariosW
    {
        static readonly Dictionary<int, OrderPoco[]> _orders = new Dictionary<int, OrderPoco[]>();
        static readonly Dictionary<int, string[]> _csvLines = new Dictionary<int, string[]>();
        static readonly Dictionary<int, string[]> _words = new Dictionary<int, string[]>();
        static readonly Dictionary<int, string> _texts = new Dictionary<int, string>();
        static readonly Random Rnd = new Random(42); // 固定种子保证可重复

        static OrderPoco[] Orders(int n)
        {
            lock (_orders)
            {
                OrderPoco[] a;
                if (!_orders.TryGetValue(n, out a))
                {
                    a = new OrderPoco[n];
                    var r = new Random(1234);
                    for (int i = 0; i < n; i++)
                        a[i] = new OrderPoco(i, "cust" + (i % 1000), r.NextDouble() * 500, (i & 1) == 0);
                    _orders[n] = a;
                }
                return a;
            }
        }
        static string[] CsvLines(int n)
        {
            lock (_csvLines)
            {
                string[] a;
                if (!_csvLines.TryGetValue(n, out a))
                {
                    a = new string[n];
                    for (int i = 0; i < n; i++) a[i] = "item" + i + "," + (i * 7) + "," + ((i % 997) * 0.5).ToString("F2", CultureInfo.InvariantCulture) + ",true";
                    _csvLines[n] = a;
                }
                return a;
            }
        }
        static string[] Words(int n)
        {
            lock (_words)
            {
                string[] a;
                if (!_words.TryGetValue(n, out a))
                {
                    a = new string[n];
                    for (int i = 0; i < n; i++) a[i] = "w" + (i % 1000);
                    _words[n] = a;
                }
                return a;
            }
        }
        static string Text(int n)
        {
            lock (_texts)
            {
                string t;
                if (!_texts.TryGetValue(n, out t)) { t = string.Join(" ", Words(n)); _texts[n] = t; }
                return t;
            }
        }

        public static void Add(List<Workload> l)
        {
            l.Add("Scenarios", "Json_ManualSerialize", Json_ManualSerialize);
            l.Add("Scenarios", "Json_ReflectionSerialize", Json_ReflectionSerialize);
            l.Add("Scenarios", "Csv_Parse", Csv_Parse);
            l.Add("Scenarios", "WordFreq_Dictionary", WordFreq_Dictionary);
            l.Add("Scenarios", "SortAggregate_Pipeline", SortAggregate_Pipeline);
            l.Add("Scenarios", "Text_SplitJoin", Text_SplitJoin);
            l.Add("Scenarios", "Tree_BuildWalk", Tree_BuildWalk);
            l.Add("Scenarios", "PrimeSieve", PrimeSieve);
        }

        public static void Json_ManualSerialize(int n)
        {
            var orders = Orders(n);
            var sb = new StringBuilder(n * 48);
            sb.Append('[');
            for (int i = 0; i < n; i++)
            {
                var o = orders[i];
                if (i > 0) sb.Append(',');
                sb.Append("{\"id\":").Append(o.Id)
                  .Append(",\"customer\":\"").Append(o.Customer)
                  .Append("\",\"amount\":").Append(o.Amount.ToString("F2", CultureInfo.InvariantCulture))
                  .Append(",\"paid\":").Append(o.Paid ? "true" : "false").Append('}');
            }
            sb.Append(']');
            Workloads.Sink = sb.Length;
        }

        static readonly FieldInfo[] OrderFields = typeof(OrderPoco).GetFields();
        public static void Json_ReflectionSerialize(int n)
        {
            var orders = Orders(n);
            var sb = new StringBuilder(n * 64);
            sb.Append('[');
            for (int i = 0; i < n; i++)
            {
                if (i > 0) sb.Append(',');
                sb.Append('{');
                var o = orders[i];
                bool first = true;
                foreach (var f in OrderFields)
                {
                    if (!first) sb.Append(',');
                    first = false;
                    sb.Append('"').Append(f.Name).Append("\":");
                    var v = f.GetValue(o);
                    if (v is string) sb.Append('"').Append(v).Append('"');
                    else if (v is bool) sb.Append((bool)v ? "true" : "false");
                    else sb.Append(Convert.ToString(v, CultureInfo.InvariantCulture));
                }
                sb.Append('}');
            }
            sb.Append(']');
            Workloads.Sink = sb.Length;
        }

        public static void Csv_Parse(int n)
        {
            var lines = CsvLines(n);
            double sum = 0; long cnt = 0;
            var sep = new[] { ',' };
            for (int i = 0; i < n; i++)
            {
                var p = lines[i].Split(sep);
                if (p.Length == 4)
                {
                    sum += double.Parse(p[2], CultureInfo.InvariantCulture);
                    cnt += long.Parse(p[1]);
                }
            }
            Workloads.SinkDouble = sum + cnt;
        }

        public static void WordFreq_Dictionary(int n)
        {
            var w = Words(n);
            var d = new Dictionary<string, int>(1000);
            for (int i = 0; i < n; i++)
            {
                int c;
                d.TryGetValue(w[i], out c);
                d[w[i]] = c + 1;
            }
            Workloads.SinkLong = d.Count;
        }

        public static void SortAggregate_Pipeline(int n)
        {
            var orders = Orders(n);
            double total = orders.Where(o => !o.Paid)
                                 .OrderBy(o => o.Amount)
                                 .GroupBy(o => o.Customer)
                                 .Select(g => g.Sum(o => o.Amount))
                                 .Sum();
            Workloads.SinkDouble = total;
        }

        public static void Text_SplitJoin(int n)
        {
            var parts = Text(n).Split(' ');
            Workloads.Sink = string.Join("|", parts);
        }

        public static void Tree_BuildWalk(int n)
        {
            // 数组表示完全二叉树，递归求和
            var tree = new int[n];
            for (int i = 0; i < n; i++) tree[i] = i;
            Workloads.SinkLong = Walk(tree, 0);
        }
        static long Walk(int[] t, int i)
        {
            if (i >= t.Length) return 0;
            return t[i] + Walk(t, 2 * i + 1) + Walk(t, 2 * i + 2);
        }

        public static void PrimeSieve(int n)
        {
            if (n < 3) n = 3;
            var isComposite = new bool[n];
            int count = 0;
            for (int i = 2; i < n; i++)
            {
                if (!isComposite[i])
                {
                    count++;
                    for (long j = (long)i * i; j < n; j += i) isComposite[j] = true;
                }
            }
            Workloads.SinkLong = count;
        }
    }
}
