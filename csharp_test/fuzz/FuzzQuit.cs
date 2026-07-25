using Godot;

// P5 fuzz test orchestrator: runs as the last child of FuzzRoot and requests
// the scene tree to quit once all fuzz _Ready callbacks have had a chance to
// run. Without this, the headless editor keeps the main loop alive and the
// fuzz runner hits its timeout.
//
// SceneTree.Quit() schedules the quit for the end of the current frame, so
// all sibling _Ready callbacks (which run in tree order before this node)
// will have already executed by the time the quit takes effect.
[Tool]
public partial class FuzzQuit : Node
{
	public override void _Ready()
	{
		GD.Print("[FUZZ] FuzzQuit: requesting quit");
		GetTree().Quit(0);
	}
}
