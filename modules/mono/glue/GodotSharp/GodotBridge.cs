using System;
using System.Runtime.CompilerServices;

namespace Godot {
    internal static class Bridge {
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_GD_Print(string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Object_Free(IntPtr nativePtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern object godot_icall_Object_Get(IntPtr nativePtr, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Object_Set(IntPtr nativePtr, string name, object value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern object godot_icall_Object_Call(IntPtr nativePtr, string method, object[] args);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr godot_icall_Object_Ctor(object thisObj);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr godot_icall_Node_GetNode(IntPtr nativePtr, string path);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool godot_icall_Object_IsInstanceValid(IntPtr nativePtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr godot_icall_Callable_CreateFromDelegate(Delegate del);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern object godot_icall_Callable_Call(IntPtr callablePtr, object[] args);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Callable_Free(IntPtr callablePtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool godot_icall_Object_Connect(IntPtr nativePtr, string signal, IntPtr callablePtr, int flags);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Object_Disconnect(IntPtr nativePtr, string signal, IntPtr callablePtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool godot_icall_Object_IsConnected(IntPtr nativePtr, string signal, IntPtr callablePtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Object_EmitSignal(IntPtr nativePtr, string signal, object[] args);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool godot_icall_Object_HasSignal(IntPtr nativePtr, string signal);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr godot_icall_Object_InstantiateFromNative(string className);
    }
}
