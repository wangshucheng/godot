using Godot;
using System;

// P5-C fuzz test #17: Cross-script Callable invocation exception.
// Creates a Callable targeting a method on a sibling node, then invokes it.
// The target method throws — verifies invoke_method catches exceptions
// raised through the Callable dispatch path (not just direct method calls).
// Fuzz17Target is in a separate file (Fuzz17Target.cs) due to Godot's
// file_name==class_name convention for script loading.
[Tool]
public partial class Fuzz17CallableException : Node
{
	public override void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz17CallableException");
		Node target = GetParent().GetNodeOrNull("Fuzz17Target");
		if (target is Fuzz17Target t)
		{
			// Build a Callable targeting the throwing method, then invoke.
			var callable = Callable.From((Action)t.ThrowInCallable);
			callable.Call();
		}
		GD.Print("[FUZZ] DONE Fuzz17CallableException (unexpected: reached end)");
	}
}
