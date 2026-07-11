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

            // NOTE: Avoid string concatenation with enum/bool values in WASM interpreter mode.
            // Boxing + Enum.ToString() triggers a virtual call that causes
            // "RuntimeError: function signature mismatch" in the WASM function table.
            GD.Print("[GodotSharp] Runtime initialized.");
        }

        public static bool IsInitialized => _initialized;
    }
}
