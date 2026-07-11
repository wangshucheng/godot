using System;
using System.Collections.Generic;
using System.Threading;

namespace Godot {
    [Preserve(AllMembers = true)]
    public class GodotSynchronizationContext : SynchronizationContext {
        private static GodotSynchronizationContext _instance;
        private readonly Queue<Action> _pending = new Queue<Action>();
        private readonly object _lock = new object();

        public override void Post(SendOrPostCallback d, object state) {
            lock (_lock) {
                _pending.Enqueue(() => d(state));
            }
        }

        public override void Send(SendOrPostCallback d, object state) {
            d(state);
        }

        public override SynchronizationContext CreateCopy() {
            return this;
        }

        public void ProcessPending() {
            Action[] actions;
            lock (_lock) {
                if (_pending.Count == 0) return;
                actions = _pending.ToArray();
                _pending.Clear();
            }
            foreach (var action in actions) {
                try {
                    action();
                } catch (Exception e) {
                    GD.PrintErr("GodotSynchronizationContext: Unhandled exception: " + e.Message);
                }
            }
        }

        public int PendingCount {
            get { lock (_lock) { return _pending.Count; } }
        }

        public static GodotSynchronizationContext Instance => _instance;

        public static void Install() {
            if (_instance == null) {
                _instance = new GodotSynchronizationContext();
            }
            SynchronizationContext.SetSynchronizationContext(_instance);
        }

        public static void Pump() {
            _instance?.ProcessPending();
        }

        // Instance-method wrapper for Pump(). Used by C++ to avoid calling
        // static methods via mono_runtime_invoke, which triggers signature
        // mismatch in the WASM interpreter. C++ reads the static _instance
        // field (a safe memory read) then invokes this instance method.
        public void PumpInstance() {
            ProcessPending();
        }
    }
}
