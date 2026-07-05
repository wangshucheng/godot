using System;

namespace Godot {
    [AttributeUsage(AttributeTargets.All, Inherited = false)]
    public sealed class PreserveAttribute : Attribute {
        public bool AllMembers { get; set; }
        public PreserveAttribute() { }
        public PreserveAttribute(bool allMembers) {
            AllMembers = allMembers;
        }
    }
}
