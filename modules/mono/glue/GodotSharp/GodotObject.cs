using System;

namespace Godot {
    public class Object : IDisposable {
        internal IntPtr NativePtr;
        private bool disposed = false;

        public Object() {
            NativePtr = Bridge.godot_icall_Object_Ctor(this);
        }

        internal Object(IntPtr nativePtr) {
            NativePtr = nativePtr;
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

        public void Free() {
            if (NativePtr != IntPtr.Zero && !disposed) {
                Bridge.godot_icall_Object_Free(NativePtr);
                NativePtr = IntPtr.Zero;
                disposed = true;
            }
        }

        public void Dispose() {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing) {
            if (!disposed) {
                Free();
                disposed = true;
            }
        }

        ~Object() {
            Dispose(false);
        }
    }
}
