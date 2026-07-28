using Godot;

// P5-C fuzz test #20: Exception during GD.Print (logging path).
// Throws inside ToString() override which is called by GD.Print's string
// interpolation. Verifies the engine's logging path handles exceptions
// in user-defined ToString without crashing the invoke_method caller.
[Tool]
public partial class Fuzz20ToStringException : Node
{
	private class BadToString
	{
		public override string ToString()
		{
			throw new System.InvalidOperationException(
				"ToString boom from Fuzz20");
		}
	}

	public override void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz20ToStringException");
		var bad = new BadToString();
		try
		{
			// String interpolation calls ToString() — this throws.
			GD.Print("Object: " + bad);
		}
		catch (System.InvalidOperationException)
		{
			GD.Print("[FUZZ] DONE Fuzz20ToStringException (caught)");
		}
	}
}
