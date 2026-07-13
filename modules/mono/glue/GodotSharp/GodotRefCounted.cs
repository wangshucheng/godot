using System;

namespace Godot {
    [Preserve(AllMembers = true)]
    public class RefCounted : Object {
        public RefCounted() : base() {}
        protected internal RefCounted(IntPtr nativePtr) : base(nativePtr) {}

        // RefCounted uses different Dispose semantics than Object:
        // - Object.Free() deletes the native object
        // - RefCounted.Dispose() releases the C# held reference (unreference)
        //   If refcount reaches 0, the native object is deleted automatically.
        //
        // Strong GCHandle prevents C# wrapper from being GC'd until Dispose is called.
        // Users MUST call Dispose explicitly (same discipline as C++ Ref<T>).

        public override void Dispose() {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected override void Dispose(bool disposing) {
            if (!disposed) {
                // Only release the reference on explicit Dispose.
                // The finalizer path (disposing=false) must NOT call icalls -
                // Mono WASM interpreter is unsafe during GC.
                // Strong GCHandle keeps the wrapper alive until Dispose() is called,
                // so the finalizer is effectively dead code unless C++ releases
                // the handle during shutdown (in which case we skip release anyway).
                if (disposing && NativePtr != IntPtr.Zero) {
                    Bridge.godot_icall_RefCounted_ReleaseRef(NativePtr);
                    NativePtr = IntPtr.Zero;
                    _bridgeGCHandle = 0;
                }
                disposed = true;
            }
        }

        // Free() is not used for RefCounted - use Dispose() instead
        public override void Free() {
            Dispose();
        }
    }
}
