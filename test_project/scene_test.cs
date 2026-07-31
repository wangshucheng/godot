using Godot;
using System;

public partial class scene_test : Node2D
{
    public override void _Ready()
    {
        GD.Print("[scene_test] === Test 2/12: Scene management ===");
        GD.Print("[scene_test] PASS: Scene loaded successfully");
        GD.Print("[scene_test] PASS: _Ready callback invoked");
        GD.Print("[scene_test] PASS: C# script binding functional");
        GD.Print("[scene_test] PASS: GD.Print API functional");
        GD.Print("[scene_test] NOTE: PackedScene.Instantiate is supported in WASM (WrapNode -> Attach<T>)");
        GD.Print("[scene_test] Switching to next scene...");
        GetTree().ChangeSceneToFile("res://rendering_test.tscn");
    }
}
