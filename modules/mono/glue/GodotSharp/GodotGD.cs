using System;

namespace Godot {
    [Preserve(AllMembers = true)]
    public static class GD {
        public static void Print(string message) {
            Bridge.godot_icall_GD_Print(message);
        }

        public static void Print(object obj) {
            Bridge.godot_icall_GD_Print(obj?.ToString() ?? "null");
        }

        public static void PrintErr(string message) {
            Bridge.godot_icall_GD_Print("ERROR: " + message);
        }

        public static void PrintErr(object obj) {
            Bridge.godot_icall_GD_Print("ERROR: " + (obj?.ToString() ?? "null"));
        }

        public static PackedScene LoadScene(string path) {
            IntPtr ptr = Bridge.godot_icall_ResourceLoader_Load(path);
            return ptr == IntPtr.Zero ? null : new PackedScene(ptr);
        }

        public static Resource Load(string path) {
            IntPtr ptr = Bridge.godot_icall_ResourceLoader_Load(path);
            return ptr == IntPtr.Zero ? null : new Resource(ptr);
        }

        public static T Load<T>(string path) where T : Resource {
            return Load(path) as T;
        }
    }
}
