using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Godot
{
    public class Callable
    {
        internal IntPtr nativeCallable;
        internal Delegate TargetDelegate;

        public Callable()
        {
        }

        public Callable(Delegate @delegate)
        {
            TargetDelegate = @delegate;
        }

        public Callable(GodotObject target, string method)
        {
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static Callable godot_icall_Callable_CreateFromDelegate(Delegate @delegate);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Callable_Call(IntPtr nativeCallable, object[] args, out object ret);

        public void Call(params object[] args)
        {
            if (nativeCallable != IntPtr.Zero)
            {
                godot_icall_Callable_Call(nativeCallable, args, out _);
            }
            else if (TargetDelegate != null)
            {
                TargetDelegate.DynamicInvoke(args);
            }
        }

        public static Callable FromDelegate(Delegate @delegate)
        {
            return godot_icall_Callable_CreateFromDelegate(@delegate) ?? new Callable(@delegate);
        }

        public static implicit operator Callable(Delegate @delegate)
        {
            return FromDelegate(@delegate);
        }
    }

    public class Signal
    {
        internal IntPtr nativeSignal;
        public GodotObject Owner { get; }
        public StringName Name { get; }

        public Signal(GodotObject owner, StringName name)
        {
            Owner = owner;
            Name = name;
        }
    }

    public static class GD
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_GD_Print(string msg);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_GD_PrintErr(string msg);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_GD_Randi();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static double godot_icall_GD_Randf();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static T godot_icall_GD_Load<T>(string path) where T : GodotObject;

        public static void Print(params object[] args)
        {
            string message = string.Join(" ", args);
            godot_icall_GD_Print(message);
        }

        public static void PrintErr(params object[] args)
        {
            string message = string.Join(" ", args);
            godot_icall_GD_PrintErr(message);
        }

        public static long Randi()
        {
            return godot_icall_GD_Randi();
        }

        public static double Randf()
        {
            return godot_icall_GD_Randf();
        }

        public static T Load<T>(string path) where T : GodotObject
        {
            return godot_icall_GD_Load<T>(path);
        }

        public static void PushWarning(string message)
        {
            PrintErr("WARNING: " + message);
        }

        public static void PushError(string message)
        {
            PrintErr("ERROR: " + message);
        }
    }

    public static class Input
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Input_IsKeyPressed(long key);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Input_IsActionPressed(string action);

        public static bool IsKeyPressed(Key keycode)
        {
            return godot_icall_Input_IsKeyPressed((long)keycode);
        }

        public static bool IsActionPressed(StringName action)
        {
            return godot_icall_Input_IsActionPressed(action.ToString());
        }

        public static bool IsActionJustPressed(StringName action)
        {
            return false;
        }

        public static bool IsActionJustReleased(StringName action)
        {
            return false;
        }

        public static Vector2 GetVector(StringName negativeX, StringName positiveX, StringName negativeY, StringName positiveY, float deadzone = 0.5f)
        {
            float x = 0, y = 0;
            if (IsActionPressed(negativeX)) x -= 1;
            if (IsActionPressed(positiveX)) x += 1;
            if (IsActionPressed(negativeY)) y -= 1;
            if (IsActionPressed(positiveY)) y += 1;
            return new Vector2(x, y);
        }
    }

    public static class Engine
    {
        public static bool IsEditorHint() => false;
    }

    public static class OS
    {
        public static string GetName() => "Windows";
    }
}