using System;
using System.Runtime.CompilerServices;

namespace Godot
{
    // Extra node types needed by test scenes. Each wraps a native Godot object
    // created via godot_icall_CreateObject(className).
    public class Button : Control
    {
        public Button()
        {
            {
        if (nativeInstance == 0)
        {
            nativeInstance = godot_icall_CreateObject("Button");
        }
        else if (!godot_icall_Object_IsClass(nativeInstance, "Button"))
        {
            godot_icall_Object_Delete(nativeInstance);
            nativeInstance = godot_icall_CreateObject("Button");
        }
    }
        }

        public string Text
        {
            set { godot_icall_Object_SetString(nativeInstance, "text", value); }
            get { return godot_icall_Object_GetString(nativeInstance, "text"); }
        }

        public bool Disabled
        {
            set { godot_icall_Object_SetBool(nativeInstance, "disabled", value); }
        }
    }

    public class LineEdit : Control
    {
        public LineEdit()
        {
            {
        if (nativeInstance == 0)
        {
            nativeInstance = godot_icall_CreateObject("LineEdit");
        }
        else if (!godot_icall_Object_IsClass(nativeInstance, "LineEdit"))
        {
            godot_icall_Object_Delete(nativeInstance);
            nativeInstance = godot_icall_CreateObject("LineEdit");
        }
    }
        }

        public string Text
        {
            set { godot_icall_Object_SetString(nativeInstance, "text", value); }
            get { return godot_icall_Object_GetString(nativeInstance, "text"); }
        }

        public string PlaceholderText
        {
            set { godot_icall_Object_SetString(nativeInstance, "placeholder_text", value); }
        }
    }

    public class ColorRect : Control
    {
        public ColorRect()
        {
            {
        if (nativeInstance == 0)
        {
            nativeInstance = godot_icall_CreateObject("ColorRect");
        }
        else if (!godot_icall_Object_IsClass(nativeInstance, "ColorRect"))
        {
            godot_icall_Object_Delete(nativeInstance);
            nativeInstance = godot_icall_CreateObject("ColorRect");
        }
    }
        }

        public Color Color
        {
            set { godot_icall_Object_SetColor(nativeInstance, "color", new FloatIntUnion { FloatValue = value.r }.IntValue, new FloatIntUnion { FloatValue = value.g }.IntValue, new FloatIntUnion { FloatValue = value.b }.IntValue, new FloatIntUnion { FloatValue = value.a }.IntValue); }
        }
    }

    public class Sprite2D : Node2D
    {
        public Sprite2D()
        {
            {
        if (nativeInstance == 0)
        {
            nativeInstance = godot_icall_CreateObject("Sprite2D");
        }
        else if (!godot_icall_Object_IsClass(nativeInstance, "Sprite2D"))
        {
            godot_icall_Object_Delete(nativeInstance);
            nativeInstance = godot_icall_CreateObject("Sprite2D");
        }
    }
        }

        public new Vector2 Position
        {
            set { godot_icall_Object_SetVector2(nativeInstance, "position", new FloatIntUnion { FloatValue = value.x }.IntValue, new FloatIntUnion { FloatValue = value.y }.IntValue); }
        }
    }

    public class Camera2D : Node2D
    {
        public Camera2D()
        {
            {
        if (nativeInstance == 0)
        {
            nativeInstance = godot_icall_CreateObject("Camera2D");
        }
        else if (!godot_icall_Object_IsClass(nativeInstance, "Camera2D"))
        {
            godot_icall_Object_Delete(nativeInstance);
            nativeInstance = godot_icall_CreateObject("Camera2D");
        }
    }
        }

        public new Vector2 Position
        {
            set { godot_icall_Object_SetVector2(nativeInstance, "position", new FloatIntUnion { FloatValue = value.x }.IntValue, new FloatIntUnion { FloatValue = value.y }.IntValue); }
        }
    }

    public class Timer : Node
    {
        public Timer()
        {
            {
        if (nativeInstance == 0)
        {
            nativeInstance = godot_icall_CreateObject("Timer");
            ownsNative = true;
        }
        else if (!godot_icall_Object_IsClass(nativeInstance, "Timer"))
        {
            godot_icall_Object_Delete(nativeInstance);
            nativeInstance = godot_icall_CreateObject("Timer");
            ownsNative = true;
        }
    }
        }

        public double WaitTime
        {
            set { godot_icall_Object_SetFloat(nativeInstance, "wait_time", new FloatIntUnion { FloatValue = (float)value }.IntValue); }
        }

        public bool Autostart
        {
            set { godot_icall_Object_SetBool(nativeInstance, "autostart", value); }
        }

        public bool OneShot
        {
            set { godot_icall_Object_SetBool(nativeInstance, "one_shot", value); }
        }

        public void Start()
        {
            godot_icall_Object_CallNoArgs(nativeInstance, "start");
        }

        public void Stop()
        {
            godot_icall_Object_CallNoArgs(nativeInstance, "stop");
        }
    }

    public class AnimationPlayer : Node
    {
        public AnimationPlayer()
        {
            {
        if (nativeInstance == 0)
        {
            nativeInstance = godot_icall_CreateObject("AnimationPlayer");
            ownsNative = true;
        }
        else if (!godot_icall_Object_IsClass(nativeInstance, "AnimationPlayer"))
        {
            godot_icall_Object_Delete(nativeInstance);
            nativeInstance = godot_icall_CreateObject("AnimationPlayer");
            ownsNative = true;
        }
    }
        }

        public void Play(string animName)
        {
            godot_icall_Object_CallString(nativeInstance, "play", animName);
        }

        public void Stop()
        {
            godot_icall_Object_CallNoArgs(nativeInstance, "stop");
        }

        public bool IsPlaying()
        {
            return godot_icall_Object_CallNoArgsBool(nativeInstance, "is_playing");
        }
    }

    public class RigidBody2D : Node2D
    {
        public RigidBody2D()
        {
            {
        if (nativeInstance == 0)
        {
            nativeInstance = godot_icall_CreateObject("RigidBody2D");
        }
        else if (!godot_icall_Object_IsClass(nativeInstance, "RigidBody2D"))
        {
            godot_icall_Object_Delete(nativeInstance);
            nativeInstance = godot_icall_CreateObject("RigidBody2D");
        }
    }
        }

        public new Vector2 Position
        {
            set { godot_icall_Object_SetVector2(nativeInstance, "position", new FloatIntUnion { FloatValue = value.x }.IntValue, new FloatIntUnion { FloatValue = value.y }.IntValue); }
        }
    }

    public class CollisionShape2D : Node2D
    {
        public CollisionShape2D()
        {
            {
        if (nativeInstance == 0)
        {
            nativeInstance = godot_icall_CreateObject("CollisionShape2D");
        }
        else if (!godot_icall_Object_IsClass(nativeInstance, "CollisionShape2D"))
        {
            godot_icall_Object_Delete(nativeInstance);
            nativeInstance = godot_icall_CreateObject("CollisionShape2D");
        }
    }
        }
    }

    public class Area2D : Node2D
    {
        public Area2D()
        {
            {
        if (nativeInstance == 0)
        {
            nativeInstance = godot_icall_CreateObject("Area2D");
        }
        else if (!godot_icall_Object_IsClass(nativeInstance, "Area2D"))
        {
            godot_icall_Object_Delete(nativeInstance);
            nativeInstance = godot_icall_CreateObject("Area2D");
        }
    }
        }
    }

    public class AudioStreamPlayer : Node
    {
        public AudioStreamPlayer()
        {
            {
        if (nativeInstance == 0)
        {
            nativeInstance = godot_icall_CreateObject("AudioStreamPlayer");
            ownsNative = true;
        }
        else if (!godot_icall_Object_IsClass(nativeInstance, "AudioStreamPlayer"))
        {
            godot_icall_Object_Delete(nativeInstance);
            nativeInstance = godot_icall_CreateObject("AudioStreamPlayer");
            ownsNative = true;
        }
    }
        }

        public void Play()
        {
            godot_icall_Object_CallNoArgs(nativeInstance, "play");
        }

        public void Stop()
        {
            godot_icall_Object_CallNoArgs(nativeInstance, "stop");
        }

        public float VolumeDb
        {
            set { godot_icall_Object_SetFloat(nativeInstance, "volume_db", new FloatIntUnion { FloatValue = value }.IntValue); }
        }
    }

    public class TouchScreenButton : Node2D
    {
        public TouchScreenButton()
        {
            {
        if (nativeInstance == 0)
        {
            nativeInstance = godot_icall_CreateObject("TouchScreenButton");
        }
        else if (!godot_icall_Object_IsClass(nativeInstance, "TouchScreenButton"))
        {
            godot_icall_Object_Delete(nativeInstance);
            nativeInstance = godot_icall_CreateObject("TouchScreenButton");
        }
    }
        }

        public string Text
        {
            set { godot_icall_Object_SetString(nativeInstance, "text", value); }
        }
    }
}
