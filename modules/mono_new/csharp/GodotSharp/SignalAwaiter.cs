using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Threading;

namespace Godot
{
    // Awaiter for `await node.ToSignal("signal_name")`.
    // 连接源对象信号；信号触发时由 C++ 通过 mono_runtime_invoke 调用 SignalCallback，
    // 完成 awaiter 并通过 GodotSynchronizationContext 在主线程调度 continuation。
    //
    // 实现 ICriticalNotifyCompletion（H7 扩展）:
    //   - UnsafeOnCompleted 不捕获 ExecutionContext，性能比 OnCompleted 更高，
    //     编译器生成的 async 状态机会优先使用此方法。
    //   - OnCompleted 仍保留以兼容手动 await 模式。
    //
    // 取消支持（H7 扩展）:
    //   - Cancel() 允许在信号触发前取消 await，会主动断开 OneShot 连接
    //     （通过 icall_SignalAwaiter_Disconnect），释放 C++ 侧的 gchandle。
    //   - 取消后 continuation 不会被调用（与 Task 取消语义一致）。
    public class SignalAwaiter : ICriticalNotifyCompletion
    {
        private volatile bool _completed;
        private volatile bool _canceled;
        private object[] _result;
        private Action _continuation;

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_SignalAwaiter_Connect(
            long sourcePtr, string signal, long targetPtr, long awaiterHandle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_SignalAwaiter_Disconnect(
            long sourcePtr, string signal, long awaiterHandle);

        // source/signal 保留以便 Cancel() 调用 Disconnect
        private readonly GodotObject _source;
        private readonly string _signal;

        public SignalAwaiter(GodotObject source, string signal, GodotObject target)
        {
            if (source == null) throw new ArgumentNullException("source");
            if (signal == null) throw new ArgumentNullException("signal");
            _source = source;
            _signal = signal;
            // S3 修复(v2): 用强 GCHandle 让 C++ 连接期间保活 awaiter。
            // 保活链: gchandle → awaiter → _continuation → async 状态机。
            // handle 所有权归 C++ SignalAwaiterCallable（与连接同寿命）：
            // OneShot 信号触发后（或源对象销毁/断开时）Callable 析构释放 handle，
            // 解除根引用，awaiter 随后可被 GC 正常回收。
            GCHandle selfHandle = GCHandle.Alloc(this, GCHandleType.Normal);
            long handleBits = GCHandle.ToIntPtr(selfHandle).ToInt64();
            godot_icall_SignalAwaiter_Connect(source.nativeInstance, signal,
                target != null ? target.nativeInstance : 0, handleBits);
        }

        public bool IsCompleted { get { return _completed; } }

        /// <summary>
        /// 取消 await。在信号触发前调用，断开 OneShot 连接。
        /// 取消后 continuation 不会被调用；后续信号触发也不会再回调。
        /// 已完成或已取消的 awaiter 调用此方法是 no-op。
        /// </summary>
        public void Cancel()
        {
            if (_completed || _canceled) return;
            _canceled = true;
            // C++ 侧 SignalAwaiterCallable 的 OneShot 连接会在信号触发时自动断开；
            // 这里主动调用 Disconnect 处理"信号未触发但需取消"的场景。
            if (_source != null && _source.nativeInstance != 0)
            {
                try
                {
                    godot_icall_SignalAwaiter_Disconnect(_source.nativeInstance, _signal, 0);
                }
                catch
                {
                    // 源对象已销毁或连接已断开：忽略
                }
            }
        }

        // INotifyCompletion: 兼容手动 await 模式，捕获 ExecutionContext
        public void OnCompleted(Action continuation)
        {
            // 用 Interlocked 防止 SignalCallback 与 OnCompleted 竞态下丢失 continuation
            _continuation = continuation;
            if (_completed)
            {
                _continuation?.Invoke();
            }
        }

        // ICriticalNotifyCompletion: 不捕获 ExecutionContext，性能更高
        // 编译器生成的 async 状态机优先使用此方法
        public void UnsafeOnCompleted(Action continuation)
        {
            _continuation = continuation;
            if (_completed)
            {
                _continuation?.Invoke();
            }
        }

        public object[] GetResult()
        {
            // 如果 awaiter 被取消，GetResult 返回空数组（与 Task 取消抛 OperationCanceledException 不同；
            // 信号 awaiter 没有统一的异常约定，这里选择静默返回空，让调用方通过 IsCompleted 判断）
            return _canceled ? new object[0] : _result;
        }

        public SignalAwaiter GetAwaiter() { return this; }

        // Called from C++ via mono_runtime_invoke when the signal fires.
        // args is an object[] of signal arguments (may be empty).
        internal void SignalCallback(object[] args)
        {
            if (_canceled) return; // 取消后忽略信号
            _completed = true;
            _result = args ?? new object[0];
            Action cont = _continuation;
            if (cont == null) return;
            // Schedule continuation on main thread via sync context.
            // If sync context is not installed (WASM), invoke directly.
            var ctx = SynchronizationContext.Current;
            if (ctx != null)
            {
                ctx.Post(_ => cont(), null);
            }
            else
            {
                cont();
            }
        }
    }

    // Extension methods for `await node.ToSignal(...)`.
    public static class SignalAwaiterExtensions
    {
        // Await a signal on a GodotObject.
        public static SignalAwaiter ToSignal(this GodotObject source, string signal)
        {
            return new SignalAwaiter(source, signal, null);
        }

        // Await a signal with a target (for scoped connections).
        public static SignalAwaiter ToSignal(this GodotObject source, StringName signal, GodotObject target)
        {
            return new SignalAwaiter(source, signal != null ? signal.ToString() : null, target);
        }

        // H7 扩展: 类型安全的 ToSignal 重载，await 后直接取第一个参数为目标类型。
        // 用法: int x = await node.ToSignal<int>("value_changed");
        // 仅适用于单参数信号；多参数信号仍用 object[] 版本手动取值。
        public static SignalAwaiter<T> ToSignal<T>(this GodotObject source, string signal)
        {
            return new SignalAwaiter<T>(source, signal, null);
        }
    }

    // H7 扩展: 泛型 SignalAwaiter，await 返回强类型 T（信号第一个参数）。
    // 仅对单参数信号有意义；多参数信号第一个参数仍可获取，其余需用 object[] 版本。
    public class SignalAwaiter<T> : ICriticalNotifyCompletion
    {
        private readonly SignalAwaiter _inner;

        public SignalAwaiter(GodotObject source, string signal, GodotObject target)
        {
            _inner = new SignalAwaiter(source, signal, target);
        }

        public bool IsCompleted { get { return _inner.IsCompleted; } }

        public void OnCompleted(Action continuation) { _inner.OnCompleted(continuation); }
        public void UnsafeOnCompleted(Action continuation) { _inner.UnsafeOnCompleted(continuation); }

        public T GetResult()
        {
            object[] args = _inner.GetResult();
            if (args == null || args.Length == 0) return default(T);
            try
            {
                return (T)args[0];
            }
            catch
            {
                return default(T);
            }
        }

        public SignalAwaiter<T> GetAwaiter() { return this; }

        public void Cancel() { _inner.Cancel(); }
    }
}
