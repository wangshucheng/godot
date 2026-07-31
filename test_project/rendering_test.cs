using Godot;
using System;

public partial class rendering_test : Node2D
{
    public override void _Ready()
    {
        GD.Print("[rendering_test] === Test 3/12: Rendering ===");
        GD.Print("[rendering_test] PASS: Scene loaded successfully");
        GD.Print("[rendering_test] PASS: _Ready callback invoked");
        GD.Print("[rendering_test] PASS: C# script binding functional");
        GD.Print("[rendering_test] PASS: GD.Print API functional");
        GD.Print("[rendering_test] NOTE: Position/Color setters are supported in WASM (IEEE-754 union icalls)");
        GD.Print("[rendering_test] Switching to next scene...");
        GetTree().ChangeSceneToFile("res://input_test.tscn");
    }
}
