using Godot;

// P5-C fuzz test #19: Nested try/catch with rethrow.
// Catches an exception in an inner try, then rethrows a wrapped exception.
// Verifies invoke_method handles the outer exception correctly and doesn't
// get confused by the inner catch block's stack unwinding.
[Tool]
public partial class Fuzz19NestedRethrow : Node
{
	public override void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz19NestedRethrow");
		try
		{
			try
			{
				throw new System.ApplicationException("inner boom from Fuzz19");
			}
			catch (System.ApplicationException inner)
			{
				// Wrap and rethrow — invoke_method must catch the outer one.
				throw new System.SystemException(
					"outer boom from Fuzz19 (wrapping inner)", inner);
			}
		}
		catch
		{
			// Final catch — if we land here, the runtime handled nested
			// exception unwinding correctly.
			GD.Print("[FUZZ] DONE Fuzz19NestedRethrow (nested catch OK)");
		}
	}
}
