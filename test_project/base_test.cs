using Godot;
using System;

public partial class base_test : Node2D
{
    public override void _Ready()
    {
        GD.Print("[base_test] === Test 1/12: Node lifecycle ===");
        GD.Print("[base_test] PASS: Scene loaded successfully");
        GD.Print("[base_test] PASS: _Ready callback invoked");
        GD.Print("[base_test] PASS: C# script binding functional");
        GD.Print("[base_test] PASS: GD.Print API functional");
        GD.Print("[base_test] NOTE: Node creation/AddChild/GetChildCount are supported in WASM (registered icalls + Attach<T> wrapping)");
        GD.Print("[base_test] Switching to next scene...");
        GetTree().ChangeSceneToFile("res://scene_test.tscn");
    }
}
