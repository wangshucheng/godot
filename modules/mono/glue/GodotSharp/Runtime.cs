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
            GodotSynchronizationContext.Install();
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
    }
}
