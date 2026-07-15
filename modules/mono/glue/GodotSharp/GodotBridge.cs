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
        internal static extern void godot_icall_RefCounted_ReleaseRef(IntPtr nativePtr);

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

        // WASM-safe icalls for Label/Control (pointer-passing icalls used by UI wrappers)
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Engine_GetFps();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Label_SetFpsText(IntPtr labelPtr, int fps);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Object_SetIntText(IntPtr objPtr, string propName, int value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Label_SetPrefixedInt(IntPtr labelPtr, string prefix, int value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Label_AppendLog(IntPtr labelPtr, string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Control_SetPosition(IntPtr ctrlPtr, int x, int y);

        // WebSocket icalls - global pointer model (NO STRING RETURNS to C#)
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_WebSocket_Init(string url);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_WebSocket_PollAndGetState();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_WebSocket_Poll();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_WebSocket_GetState();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_WebSocket_SendText(string text);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_WebSocket_SendPrefixedInt(string prefix, int value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_WebSocket_GetSendCount();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_WebSocket_GetRecvCount();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_WebSocket_ShowLastMessage();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_WebSocket_GetPacketCount();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_WebSocket_Close();

        // Debug UI icalls - global pointer model (WASM-safe, no pointer passing)
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_DebugUi_Init();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_DebugUi_Clear();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_DebugUi_AddLine(string line);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_DebugUi_AddLineInt(string prefix, int value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_DebugUi_AddRow4(string prefix, int a, int b, int c, int d);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_DebugUi_AddPassFail(string testName, int passed);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_DebugUi_AddSeparator();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_DebugUi_GetLineCount();

        // Game UI icalls - 2048 visual grid (global pointer model, WASM-safe)
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_GameUI_Init();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_GameUI_SetTile(int row, int col, int value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_GameUI_SetScore(int score);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_GameUI_SetStatus(int state);

        // Test support icalls - global pointer model (WASM-safe)
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_Create(string className);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Test_AddToScene();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_AddChild(string className);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_GetChildCount();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Test_SetName(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Test_SetIntProp(string prop, int value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_GetIntProp(string prop);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Test_SetStringProp(string prop, string value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Test_CallVoidNoArgs(string method);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_CallIntNoArgs(string method);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_CallBoolNoArgs(string method);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Test_Free();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_IsValid();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_LoadScene(string path);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_InstantiateScene();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_GetSceneChildCount();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Test_FreeScene();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_IsWebPlatform();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_ConnectSignal(string signal);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_EmitSignal(string signal);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_GetSignalCount();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_GetClassCategory(string className);

        // Extended test icalls for comprehensive scenario testing
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_GetNameLen();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_RemoveChildIdx(int idx);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_HasMethod(string method);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_FileWrite(string path, string content);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_FileRead(string path);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_FileExists(string path);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_FileDelete(string path);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_Raycast3D();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_SetAudioVolume(int volume_db_x10);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_GetAudioVolume();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_AddAnimation(string animName);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_PlayAnimation(string animName);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_GetAnimationCount();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_IsAnimationPlaying(string animName);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_BclListTest();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_BclDictTest();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_BclAsyncTest();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_GcStressTest(int count);

        // Assertion framework icalls (WASM-safe: all string/int ops in C++)
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Test_Assert(string name, int condition);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Test_FinishTest(string testName);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_GetPassCount();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_GetFailCount();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Test_ResetCounters();

        // Register the GodotSynchronizationContext singleton for instance-based
        // pumping. Called from Runtime.Initialize() to avoid static method
        // dispatch via mono_runtime_invoke (WASM interpreter signature mismatch).
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_RegisterSyncContext(object instance);

        // Reflection: ClassDB metadata exposure
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string godot_icall_ClassDB_GetClassList();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_ClassDB_ClassExists(string className);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string godot_icall_ClassDB_GetParentClass(string className);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_ClassDB_IsParentClass(string childClass, string parentClass);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_ClassDB_CanInstantiate(string className);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string godot_icall_ClassDB_GetMethodList(string className);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_ClassDB_HasMethod(string className, string methodName);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_ClassDB_GetMethodArgCount(string className, string methodName);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string godot_icall_ClassDB_GetPropertyList(string className);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_ClassDB_HasProperty(string className, string propName);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string godot_icall_ClassDB_GetSignalList(string className);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_ClassDB_HasSignal(string className, string signalName);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string godot_icall_Object_GetClassName(object obj);
    }
}
