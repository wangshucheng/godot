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
    }
}
