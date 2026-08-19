using System;
using System.Collections.Generic;

namespace CSharpBench.Core.Categories
{
    public interface IAdder { int Add(int x); }
    public sealed class AdderImpl : IAdder { public int Add(int x) { return x + 1; } }

    /// <summary>委托与事件（10 项）。</summary>
    public static class DelegatesW
    {
        static readonly Action A0 = NoOp;
        static readonly Action<int> A1 = TakeInt;
        static readonly Func<int, int> F1 = MapInt;
        static Action Multicast2 = NoOp;
        static Action Multicast8 = NoOp;
        static readonly IAdder Iface = new AdderImpl();
        static readonly AdderImpl Concrete = new AdderImpl();
        static event EventHandler<int> TestEvent;

        static void NoOp() { }
        static void TakeInt(int x) { Workloads.SinkLong = x; }
        static int MapInt(int x) { return x + 1; }

        static DelegatesW()
        {
            Multicast2 = NoOp; Multicast2 += NoOp;
            Multicast8 = NoOp;
            for (int i = 0; i < 7; i++) Multicast8 += NoOp;
        }

        public static void Add(List<Workload> l)
        {
            l.Add("Delegates", "Action_Invoke", Action_Invoke);
            l.Add("Delegates", "ActionInt_Invoke", ActionInt_Invoke);
            l.Add("Delegates", "FuncInt_Invoke", FuncInt_Invoke);
            l.Add("Delegates", "Multicast2_Invoke", Multicast2_Invoke);
            l.Add("Delegates", "Multicast8_Invoke", Multicast8_Invoke);
            l.Add("Delegates", "StaticLambdaNoCapture", StaticLambdaNoCapture);
            l.Add("Delegates", "ClosureCapture_Invoke", ClosureCapture_Invoke);
            l.Add("Delegates", "Event_AddRemove", Event_AddRemove);
            l.Add("Delegates", "Interface_vs_DirectCall", Interface_vs_DirectCall);
            l.Add("Delegates", "NewDelegate_Creation", NewDelegate_Creation);
        }

        public static void Action_Invoke(int n) { for (int i = 0; i < n; i++) A0(); }
        public static void ActionInt_Invoke(int n) { for (int i = 0; i < n; i++) A1(i); }
        public static void FuncInt_Invoke(int n) { long s = 0; for (int i = 0; i < n; i++) s += F1(i); Workloads.SinkLong = s; }
        public static void Multicast2_Invoke(int n) { for (int i = 0; i < n; i++) Multicast2(); }
        public static void Multicast8_Invoke(int n) { for (int i = 0; i < n; i++) Multicast8(); }

        public static void StaticLambdaNoCapture(int n)
        {
            Func<int, int> f = static x => x + 1;
            long s = 0;
            for (int i = 0; i < n; i++) s += f(i);
            Workloads.SinkLong = s;
        }

        public static void ClosureCapture_Invoke(int n)
        {
            // 每次迭代新建闭包（分配 + 调用成本）
            long s = 0;
            for (int i = 0; i < n; i++)
            {
                int j = i;
                Func<int, int> f = x => j + x + 1;
                s += f(0);
            }
            Workloads.SinkLong = s;
        }

        public static void Event_AddRemove(int n)
        {
            for (int i = 0; i < n; i++)
            {
                TestEvent += OnEvt;
                TestEvent -= OnEvt;
            }
        }
        static void OnEvt(object s, int e) { }

        public static void Interface_vs_DirectCall(int n)
        {
            long s = 0;
            for (int i = 0; i < n; i++) { s += Iface.Add(i); s -= Concrete.Add(i); }
            Workloads.SinkLong = s;
        }

        public static void NewDelegate_Creation(int n)
        {
            Action a = null;
            for (int i = 0; i < n; i++) a = new Action(NoOp);
            Workloads.Sink = a;
        }
    }
}
