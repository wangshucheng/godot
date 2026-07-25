using Godot;

// P5 fuzz test #5: Deep recursion that throws before stack overflow.
// Tests that invoke_method catches exceptions raised deep in the call stack.
// Note: Actual StackOverflowException cannot be caught in .NET — it terminates
// the process. This test throws explicitly at depth 500 to exercise the
// exception unwinding path through many stack frames without crashing.
[Tool]
public partial class Fuzz05RecursiveStackBlowup : Node
{
	private struct BigStruct
	{
		public long a, b, c, d, e, f, g, h;
	}

	private static long DeepRecurse(int depth, BigStruct s)
	{
		// Throw at depth 0 to test exception unwinding through many frames.
		if (depth <= 0)
		{
			throw new System.StackOverflowException(
				"simulated stack blowup from Fuzz05 at depth 0");
		}
		BigStruct next = s;
		next.a = depth;
		return DeepRecurse(depth - 1, next) + s.a;
	}

	public override void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz05RecursiveStackBlowup");
		BigStruct initial = new BigStruct { a = 1, b = 2, c = 3, d = 4 };
		long sum = DeepRecurse(500, initial);
		GD.Print("[FUZZ] DONE Fuzz05RecursiveStackBlowup (unexpected: sum=" + sum + ")");
	}
}
