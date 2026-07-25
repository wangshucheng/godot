using Godot;
using System;

// P5 fuzz test #10: Signal callback exception.
// Declares a [Signal] and a method that emits it; the connected handler throws.
// Verifies invoke_method catches exceptions raised inside signal callbacks.
[Tool]
public partial class Fuzz10SignalCallbackException : Node
{
	[Signal]
	public delegate void FuzzTriggeredEventHandler(int code);

	public override void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz10SignalCallbackException");
		// Connect our own handler so the emit below invokes it synchronously.
		Connect("FuzzTriggered", (Action<int>)OnFuzzTriggered);
		EmitSignal("FuzzTriggered", 42);
		GD.Print("[FUZZ] DONE Fuzz10SignalCallbackException (unexpected: emit returned)");
	}

	// Handler that throws — exercises the invoke_method exception path
	// when the engine dispatches the signal callback.
	private void OnFuzzTriggered(int code)
	{
		throw new SystemException(
			"signal callback boom from Fuzz10 (code=" + code + ")");
	}
}
