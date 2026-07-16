using System;
using System.Runtime.CompilerServices;
using System.Text;

namespace Godot
{
    public class WebSocketPeer : GodotObject
    {
        public enum State
        {
            Connecting = 0,
            Open = 1,
            Closing = 2,
            Closed = 3
        }

        public WebSocketPeer()
        {
            nativeInstance = godot_icall_CreateObject("WebSocketPeer");
            ownsNative = true;
        }

        public Error ConnectToUrl(string url)
        {
            long ret = godot_icall_Object_CallStringReturnsInt(nativeInstance, "connect_to_url", url);
            return (Error)ret;
        }

        public void Poll()
        {
            godot_icall_Object_CallNoArgs(nativeInstance, "poll");
        }

        public State GetReadyState()
        {
            long ret = godot_icall_Object_CallNoArgsInt(nativeInstance, "get_ready_state");
            return (State)ret;
        }

        public int GetAvailablePacketCount()
        {
            long ret = godot_icall_Object_CallNoArgsInt(nativeInstance, "get_available_packet_count");
            return (int)ret;
        }

        public byte[] GetPacket()
        {
            return godot_icall_PacketPeer_GetPacket(nativeInstance);
        }

        public string GetPacketString()
        {
            byte[] data = GetPacket();
            if (data == null || data.Length == 0) return "";
            return Encoding.UTF8.GetString(data);
        }

        public bool WasStringPacket()
        {
            return godot_icall_Object_CallNoArgsBool(nativeInstance, "was_string_packet");
        }

        public void SendText(string text)
        {
            godot_icall_Object_CallString(nativeInstance, "send_text", text);
        }

        public void Close(int code = 1000, string reason = "")
        {
            godot_icall_Object_CallStringInt(nativeInstance, "close", reason, (long)code);
        }

        public string GetSelectedProtocol()
        {
            return godot_icall_Object_GetString(nativeInstance, "selected_protocol");
        }

        public string GetRequestedUrl()
        {
            return godot_icall_Object_GetString(nativeInstance, "requested_url");
        }

        public int GetCloseCode()
        {
            return (int)godot_icall_Object_CallNoArgsInt(nativeInstance, "get_close_code");
        }

        public string GetCloseReason()
        {
            return godot_icall_Object_GetString(nativeInstance, "close_reason");
        }
    }

    public enum Error
    {
        OK = 0,
        FAILED = 1,
        ERR_UNAVAILABLE = 2,
        ERR_UNCONFIGURED = 3,
        ERR_UNAUTHORIZED = 4,
        ERR_PARAMETER_RANGE_ERROR = 5,
        ERR_OUT_OF_MEMORY = 6,
        ERR_FILE_NOT_FOUND = 7,
        ERR_FILE_BAD_DRIVE = 8,
        ERR_FILE_BAD_PATH = 9,
        ERR_FILE_NO_PERMISSION = 10,
        ERR_FILE_ALREADY_IN_USE = 11,
        ERR_FILE_CANT_OPEN = 12,
        ERR_FILE_CANT_WRITE = 13,
        ERR_FILE_CANT_READ = 14,
        ERR_FILE_UNRECOGNIZED = 15,
        ERR_FILE_CORRUPT = 16,
        ERR_FILE_MISSING_DEPENDENCIES = 17,
        ERR_FILE_EOF = 18,
        ERR_CANT_OPEN = 19,
        ERR_CANT_CREATE = 20,
        ERR_QUERY_FAILED = 21,
        ERR_ALREADY_IN_USE = 22,
        ERR_LOCKED = 23,
        ERR_TIMEOUT = 24,
        ERR_CANT_CONNECT = 25,
        ERR_CANT_RESOLVE = 26,
        ERR_CONNECTION_ERROR = 27,
        ERR_CANT_ACQUIRE_RESOURCE = 28,
        ERR_CANT_FORK = 29,
        ERR_INVALID_DATA = 30,
        ERR_INVALID_PARAMETER = 31,
        ERR_ALREADY_EXISTS = 32,
        ERR_DOES_NOT_EXIST = 33,
        ERR_DATABASE_CANT_READ = 34,
        ERR_DATABASE_CANT_WRITE = 35,
        ERR_COMPILATION_FAILED = 36,
        ERR_METHOD_NOT_FOUND = 37,
        ERR_LINK_FAILED = 38,
        ERR_SCRIPT_FAILED = 39,
        ERR_CYCLIC_LINK = 40,
        ERR_INVALID_DECLARATION = 41,
        ERR_DUPLICATE_SYMBOL = 42,
        ERR_PARSE_ERROR = 43,
        ERR_BUSY = 44,
        ERR_SKIP = 45,
        ERR_HELP = 46,
        ERR_BUG = 47,
        ERR_PRINTER_ON_FIRE = 48
    }
}