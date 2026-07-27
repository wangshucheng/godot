using System;
using System.Threading;

namespace Godot
{
    public class GodotSynchronizationContext : SynchronizationContext
    {
        private static GodotSynchronizationContext instance;
        // 主线程 ID 由 C++ 侧在 Install 时设置（install_synchronization_context 调用 SetMainThreadId）。
        // 用于 Send() 判断调用方是否在主线程：若在主线程直接同步执行，否则阻塞等待。
        // 0 表示未设置，退化为原行为（直接同步执行）。
        internal static int mainThreadId = 0;

        public static GodotSynchronizationContext Instance => instance ?? (instance = new GodotSynchronizationContext());

        public override void Post(SendOrPostCallback d, object state)
        {
            GDMonoAccess.PostSyncCallback(() => d(state));
        }

        // H7 修复: Send 语义改为符合 SynchronizationContext 标准。
        // - 主线程调用：直接同步执行（避免死锁，因为主线程 pump 在 frame tick 中）
        // - 非主线程调用：投递到主线程队列，阻塞调用线程直到主线程执行完毕
        //   用 ManualResetEventSlim 同步等待（不带超时，因为游戏主线程必须能继续 pump）
        public override void Send(SendOrPostCallback d, object state)
        {
            if (mainThreadId == 0 || System.Threading.Thread.CurrentThread.ManagedThreadId == mainThreadId)
            {
                d(state);
                return;
            }
            // 跨线程: 投递并等待
            using (var done = new ManualResetEventSlim(false))
            {
                Exception captured = null;
                GDMonoAccess.PostSyncCallback(() =>
                {
                    try { d(state); }
                    catch (Exception e) { captured = e; }
                    finally { done.Set(); }
                });
                done.Wait();
                if (captured != null)
                {
                    throw new System.Reflection.TargetInvocationException("Send callback threw", captured);
                }
            }
        }

        public override SynchronizationContext CreateCopy()
        {
            return this;
        }

        public static void Install()
        {
            // Note: SynchronizationContext.SetSynchronizationContext triggers mscorlib
            // internal icalls that cause WASM function signature mismatch in interpreter mode.
            // Async/await is not supported in WASM anyway. The sync context is installed
            // only on desktop platforms via the C++ side (gd_mono.cpp skips this in WEB_ENABLED).
#if !WEB_ENABLED
            mainThreadId = System.Threading.Thread.CurrentThread.ManagedThreadId;
            SynchronizationContext.SetSynchronizationContext(Instance);
#endif
        }
    }

    internal static class GDMonoAccess
    {
        [System.Runtime.CompilerServices.MethodImpl(System.Runtime.CompilerServices.MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_PostSyncCallback(Action callback);

        public static void PostSyncCallback(Action callback)
        {
            if (callback != null)
            {
                godot_icall_PostSyncCallback(callback);
            }
        }
    }

    [AttributeUsage(AttributeTargets.Method)]
    public class GodotMethodAttribute : Attribute
    {
        public string MethodName { get; }

        public GodotMethodAttribute(string methodName = null)
        {
            MethodName = methodName;
        }
    }

    [AttributeUsage(AttributeTargets.Class)]
    public class GodotClassNameAttribute : Attribute
    {
        public string ClassName { get; }

        public GodotClassNameAttribute(string className)
        {
            ClassName = className;
        }
    }

    public static class SignalName
    {
        public const string Ready = "ready";
        public const string Process = "process";
        public const string PhysicsProcess = "physics_process";
        public const string TreeEntered = "tree_entered";
        public const string TreeExiting = "tree_exiting";
        public const string TreeExited = "tree_exited";
        public const string Renamed = "renamed";
        public const string ChildEnteredTree = "child_entered_tree";
        public const string ChildExitingTree = "child_exiting_tree";
    }

    public static class MethodName
    {
        public const string Ready = "_Ready";
        public const string Process = "_Process";
        public const string PhysicsProcess = "_PhysicsProcess";
        public const string Input = "_Input";
        public const string UnhandledInput = "_UnhandledInput";
        public const string EnterTree = "_EnterTree";
        public const string ExitTree = "_ExitTree";
    }
}