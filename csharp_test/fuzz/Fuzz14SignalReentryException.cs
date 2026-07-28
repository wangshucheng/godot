using Godot;
using System;

// P5-C fuzz test #14: Signal reentry exception.
// Emits a signal from within its own handler, creating a reentrant call.
// The reentrant handler throws on the second invocation.
// Verifies invoke_method handles nested signal dispatch + exception.
[Tool]
public partial class Fuzz14SignalReentryException : Node
{
	[Signal]
	public delegate void ReentryTriggeredEventHandler(int depth);

	private int _depth = 0;

	public override void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz14SignalReentryException");
		Connect("ReentryTriggered", (Action<int>)OnReentry);
		// Kick off the chain — first emission is depth=1.
		EmitSignal("ReentryTriggered", 1);
		GD.Print("[FUZZ] DONE Fuzz14SignalReentryException (unexpected: reached end)");
	}

	private void OnReentry(int depth)
	{
		_depth = depth;
		if (depth >= 2)
		{
			// Second invocation throws — exercises nested exception unwinding.
			throw new System.SystemException(
				"reentry boom from Fuzz14 at depth=" + depth);
		}
		// Reenter: emit again with incremented depth.
		EmitSignal("ReentryTriggered", depth + 1);
	}
}
