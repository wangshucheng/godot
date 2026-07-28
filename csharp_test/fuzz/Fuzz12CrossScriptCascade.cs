using Godot;

// P5-C fuzz test #12: Cross-script exception cascade.
// Fuzz12 calls a method on Fuzz12Target (a sibling [Tool] node) that throws.
// Verifies invoke_method catches exceptions propagating across script boundaries.
// Fuzz12Target is in a separate file (Fuzz12Target.cs) due to Godot's
// file_name==class_name convention for script loading.
[Tool]
public partial class Fuzz12CrossScriptCascade : Node
{
	public override void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz12CrossScriptCascade");
		// Look up the sibling target node and call its throwing method.
		Node target = GetParent().GetNodeOrNull("Fuzz12Target");
		if (target is Fuzz12Target t)
		{
			// Call the throwing method — invoke_method must catch this.
			t.ThrowAcrossScriptBoundary();
		}
		GD.Print("[FUZZ] DONE Fuzz12CrossScriptCascade (unexpected: reached end)");
	}
}
