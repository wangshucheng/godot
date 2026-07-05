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

            GD.Print("[GodotSharp] Runtime initialized.");
            GD.Print("[GodotSharp] Platform: " + Platform.Current);
            GD.Print("[GodotSharp] AOT: " + Platform.IsAotMode + ", Web: " + Platform.IsWeb + ", SingleThreaded: " + Platform.IsSingleThreaded);
            GD.Print("[GodotSharp] SyncContext: " + (SynchronizationContext.Current?.GetType().Name ?? "(null)"));
        }

        public static bool IsInitialized => _initialized;
    }
}
