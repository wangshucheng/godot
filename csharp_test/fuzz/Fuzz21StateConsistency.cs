using Godot;

// P5-C fuzz test #21: Post-exception state consistency assertions.
// Runs after all other fuzz tests (placed late in scene tree) and verifies
// that SignalDB and NodeTree are still intact following the cascade of
// exceptions from Fuzz01-Fuzz20.
[Tool]
public partial class Fuzz21StateConsistency : Node
{
	public override void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz21StateConsistency");
		int checks = 0;
		int passed = 0;

		// 1. SignalDB — Node has "ready" signal.
		checks++;
		if (HasSignal("ready"))
		{
			passed++;
			GD.Print("[FUZZ] CHECK Fuzz21: SignalDB.ready OK");
		}
		else
		{
			GD.Print("[FUZZ] CHECK Fuzz21: SignalDB.ready FAIL");
		}

		// 2. NodeTree navigation — parent is FuzzRoot, has many children.
		checks++;
		Node parent = GetParent();
		if (parent != null && parent.GetChildCount() >= 10)
		{
			passed++;
			GD.Print("[FUZZ] CHECK Fuzz21: NodeTree.parent OK (children=" + parent.GetChildCount() + ")");
		}
		else
		{
			GD.Print("[FUZZ] CHECK Fuzz21: NodeTree.parent FAIL");
		}

		// 3. NodeTree navigation — GetParent returns non-null parent.
		checks++;
		if (GetParent() != null)
		{
			passed++;
			GD.Print("[FUZZ] CHECK Fuzz21: NodeTree.GetParent OK");
		}
		else
		{
			GD.Print("[FUZZ] CHECK Fuzz21: NodeTree.GetParent FAIL");
		}

		// 4. Node name set/get roundtrip.
		checks++;
		Name = "Fuzz21ConsistencyCheck";
		if (Name == "Fuzz21ConsistencyCheck")
		{
			passed++;
			GD.Print("[FUZZ] CHECK Fuzz21: Node.name OK");
		}
		else
		{
			GD.Print("[FUZZ] CHECK Fuzz21: Node.name FAIL");
		}

		GD.Print("[FUZZ] DONE Fuzz21StateConsistency (checks=" + checks + " passed=" + passed + ")");
		if (passed != checks)
		{
			GD.Print("[FUZZ] FAIL Fuzz21StateConsistency: " + (checks - passed) + " consistency checks failed");
		}
	}
}
