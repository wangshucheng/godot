using Godot;

// P5 fuzz test #1: NullReferenceException in _Ready.
// Verifies invoke_method's exc handler catches NRE and clears pending exception.
// Acceptance: editor does not crash; [FUZZ] marker is printed.
[Tool]
public partial class Fuzz01NullRef : Node
{
	public override void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz01NullRef");
		string nullStr = null;
		// Force a NullReferenceException by accessing length of null string.
		int len = nullStr.Length;
		// Unreachable on success — if we get here, the exception was silently swallowed.
		GD.Print("[FUZZ] DONE Fuzz01NullRef (unexpected: len=" + len + ")");
	}
}
