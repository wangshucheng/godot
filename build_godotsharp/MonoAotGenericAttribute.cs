using System;

namespace Godot
{
    /// <summary>
    /// Marks a generic type or method instance for explicit AOT registration.
    /// The AOT build tool scans for this attribute and generates registration code.
    /// Used by Full AOT mode to ensure generic instantiations are not stripped.
    /// </summary>
    [AttributeUsage(AttributeTargets.Class | AttributeTargets.Method |
                    AttributeTargets.Struct, AllowMultiple = true)]
    public class MonoAotGenericAttribute : Attribute
    {
        /// <summary>Generic type signature, e.g. "List&lt;string&gt;" or "Action&lt;object[]&gt;"</summary>
        public string GenericType { get; set; }

        /// <summary>Generic method signature, e.g. "Callable.From&lt;T&gt;(Action&lt;T&gt;)"</summary>
        public string GenericMethod { get; set; }
    }
}
