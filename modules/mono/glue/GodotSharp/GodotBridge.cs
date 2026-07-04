using System;
using System.Runtime.CompilerServices;
using System.Runtime.ConstrainedExecution;

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
    }
}
