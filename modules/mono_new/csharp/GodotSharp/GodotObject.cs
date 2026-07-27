using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Godot
{
    public class GodotObject : IDisposable
    {
        internal long nativeInstance;
        // 该 C# 对象是否"拥有"其原生对象。仅当 C# 构造函数显式创建原生对象
        // (godot_icall_CreateObject) 时为 true；由引擎创建、仅被包装的对象
        // (Attach / 场景加载节点) 为 false，Dispose 时不可释放，否则会导致
        // 引擎仍在使用中的原生对象被提前释放 (use-after-free)。
        internal bool ownsNative = false;
        private bool disposed = false;

        public GodotObject()
        {
        }

        // 注意：WASM 解释器下不能用 FormatterServices.GetUninitializedObject 或泛型
        // new T() (where T : new()) —— 两者都会触发 mscorlib 内部 icall 签名不匹配
        // (与 GodotSynchronizationContext 同因)。所有包装必须用具体类型的 new 构造。

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
        internal extern static void godot_icall_Object_Free(long ptr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Object_Delete(long ptr);

        protected virtual void Dispose(bool disposing)
        {
            if (disposed)
                return;
            disposed = true;

            if (nativeInstance != 0 && ownsNative)
            {
                godot_icall_Object_Free(nativeInstance);
                nativeInstance = 0;
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Object_EmitSignal(long nativePtr, string signal, object[] args);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Object_ConnectSignal(long nativePtr, string signal, Delegate callable, uint flags);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Object_DisconnectSignal(long nativePtr, string signal, Delegate callable);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Object_ConnectSignalNative(long nativePtr, string signal, long callablePtr, uint flags);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Object_DisconnectSignalNative(long nativePtr, string signal, long callablePtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Object_HasSignal(long nativePtr, string signal);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Object_IsConnected(long nativePtr, string signal, long callablePtr);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static long godot_icall_CreateObject(string className);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Node_AddChild(long parent, long child);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetString(long obj, string prop, string value);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetInt(long obj, string prop, long value);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetFloat(long obj, string prop, int valueBits);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetBool(long obj, string prop, bool value);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetVector2(long obj, string prop, int xBits, int yBits);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetColor(long obj, string prop, int rBits, int gBits, int bBits, int aBits);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetObject(long obj, string prop, long value);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallString(long obj, string method, string arg);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallInt(long obj, string method, long arg);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallStringInt(long obj, string method, string arg1, long arg2);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallStringColor(long obj, string method, string arg1, int r_bits, int g_bits, int b_bits, int a_bits);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallStringObject(long obj, string method, string arg1, long arg2);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallNoArgs(long obj, string method);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static long godot_icall_Object_CallNoArgsObject(long obj, string method);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallDeferred(long obj, string method, string arg);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_CallDeferredNoArgs(long obj, string method);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_QueueFree(long obj);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static string godot_icall_Object_GetClass(long obj);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static bool godot_icall_Object_IsClass(long obj, string className);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static long godot_icall_Object_GetFloat(long obj, string prop);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static string godot_icall_Object_GetString(long obj, string prop);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static long godot_icall_InputEventKey_GetKeycode(long nativePtr);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static bool godot_icall_InputEventKey_IsPressed(long nativePtr);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static long godot_icall_Object_CallNoArgsInt(long obj, string method);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static long godot_icall_Object_CallStringReturnsInt(long obj, string method, string arg);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static byte[] godot_icall_PacketPeer_GetPacket(long obj);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static bool godot_icall_Object_CallNoArgsBool(long obj, string method);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static object godot_icall_Object_GetVariant(long obj, string prop);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static void godot_icall_Object_SetVariant(long obj, string prop, object value);

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static object godot_icall_Object_CallVariant(long obj, string method, object[] args);

        public T Get<T>(string property)
        {
            if (nativeInstance == 0)
                throw new ObjectDisposedException(GetType().Name);
            object result = godot_icall_Object_GetVariant(nativeInstance, property);
            if (result == null) return default(T);
            return (T)result;
        }

        public void Set(string property, object value)
        {
            if (nativeInstance == 0)
                throw new ObjectDisposedException(GetType().Name);
            godot_icall_Object_SetVariant(nativeInstance, property, value);
        }

        public object Call(string method, params object[] args)
        {
            if (nativeInstance == 0)
                throw new ObjectDisposedException(GetType().Name);
            return godot_icall_Object_CallVariant(nativeInstance, method, args);
        }

        public T Call<T>(string method, params object[] args)
        {
            object result = Call(method, args);
            if (result == null) return default(T);
            return (T)result;
        }

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
            // H7 扩展: 透传完整 ConnectFlags（Deferred/Persist/OneShot/ReferenceCounted）
            // Prefer the native callable path (target+method) when available, since it
            // avoids delegate marshalling. Fall back to the delegate path otherwise.
            if (callable != null && callable.nativeCallable != 0)
            {
                return godot_icall_Object_ConnectSignalNative(nativeInstance, signal.ToString(), callable.nativeCallable, flags);
            }
            if (callable != null && callable.TargetDelegate != null)
            {
                return godot_icall_Object_ConnectSignal(nativeInstance, signal.ToString(), callable.TargetDelegate, flags);
            }
            return false;
        }

        /// <summary>
        /// 检查对象是否拥有指定信号（包括 [Signal] 声明的用户信号与原生信号）。
        /// </summary>
        public bool HasSignal(StringName signal)
        {
            if (nativeInstance == 0)
                throw new ObjectDisposedException(GetType().Name);
            return godot_icall_Object_HasSignal(nativeInstance, signal.ToString());
        }

        /// <summary>
        /// 检查指定 callable 是否已连接到信号。
        /// </summary>
        public bool IsConnected(StringName signal, Callable callable)
        {
            if (nativeInstance == 0)
                throw new ObjectDisposedException(GetType().Name);
            if (callable == null || callable.nativeCallable == 0)
                return false;
            return godot_icall_Object_IsConnected(nativeInstance, signal.ToString(), callable.nativeCallable);
        }

        public void Disconnect(StringName signal, Callable callable)
        {
            if (nativeInstance == 0)
                throw new ObjectDisposedException(GetType().Name);
            if (callable != null && callable.nativeCallable != 0)
            {
                godot_icall_Object_DisconnectSignalNative(nativeInstance, signal.ToString(), callable.nativeCallable);
                return;
            }
            if (callable != null && callable.TargetDelegate != null)
            {
                godot_icall_Object_DisconnectSignal(nativeInstance, signal.ToString(), callable.TargetDelegate);
            }
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
        internal extern static long godot_icall_Node_GetParent(long node);

        public Node()
        {
        }

        public Node GetParent()
        {
            long ptr = godot_icall_Node_GetParent(nativeInstance);
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
			if (nativeInstance == 0) return null;
			long tree = godot_icall_Object_CallNoArgsObject(nativeInstance, "get_tree");
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
        // Position / Rotation / Scale / GlobalPosition / GlobalRotation / GlobalScale
        // and related transforms are implemented in NodeExt.cs via direct Node2D
        // C++ method icalls (see glue/glue_cpp/node2d_glue.cpp) for performance.
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

        // --- Node3D icalls (see glue/glue_cpp/node3d_glue.cpp) ---
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node3D_SetPosition(long node, int xBits, int yBits, int zBits);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetPositionX(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetPositionY(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetPositionZ(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node3D_SetRotation(long node, int xBits, int yBits, int zBits);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetRotationX(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetRotationY(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetRotationZ(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node3D_SetScale(long node, int xBits, int yBits, int zBits);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetScaleX(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetScaleY(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetScaleZ(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node3D_SetGlobalPosition(long node, int xBits, int yBits, int zBits);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetGlobalPositionX(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetGlobalPositionY(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetGlobalPositionZ(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node3D_SetGlobalRotation(long node, int xBits, int yBits, int zBits);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetGlobalRotationX(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetGlobalRotationY(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetGlobalRotationZ(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node3D_GlobalScale(long node, int xBits, int yBits, int zBits);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node3D_SetVisible(long node, bool visible);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Node3D_IsVisible(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node3D_SetTransform(long node,
            int b0x, int b0y, int b0z, int b1x, int b1y, int b1z,
            int b2x, int b2y, int b2z, int ox, int oy, int oz);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetTransformB0X(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetTransformB0Y(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetTransformB0Z(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetTransformB1X(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetTransformB1Y(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetTransformB1Z(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetTransformB2X(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetTransformB2Y(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetTransformB2Z(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetTransformOX(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetTransformOY(long node);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node3D_GetTransformOZ(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node3D_RotateX(long node, int angleBits);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node3D_RotateY(long node, int angleBits);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node3D_RotateZ(long node, int angleBits);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node3D_Translate(long node, int xBits, int yBits, int zBits);

        // --- Public API ---
        public Vector3 Position
        {
            get
            {
                if (nativeInstance == 0) return Vector3.Zero;
                return new Vector3(
                    new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetPositionX(nativeInstance) }.FloatValue,
                    new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetPositionY(nativeInstance) }.FloatValue,
                    new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetPositionZ(nativeInstance) }.FloatValue);
            }
            set
            {
                if (nativeInstance == 0) return;
                godot_icall_Node3D_SetPosition(nativeInstance,
                    new FloatIntUnion { FloatValue = value.x }.IntValue,
                    new FloatIntUnion { FloatValue = value.y }.IntValue,
                    new FloatIntUnion { FloatValue = value.z }.IntValue);
            }
        }

        public Vector3 Rotation
        {
            get
            {
                if (nativeInstance == 0) return Vector3.Zero;
                return new Vector3(
                    new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetRotationX(nativeInstance) }.FloatValue,
                    new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetRotationY(nativeInstance) }.FloatValue,
                    new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetRotationZ(nativeInstance) }.FloatValue);
            }
            set
            {
                if (nativeInstance == 0) return;
                godot_icall_Node3D_SetRotation(nativeInstance,
                    new FloatIntUnion { FloatValue = value.x }.IntValue,
                    new FloatIntUnion { FloatValue = value.y }.IntValue,
                    new FloatIntUnion { FloatValue = value.z }.IntValue);
            }
        }

        public Vector3 Scale
        {
            get
            {
                if (nativeInstance == 0) return Vector3.One;
                return new Vector3(
                    new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetScaleX(nativeInstance) }.FloatValue,
                    new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetScaleY(nativeInstance) }.FloatValue,
                    new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetScaleZ(nativeInstance) }.FloatValue);
            }
            set
            {
                if (nativeInstance == 0) return;
                godot_icall_Node3D_SetScale(nativeInstance,
                    new FloatIntUnion { FloatValue = value.x }.IntValue,
                    new FloatIntUnion { FloatValue = value.y }.IntValue,
                    new FloatIntUnion { FloatValue = value.z }.IntValue);
            }
        }

        public Vector3 GlobalPosition
        {
            get
            {
                if (nativeInstance == 0) return Vector3.Zero;
                return new Vector3(
                    new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetGlobalPositionX(nativeInstance) }.FloatValue,
                    new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetGlobalPositionY(nativeInstance) }.FloatValue,
                    new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetGlobalPositionZ(nativeInstance) }.FloatValue);
            }
            set
            {
                if (nativeInstance == 0) return;
                godot_icall_Node3D_SetGlobalPosition(nativeInstance,
                    new FloatIntUnion { FloatValue = value.x }.IntValue,
                    new FloatIntUnion { FloatValue = value.y }.IntValue,
                    new FloatIntUnion { FloatValue = value.z }.IntValue);
            }
        }

        public Vector3 GlobalRotation
        {
            get
            {
                if (nativeInstance == 0) return Vector3.Zero;
                return new Vector3(
                    new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetGlobalRotationX(nativeInstance) }.FloatValue,
                    new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetGlobalRotationY(nativeInstance) }.FloatValue,
                    new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetGlobalRotationZ(nativeInstance) }.FloatValue);
            }
            set
            {
                if (nativeInstance == 0) return;
                godot_icall_Node3D_SetGlobalRotation(nativeInstance,
                    new FloatIntUnion { FloatValue = value.x }.IntValue,
                    new FloatIntUnion { FloatValue = value.y }.IntValue,
                    new FloatIntUnion { FloatValue = value.z }.IntValue);
            }
        }

        // Applies a global scale multiplier to the node (verb form).
        public void ApplyGlobalScale(Vector3 scale)
        {
            if (nativeInstance == 0) return;
            godot_icall_Node3D_GlobalScale(nativeInstance,
                new FloatIntUnion { FloatValue = scale.x }.IntValue,
                new FloatIntUnion { FloatValue = scale.y }.IntValue,
                new FloatIntUnion { FloatValue = scale.z }.IntValue);
        }

        public bool Visible
        {
            get { return nativeInstance != 0 && godot_icall_Node3D_IsVisible(nativeInstance); }
            set { if (nativeInstance != 0) godot_icall_Node3D_SetVisible(nativeInstance, value); }
        }

        public Transform3D Transform
        {
            get
            {
                if (nativeInstance == 0) return new Transform3D(Basis.Identity, Vector3.Zero);
                Basis basis = new Basis(
                    new Vector3(
                        new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetTransformB0X(nativeInstance) }.FloatValue,
                        new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetTransformB0Y(nativeInstance) }.FloatValue,
                        new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetTransformB0Z(nativeInstance) }.FloatValue),
                    new Vector3(
                        new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetTransformB1X(nativeInstance) }.FloatValue,
                        new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetTransformB1Y(nativeInstance) }.FloatValue,
                        new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetTransformB1Z(nativeInstance) }.FloatValue),
                    new Vector3(
                        new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetTransformB2X(nativeInstance) }.FloatValue,
                        new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetTransformB2Y(nativeInstance) }.FloatValue,
                        new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetTransformB2Z(nativeInstance) }.FloatValue));
                Vector3 origin = new Vector3(
                    new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetTransformOX(nativeInstance) }.FloatValue,
                    new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetTransformOY(nativeInstance) }.FloatValue,
                    new FloatIntUnion { IntValue = (int)godot_icall_Node3D_GetTransformOZ(nativeInstance) }.FloatValue);
                return new Transform3D(basis, origin);
            }
            set
            {
                if (nativeInstance == 0) return;
                godot_icall_Node3D_SetTransform(nativeInstance,
                    new FloatIntUnion { FloatValue = value.basis.row0.x }.IntValue,
                    new FloatIntUnion { FloatValue = value.basis.row0.y }.IntValue,
                    new FloatIntUnion { FloatValue = value.basis.row0.z }.IntValue,
                    new FloatIntUnion { FloatValue = value.basis.row1.x }.IntValue,
                    new FloatIntUnion { FloatValue = value.basis.row1.y }.IntValue,
                    new FloatIntUnion { FloatValue = value.basis.row1.z }.IntValue,
                    new FloatIntUnion { FloatValue = value.basis.row2.x }.IntValue,
                    new FloatIntUnion { FloatValue = value.basis.row2.y }.IntValue,
                    new FloatIntUnion { FloatValue = value.basis.row2.z }.IntValue,
                    new FloatIntUnion { FloatValue = value.origin.x }.IntValue,
                    new FloatIntUnion { FloatValue = value.origin.y }.IntValue,
                    new FloatIntUnion { FloatValue = value.origin.z }.IntValue);
            }
        }

        public void RotateX(float angle)
        {
            if (nativeInstance != 0) godot_icall_Node3D_RotateX(nativeInstance, new FloatIntUnion { FloatValue = angle }.IntValue);
        }
        public void RotateY(float angle)
        {
            if (nativeInstance != 0) godot_icall_Node3D_RotateY(nativeInstance, new FloatIntUnion { FloatValue = angle }.IntValue);
        }
        public void RotateZ(float angle)
        {
            if (nativeInstance != 0) godot_icall_Node3D_RotateZ(nativeInstance, new FloatIntUnion { FloatValue = angle }.IntValue);
        }
        public void Translate(Vector3 offset)
        {
            if (nativeInstance == 0) return;
            godot_icall_Node3D_Translate(nativeInstance,
                new FloatIntUnion { FloatValue = offset.x }.IntValue,
                new FloatIntUnion { FloatValue = offset.y }.IntValue,
                new FloatIntUnion { FloatValue = offset.z }.IntValue);
        }
    }

    public partial class Resource : GodotObject
    {
        public Resource()
        {
        }

        // --- RefCounted reference-counting icalls ---
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_RefCounted_InitRef(long ptr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_RefCounted_Reference(long ptr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_RefCounted_Unreference(long ptr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_RefCounted_GetReferenceCount(long ptr);

        // --- Resource path / name icalls ---
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Resource_SetPath(long ptr, string path);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string godot_icall_Resource_GetPath(long ptr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Resource_SetName(long ptr, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string godot_icall_Resource_GetName(long ptr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Resource_TakeOverPath(long ptr, string path);

        // --- Object-level metadata icalls ---
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Resource_SetMetaString(long ptr, string name, string value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string godot_icall_Resource_GetMetaString(long ptr, string name, string defaultValue);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Resource_HasMeta(long ptr, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Resource_RemoveMeta(long ptr, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string godot_icall_Resource_GetMetaList(long ptr);

        // H7 扩展: Resource.Duplicate — 复制资源（flags: 0=浅, 1=深, 参考 Node.DuplicateFlags）
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Resource_Duplicate(long ptr, long flags);

        /// <summary>
        /// 复制此资源。flags 用 Node.DuplicateFlags 组合（0 = 浅复制，SUBRESOURCES = 深复制）。
        /// </summary>
        public Resource Duplicate(long flags = 0)
        {
            if (nativeInstance == 0) return null;
            long ptr = godot_icall_Resource_Duplicate(nativeInstance, flags);
            if (ptr == 0) return null;
            Resource r = new Resource();
            r.nativeInstance = ptr;
            r.ownsNative = true;
            return r;
        }

        // --- Reference counting API ---
        public bool InitRef()
        {
            if (nativeInstance == 0) return false;
            return godot_icall_RefCounted_InitRef(nativeInstance);
        }

        public bool Reference()
        {
            if (nativeInstance == 0) return false;
            return godot_icall_RefCounted_Reference(nativeInstance);
        }

        public bool Unreference()
        {
            if (nativeInstance == 0) return false;
            return godot_icall_RefCounted_Unreference(nativeInstance);
        }

        public long GetReferenceCount()
        {
            if (nativeInstance == 0) return 0;
            return godot_icall_RefCounted_GetReferenceCount(nativeInstance);
        }

        // --- Resource path API ---
        public string ResourcePath
        {
            get { return nativeInstance == 0 ? string.Empty : godot_icall_Resource_GetPath(nativeInstance); }
            set { if (nativeInstance != 0) godot_icall_Resource_SetPath(nativeInstance, value); }
        }

        public string ResourceName
        {
            get { return nativeInstance == 0 ? string.Empty : godot_icall_Resource_GetName(nativeInstance); }
            set { if (nativeInstance != 0) godot_icall_Resource_SetName(nativeInstance, value); }
        }

        public void TakeOverPath(string path)
        {
            if (nativeInstance != 0) godot_icall_Resource_TakeOverPath(nativeInstance, path);
        }

        // --- Metadata API (string-typed; non-string metas require Variant helpers) ---
        public void SetMeta(string name, string value)
        {
            if (nativeInstance != 0) godot_icall_Resource_SetMetaString(nativeInstance, name, value);
        }

        public string GetMeta(string name, string defaultValue = null)
        {
            if (nativeInstance == 0) return defaultValue ?? string.Empty;
            return godot_icall_Resource_GetMetaString(nativeInstance, name, defaultValue ?? string.Empty);
        }

        public bool HasMeta(string name)
        {
            if (nativeInstance == 0) return false;
            return godot_icall_Resource_HasMeta(nativeInstance, name);
        }

        public void RemoveMeta(string name)
        {
            if (nativeInstance != 0) godot_icall_Resource_RemoveMeta(nativeInstance, name);
        }

        public string[] GetMetaList()
        {
            if (nativeInstance == 0) return new string[0];
            string joined = godot_icall_Resource_GetMetaList(nativeInstance);
            if (string.IsNullOrEmpty(joined)) return new string[0];
            // Use char-based split to avoid regex/string.Split overloads that may
            // trigger mscorlib internal icalls under WASM.
            return joined.Split('\n');
        }
    }

    // H7 扩展: ResourceSaver — 静态工具类，保存资源到文件
    public static class ResourceSaver
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int godot_icall_ResourceSaver_Save(long resource, string path, int flags);

        /// <summary>
        /// 将资源保存到指定路径。返回 0 表示成功，负值为错误码。
        /// flags: 参考 ResourceSaver.SaverFlags（0 = 默认）。
        /// </summary>
        public static int Save(Resource resource, string path, int flags = 0)
        {
            if (resource == null || resource.nativeInstance == 0) return -1;
            return godot_icall_ResourceSaver_Save(resource.nativeInstance, path, flags);
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
        Key0 = 48, Key1, Key2, Key3, Key4, Key5, Key6, Key7, Key8, Key9,
        // Special keys (Godot 4 core/os/keyboard.h, 0x400000 + n).
        // P2-11 修复: Tab 原误写为 4194308（实为 BACKSPACE），改回 4194306。
        Escape = 4194305,
        Tab = 4194306,
        Backspace = 4194308,
        Enter = 4194309,
        Left = 4194319,
        Up = 4194320,
        Right = 4194321,
        Down = 4194322,
        // F1-F12 (4194332 ~ 4194343).
        F1 = 4194332, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    }
}
