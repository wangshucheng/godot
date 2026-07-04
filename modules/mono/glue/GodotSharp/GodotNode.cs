using System;

namespace Godot {
    public class Node : Object {
        public Node() : base() {}
        internal Node(IntPtr nativePtr) : base(nativePtr) {}

        public Node GetNode(string path) {
            IntPtr ptr = Bridge.godot_icall_Node_GetNode(NativePtr, path);
            if (ptr == IntPtr.Zero) return null;
            return new Node(ptr);
        }

        public void AddChild(Node node) {
            Call("add_child", node);
        }
    }
}
