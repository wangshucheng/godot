using System;

namespace Godot {
    public enum OSPlatform {
        Unknown = 0,
        Windows = 1,
        Linux = 2,
        macOS = 3,
        Web = 4,
        Android = 5,
        iOS = 6,
    }

    [Flags]
    public enum RuntimeFlags {
        None = 0,
        AotMode = 1,
        WebPlatform = 2,
        SingleThreaded = 4,
        WindowsPlatform = 8,
        LinuxPlatform = 16,
        MacOSPlatform = 32,
    }

    public static class Platform {
        private static bool _initialized = false;
        private static RuntimeFlags _flags;
        private static OSPlatform _os;

        public static OSPlatform Current {
            get { EnsureInitialized(); return _os; }
        }

        public static bool IsWeb {
            get { EnsureInitialized(); return (_flags & RuntimeFlags.WebPlatform) != 0; }
        }

        public static bool IsAotMode {
            get { EnsureInitialized(); return (_flags & RuntimeFlags.AotMode) != 0; }
        }

        public static bool IsSingleThreaded {
            get { EnsureInitialized(); return (_flags & RuntimeFlags.SingleThreaded) != 0; }
        }

        public static RuntimeFlags Flags {
            get { EnsureInitialized(); return _flags; }
        }

        public static void Initialize() {
            if (_initialized) return;
            _initialized = true;

            try {
                _flags = (RuntimeFlags)Bridge.godot_icall_Platform_GetRuntimeInfo();
            } catch {
                _flags = RuntimeFlags.None;
            }

            // Install sync context only on non-Web platforms.
            // On Web (WASM interpreter), mono_runtime_invoke in pump_sync_context
            // triggers function signature mismatch. Web is single-threaded and
            // doesn't need cross-thread continuation pumping.
            if ((_flags & RuntimeFlags.WebPlatform) == 0) {
                GodotSynchronizationContext.Install();
            }

            if ((_flags & RuntimeFlags.WebPlatform) != 0) {
                _os = OSPlatform.Web;
            } else if ((_flags & RuntimeFlags.WindowsPlatform) != 0) {
                _os = OSPlatform.Windows;
            } else if ((_flags & RuntimeFlags.MacOSPlatform) != 0) {
                _os = OSPlatform.macOS;
            } else if ((_flags & RuntimeFlags.LinuxPlatform) != 0) {
                _os = OSPlatform.Linux;
            } else {
                _os = DetectPlatformFallback();
            }
        }

        private static OSPlatform DetectPlatformFallback() {
            PlatformID id = System.Environment.OSVersion.Platform;
            switch (id) {
                case PlatformID.Win32NT:
                case PlatformID.Win32S:
                case PlatformID.Win32Windows:
                case PlatformID.WinCE:
                    return OSPlatform.Windows;
                case PlatformID.MacOSX:
                    return OSPlatform.macOS;
                case PlatformID.Unix:
                    return OSPlatform.Linux;
                default:
                    return OSPlatform.Unknown;
            }
        }

        private static void EnsureInitialized() {
            if (!_initialized) Initialize();
        }

        public static void ThrowIfNotSupported(string feature) {
            EnsureInitialized();
            if (IsWeb || IsSingleThreaded) {
                throw new PlatformNotSupportedException(
                    feature + " is not supported in WASM Full AOT / single-threaded mode.");
            }
        }
    }
}
