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

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static IntPtr godot_icall_CreateObject(string className);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Node_AddChild(IntPtr parent, IntPtr child);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetString(IntPtr obj, string prop, string value);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetInt(IntPtr obj, string prop, int value);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetFloat(IntPtr obj, string prop, float value);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetBool(IntPtr obj, string prop, bool value);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetVector2(IntPtr obj, string prop, float x, float y);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetColor(IntPtr obj, string prop, float r, float g, float b, float a);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetObject(IntPtr obj, string prop, IntPtr value);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallString(IntPtr obj, string method, string arg);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallInt(IntPtr obj, string method, int arg);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallStringInt(IntPtr obj, string method, string arg1, int arg2);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallStringColor(IntPtr obj, string method, string arg1, float r, float g, float b, float a);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallStringObject(IntPtr obj, string method, string arg1, IntPtr arg2);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallNoArgs(IntPtr obj, string method);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static IntPtr godot_icall_Object_CallNoArgsObject(IntPtr obj, string method);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static double godot_icall_Object_GetFloat(IntPtr obj, string prop);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static string godot_icall_Object_GetString(IntPtr obj, string prop);

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

		public void AddChild(Node node)
		{
			godot_icall_Node_AddChild(nativeInstance, node.nativeInstance);
		}
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