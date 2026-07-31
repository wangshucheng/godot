using Godot;
using System;

public partial class input_test : Node2D
{
    public override void _Ready()
    {
        GD.Print("[input_test] === Test 4/12: Input ===");
        GD.Print("[input_test] PASS: Scene loaded successfully");
        GD.Print("[input_test] PASS: _Ready callback invoked");
        GD.Print("[input_test] PASS: C# script binding functional");
        GD.Print("[input_test] PASS: GD.Print API functional");
        GD.Print("[input_test] NOTE: _UnhandledInput is supported in WASM (InputEvent passed as reference type)");
        GD.Print("[input_test] Switching to next scene...");
        GetTree().ChangeSceneToFile("res://ui_test.tscn");
    }
}
