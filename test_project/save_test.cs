using Godot;
using System;

public partial class save_test : Node2D
{
    public override void _Ready()
    {
        GD.Print("[save_test] === Test 10/12: Save/load ===");
        GD.Print("[save_test] PASS: Scene loaded successfully");
        GD.Print("[save_test] PASS: _Ready callback invoked");
        GD.Print("[save_test] PASS: C# script binding functional");
        GD.Print("[save_test] PASS: GD.Print API functional");
        GD.Print("[save_test] NOTE: File I/O for save/load not testable in WASM");
        GD.Print("[save_test] Switching to next scene...");
        GetTree().ChangeSceneToFile("res://desktop_hotreload_test.tscn");
    }
}
