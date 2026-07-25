using System;

namespace Godot {
    // B0 §3.1: C# attribute definitions for mono editor features.
    // These attributes are read by mono_script_metadata.cpp via mono_custom_attrs_from_member
    // (C++ direct metadata access, no C# reflection — AOT-safe).
    // All attributes carry [Preserve] and are listed in linker.xml.

    /// Marks a field or property for export to the Godot Inspector.
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    [Preserve]
    public sealed class ExportAttribute : Attribute { }

    /// Marks a nested delegate as a Godot signal. Convention: delegate named
    /// <SignalName>EventHandler, nested inside the owning class.
    [AttributeUsage(AttributeTargets.Delegate)]
    [Preserve]
    public sealed class SignalAttribute : Attribute { }

    /// Marks a C# class as a tool script (runs in the editor).
    [AttributeUsage(AttributeTargets.Class)]
    [Preserve]
    public sealed class ToolAttribute : Attribute { }

    /// Registers a C# class as a global Godot type (appears in "Add Node" dialog).
    [AttributeUsage(AttributeTargets.Class)]
    [Preserve]
    public sealed class GlobalClassAttribute : Attribute {
        public string IconPath { get; set; }
    }
}
