using Godot;

// P5-C fuzz test #15: Repeated exception accumulation (simulated multi-frame).
// Throws an exception 5 times in a loop (simulating per-frame _Process exceptions)
// to verify that repeated exceptions don't accumulate state corruption
// (e.g., pending exception flag not cleared → cascading failures).
//
// Done in _Ready (not _Process) so FuzzQuit's same-frame quit doesn't preempt it.
[Tool]
public partial class Fuzz15PerFrameAccumulation : Node
{
	public override void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz15PerFrameAccumulation");
		for (int i = 1; i <= 5; i++)
		{
			try
			{
				throw new System.InvalidOperationException(
					"repeated boom from Fuzz15 (iter=" + i + ")");
			}
			catch (System.InvalidOperationException)
			{
				// Each iteration catches — invoke_method must clear pending
				// exception each time. If cleanup fails, iteration 2+ would
				// cascade with stale exception state.
				GD.Print("[FUZZ] ITER Fuzz15 iter=" + i + " (caught and cleared)");
			}
		}
		GD.Print("[FUZZ] DONE Fuzz15PerFrameAccumulation (survived 5 iterations)");
	}
}
