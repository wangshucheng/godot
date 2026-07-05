using System;
using System.Threading;
using System.Threading.Tasks;

namespace Godot {
    [Preserve(AllMembers = true)]
    public static class Tasking {
        public static Task Run(Action action) {
            if (Platform.IsSingleThreaded) {
                var tcs = new TaskCompletionSource<bool>();
                var ctx = SynchronizationContext.Current as GodotSynchronizationContext;
                if (ctx != null) {
                    ctx.Post(_ => {
                        try {
                            action();
                            tcs.SetResult(true);
                        } catch (Exception e) {
                            tcs.SetException(e);
                        }
                    }, null);
                } else {
                    try {
                        action();
                        tcs.SetResult(true);
                    } catch (Exception e) {
                        tcs.SetException(e);
                    }
                }
                return tcs.Task;
            }
            return Task.Run(action);
        }

        public static Task<T> Run<T>(Func<T> func) {
            if (Platform.IsSingleThreaded) {
                var tcs = new TaskCompletionSource<T>();
                var ctx = SynchronizationContext.Current as GodotSynchronizationContext;
                if (ctx != null) {
                    ctx.Post(_ => {
                        try {
                            tcs.SetResult(func());
                        } catch (Exception e) {
                            tcs.SetException(e);
                        }
                    }, null);
                } else {
                    try {
                        tcs.SetResult(func());
                    } catch (Exception e) {
                        tcs.SetException(e);
                    }
                }
                return tcs.Task;
            }
            return Task.Run(func);
        }

        public static Task Delay(int millisecondsDelay) {
            if (Platform.IsSingleThreaded) {
                throw new PlatformNotSupportedException(
                    "Task.Delay is not supported in single-threaded WASM mode. " +
                    "Use await source.ToSignal(source, signal) for signal-based waiting instead.");
            }
            return Task.Delay(millisecondsDelay);
        }

        public static void StartNewThread(ThreadStart start) {
            Platform.ThrowIfNotSupported("System.Threading.Thread.Start");
            var t = new Thread(start);
            t.IsBackground = true;
            t.Start();
        }

        public static void StartNewThread(ParameterizedThreadStart start, object state) {
            Platform.ThrowIfNotSupported("System.Threading.Thread.Start");
            var t = new Thread(start);
            t.IsBackground = true;
            t.Start(state);
        }

        public static IDisposable StartTimer(TimeSpan interval, Action callback) {
            if (Platform.IsSingleThreaded) {
                return new SingleThreadedTimer(interval, callback);
            }
            return new ThreadPoolTimer(interval, callback);
        }

        private class SingleThreadedTimer : IDisposable {
            private readonly TimeSpan _interval;
            private readonly Action _callback;
            private DateTime _nextTick;
            private bool _disposed;

            public SingleThreadedTimer(TimeSpan interval, Action callback) {
                _interval = interval;
                _callback = callback;
                _nextTick = DateTime.UtcNow + interval;
            }

            public void Tick() {
                if (_disposed) return;
                var now = DateTime.UtcNow;
                if (now >= _nextTick) {
                    _callback();
                    _nextTick = now + _interval;
                }
            }

            public void Dispose() {
                _disposed = true;
            }
        }

        private class ThreadPoolTimer : IDisposable {
            private readonly Timer _timer;

            public ThreadPoolTimer(TimeSpan interval, Action callback) {
                _timer = new Timer(_ => callback(), null, interval, interval);
            }

            public void Dispose() {
                _timer.Dispose();
            }
        }
    }
}
