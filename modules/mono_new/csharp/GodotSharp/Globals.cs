using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Godot
{
    // WASM-safe union: reinterpret int64 bit pattern as double.
    // Pure memory operation - no double arithmetic, no icall boundary.
    [StructLayout(LayoutKind.Explicit)]
    internal struct DoubleLongUnion
    {
        [FieldOffset(0)] public long LongValue;
        [FieldOffset(0)] public double DoubleValue;
    }

    // WASM-safe union: reinterpret int32 bit pattern as float.
    [StructLayout(LayoutKind.Explicit)]
    internal struct FloatIntUnion
    {
        [FieldOffset(0)] public int IntValue;
        [FieldOffset(0)] public float FloatValue;
    }

    public class Callable
    {
        internal int nativeCallable;
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
            if (target != null && method != null)
            {
                nativeCallable = godot_icall_Callable_CreateFromTarget(target.nativeInstance, method);
            }
}

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int godot_icall_Callable_CreateFromTarget(int target, string method);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static Callable godot_icall_Callable_CreateFromDelegate(Delegate @delegate);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Callable_Call(int nativeCallable, object[] args, out object ret);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Callable_Free(int nativeCallable);

        public void Call(params object[] args)
        {
            if (nativeCallable != 0)
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
        ~Callable()
        {
            if (nativeCallable != 0)
            {
                godot_icall_Callable_Free(nativeCallable);
                nativeCallable = 0;
            }
        }
    }

    public class Signal
    {
        internal int nativeSignal = 0;
        public GodotObject Owner { get; }
        public StringName Name { get; }

        public Signal(GodotObject owner, StringName name)
        {
            Owner = owner;
            Name = name;
        }
    }

    public static partial class GD
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_GD_Print(string msg);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_GD_PrintErr(string msg);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_GD_Randi();

        // WASM workaround: icall returns int64 (IEEE-754 bit pattern) to avoid
        // "CANNOT HANDLE COOKIE D" in do_icall. Union reinterprets to double.
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_GD_Randf();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static T godot_icall_GD_Load<T>(string path) where T : GodotObject;

        // String overload - avoids string.Join which is unstable in WASM interpreter mode.
        public static void Print(string msg)
        {
            godot_icall_GD_Print(msg ?? string.Empty);
        }

        // Params overload - optimizes single-string case to avoid string.Join.
        public static void Print(params object[] args)
        {
            if (args == null || args.Length == 0)
            {
                godot_icall_GD_Print(string.Empty);
                return;
            }
            if (args.Length == 1 && args[0] is string)
            {
                godot_icall_GD_Print((string)args[0]);
                return;
            }
            string message = string.Join(" ", args);
            godot_icall_GD_Print(message);
        }

        // String overload for PrintErr
        public static void PrintErr(string msg)
        {
            godot_icall_GD_PrintErr(msg ?? string.Empty);
        }

        public static void PrintErr(params object[] args)
        {
            if (args == null || args.Length == 0)
            {
                godot_icall_GD_PrintErr(string.Empty);
                return;
            }
            if (args.Length == 1 && args[0] is string)
            {
                godot_icall_GD_PrintErr((string)args[0]);
                return;
            }
            string message = string.Join(" ", args);
            godot_icall_GD_PrintErr(message);
        }

        public static long Randi()
        {
            return godot_icall_GD_Randi();
        }

        public static double Randf()
        {
            long bits = godot_icall_GD_Randf();
            return new DoubleLongUnion { LongValue = bits }.DoubleValue;
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

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string godot_icall_GD_DoubleToString(long valBits);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string godot_icall_GD_Int64ToString(long val);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string godot_icall_GD_FloatToString(int valBits);

        // Safe ToString for numeric types - avoids Double.ToString() crash in WASM
        public static string ToString(double val)
        {
            long bits = new DoubleLongUnion { DoubleValue = val }.LongValue;
            return godot_icall_GD_DoubleToString(bits);
        }

        public static string ToString(float val)
        {
            int bits = new FloatIntUnion { FloatValue = val }.IntValue;
            return godot_icall_GD_FloatToString(bits);
        }

        public static string ToString(long val)
        {
            return godot_icall_GD_Int64ToString(val);
        }

        public static string ToString(int val)
        {
            return godot_icall_GD_Int64ToString((long)val);
        }

        // Safe string concatenation helper - avoids string.Concat crash in WASM
        public static string Concat(string a, string b)
        {
            if (a == null) a = string.Empty;
            if (b == null) b = string.Empty;
            char[] chars = new char[a.Length + b.Length];
            for (int i = 0; i < a.Length; i++) chars[i] = a[i];
            for (int i = 0; i < b.Length; i++) chars[a.Length + i] = b[i];
            return new string(chars);
        }

        public static string Concat(string a, string b, string c)
        {
            return Concat(Concat(a, b), c);
        }

        public static string Concat(params string[] parts)
        {
            if (parts == null || parts.Length == 0) return string.Empty;
            string result = parts[0] ?? string.Empty;
            for (int i = 1; i < parts.Length; i++)
            {
                result = Concat(result, parts[i]);
            }
            return result;
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

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Input_IsActionJustPressed(string action);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Input_IsActionJustReleased(string action);

        public static bool IsActionJustPressed(StringName action)
        {
            return godot_icall_Input_IsActionJustPressed(action.ToString());
        }

        public static bool IsActionJustReleased(StringName action)
        {
            return godot_icall_Input_IsActionJustReleased(action.ToString());
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
		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static bool godot_icall_Engine_IsEditorHint();

		public static bool IsEditorHint() => godot_icall_Engine_IsEditorHint();

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static int godot_icall_Engine_GetFramesPerSecond();

		public static int GetFramesPerSecond()
		{
			return godot_icall_Engine_GetFramesPerSecond();
		}
	}

	public static class OS
	{
		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static string godot_icall_OS_GetName();

		public static string GetName() => godot_icall_OS_GetName();

		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static long godot_icall_OS_GetStaticMemoryUsage();

		public static long GetStaticMemoryUsage()
		{
			return godot_icall_OS_GetStaticMemoryUsage();
		}
	}

	public static class Time
	{
		[MethodImpl(MethodImplOptions.InternalCall)]
		internal extern static string godot_icall_Time_GetTimeStringFromSystem();

		public static string GetTimeStringFromSystem()
		{
			return godot_icall_Time_GetTimeStringFromSystem();
		}
	}
}
