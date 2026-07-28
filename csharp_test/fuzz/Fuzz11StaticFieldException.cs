using Godot;

// P5-C fuzz test #11: Static method exception (no instance required).
// Throws inside a static method that returns int — verifies invoke_method
// catches exceptions from static method dispatch (different IL calling
// convention than instance methods). Unlike Fuzz07 (which tests
// TypeInitializationException via static ctor), this tests a plain
// InvalidOperationException thrown from a static method body.
[Tool]
public partial class Fuzz11StaticFieldException : Node
{
	public override void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz11StaticFieldException");
		try
		{
			int v = ComputeStaticValue();
			GD.Print("[FUZZ] DONE Fuzz11StaticFieldException (unexpected: v=" + v + ")");
		}
		catch (System.InvalidOperationException)
		{
			GD.Print("[FUZZ] DONE Fuzz11StaticFieldException (caught static method exception)");
		}
	}

	private static int ComputeStaticValue()
	{
		throw new System.InvalidOperationException(
			"static method boom from Fuzz11");
	}
}
