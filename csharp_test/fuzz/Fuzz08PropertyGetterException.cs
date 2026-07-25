using Godot;

// P5 fuzz test #8: Property getter exception.
// The property getter throws InvalidOperationException when read from _Ready.
// Verifies that invoke_method catches exceptions from property access paths.
[Tool]
public partial class Fuzz08PropertyGetterException : Node
{
	private int _backing;

	public int BadProperty
	{
		get
		{
			if (_backing == 0)
			{
				throw new System.InvalidOperationException(
					"property getter boom from Fuzz08");
			}
			return _backing;
		}
		set => _backing = value;
	}

	public override void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz08PropertyGetterException");
		// Reading the property before any setter call triggers the throw.
		int v = BadProperty;
		GD.Print("[FUZZ] DONE Fuzz08PropertyGetterException (unexpected: v=" + v + ")");
	}
}
