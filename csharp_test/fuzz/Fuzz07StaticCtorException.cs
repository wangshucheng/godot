using Godot;

// P5 fuzz test #7: Static constructor exception.
// Uses a nested class with a static constructor that throws. The first access
// triggers the static ctor, which wraps the throw in TypeInitializationException.
// Verifies that invoke_method catches TypeInitializationException.
//
// Note: In .NET, a failed static constructor permanently breaks the type.
// This is expected behavior — the test verifies that invoke_method catches
// the exception gracefully on first access. The type is not accessed again
// after _Ready returns (no _Process override), so no recurring crash.
[Tool]
public partial class Fuzz07StaticCtorException : Node
{
	private class BadInit
	{
		static BadInit()
		{
			throw new System.SystemException("static ctor boom from Fuzz07");
		}

		public static int Value => 42;
	}

	public override void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz07StaticCtorException");
		// This access triggers the static ctor → TypeInitializationException.
		int v = BadInit.Value;
		GD.Print("[FUZZ] DONE Fuzz07StaticCtorException (unexpected: v=" + v + ")");
	}

	// No _Process override — avoids re-triggering the broken type every frame.
}
