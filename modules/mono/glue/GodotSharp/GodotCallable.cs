using System;
using System.Runtime.CompilerServices;

namespace Godot {
    public class Callable : IDisposable {
        internal IntPtr NativePtr;
        private bool disposed = false;

        private Callable(IntPtr nativePtr) {
            NativePtr = nativePtr;
        }

        private static Action<object[]> CreateWrapper(Delegate del) {
            return (args) => {
                try {
                    int paramCount = del.Method.GetParameters().Length;
                    if (paramCount == 0) {
                        del.DynamicInvoke();
                    } else if (paramCount == 1 && del.Method.GetParameters()[0].ParameterType == typeof(object[])) {
                        del.DynamicInvoke(new object[] { args });
                    } else {
                        object[] passArgs = new object[paramCount];
                        for (int i = 0; i < paramCount && i < args.Length; i++) {
                            passArgs[i] = args[i];
                        }
                        del.DynamicInvoke(passArgs);
                    }
                } catch (Exception e) {
                    Console.WriteLine($"[Godot] Exception in signal callback: {e}");
                }
            };
        }

        public static Callable From(Action action) {
            Action<object[]> wrapper = CreateWrapper(action);
            IntPtr ptr = Bridge.godot_icall_Callable_CreateFromDelegate(wrapper);
            if (ptr == IntPtr.Zero) throw new InvalidOperationException("Failed to create Callable.");
            return new Callable(ptr);
        }

        public static Callable From<T>(Action<T> action) {
            Action<object[]> wrapper = CreateWrapper(action);
            IntPtr ptr = Bridge.godot_icall_Callable_CreateFromDelegate(wrapper);
            if (ptr == IntPtr.Zero) throw new InvalidOperationException("Failed to create Callable.");
            return new Callable(ptr);
        }

        public static Callable From(Delegate del) {
            Action<object[]> wrapper = CreateWrapper(del);
            IntPtr ptr = Bridge.godot_icall_Callable_CreateFromDelegate(wrapper);
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
            if (!disposed) {
                if (NativePtr != IntPtr.Zero) {
                    Bridge.godot_icall_Callable_Free(NativePtr);
                    NativePtr = IntPtr.Zero;
                }
                disposed = true;
            }
            GC.SuppressFinalize(this);
        }

        ~Callable() {
            if (!disposed && NativePtr != IntPtr.Zero) {
                Bridge.godot_icall_Callable_Free(NativePtr);
            }
        }
    }
}
