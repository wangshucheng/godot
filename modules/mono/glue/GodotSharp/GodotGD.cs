using System;

namespace Godot {
    public static class GD {
        public static void Print(string message) {
            Bridge.godot_icall_GD_Print(message);
        }

        public static void Print(object obj) {
            Bridge.godot_icall_GD_Print(obj?.ToString() ?? "null");
        }
    }
}
