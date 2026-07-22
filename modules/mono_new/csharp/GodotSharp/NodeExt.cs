using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Godot
{
    // Extension methods for Node via partial class - adds child manipulation, naming, etc.
    public partial class Node
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int godot_icall_Node_GetChildCount(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node_GetChild(long node, int index);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string godot_icall_Node_GetName(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node_SetName(long node, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node_RemoveChild(long node, long child);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string godot_icall_Node_GetPath(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node_QueueFree(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node_GetNode(long node, string path);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string godot_icall_Node_GetClassName(long node);

        // --- Auto-generated Node icalls (see glue/glue_cpp/node_glue.cpp) ---
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node_GetTree(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Node_IsInsideTree(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node_GetProcessMode(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node_SetProcess(long node, bool enable);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node_SetPhysicsProcess(long node, bool enable);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node_SetProcessInput(long node, bool enable);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node_SetProcessUnhandledInput(long node, bool enable);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node_GetIndex(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node_MoveChild(long node, long child, long index);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string godot_icall_Node_PrintTree(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node_GetOwner(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node_SetOwner(long node, long owner);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node_Duplicate(long node, long flags);

        // --- Phase 3.5: Additional Node icalls (see node_glue.cpp) ---
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int godot_icall_Node_GetChildCountAll(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Node_HasNode(long node, string path);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Node_IsProcessing(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Node_IsPhysicsProcessing(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Node_IsProcessingInput(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Node_IsProcessingUnhandledInput(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node_Reparent(long node, long newParent);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Node_IsInGroup(long node, string group);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node_AddToGroup(long node, string group);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node_RemoveFromGroup(long node, string group);

        public int GetChildCount()
        {
            return godot_icall_Node_GetChildCount(nativeInstance);
        }

        public Node GetChild(int index)
        {
            long ptr = godot_icall_Node_GetChild(nativeInstance, index);
            if (ptr == 0) return null;
            return WrapNode(ptr);
        }

        public string GetName()
        {
            return godot_icall_Node_GetName(nativeInstance);
        }

        public void SetName(string name)
        {
            godot_icall_Node_SetName(nativeInstance, name);
        }

        public void RemoveChild(Node child)
        {
            godot_icall_Node_RemoveChild(nativeInstance, child.nativeInstance);
        }

        public string GetPath()
        {
            return godot_icall_Node_GetPath(nativeInstance);
        }

        public new void QueueFree()
        {
            godot_icall_Node_QueueFree(nativeInstance);
        }

        public T GetNode<T>(string path) where T : Node
        {
            long ptr = godot_icall_Node_GetNode(nativeInstance, path);
            if (ptr == 0) return null;
            return (T)WrapNode(ptr);
        }

        public string GetClassName()
        {
            return godot_icall_Node_GetClassName(nativeInstance);
        }

        // --- New Node API wrappers (auto-generated icalls) ---
        public bool IsInsideTree()
        {
            return godot_icall_Node_IsInsideTree(nativeInstance);
        }

        public long GetProcessMode()
        {
            return godot_icall_Node_GetProcessMode(nativeInstance);
        }

        public void SetProcess(bool enable)
        {
            godot_icall_Node_SetProcess(nativeInstance, enable);
        }

        public void SetPhysicsProcess(bool enable)
        {
            godot_icall_Node_SetPhysicsProcess(nativeInstance, enable);
        }

        public void SetProcessInput(bool enable)
        {
            godot_icall_Node_SetProcessInput(nativeInstance, enable);
        }

        public void SetProcessUnhandledInput(bool enable)
        {
            godot_icall_Node_SetProcessUnhandledInput(nativeInstance, enable);
        }

        public long GetIndex()
        {
            return godot_icall_Node_GetIndex(nativeInstance);
        }

        public void MoveChild(Node child, long index)
        {
            godot_icall_Node_MoveChild(nativeInstance, child.nativeInstance, index);
        }

        public string PrintTree()
        {
            return godot_icall_Node_PrintTree(nativeInstance);
        }

        public Node GetOwner()
        {
            long ptr = godot_icall_Node_GetOwner(nativeInstance);
            if (ptr == 0) return null;
            return WrapNode(ptr);
        }

        public void SetOwner(Node owner)
        {
            godot_icall_Node_SetOwner(nativeInstance, owner != null ? owner.nativeInstance : 0);
        }

        public Node Duplicate(long flags = 0)
        {
            long ptr = godot_icall_Node_Duplicate(nativeInstance, flags);
            if (ptr == 0) return null;
            return WrapNode(ptr);
        }

        // --- Phase 3.5: Additional Node API wrappers ---
        // Returns child count including internal nodes (use GetChildCount() for external only).
        public int GetChildCountAll()
        {
            return godot_icall_Node_GetChildCountAll(nativeInstance);
        }

        public bool HasNode(string path)
        {
            return godot_icall_Node_HasNode(nativeInstance, path);
        }

        public bool IsProcessing()
        {
            return godot_icall_Node_IsProcessing(nativeInstance);
        }

        public bool IsPhysicsProcessing()
        {
            return godot_icall_Node_IsPhysicsProcessing(nativeInstance);
        }

        public bool IsProcessingInput()
        {
            return godot_icall_Node_IsProcessingInput(nativeInstance);
        }

        public bool IsProcessingUnhandledInput()
        {
            return godot_icall_Node_IsProcessingUnhandledInput(nativeInstance);
        }

        public void Reparent(Node newParent)
        {
            godot_icall_Node_Reparent(nativeInstance, newParent != null ? newParent.nativeInstance : 0);
        }

        public bool IsInGroup(string group)
        {
            return godot_icall_Node_IsInGroup(nativeInstance, group);
        }

        public void AddToGroup(string group)
        {
            godot_icall_Node_AddToGroup(nativeInstance, group);
        }

        public void RemoveFromGroup(string group)
        {
            godot_icall_Node_RemoveFromGroup(nativeInstance, group);
        }

        // Internal helper to wrap a raw native pointer into a Node subclass.
        // Uses the class name to determine the proper C# type.
        internal static Node WrapNode(long ptr)
        {
            if (ptr == 0) return null;
            string cls = godot_icall_Node_GetClassName(ptr);
            Node n;
            switch (cls)
            {
                case "Node2D": n = GodotObject.Attach<Node2D>(ptr); break;
                case "Node3D": n = GodotObject.Attach<Node3D>(ptr); break;
                case "Label": n = GodotObject.Attach<Label>(ptr); break;
                case "Button": n = GodotObject.Attach<Button>(ptr); break;
                case "Control": n = GodotObject.Attach<Control>(ptr); break;
                case "Panel": n = GodotObject.Attach<Panel>(ptr); break;
                case "Camera2D": n = GodotObject.Attach<Camera2D>(ptr); break;
                case "Sprite2D": n = GodotObject.Attach<Sprite2D>(ptr); break;
                case "Timer": n = GodotObject.Attach<Timer>(ptr); break;
                case "AnimationPlayer": n = GodotObject.Attach<AnimationPlayer>(ptr); break;
                case "RigidBody2D": n = GodotObject.Attach<RigidBody2D>(ptr); break;
                case "CollisionShape2D": n = GodotObject.Attach<CollisionShape2D>(ptr); break;
                case "Area2D": n = GodotObject.Attach<Area2D>(ptr); break;
                case "AudioStreamPlayer": n = GodotObject.Attach<AudioStreamPlayer>(ptr); break;
                case "LineEdit": n = GodotObject.Attach<LineEdit>(ptr); break;
                case "ColorRect": n = GodotObject.Attach<ColorRect>(ptr); break;
                case "VBoxContainer": n = GodotObject.Attach<VBoxContainer>(ptr); break;
                case "HBoxContainer": n = GodotObject.Attach<HBoxContainer>(ptr); break;
                case "CanvasLayer": n = GodotObject.Attach<CanvasLayer>(ptr); break;
                default: n = GodotObject.Attach<Node>(ptr); break;
            }
            n.nativeInstance = ptr;
            return n;
        }
    }

    // Auto-generated Node2D icalls (see glue/glue_cpp/node2d_glue.cpp).
    // All float values are passed as int32 bit-patterns (FloatIntUnion) and
    // returned as packed int64 (low 32 = x bits, high 32 = y bits) to comply
    // with the WASM interpreter's no-direct-double-icall constraint.
    public partial class Node2D
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node2D_SetPosition(long node, int xBits, int yBits);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node2D_GetPosition(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node2D_SetRotation(long node, int rotBits);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node2D_GetRotation(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node2D_SetScale(long node, int xBits, int yBits);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node2D_GetScale(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node2D_SetGlobalPosition(long node, int xBits, int yBits);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node2D_GetGlobalPosition(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node2D_SetGlobalRotation(long node, int rotBits);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node2D_GetGlobalRotation(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node2D_SetGlobalScale(long node, int xBits, int yBits);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node2D_GetGlobalScale(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node2D_Rotate(long node, int deltaBits);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node2D_MoveLocalX(long node, int deltaBits, bool scaled);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node2D_MoveLocalY(long node, int deltaBits, bool scaled);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node2D_SetZIndex(long node, long z);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Node2D_GetZIndex(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node2D_SetZAsRelative(long node, bool enable);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Node2D_IsZAsRelative(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node2D_SetYSortEnabled(long node, bool enable);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Node2D_IsYSortEnabled(long node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node2D_SetVisible(long node, bool visible);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Node2D_IsVisible(long node);

        // --- Wrapper properties using the auto-generated icalls ---
        // These use direct C++ method calls (faster, no string lookup).
        public Vector2 Position
        {
            set { godot_icall_Node2D_SetPosition(nativeInstance, new FloatIntUnion { FloatValue = value.x }.IntValue, new FloatIntUnion { FloatValue = value.y }.IntValue); }
            get
            {
                long packed = godot_icall_Node2D_GetPosition(nativeInstance);
                int xBits = (int)(packed & 0xFFFFFFFFu);
                int yBits = (int)((packed >> 32) & 0xFFFFFFFFu);
                float x = new FloatIntUnion { IntValue = xBits }.FloatValue;
                float y = new FloatIntUnion { IntValue = yBits }.FloatValue;
                return new Vector2(x, y);
            }
        }

        public float Rotation
        {
            set { godot_icall_Node2D_SetRotation(nativeInstance, new FloatIntUnion { FloatValue = value }.IntValue); }
            get
            {
                long bits = godot_icall_Node2D_GetRotation(nativeInstance);
                return new FloatIntUnion { IntValue = (int)bits }.FloatValue;
            }
        }

        public Vector2 Scale
        {
            set { godot_icall_Node2D_SetScale(nativeInstance, new FloatIntUnion { FloatValue = value.x }.IntValue, new FloatIntUnion { FloatValue = value.y }.IntValue); }
            get
            {
                long packed = godot_icall_Node2D_GetScale(nativeInstance);
                int xBits = (int)(packed & 0xFFFFFFFFu);
                int yBits = (int)((packed >> 32) & 0xFFFFFFFFu);
                float x = new FloatIntUnion { IntValue = xBits }.FloatValue;
                float y = new FloatIntUnion { IntValue = yBits }.FloatValue;
                return new Vector2(x, y);
            }
        }

        public Vector2 GlobalPosition
        {
            set { godot_icall_Node2D_SetGlobalPosition(nativeInstance, new FloatIntUnion { FloatValue = value.x }.IntValue, new FloatIntUnion { FloatValue = value.y }.IntValue); }
            get
            {
                long packed = godot_icall_Node2D_GetGlobalPosition(nativeInstance);
                int xBits = (int)(packed & 0xFFFFFFFFu);
                int yBits = (int)((packed >> 32) & 0xFFFFFFFFu);
                float x = new FloatIntUnion { IntValue = xBits }.FloatValue;
                float y = new FloatIntUnion { IntValue = yBits }.FloatValue;
                return new Vector2(x, y);
            }
        }

        public float GlobalRotation
        {
            set { godot_icall_Node2D_SetGlobalRotation(nativeInstance, new FloatIntUnion { FloatValue = value }.IntValue); }
            get
            {
                long bits = godot_icall_Node2D_GetGlobalRotation(nativeInstance);
                return new FloatIntUnion { IntValue = (int)bits }.FloatValue;
            }
        }

        public Vector2 GlobalScale
        {
            set { godot_icall_Node2D_SetGlobalScale(nativeInstance, new FloatIntUnion { FloatValue = value.x }.IntValue, new FloatIntUnion { FloatValue = value.y }.IntValue); }
            get
            {
                long packed = godot_icall_Node2D_GetGlobalScale(nativeInstance);
                int xBits = (int)(packed & 0xFFFFFFFFu);
                int yBits = (int)((packed >> 32) & 0xFFFFFFFFu);
                float x = new FloatIntUnion { IntValue = xBits }.FloatValue;
                float y = new FloatIntUnion { IntValue = yBits }.FloatValue;
                return new Vector2(x, y);
            }
        }

        public void Rotate(float delta)
        {
            godot_icall_Node2D_Rotate(nativeInstance, new FloatIntUnion { FloatValue = delta }.IntValue);
        }

        public void MoveLocalX(float delta, bool scaled = false)
        {
            godot_icall_Node2D_MoveLocalX(nativeInstance, new FloatIntUnion { FloatValue = delta }.IntValue, scaled);
        }

        public void MoveLocalY(float delta, bool scaled = false)
        {
            godot_icall_Node2D_MoveLocalY(nativeInstance, new FloatIntUnion { FloatValue = delta }.IntValue, scaled);
        }

        public long ZIndex
        {
            set { godot_icall_Node2D_SetZIndex(nativeInstance, value); }
            get { return godot_icall_Node2D_GetZIndex(nativeInstance); }
        }

        public bool ZAsRelative
        {
            set { godot_icall_Node2D_SetZAsRelative(nativeInstance, value); }
            get { return godot_icall_Node2D_IsZAsRelative(nativeInstance); }
        }

        public bool YSortEnabled
        {
            set { godot_icall_Node2D_SetYSortEnabled(nativeInstance, value); }
            get { return godot_icall_Node2D_IsYSortEnabled(nativeInstance); }
        }

        public bool Visible
        {
            set { godot_icall_Node2D_SetVisible(nativeInstance, value); }
            get { return godot_icall_Node2D_IsVisible(nativeInstance); }
        }
    }

    // SceneTree extensions - adds scene switching.
    public partial class SceneTree
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int godot_icall_SceneTree_ChangeSceneToFile(long tree, string path);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_SceneTree_GetCurrentScene(long tree);

        public Error ChangeSceneToFile(string path)
        {
            return (Error)godot_icall_SceneTree_ChangeSceneToFile(nativeInstance, path);
        }

        public Node CurrentScene
        {
            get
            {
                long ptr = godot_icall_SceneTree_GetCurrentScene(nativeInstance);
                return WrapNode(ptr);
            }
        }
    }

    // GD extensions - adds resource loading.
    public static partial class GD
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_ResourceLoader_Load(string path);

        public static PackedScene LoadPackedScene(string path)
        {
            long ptr = godot_icall_ResourceLoader_Load(path);
            if (ptr == 0) return null;
            // H4 修复: C++ 侧 icall_ResourceLoader_Load 调用了 res->reference()，
            // 因此 C# 侧必须设置 ownsNative=true，这样 Dispose/终结器会调用
            // godot_icall_Object_Free 来 unreference，避免引用计数泄漏。
            PackedScene ps = new PackedScene();
            ps.nativeInstance = ptr;
            ps.ownsNative = true;
            return ps;
        }
    }

    // PackedScene - loaded scene resource that can be instantiated.
    public partial class PackedScene : Resource
    {
        public PackedScene() { }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_PackedScene_Instantiate(long scene);

        public Node Instantiate()
        {
            long ptr = godot_icall_PackedScene_Instantiate(nativeInstance);
            return Node.WrapNode(ptr);
        }
    }
}
