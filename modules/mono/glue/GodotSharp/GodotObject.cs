using System;
using System.Collections.Generic;

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
        private bool disposed = false;
        private bool _isNativeWrapper;
        private readonly Dictionary<(string signal, Delegate callback), Callable> _connectedCallables = new Dictionary<(string, Delegate), Callable>();

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
            var key = (signal, callback);
            if (_connectedCallables.ContainsKey(key)) return;
            Callable callable = Callable.From(callback);
            bool ok = Bridge.godot_icall_Object_Connect(NativePtr, signal, callable.NativePtr, flags);
            if (ok) {
                _connectedCallables[key] = callable;
            } else {
                callable.Dispose();
                throw new InvalidOperationException($"Failed to connect signal '{signal}'.");
            }
        }

        public void Disconnect(string signal, Delegate callback) {
            ThrowIfDisposed();
            var key = (signal, callback);
            if (_connectedCallables.TryGetValue(key, out Callable callable)) {
                Bridge.godot_icall_Object_Disconnect(NativePtr, signal, callable.NativePtr);
                callable.Dispose();
                _connectedCallables.Remove(key);
            }
        }

        public bool IsConnected(string signal, Delegate callback) {
            ThrowIfDisposed();
            var key = (signal, callback);
            if (_connectedCallables.TryGetValue(key, out Callable callable)) {
                return Bridge.godot_icall_Object_IsConnected(NativePtr, signal, callable.NativePtr);
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

        public void QueueFree() {
            if (NativePtr != IntPtr.Zero && !disposed) {
                Call("queue_free");
            }
        }

        public void Free() {
            if (NativePtr != IntPtr.Zero && !disposed) {
                foreach (var kv in _connectedCallables) {
                    kv.Value.Dispose();
                }
                _connectedCallables.Clear();
                Bridge.godot_icall_Object_Free(NativePtr);
                NativePtr = IntPtr.Zero;
                _bridgeGCHandle = 0;
                disposed = true;
            }
        }

        public void Dispose() {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing) {
            if (!disposed) {
                if (disposing) {
                    Free();
                }
                disposed = true;
            }
        }

        ~Object() {
            Dispose(false);
        }
    }
}
