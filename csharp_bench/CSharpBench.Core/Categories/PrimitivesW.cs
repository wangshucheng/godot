using System;
using System.Collections.Generic;
using System.Text;

namespace CSharpBench.Core.Categories
{
    /// <summary>基础数据类型操作（14 项）。size = 基本操作重复次数。</summary>
    public static class PrimitivesW
    {
        static string[] _numStrings;
        static string GetNum(int i)
        {
            if (_numStrings == null) _numStrings = new string[64];
            int k = i & 63;
            if (_numStrings[k] == null) _numStrings[k] = NumStr(k * 977);
            return _numStrings[k];
        }

        // 手动构造数字字符串（字符算术）。绕开 Mono 6.12 Int32.ToString 的内部
        // 数组填充路径 —— 该路径在大规模迭代下触发运行时断言
        // ("Array fill produced wrong size")，破坏 Mono 状态导致引擎连锁崩溃。
        static string NumStr(int v)
        {
            if (v == 0) return "0";
            var c = new char[12];
            int len = 0;
            while (v > 0) { c[len++] = (char)('0' + (v % 10)); v /= 10; }
            var s = new char[len];
            for (int i = 0; i < len; i++) s[i] = c[len - 1 - i];
            return new string(s);
        }

        public static void Add(List<Workload> l)
        {
            l.Add("Primitives", "Int_Add_Loop", Int_Add_Loop);
            l.Add("Primitives", "Double_Mul_Loop", Double_Mul_Loop);
            l.Add("Primitives", "Decimal_Add_Loop", Decimal_Add_Loop);
            l.Add("Primitives", "Int_Boxing", Int_Boxing);
            l.Add("Primitives", "String_Concat_Plus", String_Concat_Plus);
            l.Add("Primitives", "String_Concat_Builder", String_Concat_Builder);
            l.Add("Primitives", "String_Concat_Interp", String_Concat_Interp);
            l.Add("Primitives", "Int_Parse", Int_Parse);
            l.Add("Primitives", "Int_TryParse", Int_TryParse);
            l.Add("Primitives", "Double_Parse", Double_Parse);
            l.Add("Primitives", "Guid_NewGuid", Guid_NewGuid);
            l.Add("Primitives", "DateTime_Now", DateTime_Now);
            l.Add("Primitives", "Enum_Parse", Enum_Parse);
            l.Add("Primitives", "String_Compare_Ordinal", String_Compare_Ordinal);
        }

        public static void Int_Add_Loop(int n) { long s = 0; for (int i = 0; i < n; i++) s += i * 3; Workloads.SinkLong = s; }
        public static void Double_Mul_Loop(int n) { double x = 1.0; for (int i = 0; i < n; i++) x *= 1.0000001; Workloads.SinkDouble = x; }
        public static void Decimal_Add_Loop(int n) { decimal d = 0; for (int i = 0; i < n; i++) d += 0.01m; Workloads.Sink = d; }
        public static void Int_Boxing(int n) { object o = null; for (int i = 0; i < n; i++) o = i; Workloads.Sink = o; }

        public static void String_Concat_Plus(int n)
        {
            string s = "a";
            for (int i = 0; i < n; i++) s = "a" + i;
            Workloads.Sink = s;
        }
        public static void String_Concat_Builder(int n)
        {
            var sb = new StringBuilder();
            for (int i = 0; i < n; i++) sb.Append('a').Append(i);
            Workloads.Sink = sb.ToString();
        }
        public static void String_Concat_Interp(int n)
        {
            string s = "";
            for (int i = 0; i < n; i++) s = $"{s.Length}:{i}";
            Workloads.Sink = s;
        }
        public static void Int_Parse(int n)
        {
            long v = 0;
            for (int i = 0; i < n; i++) v += int.Parse(GetNum(i));
            Workloads.SinkLong = v;
        }
        public static void Int_TryParse(int n)
        {
            long v = 0; int r;
            for (int i = 0; i < n; i++) { if (int.TryParse(GetNum(i), out r)) v += r; }
            Workloads.SinkLong = v;
        }
        public static void Double_Parse(int n)
        {
            double v = 0;
            for (int i = 0; i < n; i++) v += double.Parse(GetNum(i));
            Workloads.SinkDouble = v;
        }
        public static void Guid_NewGuid(int n)
        {
            Guid g = Guid.Empty;
            for (int i = 0; i < n; i++) g = Guid.NewGuid();
            Workloads.Sink = g;
        }
        public static void DateTime_Now(int n)
        {
            DateTime t = DateTime.MinValue;
            for (int i = 0; i < n; i++) t = DateTime.Now;
            Workloads.Sink = t;
        }
        public static void Enum_Parse(int n)
        {
            object e = null;
            for (int i = 0; i < n; i++) e = Enum.Parse(typeof(DayOfWeek), GetNum(i % 7));
            Workloads.Sink = e;
        }
        public static void String_Compare_Ordinal(int n)
        {
            string a = GetNum(1), b = GetNum(2); int c = 0;
            for (int i = 0; i < n; i++) c += string.Compare(a, b, StringComparison.Ordinal);
            Workloads.SinkLong = c;
        }
    }
}
