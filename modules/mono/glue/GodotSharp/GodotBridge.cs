using System;
using System.Runtime.CompilerServices;

namespace Godot {
    [Preserve(AllMembers = true)]
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
        internal static extern void godot_icall_Object_BindNativePtr(object thisObj, IntPtr nativePtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr godot_icall_Node_GetNode(IntPtr nativePtr, string path);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr godot_icall_Node_GetParent(IntPtr nativePtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr godot_icall_Node_GetChild(IntPtr nativePtr, int idx);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Node_GetChildCount(IntPtr nativePtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Node_AddChild(IntPtr nativePtr, IntPtr childPtr, bool readable);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Node_RemoveChild(IntPtr nativePtr, IntPtr childPtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Node_QueueFree(IntPtr nativePtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Node_SetProcess(IntPtr nativePtr, bool enable);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Node_SetPhysicsProcess(IntPtr nativePtr, bool enable);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Node_SetProcessInput(IntPtr nativePtr, bool enable);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr godot_icall_Node_GetTree(IntPtr nativePtr);

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

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr godot_icall_ResourceLoader_Load(string path);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr godot_icall_PackedScene_Instantiate(IntPtr scenePtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Platform_GetRuntimeInfo();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool godot_icall_Input_IsKeyPressed(int keyCode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool godot_icall_Input_IsMouseButtonPressed(int button);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern object godot_icall_Input_GetMousePosition();
    }
}
