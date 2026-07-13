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
            if (millisecondsDelay <= 0) return Task.CompletedTask;

            if (Platform.IsSingleThreaded) {
                // Attempt BCL Task.Delay first. On Mono WASM, this typically
                // works via the runtime's browser timer bridge.
                try {
                    return Task.Delay(millisecondsDelay);
                } catch (Exception) {
                    // BCL timer unavailable; fall through to frame-driven approach.
                }

                // Frame-driven delay via GodotSynchronizationContext. The check
                // re-posts each frame until the target time is reached.
                var ctx = SynchronizationContext.Current as GodotSynchronizationContext
                          ?? GodotSynchronizationContext.Instance;
                if (ctx != null) {
                    var tcs = new TaskCompletionSource<bool>();
                    var target = DateTime.UtcNow.AddMilliseconds(millisecondsDelay);
                    PostDelayCheck(ctx, tcs, target);
                    return tcs.Task;
                }

                // No frame-driven mechanism available. Throw via Task.FromException
                // so the caller can observe the failure rather than awaiting a task
                // that never completes (which would hang the application silently).
                return Task.FromException(
                    new NotSupportedException(
                        "Task.Delay is not supported in single-threaded mode without GodotSynchronizationContext."));
            }
            return Task.Delay(millisecondsDelay);
        }

        private static void PostDelayCheck(GodotSynchronizationContext ctx,
                TaskCompletionSource<bool> tcs, DateTime target) {
            ctx.Post(_ => {
                if (tcs.Task.IsCompleted) return;
                if (DateTime.UtcNow >= target) {
                    tcs.TrySetResult(true);
                } else {
                    PostDelayCheck(ctx, tcs, target);
                }
            }, null);
        }

        public static void StartNewThread(ThreadStart start) {
            Platform.ThrowIfNotSupported("System.Threading.Thread.Start");
            var t = new System.Threading.Thread(start);
            t.IsBackground = true;
            t.Start();
        }

        public static void StartNewThread(ParameterizedThreadStart start, object state) {
            Platform.ThrowIfNotSupported("System.Threading.Thread.Start");
            var t = new System.Threading.Thread(start);
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
            private readonly System.Threading.Timer _timer;

            public ThreadPoolTimer(TimeSpan interval, Action callback) {
                _timer = new System.Threading.Timer(_ => callback(), null, interval, interval);
            }

            public void Dispose() {
                _timer.Dispose();
            }
        }
    }
}
