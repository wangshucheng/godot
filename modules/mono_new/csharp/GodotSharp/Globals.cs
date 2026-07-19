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
    // IMPORTANT: This struct is 4 bytes ONLY. Do NOT add an 8-byte LongValue
    // field here - it would cause 8-byte writes past the struct boundary and
    // corrupt WASM linear memory, leading to "function signature mismatch".
    // For float-returning icalls that give back a sign-extended int64 bit pattern,
    // cast to int FIRST: new FloatIntUnion { IntValue = (int)icall() }.FloatValue
    [StructLayout(LayoutKind.Explicit)]
    internal struct FloatIntUnion
    {
        [FieldOffset(0)] public int IntValue;
        [FieldOffset(0)] public float FloatValue;
    }

    public class Callable
    {
        internal long nativeCallable;
        internal Delegate TargetDelegate;

        public Callable()
        {
        }

        public Callable(Delegate @delegate)
        {
            TargetDelegate = @delegate;
            // S5 修复: 立即创建 native Callable 并缓存到 nativeCallable。
            // 这样 Connect/Disconnect/IsConnected 用同一指针，compare_equal 能匹配。
            // 原实现每次都新建 Callable（不同 gchandle），导致 Disconnect 永远找不到匹配项。
            // 注意: godot_icall_Callable_CreateFromDelegatePtr 声明在 Signal 类
            // (与 C++ 注册名 Godot.Signal::... 对齐)，需用 Signal. 限定。
            if (@delegate != null)
            {
                nativeCallable = Signal.godot_icall_Callable_CreateFromDelegatePtr(@delegate);
            }
        }

        public Callable(GodotObject target, string method)
        {
            if (target != null && method != null)
            {
                nativeCallable = godot_icall_Callable_CreateFromTarget(target.nativeInstance, method);
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Callable_CreateFromTarget(long target, string method);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static Callable godot_icall_Callable_CreateFromDelegate(Delegate @delegate);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Callable_Call(long nativeCallable, object[] args, out object ret);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Callable_Free(long nativeCallable);

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
            // S5 修复: 直接用构造函数，构造函数内部会创建并缓存 nativeCallable
            return new Callable(@delegate);
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
        // nativeSignal holds an int64 pointer to a native Callable (signal connection).
        // Must be `long` (8 bytes) to match C++ pointer width on 64-bit platforms.
        internal long nativeSignal = 0;
        public GodotObject Owner { get; }
        public StringName Name { get; }

        public Signal(GodotObject owner, StringName name)
        {
            Owner = owner;
            Name = name;
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Signal_Connect(long ownerPtr, string signal, long callablePtr, bool oneshot);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Signal_Disconnect(long ownerPtr, string signal, long callablePtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_Signal_IsConnected(long ownerPtr, string signal, long callablePtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_Signal_Emit(long ownerPtr, string signal, object[] args);

        public bool Connect(Callable callable, uint flags = 0)
        {
            if (Owner == null || Name == null || callable == null) return false;
            bool oneshot = (flags & (uint)GodotObject.ConnectFlags.OneShot) != 0;
            long callablePtr = callable.nativeCallable;
            if (callablePtr == 0 && callable.TargetDelegate != null)
            {
                // Fallback: wrap delegate-based Callable into native Callable.
                callablePtr = godot_icall_Callable_CreateFromDelegatePtr(callable.TargetDelegate);
            }
            if (callablePtr == 0) return false;
            return godot_icall_Signal_Connect(Owner.nativeInstance, Name.ToString(), callablePtr, oneshot);
        }

        public void Disconnect(Callable callable)
        {
            if (Owner == null || Name == null || callable == null) return;
            long callablePtr = callable.nativeCallable;
            if (callablePtr == 0 && callable.TargetDelegate != null)
            {
                callablePtr = godot_icall_Callable_CreateFromDelegatePtr(callable.TargetDelegate);
            }
            if (callablePtr == 0) return;
            godot_icall_Signal_Disconnect(Owner.nativeInstance, Name.ToString(), callablePtr);
        }

        public bool IsConnected(Callable callable)
        {
            if (Owner == null || Name == null || callable == null) return false;
            long callablePtr = callable.nativeCallable;
            if (callablePtr == 0 && callable.TargetDelegate != null)
            {
                callablePtr = godot_icall_Callable_CreateFromDelegatePtr(callable.TargetDelegate);
            }
            if (callablePtr == 0) return false;
            return godot_icall_Signal_IsConnected(Owner.nativeInstance, Name.ToString(), callablePtr);
        }

        public void Emit(params object[] args)
        {
            if (Owner == null || Name == null) return;
            godot_icall_Signal_Emit(Owner.nativeInstance, Name.ToString(), args ?? new object[0]);
        }

        // Helper icall: wrap a Delegate into a native Callable and return its pointer.
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static long godot_icall_Callable_CreateFromDelegatePtr(Delegate @delegate);
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
        internal extern static GodotObject godot_icall_GD_Load(string path);

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
            GodotObject obj = godot_icall_GD_Load(path);
            return obj as T;
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
