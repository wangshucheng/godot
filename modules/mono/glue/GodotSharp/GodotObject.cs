using System;
using System.Collections;

namespace Godot {
    [Flags]
    public enum ConnectFlags {
        None = 0,
        Deferred = 1,
        Persist = 2,
        OneShot = 4,
        ReferenceCounted = 8
    }

    [Preserve(AllMembers = true)]
    public class Object : IDisposable {
        internal IntPtr NativePtr;
        internal uint _bridgeGCHandle;
        protected bool disposed = false;
        private bool _isNativeWrapper;
        // Use ArrayList instead of Dictionary<(string,Delegate),Callable> to avoid
        // complex generic type resolution issues in Mono WASM interpreter mode.
        // Lazy-initialized to avoid constructor-phase type loading issues in
        // Mono WASM interpreter (field initializers run before ctor body).
        private ArrayList _connectedCallables;

        // Lazily create the callables list on first use.
        private ArrayList Callables {
            get {
                if (_connectedCallables == null) {
                    _connectedCallables = new ArrayList();
                }
                return _connectedCallables;
            }
        }

        public Object() {
            _bridgeGCHandle = 0;
            _isNativeWrapper = false;
            if (NativePtr == IntPtr.Zero) {
                NativePtr = Bridge.godot_icall_Object_Ctor(this);
            }
        }

        internal Object(IntPtr nativePtr) : this(nativePtr, true) {
        }

        internal Object(IntPtr nativePtr, bool isWrapper) {
            NativePtr = nativePtr;
            _bridgeGCHandle = 0;
            _isNativeWrapper = isWrapper;
            if (isWrapper && nativePtr != IntPtr.Zero) {
                Bridge.godot_icall_Object_BindNativePtr(this, nativePtr);
            }
        }

        internal void AssignNativePtr(IntPtr nativePtr, uint gcHandle = 0) {
            NativePtr = nativePtr;
            _bridgeGCHandle = gcHandle;
        }

        // Public accessor for the native pointer. Needed by user code that
        // uses the WASM-safe Runtime.R2D* icalls (e.g. passing `this` as
        // IntPtr parent to Runtime.R2DAddChild). Without this, user classes
        // (which live outside the Godot namespace) cannot access the internal
        // NativePtr field.
        public IntPtr GetNativePtr() {
            return NativePtr;
        }

        public bool IsInstanceValid() {
            if (NativePtr == IntPtr.Zero) return false;
            return Bridge.godot_icall_Object_IsInstanceValid(NativePtr);
        }

        protected void ThrowIfDisposed() {
            if (NativePtr == IntPtr.Zero || disposed) {
                throw new ObjectDisposedException(GetType().FullName ?? "Godot.Object");
            }
        }

        public void Set(string property, object value) {
            ThrowIfDisposed();
            Bridge.godot_icall_Object_Set(NativePtr, property, value);
        }

        public object Get(string property) {
            ThrowIfDisposed();
            return Bridge.godot_icall_Object_Get(NativePtr, property);
        }

        public object Call(string method, params object[] args) {
            ThrowIfDisposed();
            return Bridge.godot_icall_Object_Call(NativePtr, method, args);
        }

        public void Connect(string signal, Action callback, ConnectFlags flags = ConnectFlags.None) {
            ConnectImpl(signal, callback, (int)flags);
        }

        public void Connect<T>(string signal, Action<T> callback, ConnectFlags flags = ConnectFlags.None) {
            ConnectImpl(signal, callback, (int)flags);
        }

        public void Connect(string signal, Delegate callback, ConnectFlags flags = ConnectFlags.None) {
            ConnectImpl(signal, callback, (int)flags);
        }

        private void ConnectImpl(string signal, Delegate callback, int flags) {
            ThrowIfDisposed();
            // Check if already connected
            for (int i = 0; i < Callables.Count; i++) {
                object[] entry = (object[])Callables[i];
                if ((string)entry[0] == signal && (Delegate)entry[1] == callback) return;
            }
            Callable callable = Callable.From(callback);
            bool ok = Bridge.godot_icall_Object_Connect(NativePtr, signal, callable.NativePtr, flags);
            if (ok) {
                Callables.Add(new object[] { signal, callback, callable });
            } else {
                callable.Dispose();
                throw new InvalidOperationException("Failed to connect signal '" + signal + "'.");
            }
        }

        public void Disconnect(string signal, Delegate callback) {
            ThrowIfDisposed();
            if (_connectedCallables == null) return;
            for (int i = 0; i < _connectedCallables.Count; i++) {
                object[] entry = (object[])_connectedCallables[i];
                if ((string)entry[0] == signal && (Delegate)entry[1] == callback) {
                    Callable callable = (Callable)entry[2];
                    Bridge.godot_icall_Object_Disconnect(NativePtr, signal, callable.NativePtr);
                    callable.Dispose();
                    _connectedCallables.RemoveAt(i);
                    return;
                }
            }
        }

        public bool IsConnected(string signal, Delegate callback) {
            ThrowIfDisposed();
            if (_connectedCallables == null) return false;
            for (int i = 0; i < _connectedCallables.Count; i++) {
                object[] entry = (object[])_connectedCallables[i];
                if ((string)entry[0] == signal && (Delegate)entry[1] == callback) {
                    Callable callable = (Callable)entry[2];
                    return Bridge.godot_icall_Object_IsConnected(NativePtr, signal, callable.NativePtr);
                }
            }
            return false;
        }

        public void EmitSignal(string signal, params object[] args) {
            ThrowIfDisposed();
            Bridge.godot_icall_Object_EmitSignal(NativePtr, signal, args);
        }

        public bool HasSignal(string signal) {
            ThrowIfDisposed();
            return Bridge.godot_icall_Object_HasSignal(NativePtr, signal);
        }

        public SignalAwaiter ToSignal(Object source, string signal) {
            return new SignalAwaiter(source, signal);
        }

        public virtual void QueueFree() {
            if (NativePtr != IntPtr.Zero && !disposed) {
                Call("queue_free");
            }
        }

        public virtual void Free() {
            if (disposed) return;

            // Collect callables first to avoid modifying the collection during iteration
            int count = _connectedCallables != null ? _connectedCallables.Count : 0;
            Callable[] toDispose = new Callable[count];
            for (int i = 0; i < count; i++) {
                toDispose[i] = (Callable)((object[])_connectedCallables[i])[2];
            }
            if (_connectedCallables != null) _connectedCallables.Clear();

            if (NativePtr != IntPtr.Zero) {
                Bridge.godot_icall_Object_Free(this, NativePtr);
                NativePtr = IntPtr.Zero;
                _bridgeGCHandle = 0;
            }

            // Dispose callables after iteration completes
            for (int i = 0; i < count; i++) {
                toDispose[i].Dispose();
            }

            disposed = true;
        }

        public virtual void Dispose() {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing) {
            if (disposed) return;

            if (disposing) {
                // Explicit Dispose() → Free() → release native object on the
                // caller's thread (must be the main thread).
                Free();
            } else {
                // H8: Finalizer (~GodotObject) runs on the Mono GC thread.
                // It must NOT call godot_icall_Object_Free or any other icall
                // — engine objects are not safe to touch off the main thread,
                // and a temporary wrapper (e.g. GetNode<T>() returns a new
                // wrapper each call) being finalized must not release the
                // underlying native node it only borrows. Match the
                // GodotRefCounted.cs rule: "finalizer path must not call icall".
                //
                // We only clear the managed-side fields so the managed object
                // is inert. The native object's lifetime is owned elsewhere
                // (RefCounted refcount, or the scene tree for Nodes, or an
                // explicit Dispose() that the user forgot — the last case
                // leaks, which is safer than a cross-thread UAF).
                NativePtr = IntPtr.Zero;
                _bridgeGCHandle = 0;
                disposed = true;
            }
        }

        ~Object() {
            Dispose(false);
        }
    }
}
