using System;
using System.Runtime.CompilerServices;

namespace Godot
{
    /// <summary>
    /// Runtime reflection API for Godot ClassDB introspection.
    /// Provides access to class hierarchy, methods, properties, and signals
    /// registered in the engine's ClassDB.
    /// </summary>
    public static class Reflection
    {
        /// <summary>
        /// Get all registered class names in ClassDB.
        /// </summary>
        public static string[] GetClassList()
        {
            string raw = Bridge.godot_icall_ClassDB_GetClassList();
            if (string.IsNullOrEmpty(raw)) return Array.Empty<string>();
            return raw.Split('\n');
        }

        /// <summary>
        /// Check if a class exists in ClassDB.
        /// </summary>
        public static bool ClassExists(string className)
        {
            return Bridge.godot_icall_ClassDB_ClassExists(className) != 0;
        }

        /// <summary>
        /// Get the parent class name. Returns null if the class has no parent.
        /// </summary>
        public static string GetParentClass(string className)
        {
            string parent = Bridge.godot_icall_ClassDB_GetParentClass(className);
            return string.IsNullOrEmpty(parent) ? null : parent;
        }

        /// <summary>
        /// Check if childClass inherits from parentClass (directly or indirectly).
        /// </summary>
        public static bool IsSubclassOf(string childClass, string parentClass)
        {
            return Bridge.godot_icall_ClassDB_IsParentClass(childClass, parentClass) != 0;
        }

        /// <summary>
        /// Check if a class can be instantiated (not abstract/virtual).
        /// </summary>
        public static bool CanInstantiate(string className)
        {
            return Bridge.godot_icall_ClassDB_CanInstantiate(className) != 0;
        }

        /// <summary>
        /// Get all method names for a class (including inherited).
        /// </summary>
        public static string[] GetMethodList(string className)
        {
            string raw = Bridge.godot_icall_ClassDB_GetMethodList(className);
            if (string.IsNullOrEmpty(raw)) return Array.Empty<string>();
            return raw.Split('\n');
        }

        /// <summary>
        /// Check if a class has a specific method.
        /// </summary>
        public static bool HasMethod(string className, string methodName)
        {
            return Bridge.godot_icall_ClassDB_HasMethod(className, methodName) != 0;
        }

        /// <summary>
        /// Get the argument count for a method. Returns -1 if method not found.
        /// </summary>
        public static int GetMethodArgumentCount(string className, string methodName)
        {
            return Bridge.godot_icall_ClassDB_GetMethodArgCount(className, methodName);
        }

        /// <summary>
        /// Get all user-visible property names for a class (including inherited).
        /// </summary>
        public static string[] GetPropertyList(string className)
        {
            string raw = Bridge.godot_icall_ClassDB_GetPropertyList(className);
            if (string.IsNullOrEmpty(raw)) return Array.Empty<string>();
            return raw.Split('\n');
        }

        /// <summary>
        /// Check if a class has a specific property.
        /// </summary>
        public static bool HasProperty(string className, string propertyName)
        {
            return Bridge.godot_icall_ClassDB_HasProperty(className, propertyName) != 0;
        }

        /// <summary>
        /// Get all signal names for a class (including inherited).
        /// </summary>
        public static string[] GetSignalList(string className)
        {
            string raw = Bridge.godot_icall_ClassDB_GetSignalList(className);
            if (string.IsNullOrEmpty(raw)) return Array.Empty<string>();
            return raw.Split('\n');
        }

        /// <summary>
        /// Check if a class has a specific signal.
        /// </summary>
        public static bool HasSignal(string className, string signalName)
        {
            return Bridge.godot_icall_ClassDB_HasSignal(className, signalName) != 0;
        }
    }
}
