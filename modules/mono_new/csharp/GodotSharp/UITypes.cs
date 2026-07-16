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

        public void AddThemeFontSizeOverride(string name, int size)
        {
            godot_icall_Object_CallStringInt(nativeInstance, "add_theme_font_size_override", name, (long)size);
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
            int ptr = godot_icall_Object_CallNoArgsObject(nativeInstance, "get_v_scroll_bar");
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
