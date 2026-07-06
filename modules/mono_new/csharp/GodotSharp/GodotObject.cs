using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Godot
{
    public class GodotObject : IDisposable
    {
        internal IntPtr nativeInstance;
        private bool disposed = false;

        public GodotObject()
        {
        }

        ~GodotObject()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (disposed)
                return;

            if (disposing)
            {
            }

            disposed = true;
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Object_EmitSignal(IntPtr nativePtr, string signal, object[] args);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Object_ConnectSignal(IntPtr nativePtr, string signal, Delegate callable, bool oneshot);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Object_DisconnectSignal(IntPtr nativePtr, string signal, Delegate callable);

        public void EmitSignal(StringName signal, params object[] args)
        {
            if (nativeInstance == IntPtr.Zero)
                throw new ObjectDisposedException(GetType().Name);
            godot_icall_Object_EmitSignal(nativeInstance, signal.ToString(), args);
        }

        public bool Connect(StringName signal, Callable callable, uint flags = 0)
        {
            if (nativeInstance == IntPtr.Zero)
                throw new ObjectDisposedException(GetType().Name);
            bool oneshot = (flags & (uint)ConnectFlags.OneShot) != 0;
            return godot_icall_Object_ConnectSignal(nativeInstance, signal.ToString(), callable.TargetDelegate, oneshot);
        }

        public void Disconnect(StringName signal, Callable callable)
        {
            if (nativeInstance == IntPtr.Zero)
                throw new ObjectDisposedException(GetType().Name);
            godot_icall_Object_DisconnectSignal(nativeInstance, signal.ToString(), callable.TargetDelegate);
        }

        public enum ConnectFlags : uint
        {
            Deferred = 1,
            Persist = 2,
            OneShot = 4,
            ReferenceCounted = 8
        }
    }

    public class StringName
    {
        private string value;

        public StringName(string name)
        {
            value = name;
        }

        public override string ToString()
        {
            return value;
        }

        public static implicit operator StringName(string name)
        {
            return new StringName(name);
        }
    }

    public class NodePath
    {
        private string path;

        public NodePath(string path)
        {
            this.path = path;
        }

        public override string ToString()
        {
            return path;
        }
    }

    public partial class Node : GodotObject
    {
        public Node()
        {
        }

        public virtual void _Ready() { }
        public virtual void _Process(double delta) { }
        public virtual void _PhysicsProcess(double delta) { }
        public virtual void _Input(InputEvent @event) { }
        public virtual void _UnhandledInput(InputEvent @event) { }
        public virtual void _UnhandledKeyInput(InputEvent @event) { }
        public virtual void _EnterTree() { }
        public virtual void _ExitTree() { }
    }

    public partial class Node2D : Node
    {
        public Node2D()
        {
        }
    }

    public partial class Node3D : Node
    {
        public Node3D()
        {
        }
    }

    public partial class Resource : GodotObject
    {
        public Resource()
        {
        }
    }

    public class InputEvent : GodotObject
    {
    }

    public class InputEventKey : InputEvent
    {
        public Key Keycode { get; set; }
        public bool Pressed { get; set; }
    }

    public enum Key : long
    {
        None = 0,
        Space = 32,
        A = 65, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        Escape = 4194305,
        Enter = 4194309,
        Tab = 4194308,
        Left = 4194319,
        Up = 4194320,
        Right = 4194321,
        Down = 4194322,
        Key0 = 48, Key1, Key2, Key3, Key4, Key5, Key6, Key7, Key8, Key9,
    }
}