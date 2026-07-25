using Godot;

// P5 fuzz test #9: Method argument exception.
// Calls a method that validates its argument and throws ArgumentException.
// Verifies invoke_method catches ArgumentException from method body.
[Tool]
public partial class Fuzz09MethodArgException : Node
{
	private static int StrictParse(string s)
	{
		if (string.IsNullOrEmpty(s))
		{
			throw new System.ArgumentException(
				"argument null/empty not allowed (Fuzz09)", nameof(s));
		}
		return int.Parse(s);
	}

	public override void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz09MethodArgException");
		int v = StrictParse(null);
		GD.Print("[FUZZ] DONE Fuzz09MethodArgException (unexpected: v=" + v + ")");
	}
}
