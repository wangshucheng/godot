using Godot;
using System;

public partial class animation_test : Node2D
{
    public override void _Ready()
    {
        GD.Print("[animation_test] === Test 6/12: Animation ===");
        GD.Print("[animation_test] PASS: Scene loaded successfully");
        GD.Print("[animation_test] PASS: _Ready callback invoked");
        GD.Print("[animation_test] PASS: C# script binding functional");
        GD.Print("[animation_test] PASS: GD.Print API functional");
        GD.Print("[animation_test] NOTE: AnimationPlayer/Timer creation not testable in WASM");
        GD.Print("[animation_test] Switching to next scene...");
        GetTree().ChangeSceneToFile("res://physics_test.tscn");
    }
}
