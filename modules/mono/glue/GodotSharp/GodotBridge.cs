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
        internal static extern string godot_icall_GetUserDataDir();

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

        // Runtime2D icalls: WASM-safe general-purpose Godot node manipulation.
        // All icalls use only IntPtr/string/int parameters - no object/array.
        // C# side tracks native nodes via IntPtr handles returned by NodeCreate.
        // Lifetime: explicit NodeFree(IntPtr), or freed with parent scene tree.
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr godot_icall_R2D_NodeCreate(string className);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_NodeFree(IntPtr node);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_AddChild(IntPtr parent, IntPtr child);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_SetName(IntPtr node, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_SetPosition(IntPtr node, int x, int y);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_SetSize(IntPtr node, int w, int h);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_SetAnchorsPreset(IntPtr node, int preset);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_LabelSetText(IntPtr node, string text);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_LabelSetPrefixedInt(IntPtr node, string prefix, int value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_LabelSetAlign(IntPtr node, int halign, int valign);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_LabelSetFontSize(IntPtr node, int size);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_LabelSetFontColor(IntPtr node, int r, int g, int b, int a);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_ColorRectSetColor(IntPtr node, int r, int g, int b, int a);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr godot_icall_R2D_GetTreeRoot();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr godot_icall_R2D_CreateCanvasLayer(IntPtr parent);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_R2D_GetMouseX();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_R2D_GetMouseY();

        // R2D Tile/Score/Status icalls: 彻底消除 C# 侧 BCL string/int 操作。
        // C# 只传 IntPtr/int，所有 string 构建/查表/颜色映射在 C++ 完成。
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_SetTileValue(IntPtr bg, IntPtr label, int value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_SetScore(IntPtr label, int score);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_LabelSetInt(IntPtr label, int value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_SetStatusText(IntPtr label, int state);

        // R2D Grid icalls: C++ 侧全局 int 数组，替代 C# new int[] 分配。
        // idx = row * dim + col。所有数组操作在 C++ 完成，零 BCL 调用。
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_GridCreate(int dim);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_GridSet(int idx, int val);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_R2D_GridGet(int idx);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_GridFill(int val);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_GridSave();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_GridRestore();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_R2D_GridSaveScore(int score);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_R2D_GridGetSavedScore();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_R2D_GridHasAdjacentEqual();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_R2D_GridHasZero();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_R2D_GridCountZero();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_R2D_GridFirstZero();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_R2D_GridRandomZero();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_R2D_GridSlideLine(int lineIdx, int direction, int isRow);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_R2D_GridChanged();

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
        internal static extern string godot_icall_Test_GetStringProp(string prop);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_SelectChild(int idx);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_SelectParent();

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

        // ===== Phase 0.1: Delegate probe icalls =====
        // Receives a delegate (as MonoObject) and pins it with a strong GCHandle.
        // Returns 1 on success, 0 on failure.
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_RegisterDelegateProbe(object delegateObj);

        // Invokes the registered delegate via mono_runtime_invoke on its Invoke method.
        // Returns 1 if mono_runtime_invoke succeeded without exception, 0 otherwise.
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_InvokeDelegateViaMRI();

        // Invokes the registered delegate via mono_compile_method +
        // direct function pointer call (no mono_runtime_invoke).
        // Returns 1 on success, 0 on failure.
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Test_InvokeDelegateViaFtnPtr();

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

        // A2 (W4): engine-data completion provider. Editor builds only —
        // export templates do not register this icall (TOOLS_ENABLED).
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string godot_icall_Editor_GetCodeCompletion(int kind, string scriptFile);

        // W5 SG PoC: compile-time [GlobalClass] registry push, called from
        // SourceGenerators-generated module initializers at assembly load.
        // int flags instead of bool — WASM interpreter icall convention.
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_ScriptRegistry_RegisterGlobalClass(string className, string baseType, int isTool, int isAbstract, string iconPath);

        // M10: Godot.Collections.Array icalls
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr godot_icall_Array_Ctor();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Array_Size(IntPtr ptr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern object godot_icall_Array_Get(IntPtr ptr, int index);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Array_Set(IntPtr ptr, int index, object value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Array_PushBack(IntPtr ptr, object value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Array_Clear(IntPtr ptr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Array_Dispose(IntPtr ptr);

        // M10: Godot.Collections.Dictionary icalls
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr godot_icall_Dict_Ctor();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Dict_Size(IntPtr ptr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern object godot_icall_Dict_Get(IntPtr ptr, object key);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Dict_Set(IntPtr ptr, object key, object value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool godot_icall_Dict_Has(IntPtr ptr, object key);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool godot_icall_Dict_Remove(IntPtr ptr, object key);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Dict_Clear(IntPtr ptr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Dict_Dispose(IntPtr ptr);

        // WeChat minigame audio adapter (InnerAudioContext-based).
        // Returns audio id (0 on failure). loop: 0=false, 1=true.
        // Desktop platforms stub: return 0 / no-op.
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_WXAudio_Play(string src, int loop);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_WXAudio_Stop(int id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_WXAudio_StopAll();

        // volume_x100: 0~100 (0.0~1.0 scaled by 100 to avoid float ABI issues)
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_WXAudio_SetVolume(int id, int volume_x100);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_WXAudio_Pause(int id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_WXAudio_Resume(int id);

        // ============================================================
        // Performance Benchmark icalls: WASM-safe high-resolution timer.
        // All timing, division, and formatting done in C++ to avoid
        // Mono WASM interpreter signature mismatch on arithmetic/BCL ops.
        // Column: G471 official (single column output for our port).
        // ============================================================

        // Print benchmark table header: "Benchmark                              G471 official"
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Bench_PrintHeader();

        // Print a section separator line, e.g. "--- Object.Call ---"
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Bench_PrintSection(string section_name);

        // Start timer (records high-res timestamp via Time::get_ticks_usec).
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Bench_Start();

        // Stop timer, compute average per-iteration in nanoseconds, format
        // as "X.XXXX" (4 decimal places), and append a DebugUi table row.
        // Parameters: benchmark_name = row label, iterations = loop count used.
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_Bench_EndPrint(string benchmark_name, int iterations);

        // Get default iteration count for this platform (WASM -> smaller, desktop -> larger).
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_Bench_DefaultIters();
    }
}
