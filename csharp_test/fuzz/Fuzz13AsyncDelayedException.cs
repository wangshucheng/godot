using Godot;
using System.Threading.Tasks;

// P5-C fuzz test #13: Async exception with delayed arrival.
// Spawns a Task that throws after a short delay. The exception arrives on
// a thread pool thread and propagates to the SynchronizationContext.
// Verifies the runtime handles unobserved task exceptions without crashing.
[Tool]
public partial class Fuzz13AsyncDelayedException : Node
{
	public override async void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz13AsyncDelayedException");
		// Fire-and-forget task that throws after 50ms.
		// Without observation, this would surface as UnobservedTaskException.
		_ = Task.Run(async () =>
		{
			await Task.Delay(50);
			throw new System.InvalidOperationException(
				"delayed async boom from Fuzz13 (50ms)");
		});

		// Give the task time to throw before we exit.
		await Task.Delay(150);
		GD.Print("[FUZZ] DONE Fuzz13AsyncDelayedException (runtime survived)");
	}
}
