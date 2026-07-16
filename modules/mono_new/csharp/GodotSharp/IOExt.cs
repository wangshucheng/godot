using System;
using System.Runtime.CompilerServices;
using System.Text;

namespace Godot
{
    // FileAccess static helpers - wraps Godot's FileAccess C++ API via icalls.
    public static class FileAccess
    {
        public enum ModeFlags : int
        {
            Read = 1,
            Write = 2,
            ReadWrite = 3,
            WriteRead = 7,
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_FileAccess_FileExists(string path);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string godot_icall_FileAccess_GetFileAsString(string path);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static byte[] godot_icall_FileAccess_GetFileAsBytes(string path);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int godot_icall_FileAccess_WriteFile(string path, byte[] data);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int godot_icall_FileAccess_MakeDirRecursive(string path);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool godot_icall_FileAccess_DirExists(string path);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int godot_icall_FileAccess_Remove(string path);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string godot_icall_FileAccess_GetUserDataDir();

        public static bool FileExists(string path) => godot_icall_FileAccess_FileExists(path);
        public static string GetFileAsString(string path) => godot_icall_FileAccess_GetFileAsString(path);
        public static byte[] GetFileAsBytes(string path) => godot_icall_FileAccess_GetFileAsBytes(path);

        public static Error WriteFile(string path, byte[] data)
        {
            return (Error)godot_icall_FileAccess_WriteFile(path, data);
        }

        public static Error WriteString(string path, string data)
        {
            byte[] bytes = Encoding.UTF8.GetBytes(data);
            return (Error)godot_icall_FileAccess_WriteFile(path, bytes);
        }

        public static Error MakeDirRecursive(string path)
        {
            return (Error)godot_icall_FileAccess_MakeDirRecursive(path);
        }

        public static bool DirExists(string path) => godot_icall_FileAccess_DirExists(path);

        public static Error Remove(string path)
        {
            return (Error)godot_icall_FileAccess_Remove(path);
        }

        public static string GetUserDataDir() => godot_icall_FileAccess_GetUserDataDir();
    }
}
