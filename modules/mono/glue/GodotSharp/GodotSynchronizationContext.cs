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
    }
}
