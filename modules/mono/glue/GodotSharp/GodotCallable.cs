using System;
using System.Runtime.CompilerServices;

namespace Godot {
    [Preserve(AllMembers = true)]
    public class Callable : IDisposable {
        internal IntPtr NativePtr;
        private bool disposed = false;

        private Callable(IntPtr nativePtr) {
            NativePtr = nativePtr;
        }

        // 2026-07-27 rework: pass the ORIGINAL delegate to the native side.
        // Previously every delegate was wrapped in Action<object[]> while the
        // native CallableCustomMono invoked it with per-argument parameters —
        // a signature shape mismatch that made managed signal callbacks throw
        // or never fire at all (csharp_test scenario 24e; Fuzz10's callback
        // never actually ran). The native call path (mono_callable.cpp) now
        // boxes each argument with the delegate's real Invoke signature, so
        // no managed wrapper is needed. Exceptions raised inside a callback
        // surface through CallableCustomMono::call's exc handler and are
        // logged there ("[Mono] Exception in C# delegate call").
        public static Callable From(Action action) {
            IntPtr ptr = Bridge.godot_icall_Callable_CreateFromDelegate(action);
            if (ptr == IntPtr.Zero) throw new InvalidOperationException("Failed to create Callable.");
            return new Callable(ptr);
        }

        public static Callable From<T>(Action<T> action) {
            IntPtr ptr = Bridge.godot_icall_Callable_CreateFromDelegate(action);
            if (ptr == IntPtr.Zero) throw new InvalidOperationException("Failed to create Callable.");
            return new Callable(ptr);
        }

        public static Callable From(Delegate del) {
            IntPtr ptr = Bridge.godot_icall_Callable_CreateFromDelegate(del);
            if (ptr == IntPtr.Zero) throw new InvalidOperationException("Failed to create Callable.");
            return new Callable(ptr);
        }

        public object Call(params object[] args) {
            ThrowIfDisposed();
            return Bridge.godot_icall_Callable_Call(NativePtr, args);
        }

        internal void ThrowIfDisposed() {
            if (NativePtr == IntPtr.Zero || disposed) {
                throw new ObjectDisposedException("Callable");
            }
        }

        public void Dispose() {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing) {
            if (disposed) return;

            if (disposing) {
                if (NativePtr != IntPtr.Zero) {
                    Bridge.godot_icall_Callable_Free(NativePtr);
                    NativePtr = IntPtr.Zero;
                }
            }
            // When disposing==false (finalizer), do not call icall -
            // it may be unsafe during runtime shutdown.

            disposed = true;
        }

        ~Callable() {
            Dispose(false);
        }
    }
}
