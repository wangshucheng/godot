using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Threading;

namespace Godot
{
    // Awaiter for `await node.ToSignal("signal_name")`.
    // Connects to the source object's signal via C++ icall; when the signal fires,
    // C++ invokes SignalCallback which completes the awaiter and schedules the continuation
    // through GodotSynchronizationContext (main thread pump).
    public class SignalAwaiter : INotifyCompletion
    {
        private bool _completed;
        private object[] _result;
        private Action _continuation;

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_SignalAwaiter_Connect(
            long sourcePtr, string signal, long targetPtr, long awaiterHandle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void godot_icall_SignalAwaiter_Disconnect(
            long sourcePtr, string signal, long awaiterHandle);

        public SignalAwaiter(GodotObject source, string signal, GodotObject target)
        {
            if (source == null) throw new ArgumentNullException("source");
            if (signal == null) throw new ArgumentNullException("signal");
            // S3 修复(v2): 用强 GCHandle 让 C++ 连接期间保活 awaiter。
            // 保活链: gchandle → awaiter → _continuation → async 状态机。
            // handle 所有权归 C++ SignalAwaiterCallable（与连接同寿命）：
            // OneShot 信号触发后（或源对象销毁/断开时）Callable 析构释放 handle，
            // 解除根引用，awaiter 随后可被 GC 正常回收。
            // 原实现由 C# 终结器释放 handle —— 强句柄把自身钉为 GC 根，
            // 终结器永不运行，每次 ToSignal 泄漏一个 handle + 对象。
            // C# 侧不再持有也不再释放该 handle。
            GCHandle selfHandle = GCHandle.Alloc(this, GCHandleType.Normal);
            // GCHandle.ToIntPtr returns an IntPtr whose bits are the handle.
            // We pass as long (int64) to avoid IntPtr marshalling issues in WASM.
            long handleBits = GCHandle.ToIntPtr(selfHandle).ToInt64();
            godot_icall_SignalAwaiter_Connect(source.nativeInstance, signal,
                target != null ? target.nativeInstance : 0, handleBits);
        }

        public bool IsCompleted { get { return _completed; } }

        public void OnCompleted(Action continuation)
        {
            _continuation = continuation;
            // If already completed by the time OnCompleted is called, run immediately.
            if (_completed) _continuation?.Invoke();
        }

        public object[] GetResult()
        {
            return _result;
        }

        public SignalAwaiter GetAwaiter() { return this; }

        // Called from C++ via mono_runtime_invoke when the signal fires.
        // args is an object[] of signal arguments (may be empty).
        internal void SignalCallback(object[] args)
        {
            _completed = true;
            _result = args ?? new object[0];
            // Schedule continuation on main thread via sync context.
            // If sync context is not installed (WASM), invoke directly.
            var ctx = SynchronizationContext.Current;
            if (ctx != null)
            {
                ctx.Post(_ => _continuation?.Invoke(), null);
            }
            else
            {
                _continuation?.Invoke();
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
    }
}
