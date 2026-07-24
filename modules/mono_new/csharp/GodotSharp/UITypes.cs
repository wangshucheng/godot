using System;
using System.Runtime.CompilerServices;

namespace Godot
{
    public class CanvasLayer : Node
    {
        public CanvasLayer()
        {
            if (nativeInstance == 0)
            {
                nativeInstance = godot_icall_CreateObject("CanvasLayer");
                ownsNative = true;
            }
            else if (!godot_icall_Object_IsClass(nativeInstance, "CanvasLayer"))
            {
                godot_icall_Object_Delete(nativeInstance);
                nativeInstance = godot_icall_CreateObject("CanvasLayer");
                ownsNative = true;
            }
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

        public enum MouseFilterEnum
        {
            Stop = 0,
            Pass = 1,
            Ignore = 2,
        }

        public enum LayoutDirectionEnum
        {
            Inherited = 0,
            ApplicationLocale = 1,
            Ltr = 2,
            Rtl = 3,
            SystemLocale = 4,
        }

        public Control()
        {
            if (nativeInstance == 0)
            {
                nativeInstance = godot_icall_CreateObject("Control");
                ownsNative = true;
            }
        }

        public void SetAnchorsPreset(LayoutPreset preset)
        {
            godot_icall_Object_CallInt(nativeInstance, "set_anchors_preset", (int)preset);
        }

        public Vector2 Position
        {
            set { godot_icall_Object_SetVector2(nativeInstance, "position", new FloatIntUnion { FloatValue = value.x }.IntValue, new FloatIntUnion { FloatValue = value.y }.IntValue); }
        }

        public Vector2 CustomMinimumSize
        {
            set { godot_icall_Object_SetVector2(nativeInstance, "custom_minimum_size", new FloatIntUnion { FloatValue = value.x }.IntValue, new FloatIntUnion { FloatValue = value.y }.IntValue); }
        }

        public int SizeFlagsVertical
        {
            set { godot_icall_Object_SetInt(nativeInstance, "size_flags_vertical", value); }
        }

        public int SizeFlagsHorizontal
        {
            set { godot_icall_Object_SetInt(nativeInstance, "size_flags_horizontal", value); }
        }

        public float AnchorLeft
        {
            set { godot_icall_Object_SetFloat(nativeInstance, "anchor_left", new FloatIntUnion { FloatValue = value }.IntValue); }
        }
        public float AnchorTop
        {
            set { godot_icall_Object_SetFloat(nativeInstance, "anchor_top", new FloatIntUnion { FloatValue = value }.IntValue); }
        }
        public float AnchorRight
        {
            set { godot_icall_Object_SetFloat(nativeInstance, "anchor_right", new FloatIntUnion { FloatValue = value }.IntValue); }
        }
        public float AnchorBottom
        {
            set { godot_icall_Object_SetFloat(nativeInstance, "anchor_bottom", new FloatIntUnion { FloatValue = value }.IntValue); }
        }

        public float OffsetLeft
        {
            set { godot_icall_Object_SetFloat(nativeInstance, "offset_left", new FloatIntUnion { FloatValue = value }.IntValue); }
        }
        public float OffsetTop
        {
            set { godot_icall_Object_SetFloat(nativeInstance, "offset_top", new FloatIntUnion { FloatValue = value }.IntValue); }
        }
        public float OffsetRight
        {
            set { godot_icall_Object_SetFloat(nativeInstance, "offset_right", new FloatIntUnion { FloatValue = value }.IntValue); }
        }
        public float OffsetBottom
        {
            set { godot_icall_Object_SetFloat(nativeInstance, "offset_bottom", new FloatIntUnion { FloatValue = value }.IntValue); }
        }

        public enum GrowDirectionEnum
        {
            Begin = 0,
            End = 1,
            Both = 2,
        }

        public int GrowHorizontal
        {
            set { godot_icall_Object_SetInt(nativeInstance, "grow_horizontal", value); }
        }
        public int GrowVertical
        {
            set { godot_icall_Object_SetInt(nativeInstance, "grow_vertical", value); }
        }

        public Color Modulate
        {
            set
            {
                godot_icall_Object_SetColor(nativeInstance, "modulate",
                    new FloatIntUnion { FloatValue = value.r }.IntValue,
                    new FloatIntUnion { FloatValue = value.g }.IntValue,
                    new FloatIntUnion { FloatValue = value.b }.IntValue,
                    new FloatIntUnion { FloatValue = value.a }.IntValue);
            }
        }

        public Color SelfModulate
        {
            set
            {
                godot_icall_Object_SetColor(nativeInstance, "self_modulate",
                    new FloatIntUnion { FloatValue = value.r }.IntValue,
                    new FloatIntUnion { FloatValue = value.g }.IntValue,
                    new FloatIntUnion { FloatValue = value.b }.IntValue,
                    new FloatIntUnion { FloatValue = value.a }.IntValue);
            }
        }

        public bool Visible
        {
            set { godot_icall_Object_SetBool(nativeInstance, "visible", value); }
            get { return godot_icall_Object_CallNoArgsBool(nativeInstance, "is_visible"); }
        }

        public MouseFilterEnum MouseFilter
        {
            set { godot_icall_Object_SetInt(nativeInstance, "mouse_filter", (int)value); }
        }

        public LayoutDirectionEnum LayoutDirection
        {
            set { godot_icall_Object_SetInt(nativeInstance, "layout_direction", (int)value); }
        }

        public void AddThemeFontSizeOverride(string name, int size)
        {
            godot_icall_Object_CallStringInt(nativeInstance, "add_theme_font_size_override", name, (long)size);
        }

        public void AddThemeColorOverride(string name, Color color)
        {
            int rb = new FloatIntUnion { FloatValue = color.r }.IntValue;
            int gb = new FloatIntUnion { FloatValue = color.g }.IntValue;
            int bb = new FloatIntUnion { FloatValue = color.b }.IntValue;
            int ab = new FloatIntUnion { FloatValue = color.a }.IntValue;
            godot_icall_Object_CallStringColor(nativeInstance, "add_theme_color_override", name, rb, gb, bb, ab);
        }

        public void AddThemeStyleboxOverride(string name, StyleBoxFlat stylebox)
        {
            godot_icall_Object_CallStringObject(nativeInstance, "add_theme_stylebox_override", name, stylebox.nativeInstance);
        }

        public void AddThemeConstantOverride(string name, int constant)
        {
            godot_icall_Object_CallStringInt(nativeInstance, "add_theme_constant_override", name, (long)constant);
        }
    }

    public class Panel : Control
    {
        public Panel()
        {
            if (nativeInstance == 0)
            {
                nativeInstance = godot_icall_CreateObject("Panel");
            }
            else if (!godot_icall_Object_IsClass(nativeInstance, "Panel"))
            {
                godot_icall_Object_Delete(nativeInstance);
                nativeInstance = godot_icall_CreateObject("Panel");
            }
        }
    }

    public class Label : Control
    {
        public enum AutowrapModeEnum
        {
            Off = 0,
            Arbitrary = 1,
            Word = 2,
            WordSmart = 3,
        }

        public enum OverrunBehaviorEnum
        {
            TrimNone = 0,
            TrimChar = 1,
            TrimWord = 2,
            Ellipsis = 3,
        }

        public Label()
        {
            if (nativeInstance == 0)
            {
                nativeInstance = godot_icall_CreateObject("Label");
            }
            else if (!godot_icall_Object_IsClass(nativeInstance, "Label"))
            {
                godot_icall_Object_Delete(nativeInstance);
                nativeInstance = godot_icall_CreateObject("Label");
            }
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

        public int VerticalAlignment
        {
            set { godot_icall_Object_SetInt(nativeInstance, "vertical_alignment", value); }
        }

        public AutowrapModeEnum AutowrapMode
        {
            set { godot_icall_Object_SetInt(nativeInstance, "autowrap_mode", (int)value); }
        }

        public bool ClipText
        {
            set { godot_icall_Object_SetBool(nativeInstance, "clip_text", value); }
            get { return godot_icall_Object_CallNoArgsBool(nativeInstance, "is_clipping_text"); }
        }

        public OverrunBehaviorEnum TextOverrunBehavior
        {
            set { godot_icall_Object_SetInt(nativeInstance, "text_overrun_behavior", (int)value); }
        }

        public int LinesSkipped
        {
            set { godot_icall_Object_SetInt(nativeInstance, "lines_skipped", value); }
        }

        public int MaxLinesVisible
        {
            set { godot_icall_Object_SetInt(nativeInstance, "max_lines_visible", value); }
        }

        public int VisibleCharacters
        {
            set { godot_icall_Object_SetInt(nativeInstance, "visible_characters", value); }
        }

        public float VisibleRatio
        {
            set { godot_icall_Object_SetFloat(nativeInstance, "visible_ratio", new FloatIntUnion { FloatValue = value }.IntValue); }
        }

        public bool UpperCase
        {
            set { godot_icall_Object_SetBool(nativeInstance, "uppercase", value); }
            get { return godot_icall_Object_CallNoArgsBool(nativeInstance, "is_uppercase"); }
        }
    }

    public class VBoxContainer : Control
    {
        public VBoxContainer()
        {
            if (nativeInstance == 0)
            {
                nativeInstance = godot_icall_CreateObject("VBoxContainer");
            }
            else if (!godot_icall_Object_IsClass(nativeInstance, "VBoxContainer"))
            {
                godot_icall_Object_Delete(nativeInstance);
                nativeInstance = godot_icall_CreateObject("VBoxContainer");
            }
        }
    }

    public class HBoxContainer : Control
    {
        public HBoxContainer()
        {
            if (nativeInstance == 0)
            {
                nativeInstance = godot_icall_CreateObject("HBoxContainer");
            }
            else if (!godot_icall_Object_IsClass(nativeInstance, "HBoxContainer"))
            {
                godot_icall_Object_Delete(nativeInstance);
                nativeInstance = godot_icall_CreateObject("HBoxContainer");
            }
        }
    }

    public class ScrollContainer : Control
    {
        public ScrollContainer()
        {
            if (nativeInstance == 0)
            {
                nativeInstance = godot_icall_CreateObject("ScrollContainer");
            }
            else if (!godot_icall_Object_IsClass(nativeInstance, "ScrollContainer"))
            {
                godot_icall_Object_Delete(nativeInstance);
                nativeInstance = godot_icall_CreateObject("ScrollContainer");
            }
        }

        public int ScrollVertical
        {
            set { godot_icall_Object_SetInt(nativeInstance, "scroll_vertical", value); }
        }

        public VScrollBar GetVScrollBar()
        {
            long ptr = godot_icall_Object_CallNoArgsObject(nativeInstance, "get_v_scroll_bar");
            VScrollBar bar = new VScrollBar();
            bar.nativeInstance = ptr;
            return bar;
        }
    }

    public class VScrollBar : Control
    {
        public VScrollBar() { }

        public double MaxValue
        {
            get { long bits = godot_icall_Object_GetFloat(nativeInstance, "max_value"); return new DoubleLongUnion { LongValue = bits }.DoubleValue; }
        }
    }

    public class HSeparator : Control
    {
        public HSeparator()
        {
            if (nativeInstance == 0)
            {
                nativeInstance = godot_icall_CreateObject("HSeparator");
            }
            else if (!godot_icall_Object_IsClass(nativeInstance, "HSeparator"))
            {
                godot_icall_Object_Delete(nativeInstance);
                nativeInstance = godot_icall_CreateObject("HSeparator");
            }
        }
    }

    public class StyleBoxFlat : GodotObject
    {
        public StyleBoxFlat()
        {
            if (nativeInstance == 0)
            {
                nativeInstance = godot_icall_CreateObject("StyleBoxFlat");
            }
            else if (!godot_icall_Object_IsClass(nativeInstance, "StyleBoxFlat"))
            {
                godot_icall_Object_Delete(nativeInstance);
                nativeInstance = godot_icall_CreateObject("StyleBoxFlat");
            }
        }

        public Color BgColor
        {
            set { godot_icall_Object_SetColor(nativeInstance, "bg_color", new FloatIntUnion { FloatValue = value.r }.IntValue, new FloatIntUnion { FloatValue = value.g }.IntValue, new FloatIntUnion { FloatValue = value.b }.IntValue, new FloatIntUnion { FloatValue = value.a }.IntValue); }
        }

        public Color BorderColor
        {
            set { godot_icall_Object_SetColor(nativeInstance, "border_color", new FloatIntUnion { FloatValue = value.r }.IntValue, new FloatIntUnion { FloatValue = value.g }.IntValue, new FloatIntUnion { FloatValue = value.b }.IntValue, new FloatIntUnion { FloatValue = value.a }.IntValue); }
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
