using Godot;

// P5-C fuzz test #12 target: throws when called by Fuzz12CrossScriptCascade.
// Must be in its own file (Fuzz12Target.cs) for Godot's script loader to
// find it via the file_name==class_name convention.
[Tool]
public partial class Fuzz12Target : Node
{
	public void ThrowAcrossScriptBoundary()
	{
		throw new System.ApplicationException(
			"cross-script boom from Fuzz12Target (called by Fuzz12)");
	}
}
