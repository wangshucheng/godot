using System;

namespace Godot {
    public class Object : IDisposable {
        internal IntPtr NativePtr;
        private bool disposed = false;

        public Object() {
            NativePtr = Bridge.godot_icall_Object_Ctor(GetType().Name);
        }

        internal Object(IntPtr nativePtr) {
            NativePtr = nativePtr;
        }

        public void Set(string property, object value) {
            Bridge.godot_icall_Object_Set(NativePtr, property, value);
        }

        public object Get(string property) {
            return Bridge.godot_icall_Object_Get(NativePtr, property);
        }

        public object Call(string method, params object[] args) {
            return Bridge.godot_icall_Object_Call(NativePtr, method, args);
        }

        public void Free() {
            if (NativePtr != IntPtr.Zero) {
                Bridge.godot_icall_Object_Free(NativePtr);
                NativePtr = IntPtr.Zero;
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
