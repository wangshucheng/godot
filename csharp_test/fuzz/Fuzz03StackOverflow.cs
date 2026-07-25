using Godot;

// P5 fuzz test #3: StackOverflowException (simulated).
// Throws StackOverflowException explicitly. A *real* stack overflow (deep
// recursion until the OS kills the process) cannot be caught in .NET — it
// corrupts the stack and terminates the process. This simulated version
// tests that invoke_method catches the StackOverflowException exception
// object when thrown explicitly, without actually exhausting the stack.
//
// Fuzz05 separately exercises deep-recursion exception unwinding (throw at
// depth 500) — the two tests are complementary.
[Tool]
public partial class Fuzz03StackOverflow : Node
{
	public override void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz03StackOverflow");
		// Explicit throw: the exception object is catchable, unlike a real
		// stack overflow which would terminate the process.
		throw new System.StackOverflowException(
			"simulated stack overflow from Fuzz03 (explicit throw)");
	}
}
