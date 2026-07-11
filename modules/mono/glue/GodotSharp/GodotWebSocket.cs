using System;
using System.Runtime.CompilerServices;

namespace Godot {
    [Preserve(AllMembers = true)]
    public class WebSocketPeer : Object {
        public const int StateConnecting = 0;
        public const int StateOpen = 1;
        public const int StateClosing = 2;
        public const int StateClosed = 3;

        public WebSocketPeer() : base() {}

        public int ConnectToUrl(string url) {
            return Runtime.WsInit(url);
        }
        public void Poll() {
            Runtime.WsPoll();
        }
        public int GetReadyState() {
            return Runtime.WsGetState();
        }
        public int SendText(string text) {
            return Runtime.WsSendText(text);
        }
        public int GetAvailablePacketCount() {
            return Runtime.WsGetPacketCount();
        }
        public void Close() {
            Runtime.WsClose();
        }
    }
}
