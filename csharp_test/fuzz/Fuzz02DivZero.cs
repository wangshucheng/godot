using Godot;

// P5 fuzz test #2: DivideByZeroException in _Ready.
// Verifies invoke_method's exc handler catches DBZ and clears pending exception.
[Tool]
public partial class Fuzz02DivZero : Node
{
	public override void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz02DivZero");
		int zero = 0;
		int result = 42 / zero;
		GD.Print("[FUZZ] DONE Fuzz02DivZero (unexpected: result=" + result + ")");
	}
}
