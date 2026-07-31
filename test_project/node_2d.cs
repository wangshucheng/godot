using Godot;
using System;

public partial class node_2d : Node2D
{
	private int frameCount = 0;
	private int seed = 12345;

	// Simple deterministic PRNG to avoid System.Random which triggers libmono-native DllImport
	private int NextRand()
	{
		seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
		return seed;
	}

	public override void _Ready()
	{
		GD.Print("[2048] _Ready called!");
	}

	// WASM-safe _Process: minimal version to verify callback works.
	// Only uses literal string printing and integer arithmetic.
	// NOTE: Position setter triggers "CANNOT HANDLE COOKIE VLII" (long param issue).
	// NOTE: GD.Concat/GD.ToString trigger "CANNOT HANDLE COOKIE IL" (string ctor/get_Chars icall).
	public override void _Process(double delta)
	{
		frameCount++;
		// Print on frame 1 and frame 61 to verify _Process runs repeatedly
		if (frameCount == 1)
		{
			GD.Print("[2048] _Process frame 1 ok");
		}
		else if (frameCount == 61)
		{
			GD.Print("[2048] _Process frame 61 ok");
		}
	}

	// Re-enabled: _UnhandledInput(InputEvent) works in WASM (reference type).
	public override void _UnhandledInput(InputEvent @event)
	{
		GD.Print(GD.Concat("[2048] UnhandledInput frame=", GD.ToString(frameCount)));
	}
}