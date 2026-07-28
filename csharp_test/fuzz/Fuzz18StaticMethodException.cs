using Godot;

// P5-C fuzz test #18: Exception in static method (no instance required).
// Throws inside a static method called from _Ready.
// Verifies invoke_method catches exceptions from static method dispatch
// (different IL calling convention than instance methods).
[Tool]
public partial class Fuzz18StaticMethodException : Node
{
	public override void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz18StaticMethodException");
		// Call a static method that throws — exercises the static dispatch path.
		ThrowingStatic();
		GD.Print("[FUZZ] DONE Fuzz18StaticMethodException (unexpected: reached end)");
	}

	private static void ThrowingStatic()
	{
		throw new System.NotImplementedException(
			"static method boom from Fuzz18");
	}
}
