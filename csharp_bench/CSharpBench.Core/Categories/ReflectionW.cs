using System;
using System.Collections.Generic;
using System.Linq.Expressions;
using System.Reflection;

namespace CSharpBench.Core.Categories
{
    // 反射目标类型
    public class RefPoco
    {
        public int A { get; set; }
        public string B { get; set; }
        public RefPoco() { B = "x"; }
        public int InstanceMethod(int x) { return x + 1; }
        public static int StaticMethod(int x) { return x + 2; }
    }
    [Serializable]
    public class RefAttr : Attribute { public string Tag = "t"; }

    /// <summary>反射（10 项，规模上限 10k）。</summary>
    public static class ReflectionW
    {
        static readonly Type T = typeof(RefPoco);
        static readonly MethodInfo MiInst = T.GetMethod("InstanceMethod");
        static readonly MethodInfo MiStatic = T.GetMethod("StaticMethod");
        static readonly PropertyInfo PiA = T.GetProperty("A");
    static readonly PropertyInfo[] Props = T.GetProperties();
        static readonly RefPoco Inst = new RefPoco();
        static readonly MemberInfo CtorInfo = typeof(RefAttr).GetCustomAttribute<RefAttr>() == null ? null : T;

        static readonly MethodInfo PiAGetter = PiA.GetGetMethod();

        public static void Add(List<Workload> l)
        {
            l.AddSmall("Reflection", "Type_GetMethod", Type_GetMethod);
            l.AddSmall("Reflection", "Method_Invoke_Instance", Method_Invoke_Instance);
            l.AddSmall("Reflection", "Method_Invoke_Static", Method_Invoke_Static);
            l.AddSmall("Reflection", "Property_GetValue", Property_GetValue);
            // Mono 6.12 规避路径：PropertyInfo.GetValue 在 warm 重复循环（数千次调用）下
            // 触发运行时状态损坏（进程级崩溃，try/catch 不可捕获）；冷启动单次调用正常。
            // 经 getter.Invoke() 走 MethodInfo.Invoke 路径在 Mono 下稳定（见 Method_Invoke_Instance），
            // 故提供本对照负载：所有运行时可对比两条路径的成本差异，Mono 列以此替代直接测量。
            l.AddSmall("Reflection", "Property_GetValue_ViaGetter", Property_GetValue_ViaGetter);
            l.AddSmall("Reflection", "Property_SetValue", Property_SetValue);
            l.AddSmall("Reflection", "Type_GetProperties", Type_GetProperties);
            l.AddSmall("Reflection", "CreateDelegate_ThenInvoke", CreateDelegate_ThenInvoke);
            l.AddSmall("Reflection", "Expression_Compile", Expression_Compile);
            l.AddSmall("Reflection", "Activator_CreateInstance", Activator_CreateInstance);
            l.AddSmall("Reflection", "GetCustomAttribute", GetCustomAttribute);
        }

        public static void Type_GetMethod(int n) { MethodInfo m = null; for (int i = 0; i < n; i++) m = T.GetMethod("InstanceMethod"); Workloads.Sink = m; }
        public static void Method_Invoke_Instance(int n) { object r = null; var ps = new object[] { 1 }; for (int i = 0; i < n; i++) r = MiInst.Invoke(Inst, ps); Workloads.Sink = r; }
        public static void Method_Invoke_Static(int n) { object r = null; var ps = new object[] { 1 }; for (int i = 0; i < n; i++) r = MiStatic.Invoke(null, ps); Workloads.Sink = r; }
        public static void Property_GetValue(int n) { object r = null; for (int i = 0; i < n; i++) r = PiA.GetValue(Inst); Workloads.Sink = r; }
        public static void Property_GetValue_ViaGetter(int n) { object r = null; for (int i = 0; i < n; i++) r = PiAGetter.Invoke(Inst, null); Workloads.Sink = r; }
        public static void Property_SetValue(int n) { for (int i = 0; i < n; i++) PiA.SetValue(Inst, i); }
        public static void Type_GetProperties(int n) { PropertyInfo[] p = null; for (int i = 0; i < n; i++) p = T.GetProperties(); Workloads.Sink = p; }
        public static void CreateDelegate_ThenInvoke(int n)
        {
            long s = 0;
            for (int i = 0; i < n; i++)
            {
                var d = (Func<RefPoco, int, int>)Delegate.CreateDelegate(typeof(Func<RefPoco, int, int>), MiInst);
                s += d(Inst, 1);
            }
            Workloads.SinkLong = s;
        }
        public static void Expression_Compile(int n)
        {
            for (int i = 0; i < n; i++)
            {
                var p1 = Expression.Parameter(typeof(RefPoco), "o");
                var p2 = Expression.Parameter(typeof(int), "x");
                var f = Expression.Lambda<Func<RefPoco, int, int>>(
                    Expression.Call(p1, MiInst, p2), p1, p2).Compile();
                Workloads.Sink = f;
            }
        }
        public static void Activator_CreateInstance(int n) { object o = null; for (int i = 0; i < n; i++) o = Activator.CreateInstance<RefPoco>(); Workloads.Sink = o; }
        public static void GetCustomAttribute(int n)
        {
            RefAttr a = null;
            var mi = MiInst;
            for (int i = 0; i < n; i++) a = mi.GetCustomAttribute<RefAttr>();
            Workloads.Sink = a;
        }
    }
}
