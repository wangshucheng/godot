using System;
using System.Threading;

namespace Godot
{
    public class GodotSynchronizationContext : SynchronizationContext
    {
        private static GodotSynchronizationContext instance;

        public static GodotSynchronizationContext Instance => instance ?? (instance = new GodotSynchronizationContext());

        public override void Post(SendOrPostCallback d, object state)
        {
            GDMonoAccess.PostSyncCallback(() => d(state));
        }

        public override void Send(SendOrPostCallback d, object state)
        {
            d(state);
        }

        public override SynchronizationContext CreateCopy()
        {
            return this;
        }

        public static void Install()
        {
            SynchronizationContext.SetSynchronizationContext(Instance);
        }
    }

    internal static class GDMonoAccess
    {
        [System.Runtime.CompilerServices.MethodImpl(System.Runtime.CompilerServices.MethodImplOptions.InternalCall)]
        public extern static void PostSyncCallback(Action callback);
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