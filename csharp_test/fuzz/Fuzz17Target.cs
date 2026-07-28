using Godot;

// P5-C fuzz test #17 target: throws when called via Callable by Fuzz17CallableException.
// Must be in its own file (Fuzz17Target.cs) for Godot's script loader.
[Tool]
public partial class Fuzz17Target : Node
{
	public void ThrowInCallable()
	{
		throw new System.SystemException(
			"callable boom from Fuzz17Target");
	}
}
