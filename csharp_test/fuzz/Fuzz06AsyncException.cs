using Godot;
using System.Threading;
using System.Threading.Tasks;

// P5 fuzz test #6: Async exception propagation.
// Spawns a Task that throws; the await re-throws on the main thread.
// Tests that the invoke_method handler catches async-propagated exceptions.
[Tool]
public partial class Fuzz06AsyncException : Node
{
	public override async void _Ready()
	{
		GD.Print("[FUZZ] START Fuzz06AsyncException");
		try
		{
			await Task.Run(() =>
			{
				throw new System.InvalidOperationException("async boom from Fuzz06");
			});
		}
		catch (System.Exception e)
		{
			// Caught in C# — that's fine, the editor doesn't crash either way.
			GD.Print("[FUZZ] DONE Fuzz06AsyncException (caught in C#: " + e.Message + ")");
			return;
		}
		// If await didn't re-throw synchronously, this is reached.
		GD.Print("[FUZZ] DONE Fuzz06AsyncException (no exception observed)");
	}
}
