using Godot;

// P5-C fuzz test #16: _ExitTree exception during tree teardown.
// Throws in _ExitTree which fires during scene tree teardown (after FuzzQuit
// requests quit). Verifies invoke_method catches exceptions during node
// destruction — a critical path where an uncaught exception could abort
// the shutdown sequence.
[Tool]
public partial class Fuzz16ExitTreeException : Node
{
	public override void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz16ExitTreeException");
		// Register a deferred call — we want to be alive when FuzzQuit fires.
		GD.Print("[FUZZ] DONE Fuzz16ExitTreeException (registered, awaiting tree teardown)");
	}

	public override void _ExitTree()
	{
		// This fires during GetTree().Quit() teardown.
		// invoke_method must catch this or the editor aborts with non-zero exit.
		throw new System.ApplicationException(
			"exit-tree boom from Fuzz16 (during teardown)");
	}
}
