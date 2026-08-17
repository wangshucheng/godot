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

        // Get user data dir via C++ (for hot update staging area)
        public static string GetUserDataDir() {
            return Bridge.godot_icall_GetUserDataDir();
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
        // R2D Tile/Score/Status: 彻底消除 C# BCL 操作。
        //
        // 解决问题（Mono WASM 解释器 function signature mismatch）：
        //   1. "Score: " + _score → R2DSetScore (C++ snprintf)
        //   2. value.ToString()  → R2DLabelSetInt (C++ snprintf)
        //   3. tileTexts[] + TileColorRgb → R2DSetTileValue (C++ 查表)
        //   4. 状态字符串拼接 → R2DSetStatusText (C++ 预定义文本)
        // =============================================

        // 一个 icall 完成 2048 瓦片渲染：设置文字+背景色+字体色+字号。
        // bg = ColorRect IntPtr, label = Label IntPtr, value = 瓦片值 (0=空)。
        // 替代 C# 侧 tileTexts[] 查表 + TileColorRgb() + int.ToString()。
        public static void R2DSetTileValue(IntPtr bg, IntPtr label, int value) {
            Bridge.godot_icall_R2D_SetTileValue(bg, label, value);
        }

        // 设置 "Score: N" 文本。替代 "Score: " + score 字符串拼接。
        public static void R2DSetScore(IntPtr label, int score) {
            Bridge.godot_icall_R2D_SetScore(label, score);
        }

        // 设置 Label 文本为纯整数。替代 value.ToString()。
        public static void R2DLabelSetInt(IntPtr label, int value) {
            Bridge.godot_icall_R2D_LabelSetInt(label, value);
        }

        // 设置状态文本（预定义）。
        // state: 0=默认提示, 1=胜利, 2=失败, 3=新游戏
        public static void R2DSetStatusText(IntPtr label, int state) {
            Bridge.godot_icall_R2D_SetStatusText(label, state);
        }

        // =============================================
        // R2D Grid: C++ 侧全局 int 数组，替代 C# new int[]。
        //
        // 解决问题：Mono WASM 解释器在方法内 new int[]{...}
        // 触发 function signature mismatch。
        // 将数组分配/操作全部移到 C++，C# 用 idx = row*dim+col 索引。
        // =============================================

        // 创建/重置 NxN 网格（全部置 0）。
        public static void R2DGridCreate(int dim) {
            Bridge.godot_icall_R2D_GridCreate(dim);
        }

        // 设置网格单元。idx = row * dim + col。
        public static void R2DGridSet(int idx, int val) {
            Bridge.godot_icall_R2D_GridSet(idx, val);
        }

        // 获取网格单元。
        public static int R2DGridGet(int idx) {
            return Bridge.godot_icall_R2D_GridGet(idx);
        }

        // 全部填充为指定值。
        public static void R2DGridFill(int val) {
            Bridge.godot_icall_R2D_GridFill(val);
        }

        // 保存网格到备份缓冲区（undo 用）。
        public static void R2DGridSave() {
            Bridge.godot_icall_R2D_GridSave();
        }

        // 从备份恢复网格。
        public static void R2DGridRestore() {
            Bridge.godot_icall_R2D_GridRestore();
        }

        // 保存分数到备份（undo 用）。
        public static void R2DGridSaveScore(int score) {
            Bridge.godot_icall_R2D_GridSaveScore(score);
        }

        // 获取备份分数。
        public static int R2DGridGetSavedScore() {
            return Bridge.godot_icall_R2D_GridGetSavedScore();
        }

        // 检查是否有相邻相等元素（还能移动）。返回 1=是, 0=否。
        public static int R2DGridHasAdjacentEqual() {
            return Bridge.godot_icall_R2D_GridHasAdjacentEqual();
        }

        // 检查是否有空单元。返回 1=有, 0=无。
        public static int R2DGridHasZero() {
            return Bridge.godot_icall_R2D_GridHasZero();
        }

        // 获取空单元数量。
        public static int R2DGridCountZero() {
            return Bridge.godot_icall_R2D_GridCountZero();
        }

        // 获取第一个空单元 idx（无空返回 -1）。
        public static int R2DGridFirstZero() {
            return Bridge.godot_icall_R2D_GridFirstZero();
        }

        // 获取随机空单元 idx（无空返回 -1）。C++ LCG 随机。
        public static int R2DGridRandomZero() {
            return Bridge.godot_icall_R2D_GridRandomZero();
        }

        // 执行一行/列压缩+合并（2048 核心逻辑）。
        // lineIdx: 行/列索引, direction: 0=左/上, 1=右/下, isRow: 1=行, 0=列。
        // 返回合并产生的分数增量。
        public static int R2DGridSlideLine(int lineIdx, int direction, int isRow) {
            return Bridge.godot_icall_R2D_GridSlideLine(lineIdx, direction, isRow);
        }

        // 检查网格是否发生变化（与备份比较）。返回 1=有变化, 0=无。
        public static int R2DGridChanged() {
            return Bridge.godot_icall_R2D_GridChanged();
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
        // Round-trip counterpart of TestSetStringProp: actually READS the
        // string property back (previously string props were write-only in
        // the test API, so "set" assertions were unverifiable).
        public static string TestGetStringProp(string prop) {
            return Bridge.godot_icall_Test_GetStringProp(prop);
        }
        // Move the test context to the child at idx, so child properties can
        // be asserted on the actual child (not silently on the parent).
        public static int TestSelectChild(int idx) {
            return Bridge.godot_icall_Test_SelectChild(idx);
        }
        public static int TestSelectParent() {
            return Bridge.godot_icall_Test_SelectParent();
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

        // ===== Phase 0.1: Delegate probe helpers =====
        // Register a delegate (Action) for cross-language invocation tests.
        public static int TestRegisterDelegateProbe(System.Action del) {
            return Bridge.godot_icall_Test_RegisterDelegateProbe(del);
        }
        // Invoke the registered delegate via C++ mono_runtime_invoke(delegate.Invoke).
        public static int TestInvokeDelegateViaMRI() {
            return Bridge.godot_icall_Test_InvokeDelegateViaMRI();
        }
        // Invoke the registered delegate via C++ mono_compile_method + function pointer.
        public static int TestInvokeDelegateViaFtnPtr() {
            return Bridge.godot_icall_Test_InvokeDelegateViaFtnPtr();
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
