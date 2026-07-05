using System;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;

namespace Godot {
    [Preserve(AllMembers = true)]
    public class SignalAwaiter : INotifyCompletion {
        private readonly Object _source;
        private readonly string _signal;
        private Action _continuation;
        private bool _isCompleted = false;
        private object[] _result;
        private Callable _callable;
        private Exception _exception;

        public SignalAwaiter(Object source, string signal) {
            _source = source;
            _signal = signal;
            Action<object[]> callback = OnSignalCallback;
            _callable = Callable.From(callback);
            bool ok = Bridge.godot_icall_Object_Connect(source.NativePtr, signal, _callable.NativePtr, (int)ConnectFlags.OneShot);
            if (!ok) {
                _callable.Dispose();
                _callable = null;
                _exception = new InvalidOperationException("Failed to connect to signal '" + signal + "'");
                _isCompleted = true;
            }
        }

        private void OnSignalCallback(object[] args) {
            _isCompleted = true;
            _result = args ?? new object[0];
            if (_callable != null) {
                _callable.Dispose();
                _callable = null;
            }
            if (_continuation != null) {
                var cont = _continuation;
                _continuation = null;
                var ctx = SynchronizationContext.Current;
                if (ctx != null && ctx is GodotSynchronizationContext) {
                    ctx.Post(_ => cont(), null);
                } else {
                    cont();
                }
            }
        }

        public bool IsCompleted => _isCompleted;

        public void OnCompleted(Action continuation) {
            if (_isCompleted) {
                continuation();
            } else {
                _continuation = continuation;
            }
        }

        public SignalAwaiter GetAwaiter() => this;

        public object[] GetResult() {
            if (_exception != null) throw _exception;
            return _result;
        }
    }
}
