using System;

namespace Godot {
    [Preserve(AllMembers = true)]
    public class Node : Object {
        public Node() : base() {}
        protected internal Node(IntPtr nativePtr) : base(nativePtr) {}

        public string Name {
            get { return Get("name") as string; }
            set { Set("name", value); }
        }

        public Node GetNode(string path) {
            IntPtr ptr = Bridge.godot_icall_Node_GetNode(NativePtr, path);
            return ptr == IntPtr.Zero ? null : new Node(ptr);
        }

        public T GetNode<T>(string path) where T : Node {
            return GetNode(path) as T;
        }

        public T GetNodeOrNull<T>(string path) where T : Node {
            return GetNode(path) as T;
        }

        public Node GetNodeOrNull(string path) {
            return GetNode(path);
        }

        public bool HasNode(string path) {
            return GetNode(path) != null;
        }

        public Node GetParent() {
            IntPtr ptr = Bridge.godot_icall_Node_GetParent(NativePtr);
            return ptr == IntPtr.Zero ? null : new Node(ptr);
        }

        public T GetParent<T>() where T : Node {
            return GetParent() as T;
        }

        public Node GetChild(int idx) {
            IntPtr ptr = Bridge.godot_icall_Node_GetChild(NativePtr, idx);
            return ptr == IntPtr.Zero ? null : new Node(ptr);
        }

        public T GetChild<T>(int idx) where T : Node {
            return GetChild(idx) as T;
        }

        public int GetChildCount() {
            return Bridge.godot_icall_Node_GetChildCount(NativePtr);
        }

        public void AddChild(Node child) {
            AddChild(child, false);
        }

        public void AddChild(Node child, bool forceReadableName = false) {
            if (child == null) throw new ArgumentNullException(nameof(child));
            Bridge.godot_icall_Node_AddChild(NativePtr, child.NativePtr, forceReadableName);
        }

        public void RemoveChild(Node child) {
            if (child == null) throw new ArgumentNullException(nameof(child));
            Bridge.godot_icall_Node_RemoveChild(NativePtr, child.NativePtr);
        }

        public override void QueueFree() {
            Bridge.godot_icall_Node_QueueFree(NativePtr);
        }

        public void SetProcess(bool enable) {
            Bridge.godot_icall_Node_SetProcess(NativePtr, enable);
        }

        public void SetPhysicsProcess(bool enable) {
            Bridge.godot_icall_Node_SetPhysicsProcess(NativePtr, enable);
        }

        public void SetProcessInput(bool enable) {
            Bridge.godot_icall_Node_SetProcessInput(NativePtr, enable);
        }

        public SceneTree GetTree() {
            IntPtr ptr = Bridge.godot_icall_Node_GetTree(NativePtr);
            return ptr == IntPtr.Zero ? null : new SceneTree(ptr);
        }

        public bool IsInsideTree() {
            return GetTree() != null;
        }

        public virtual void _Ready() {}
        public virtual void _Process(double delta) {}
        public virtual void _PhysicsProcess(double delta) {}
        public virtual void _EnterTree() {}
        public virtual void _ExitTree() {}
        public virtual void _Input(InputEvent @event) {}
        public virtual void _UnhandledInput(InputEvent @event) {}
        public virtual void _Notification(int what) {}
    }

    [Preserve(AllMembers = true)]
    public class SceneTree : Node {
        public SceneTree() : base() {}
        protected internal SceneTree(IntPtr nativePtr) : base(nativePtr) {}

        public Node Root => new Node(Bridge.godot_icall_Node_GetNode(NativePtr, "."));

        public void Quit(int exitCode = 0) {
            Call("quit", exitCode);
        }
    }

    [Preserve(AllMembers = true)]
    public class InputEvent : Object {
        public InputEvent() : base() {}
        protected internal InputEvent(IntPtr nativePtr) : base(nativePtr) {}
    }

    [Preserve(AllMembers = true)]
    public class Resource : Object {
        public Resource() : base() {}
        protected internal Resource(IntPtr nativePtr) : base(nativePtr) {}
    }

    [Preserve(AllMembers = true)]
    public class PackedScene : Resource {
        public PackedScene() : base() {}
        protected internal PackedScene(IntPtr nativePtr) : base(nativePtr) {}

        public Node Instantiate() {
            IntPtr ptr = Bridge.godot_icall_PackedScene_Instantiate(NativePtr);
            return ptr == IntPtr.Zero ? null : new Node(ptr);
        }

        public T Instantiate<T>() where T : Node {
            return Instantiate() as T;
        }
    }

    [Preserve(AllMembers = true)]
    public static class Input {
        public static bool IsKeyPressed(Key keycode) {
            return Bridge.godot_icall_Input_IsKeyPressed((int)keycode);
        }

        public static bool IsMouseButtonPressed(MouseButton button) {
            return Bridge.godot_icall_Input_IsMouseButtonPressed((int)button);
        }

        public static Vector2 GetMousePosition() {
            object obj = Bridge.godot_icall_Input_GetMousePosition();
            if (obj is Vector2 v) return v;
            return Vector2.Zero;
        }
    }

    public enum Key {
        Space = 32,
        A = 65, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        Num0 = 48, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
        Escape = 4194305,
        Enter = 4194309,
        Tab = 4194306,
        Left = 4194319,
        Up = 4194320,
        Right = 4194321,
        Down = 4194322,
        Shift = 4194325,
        Ctrl = 4194326,
        Alt = 4194328,
        F1 = 4194332, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12
    }

    public enum MouseButton {
        Left = 1,
        Right = 2,
        Middle = 3,
        WheelUp = 4,
        WheelDown = 5
    }
}
