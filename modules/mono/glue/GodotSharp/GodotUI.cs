using System;

namespace Godot {
    [Preserve(AllMembers = true)]
    public class CanvasLayer : Node {
        public CanvasLayer() : base(Bridge.godot_icall_Object_InstantiateFromNative("CanvasLayer")) {}
        internal CanvasLayer(IntPtr nativePtr) : base(nativePtr) {}

        public int Layer {
            get { return (int)Get("layer"); }
            set { Set("layer", value); }
        }
    }

    [Preserve(AllMembers = true)]
    public class Control : CanvasItem {
        public Control() : base(Bridge.godot_icall_Object_InstantiateFromNative("Control")) {}
        internal Control(IntPtr nativePtr) : base(nativePtr) {}

        public Vector2 Position {
            get { return (Vector2)Get("position"); }
            set { Set("position", value); }
        }

        public Vector2 Size {
            get { return (Vector2)Get("size"); }
            set { Set("size", value); }
        }

        // WASM-safe: set position via C++ icall (no Vector2 construction in C#)
        public void SetPosition(int x, int y) {
            Bridge.godot_icall_Control_SetPosition(NativePtr, x, y);
        }
    }

    [Preserve(AllMembers = true)]
    public class CanvasItem : Node {
        public CanvasItem() : base() {}
        internal CanvasItem(IntPtr nativePtr) : base(nativePtr) {}

        public bool Visible {
            get { return (bool)Get("visible"); }
            set { Set("visible", value); }
        }

        // =============================================
        // P6 WorldCanvas drawing (§1.8). WASM-safe wrappers over the
        // Canvas_* icalls: every coordinate/radius/color is an int so nothing
        // crosses the interop boundary as object/array. Colors are packed
        // ARGB (0xAARRGGBB). Draw commands are only valid inside
        // _Notification(NOTIFICATION_DRAW); QueueRedraw2 schedules it.
        // =============================================

        public static int PackColor(Color c) {
            int r = ((int)(c.r * 255f)) & 0xff;
            int g = ((int)(c.g * 255f)) & 0xff;
            int b = ((int)(c.b * 255f)) & 0xff;
            int a = ((int)(c.a * 255f)) & 0xff;
            return (a << 24) | (r << 16) | (g << 8) | b;
        }

        // Pan this Node2D (camera follow). Only meaningful for Node2D-derived.
        public void SetCanvasPosition(int x, int y) {
            Bridge.godot_icall_Canvas_SetPosition(NativePtr, x, y);
        }

        // Schedule a redraw (NOTIFICATION_DRAW fires on next frame draw pass).
        public void QueueRedraw2() {
            Bridge.godot_icall_Canvas_QueueRedraw(NativePtr);
        }

        // Draw primitives. Valid only during NOTIFICATION_DRAW (_Notification(30)).
        public void DrawRectI(int x, int y, int w, int h, int argb, bool filled = true) {
            Bridge.godot_icall_Canvas_DrawRect(NativePtr, x, y, w, h, argb, filled ? 1 : 0);
        }
        public void DrawCircleI(int x, int y, int r, int argb) {
            Bridge.godot_icall_Canvas_DrawCircle(NativePtr, x, y, r, argb);
        }
        public void DrawTriangleI(int x, int y, int r, int argb) {
            Bridge.godot_icall_Canvas_DrawTriangle(NativePtr, x, y, r, argb);
        }
        public void DrawDiamondI(int x, int y, int h, int argb) {
            Bridge.godot_icall_Canvas_DrawDiamond(NativePtr, x, y, h, argb);
        }
        public void DrawHexI(int x, int y, int r, int argb) {
            Bridge.godot_icall_Canvas_DrawHex(NativePtr, x, y, r, argb);
        }
    }

    [Preserve(AllMembers = true)]
    public class Label : Control {
        public Label() : base(Bridge.godot_icall_Object_InstantiateFromNative("Label")) {}
        internal Label(IntPtr nativePtr) : base(nativePtr) {}

        public string Text {
            get { return (string)Get("text"); }
            set { Set("text", value); }
        }

        public int HorizontalAlignment {
            get { return (int)Get("horizontal_alignment"); }
            set { Set("horizontal_alignment", value); }
        }

        public int VerticalAlignment {
            get { return (int)Get("vertical_alignment"); }
            set { Set("vertical_alignment", value); }
        }

        // =============================================
        // WASM-safe methods: use C++ icalls to bypass
        // Mono WASM interpreter signature mismatch bugs.
        // These are safe to call from _Process, _Ready, etc.
        // =============================================

        // Set text to "FPS: N" via C++ (safe in _Process, no C# string ops)
        public void SetFpsText(int fps) {
            Bridge.godot_icall_Label_SetFpsText(NativePtr, fps);
        }

        // Set text to prefix + int via C++ (safe in _Process)
        public void SetPrefixedInt(string prefix, int value) {
            Bridge.godot_icall_Label_SetPrefixedInt(NativePtr, prefix, value);
        }

        // Append a log line to existing text via C++ (safe in _Ready)
        public void AppendLog(string message) {
            Bridge.godot_icall_Label_AppendLog(NativePtr, message);
        }
    }
}
