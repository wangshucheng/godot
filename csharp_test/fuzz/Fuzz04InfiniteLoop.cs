using Godot;

// P5 fuzz test #4: Long-running loop that simulates an "infinite loop".
// Bounded to 10M iterations with periodic progress prints.
// Verifies the editor remains responsive during long tool-script execution.
// NOTE: True infinite loops would hang the editor (synchronous _Process);
// this test simulates a runaway loop with a defined end.
[Tool]
public partial class Fuzz04InfiniteLoop : Node
{
	public override void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz04InfiniteLoop");
		long accumulator = 0;
		for (long i = 0; i < 10_000_000; i++)
		{
			accumulator += i;
			if (i % 1_000_000 == 0)
			{
				GD.Print("[FUZZ] Fuzz04InfiniteLoop progress i=" + i);
			}
		}
		GD.Print("[FUZZ] DONE Fuzz04InfiniteLoop (accumulator=" + accumulator + ")");
	}
}
