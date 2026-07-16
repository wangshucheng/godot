using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Serialization;

namespace Godot
{
    public class GodotObject : IDisposable
    {
        internal int nativeInstance;
        // 该 C# 对象是否"拥有"其原生对象。仅当 C# 构造函数显式创建原生对象
        // (godot_icall_CreateObject) 时为 true；由引擎创建、仅被包装的对象
        // (Attach / 场景加载节点) 为 false，Dispose 时不可释放，否则会导致
        // 引擎仍在使用中的原生对象被提前释放 (use-after-free)。
        internal bool ownsNative = false;
        private bool disposed = false;

        public GodotObject()
        {
        }

        // 包装一个已存在的原生对象 (不触发任何 CreateObject, 避免为子节点
        // /资源额外创建孤儿原生对象导致内存泄漏)。仅用于从 C++ 侧返回的指针。
        // 在 WASM 解释器下 GetUninitializedObject 比 new + 覆盖字段更安全。
        internal static T Attach<T>(int p_native) where T : GodotObject
        {
            // net48 (Mono 6.12) 无 RuntimeHelpers.GetUninitializedObject (仅 .NET 5+ 提供)，
            // 使用 FormatterServices 的等价 API 创建跳过构造函数的空对象，
            // 仅用于包装 C++ 侧已有的原生对象，避免重复 CreateObject 造成泄漏。
            T obj = (T)FormatterServices.GetUninitializedObject(typeof(T));
            obj.nativeInstance = p_native;
            return obj;
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

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Object_Free(int ptr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Object_Delete(int ptr);

        protected virtual void Dispose(bool disposing)
        {
            if (disposed)
                return;

            if (nativeInstance != 0 && ownsNative)
            {
                godot_icall_Object_Free(nativeInstance);
                nativeInstance = 0;
            }

        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Object_EmitSignal(int nativePtr, string signal, object[] args);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Object_ConnectSignal(int nativePtr, string signal, Delegate callable, bool oneshot);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Object_DisconnectSignal(int nativePtr, string signal, Delegate callable);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static int godot_icall_CreateObject(string className);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Node_AddChild(int parent, int child);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetString(int obj, string prop, string value);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetInt(int obj, string prop, int value);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetFloat(int obj, string prop, int valueBits);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetBool(int obj, string prop, bool value);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetVector2(int obj, string prop, int xBits, int yBits);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetColor(int obj, string prop, int rBits, int gBits, int bBits, int aBits);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetObject(int obj, string prop, int value);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallString(int obj, string method, string arg);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallInt(int obj, string method, int arg);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallStringInt(int obj, string method, string arg1, long arg2);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallStringColor(int obj, string method, string arg1, float r, float g, float b, float a);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallStringObject(int obj, string method, string arg1, int arg2);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallNoArgs(int obj, string method);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static int godot_icall_Object_CallNoArgsObject(int obj, string method);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallDeferred(int obj, string method, string arg);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallDeferredNoArgs(int obj, string method);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_QueueFree(int obj);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static string godot_icall_Object_GetClass(int obj);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static bool godot_icall_Object_IsClass(int obj, string className);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static long godot_icall_Object_GetFloat(int obj, string prop);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static string godot_icall_Object_GetString(int obj, string prop);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static long godot_icall_InputEventKey_GetKeycode(int nativePtr);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static bool godot_icall_InputEventKey_IsPressed(int nativePtr);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static long godot_icall_Object_CallNoArgsInt(int obj, string method);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static long godot_icall_Object_CallStringReturnsInt(int obj, string method, string arg);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static byte[] godot_icall_PacketPeer_GetPacket(int obj);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static bool godot_icall_Object_CallNoArgsBool(int obj, string method);

        public void EmitSignal(StringName signal, params object[] args)
        {
            if (nativeInstance == 0)
                throw new ObjectDisposedException(GetType().Name);
            godot_icall_Object_EmitSignal(nativeInstance, signal.ToString(), args);
        }

        public void CallDeferred(string method, string arg = null)
        {
            if (nativeInstance == 0)
                throw new ObjectDisposedException(GetType().Name);
            if (arg != null)
                godot_icall_Object_CallDeferred(nativeInstance, method, arg);
            else
                godot_icall_Object_CallDeferredNoArgs(nativeInstance, method);
        }

        public void QueueFree()
        {
            if (nativeInstance != 0)
            {
                godot_icall_Object_QueueFree(nativeInstance);
            }
        }

        public string GetClass()
        {
            if (nativeInstance == 0)
                return GetType().Name;
            return godot_icall_Object_GetClass(nativeInstance);
        }

        public bool IsClass(string className)
        {
            if (nativeInstance == 0)
                return false;
            return godot_icall_Object_IsClass(nativeInstance, className);
        }

        public bool Connect(StringName signal, Callable callable, uint flags = 0)
        {
            if (nativeInstance == 0)
                throw new ObjectDisposedException(GetType().Name);
            bool oneshot = (flags & (uint)ConnectFlags.OneShot) != 0;
            return godot_icall_Object_ConnectSignal(nativeInstance, signal.ToString(), callable.TargetDelegate, oneshot);
        }

        public void Disconnect(StringName signal, Callable callable)
        {
            if (nativeInstance == 0)
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

    public partial class SceneTree : Node
    {
        public SceneTree() { }

        public void Quit()
        {
            godot_icall_Object_CallNoArgs(nativeInstance, "quit");
        }
    }

    public partial class Node : GodotObject
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int godot_icall_Node_GetParent(int node);

        public Node()
        {
        }

        public Node GetParent()
        {
            int ptr = godot_icall_Node_GetParent(nativeInstance);
            if (ptr == 0) return null;
            return WrapNode(ptr);
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

		public SceneTree GetTree()
		{
			int tree = godot_icall_Object_CallNoArgsObject(nativeInstance, "get_tree");
			if (tree == 0) return null;
			SceneTree st = new SceneTree();
			st.nativeInstance = tree;
			return st;
		}
    }

    public partial class Node2D : Node
    {
        public Node2D()
        {
            if (nativeInstance == 0)
            {
                nativeInstance = godot_icall_CreateObject("Node2D");
                ownsNative = true;
            }
        }

    public Vector2 Position
    {
        set { godot_icall_Object_SetVector2(nativeInstance, "position", new FloatIntUnion { FloatValue = value.x }.IntValue, new FloatIntUnion { FloatValue = value.y }.IntValue); }
        get
        {
            long xBits = godot_icall_Object_GetFloat(nativeInstance, "position:x");
            long yBits = godot_icall_Object_GetFloat(nativeInstance, "position:y");
            double x = new DoubleLongUnion { LongValue = xBits }.DoubleValue;
            double y = new DoubleLongUnion { LongValue = yBits }.DoubleValue;
            return new Vector2((float)x, (float)y);
        }
    }

    public Vector2 GlobalPosition
    {
        set { godot_icall_Object_SetVector2(nativeInstance, "global_position", new FloatIntUnion { FloatValue = value.x }.IntValue, new FloatIntUnion { FloatValue = value.y }.IntValue); }
    }

    public float Rotation
    {
        set { godot_icall_Object_SetFloat(nativeInstance, "rotation", new FloatIntUnion { FloatValue = value }.IntValue); }
    }

    public Vector2 Scale
    {
        set { godot_icall_Object_SetVector2(nativeInstance, "scale", new FloatIntUnion { FloatValue = value.x }.IntValue, new FloatIntUnion { FloatValue = value.y }.IntValue); }
    }
    }

    public partial class Node3D : Node
    {
        public Node3D()
        {
            if (nativeInstance == 0)
            {
                nativeInstance = godot_icall_CreateObject("Node3D");
                ownsNative = true;
            }
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
        public long Keycode
        {
            get { return godot_icall_InputEventKey_GetKeycode(nativeInstance); }
        }

        public bool Pressed
        {
            get { return godot_icall_InputEventKey_IsPressed(nativeInstance); }
        }
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
