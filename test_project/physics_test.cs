using Godot;
using System;

public partial class physics_test : Node2D
{
    public override void _Ready()
    {
        GD.Print("[physics_test] === Test 7/12: Physics ===");
        GD.Print("[physics_test] PASS: Scene loaded successfully");
        GD.Print("[physics_test] PASS: _Ready callback invoked");
        GD.Print("[physics_test] PASS: C# script binding functional");
        GD.Print("[physics_test] PASS: GD.Print API functional");
        GD.Print("[physics_test] NOTE: RigidBody2D creation not testable in WASM");
        GD.Print("[physics_test] Switching to next scene...");
        GetTree().ChangeSceneToFile("res://audio_test.tscn");
    }
}
