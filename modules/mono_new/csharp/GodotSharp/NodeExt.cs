using System;
using System.Runtime.CompilerServices;

namespace Godot
{
    // Extension methods for Node via partial class - adds child manipulation, naming, etc.
    public partial class Node
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int godot_icall_Node_GetChildCount(int node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int godot_icall_Node_GetChild(int node, int index);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string godot_icall_Node_GetName(int node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node_SetName(int node, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node_RemoveChild(int node, int child);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string godot_icall_Node_GetPath(int node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Node_QueueFree(int node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int godot_icall_Node_GetNode(int node, string path);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string godot_icall_Node_GetClassName(int node);

        public int GetChildCount()
        {
            return godot_icall_Node_GetChildCount(nativeInstance);
        }

        public Node GetChild(int index)
        {
            int ptr = godot_icall_Node_GetChild(nativeInstance, index);
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
            int ptr = godot_icall_Node_GetNode(nativeInstance, path);
            if (ptr == 0) return null;
            return (T)WrapNode(ptr);
        }

        public string GetClassName()
        {
            return godot_icall_Node_GetClassName(nativeInstance);
        }

        // Internal helper to wrap a raw native pointer into a Node subclass.
        // Uses the class name to determine the proper C# type.
        internal static Node WrapNode(int ptr)
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

    // SceneTree extensions - adds scene switching.
    public partial class SceneTree
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int godot_icall_SceneTree_ChangeSceneToFile(int tree, string path);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int godot_icall_SceneTree_GetCurrentScene(int tree);

        public Error ChangeSceneToFile(string path)
        {
            return (Error)godot_icall_SceneTree_ChangeSceneToFile(nativeInstance, path);
        }

        public Node CurrentScene
        {
            get
            {
                int ptr = godot_icall_SceneTree_GetCurrentScene(nativeInstance);
                return WrapNode(ptr);
            }
        }
    }

    // GD extensions - adds resource loading.
    public static partial class GD
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int godot_icall_ResourceLoader_Load(string path);

        public static PackedScene LoadPackedScene(string path)
        {
            int ptr = godot_icall_ResourceLoader_Load(path);
            if (ptr == 0) return null;
            PackedScene ps = new PackedScene();
            ps.nativeInstance = ptr;
            return ps;
        }
    }

    // PackedScene - loaded scene resource that can be instantiated.
    public partial class PackedScene : Resource
    {
        public PackedScene() { }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int godot_icall_PackedScene_Instantiate(int scene);

        public Node Instantiate()
        {
            int ptr = godot_icall_PackedScene_Instantiate(nativeInstance);
            return Node.WrapNode(ptr);
        }
    }
}
