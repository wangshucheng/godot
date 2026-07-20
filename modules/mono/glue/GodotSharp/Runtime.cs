using System;
using System.Threading;

namespace Godot {
    [Preserve(AllMembers = true)]
    public static class Runtime {
        private static bool _initialized = false;

        public static void Initialize() {
            if (_initialized) return;
            _initialized = true;

            Platform.Initialize();
            // Register the sync context instance with C++ only on non-Web platforms.
            // On Web (WASM interpreter), mono_runtime_invoke in pump_sync_context
            // triggers function signature mismatch. Web is single-threaded and
            // doesn't need cross-thread continuation pumping.
            if (!Platform.IsWeb && GodotSynchronizationContext.Instance != null) {
                Bridge.godot_icall_RegisterSyncContext(GodotSynchronizationContext.Instance);
            }
        }

        public static bool IsInitialized => _initialized;

        // Get engine FPS via C++ (safe in _Process)
        public static int GetEngineFps() {
            return Bridge.godot_icall_Engine_GetFps();
        }

        // =============================================
        // WebSocket helpers: global pointer model (WASM-safe).
        // NO STRING RETURNS - all text ops done in C++.
        // =============================================

        public const int WsStateConnecting = 0;
        public const int WsStateOpen = 1;
        public const int WsStateClosing = 2;
        public const int WsStateClosed = 3;

        public static int WsInit(string url) {
            return Bridge.godot_icall_WebSocket_Init(url);
        }
        public static int WsPollAndGetState() {
            return Bridge.godot_icall_WebSocket_PollAndGetState();
        }
        public static void WsPoll() {
            Bridge.godot_icall_WebSocket_Poll();
        }
        public static int WsGetState() {
            return Bridge.godot_icall_WebSocket_GetState();
        }
        public static int WsSendText(string text) {
            return Bridge.godot_icall_WebSocket_SendText(text);
        }
        public static int WsSendPrefixedInt(string prefix, int value) {
            return Bridge.godot_icall_WebSocket_SendPrefixedInt(prefix, value);
        }
        public static int WsGetSendCount() {
            return Bridge.godot_icall_WebSocket_GetSendCount();
        }
        public static int WsGetRecvCount() {
            return Bridge.godot_icall_WebSocket_GetRecvCount();
        }
        public static void WsShowLastMessage() {
            Bridge.godot_icall_WebSocket_ShowLastMessage();
        }
        public static int WsGetPacketCount() {
            return Bridge.godot_icall_WebSocket_GetPacketCount();
        }
        public static void WsClose() {
            Bridge.godot_icall_WebSocket_Close();
        }

        // =============================================
        // Debug UI helpers: global pointer model (WASM-safe).
        // All operations are void or int return; no string ops in C#.
        // =============================================

        public static int DebugUiInit() {
            return Bridge.godot_icall_DebugUi_Init();
        }
        public static void DebugUiClear() {
            Bridge.godot_icall_DebugUi_Clear();
        }
        public static void DebugUiAddLine(string line) {
            Bridge.godot_icall_DebugUi_AddLine(line);
        }
        public static void DebugUiAddLineInt(string prefix, int value) {
            Bridge.godot_icall_DebugUi_AddLineInt(prefix, value);
        }
        public static void DebugUiAddRow4(string prefix, int a, int b, int c, int d) {
            Bridge.godot_icall_DebugUi_AddRow4(prefix, a, b, c, d);
        }
        public static void DebugUiAddPassFail(string testName, bool passed) {
            Bridge.godot_icall_DebugUi_AddPassFail(testName, passed ? 1 : 0);
        }
        public static void DebugUiAddSeparator() {
            Bridge.godot_icall_DebugUi_AddSeparator();
        }
        public static int DebugUiGetLineCount() {
            return Bridge.godot_icall_DebugUi_GetLineCount();
        }

        // =============================================
        // Game UI helpers: 2048 visual grid (WASM-safe).
        // All operations are void or int return; no string ops in C#.
        // =============================================

        // Initialize the 4x4 game grid UI. Call once.
        public static int GameUiInit() {
            return Bridge.godot_icall_GameUI_Init();
        }

        // Update a tile's value (row/col 0-3, value 0=empty)
        public static void GameUiSetTile(int row, int col, int value) {
            Bridge.godot_icall_GameUI_SetTile(row, col, value);
        }

        // Update the score display
        public static void GameUiSetScore(int score) {
            Bridge.godot_icall_GameUI_SetScore(score);
        }

        // Set status text: 0=playing, 1=win, 2=gameover
        public static void GameUiSetStatus(int state) {
            Bridge.godot_icall_GameUI_SetStatus(state);
        }

        // =============================================
        // Runtime2D helpers: WASM-safe general-purpose Godot node manipulation.
        //
        // 用途：让 C# 用户代码在 WASM 端能直接创建/操作 Godot 节点，
        // 绕开 GodotObject.Set/Call（带 object 参数，在 WASM 解释器下
        // 触发 signature mismatch）。
        //
        // 模式：C# 持 IntPtr 句柄，所有操作走静态 icall。
        // 参数只用 IntPtr/string/int；返回 IntPtr/int/void。
        //
        // 对齐枚举：
        //   Label halign/valign: 0=Begin, 1=Center, 2=End
        //   Control anchors preset: 15=FullRect, 0=TopLeft, 1=TopRight...
        //   颜色 RGBA: 0-255 整数（避免 WASM float ABI 问题）
        // =============================================

        // Anchors preset constants (Control::LayoutPreset)
        public const int PresetTopLeft = 0;
        public const int PresetTopRight = 1;
        public const int PresetBottomRight = 2;
        public const int PresetBottomLeft = 3;
        public const int PresetCenter = 8;
        public const int PresetFullRect = 15;

        // Label alignment constants
        public const int AlignBegin = 0;
        public const int AlignCenter = 1;
        public const int AlignEnd = 2;

        // Create a Node by class name. Returns IntPtr (Zero on failure).
        // Example: Runtime.R2DNodeCreate("Label")
        public static IntPtr R2DNodeCreate(string className) {
            return Bridge.godot_icall_R2D_NodeCreate(className);
        }

        // Free a Node (queue_free if Node, else memdelete).
        public static void R2DNodeFree(IntPtr node) {
            Bridge.godot_icall_R2D_NodeFree(node);
        }

        // Add child node. parent must be a Node.
        public static void R2DAddChild(IntPtr parent, IntPtr child) {
            Bridge.godot_icall_R2D_AddChild(parent, child);
        }

        // Set Node name.
        public static void R2DSetName(IntPtr node, string name) {
            Bridge.godot_icall_R2D_SetName(node, name);
        }

        // Set Control position (x, y).
        public static void R2DSetPosition(IntPtr node, int x, int y) {
            Bridge.godot_icall_R2D_SetPosition(node, x, y);
        }

        // Set Control size (w, h).
        public static void R2DSetSize(IntPtr node, int w, int h) {
            Bridge.godot_icall_R2D_SetSize(node, w, h);
        }

        // Set Control anchors preset (use PresetFullRect etc.)
        public static void R2DSetAnchorsPreset(IntPtr node, int preset) {
            Bridge.godot_icall_R2D_SetAnchorsPreset(node, preset);
        }

        // Set Label text.
        public static void R2DLabelSetText(IntPtr node, string text) {
            Bridge.godot_icall_R2D_LabelSetText(node, text);
        }

        // Set Label text to prefix + int (avoids C# int.ToString() WASM mismatch).
        public static void R2DLabelSetPrefixedInt(IntPtr node, string prefix, int value) {
            Bridge.godot_icall_R2D_LabelSetPrefixedInt(node, prefix, value);
        }

        // Set Label alignment (halign/valign: AlignBegin/AlignCenter/AlignEnd).
        public static void R2DLabelSetAlign(IntPtr node, int halign, int valign) {
            Bridge.godot_icall_R2D_LabelSetAlign(node, halign, valign);
        }

        // Set Label font size.
        public static void R2DLabelSetFontSize(IntPtr node, int size) {
            Bridge.godot_icall_R2D_LabelSetFontSize(node, size);
        }

        // Set Label font color (r,g,b,a 0-255).
        public static void R2DLabelSetFontColor(IntPtr node, int r, int g, int b, int a) {
            Bridge.godot_icall_R2D_LabelSetFontColor(node, r, g, b, a);
        }

        // Set ColorRect color (r,g,b,a 0-255).
        public static void R2DColorRectSetColor(IntPtr node, int r, int g, int b, int a) {
            Bridge.godot_icall_R2D_ColorRectSetColor(node, r, g, b, a);
        }

        // Get SceneTree root Window (the main viewport).
        public static IntPtr R2DGetTreeRoot() {
            return Bridge.godot_icall_R2D_GetTreeRoot();
        }

        // Create a CanvasLayer and add it to a parent Node. Returns IntPtr.
        public static IntPtr R2DCreateCanvasLayer(IntPtr parent) {
            return Bridge.godot_icall_R2D_CreateCanvasLayer(parent);
        }

        // Get mouse X (WASM-safe: int return, no Vector2 object).
        public static int R2DGetMouseX() {
            return Bridge.godot_icall_R2D_GetMouseX();
        }

        // Get mouse Y.
        public static int R2DGetMouseY() {
            return Bridge.godot_icall_R2D_GetMouseY();
        }

        // =============================================
        // Test support helpers: global pointer model (WASM-safe).
        // All operations use string/int only - no IntPtr.
        // =============================================

        public static int TestCreate(string className) {
            return Bridge.godot_icall_Test_Create(className);
        }
        public static void TestAddToScene() {
            Bridge.godot_icall_Test_AddToScene();
        }
        public static int TestAddChild(string className) {
            return Bridge.godot_icall_Test_AddChild(className);
        }
        public static int TestGetChildCount() {
            return Bridge.godot_icall_Test_GetChildCount();
        }
        public static void TestSetName(string name) {
            Bridge.godot_icall_Test_SetName(name);
        }
        public static void TestSetIntProp(string prop, int value) {
            Bridge.godot_icall_Test_SetIntProp(prop, value);
        }
        public static int TestGetIntProp(string prop) {
            return Bridge.godot_icall_Test_GetIntProp(prop);
        }
        public static void TestSetStringProp(string prop, string value) {
            Bridge.godot_icall_Test_SetStringProp(prop, value);
        }
        public static void TestCallVoid(string method) {
            Bridge.godot_icall_Test_CallVoidNoArgs(method);
        }
        public static int TestCallInt(string method) {
            return Bridge.godot_icall_Test_CallIntNoArgs(method);
        }
        public static int TestCallBool(string method) {
            return Bridge.godot_icall_Test_CallBoolNoArgs(method);
        }
        public static void TestFree() {
            Bridge.godot_icall_Test_Free();
        }
        public static int TestIsValid() {
            return Bridge.godot_icall_Test_IsValid();
        }
        public static int TestLoadScene(string path) {
            return Bridge.godot_icall_Test_LoadScene(path);
        }
        public static int TestInstantiateScene() {
            return Bridge.godot_icall_Test_InstantiateScene();
        }
        public static int TestGetSceneChildCount() {
            return Bridge.godot_icall_Test_GetSceneChildCount();
        }
        public static void TestFreeScene() {
            Bridge.godot_icall_Test_FreeScene();
        }
        public static int TestIsWebPlatform() {
            return Bridge.godot_icall_Test_IsWebPlatform();
        }
        public static int TestConnectSignal(string signal) {
            return Bridge.godot_icall_Test_ConnectSignal(signal);
        }
        public static int TestEmitSignal(string signal) {
            return Bridge.godot_icall_Test_EmitSignal(signal);
        }
        public static int TestGetSignalCount() {
            return Bridge.godot_icall_Test_GetSignalCount();
        }
        public static int TestGetClassCategory(string className) {
            return Bridge.godot_icall_Test_GetClassCategory(className);
        }

        // =============================================
        // Extended test helpers for comprehensive scenarios
        // =============================================

        public static int TestGetNameLen() {
            return Bridge.godot_icall_Test_GetNameLen();
        }
        public static int TestRemoveChildIdx(int idx) {
            return Bridge.godot_icall_Test_RemoveChildIdx(idx);
        }
        public static int TestHasMethod(string method) {
            return Bridge.godot_icall_Test_HasMethod(method);
        }
        public static int TestFileWrite(string path, string content) {
            return Bridge.godot_icall_Test_FileWrite(path, content);
        }
        public static int TestFileRead(string path) {
            return Bridge.godot_icall_Test_FileRead(path);
        }
        public static int TestFileExists(string path) {
            return Bridge.godot_icall_Test_FileExists(path);
        }
        public static int TestFileDelete(string path) {
            return Bridge.godot_icall_Test_FileDelete(path);
        }
        public static int TestRaycast3D() {
            return Bridge.godot_icall_Test_Raycast3D();
        }
        public static int TestSetAudioVolume(int volumeDbX10) {
            return Bridge.godot_icall_Test_SetAudioVolume(volumeDbX10);
        }
        public static int TestGetAudioVolume() {
            return Bridge.godot_icall_Test_GetAudioVolume();
        }
        public static int TestAddAnimation(string animName) {
            return Bridge.godot_icall_Test_AddAnimation(animName);
        }
        public static int TestPlayAnimation(string animName) {
            return Bridge.godot_icall_Test_PlayAnimation(animName);
        }
        public static int TestGetAnimationCount() {
            return Bridge.godot_icall_Test_GetAnimationCount();
        }
        public static int TestIsAnimationPlaying(string animName) {
            return Bridge.godot_icall_Test_IsAnimationPlaying(animName);
        }
        public static int TestBclListTest() {
            return Bridge.godot_icall_Test_BclListTest();
        }
        public static int TestBclDictTest() {
            return Bridge.godot_icall_Test_BclDictTest();
        }
        public static int TestBclAsyncTest() {
            return Bridge.godot_icall_Test_BclAsyncTest();
        }
        public static int TestGcStressTest(int count) {
            return Bridge.godot_icall_Test_GcStressTest(count);
        }
        public static void TestAssert(string name, int condition) {
            Bridge.godot_icall_Test_Assert(name, condition);
        }
        public static void TestFinishTest(string testName) {
            Bridge.godot_icall_Test_FinishTest(testName);
        }
        public static int TestGetPassCount() {
            return Bridge.godot_icall_Test_GetPassCount();
        }
        public static int TestGetFailCount() {
            return Bridge.godot_icall_Test_GetFailCount();
        }
        public static void TestResetCounters() {
            Bridge.godot_icall_Test_ResetCounters();
        }

        // ===== WeChat minigame audio adapter (InnerAudioContext-based) =====
        // Desktop platforms: stubs (return 0 / no-op).
        // WeChat minigame: routes to GameGlobal.GodotAudioWX via EM_ASM.
        // Returns audio id (>0) on success, 0 on failure or unsupported platform.
        public static int WxAudioPlay(string src, bool loop) {
            return Bridge.godot_icall_WXAudio_Play(src, loop ? 1 : 0);
        }
        public static void WxAudioStop(int id) {
            Bridge.godot_icall_WXAudio_Stop(id);
        }
        public static void WxAudioStopAll() {
            Bridge.godot_icall_WXAudio_StopAll();
        }
        // volume: 0.0~1.0 (will be scaled to int 0~100 in icall)
        public static void WxAudioSetVolume(int id, float volume) {
            int vol_x100 = (int)(volume * 100.0f);
            if (vol_x100 < 0) vol_x100 = 0;
            if (vol_x100 > 100) vol_x100 = 100;
            Bridge.godot_icall_WXAudio_SetVolume(id, vol_x100);
        }
        public static void WxAudioPause(int id) {
            Bridge.godot_icall_WXAudio_Pause(id);
        }
        public static void WxAudioResume(int id) {
            Bridge.godot_icall_WXAudio_Resume(id);
        }
    }
}
