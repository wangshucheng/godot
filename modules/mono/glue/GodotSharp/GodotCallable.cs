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

        private static Action<object[]> CreateWrapper(Delegate del) {
            if (del is Action simpleAction) {
                return (args) => {
                    try { simpleAction(); }
                    catch (Exception e) { LogException(e); }
                };
            }
            if (del is Action<object[]> arrayAction) {
                return (args) => {
                    try { arrayAction(args ?? new object[0]); }
                    catch (Exception e) { LogException(e); }
                };
            }
            return (args) => {
                try {
                    int paramCount = del.Method.GetParameters().Length;
                    if (paramCount == 0) {
                        del.DynamicInvoke();
                    } else if (paramCount == 1 && del.Method.GetParameters()[0].ParameterType == typeof(object[])) {
                        del.DynamicInvoke(new object[] { args });
                    } else {
                        object[] passArgs = new object[paramCount];
                        int copyCount = paramCount;
                        if (args != null && args.Length < copyCount) copyCount = args.Length;
                        for (int i = 0; i < copyCount; i++) {
                            passArgs[i] = args[i];
                        }
                        del.DynamicInvoke(passArgs);
                    }
                } catch (Exception e) {
                    LogException(e);
                }
            };
        }

        [MethodImpl(MethodImplOptions.NoInlining)]
        private static void LogException(Exception e) {
            GD.PrintErr("Exception in signal callback: " + e.GetType().Name + ": " + e.Message);
        }

        public static Callable From(Action action) {
            Action<object[]> wrapper = CreateWrapper(action);
            IntPtr ptr = Bridge.godot_icall_Callable_CreateFromDelegate(wrapper);
            if (ptr == IntPtr.Zero) throw new InvalidOperationException("Failed to create Callable.");
            return new Callable(ptr);
        }

        public static Callable From<T>(Action<T> action) {
            Action<object[]> wrapper = (args) => {
                try {
                    T arg = args != null && args.Length > 0 && args[0] is T tval ? tval : default(T);
                    action(arg);
                } catch (Exception e) { LogException(e); }
            };
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
