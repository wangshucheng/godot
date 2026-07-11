using System;
using System.Runtime.CompilerServices;

namespace Godot
{
    public class CanvasLayer : Node
    {
        public CanvasLayer()
        {
            nativeInstance = godot_icall_CreateObject("CanvasLayer");
        }

        public int Layer
        {
            set { godot_icall_Object_SetInt(nativeInstance, "layer", value); }
        }
    }

    public class Control : Node
    {
        public enum LayoutPreset
        {
            TopLeft = 0,
            TopRight = 1,
            BottomRight = 2,
            BottomLeft = 3,
            CenterLeft = 4,
            CenterTop = 5,
            CenterRight = 6,
            CenterBottom = 7,
            Center = 8,
            LeftWide = 9,
            TopWide = 10,
            RightWide = 11,
            BottomWide = 12,
            VCenterWide = 13,
            HCenterWide = 14,
            FullRect = 15,
        }

        public enum SizeFlagsEnum
        {
            Fill = 1,
            Expand = 2,
            ExpandFill = 3,
            ShrinkCenter = 4,
            ShrinkEnd = 8,
        }

        protected Control() { }

        public void SetAnchorsPreset(LayoutPreset preset)
        {
            godot_icall_Object_CallInt(nativeInstance, "set_anchors_preset", (int)preset);
        }

        public Vector2 CustomMinimumSize
        {
            set { godot_icall_Object_SetVector2(nativeInstance, "custom_minimum_size", value.x, value.y); }
        }

        public int SizeFlagsVertical
        {
            set { godot_icall_Object_SetInt(nativeInstance, "size_flags_vertical", value); }
        }

        public int SizeFlagsHorizontal
        {
            set { godot_icall_Object_SetInt(nativeInstance, "size_flags_horizontal", value); }
        }

        public void AddThemeFontSizeOverride(string name, int size)
        {
            godot_icall_Object_CallStringInt(nativeInstance, "add_theme_font_size_override", name, size);
        }

        public void AddThemeColorOverride(string name, Color color)
        {
            godot_icall_Object_CallStringColor(nativeInstance, "add_theme_color_override", name, color.r, color.g, color.b, color.a);
        }

        public void AddThemeStyleboxOverride(string name, StyleBoxFlat stylebox)
        {
            godot_icall_Object_CallStringObject(nativeInstance, "add_theme_stylebox_override", name, stylebox.nativeInstance);
        }

        public void AddThemeConstantOverride(string name, int constant)
        {
            godot_icall_Object_CallStringInt(nativeInstance, "add_theme_constant_override", name, constant);
        }
    }

    public class Panel : Control
    {
        public Panel()
        {
            nativeInstance = godot_icall_CreateObject("Panel");
        }
    }

    public class Label : Control
    {
        public Label()
        {
            nativeInstance = godot_icall_CreateObject("Label");
        }

        public string Text
        {
            set { godot_icall_Object_SetString(nativeInstance, "text", value); }
            get { return godot_icall_Object_GetString(nativeInstance, "text"); }
        }

        public int HorizontalAlignment
        {
            set { godot_icall_Object_SetInt(nativeInstance, "horizontal_alignment", value); }
        }
    }

    public class VBoxContainer : Control
    {
        public VBoxContainer()
        {
            nativeInstance = godot_icall_CreateObject("VBoxContainer");
        }
    }

    public class ScrollContainer : Control
    {
        public ScrollContainer()
        {
            nativeInstance = godot_icall_CreateObject("ScrollContainer");
        }

        public int ScrollVertical
        {
            set { godot_icall_Object_SetInt(nativeInstance, "scroll_vertical", value); }
        }

        public VScrollBar GetVScrollBar()
        {
            IntPtr ptr = godot_icall_Object_CallNoArgsObject(nativeInstance, "get_v_scroll_bar");
            VScrollBar bar = new VScrollBar();
            bar.nativeInstance = ptr;
            return bar;
        }
    }

    public class VScrollBar : Control
    {
        internal VScrollBar() { }

        public double MaxValue
        {
            get { return godot_icall_Object_GetFloat(nativeInstance, "max_value"); }
        }
    }

    public class HSeparator : Control
    {
        public HSeparator()
        {
            nativeInstance = godot_icall_CreateObject("HSeparator");
        }
    }

    public class StyleBoxFlat : GodotObject
    {
        public StyleBoxFlat()
        {
            nativeInstance = godot_icall_CreateObject("StyleBoxFlat");
        }

        public Color BgColor
        {
            set { godot_icall_Object_SetColor(nativeInstance, "bg_color", value.r, value.g, value.b, value.a); }
        }

        public Color BorderColor
        {
            set { godot_icall_Object_SetColor(nativeInstance, "border_color", value.r, value.g, value.b, value.a); }
        }

        public void SetBorderWidthAll(int width)
        {
            godot_icall_Object_CallInt(nativeInstance, "set_border_width_all", width);
        }

        public void SetContentMarginAll(int margin)
        {
            godot_icall_Object_CallInt(nativeInstance, "set_content_margin_all", margin);
        }
    }
}