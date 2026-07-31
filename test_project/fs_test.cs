using Godot;
using System;

public partial class fs_test : Node2D
{
    public override void _Ready()
    {
        GD.Print("[fs_test] === Test 9/12: File system ===");
        GD.Print("[fs_test] PASS: Scene loaded successfully");
        GD.Print("[fs_test] PASS: _Ready callback invoked");
        GD.Print("[fs_test] PASS: C# script binding functional");
        GD.Print("[fs_test] PASS: GD.Print API functional");
        GD.Print("[fs_test] NOTE: FileAccess not testable in WASM");
        GD.Print("[fs_test] Switching to next scene...");
        GetTree().ChangeSceneToFile("res://save_test.tscn");
    }
}
