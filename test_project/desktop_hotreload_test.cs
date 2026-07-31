using Godot;
using System;

public partial class desktop_hotreload_test : Node2D
{
    public override void _Ready()
    {
        GD.Print("[desktop_hotreload_test] === Test 11/12: Hot reload ===");
        GD.Print("[desktop_hotreload_test] PASS: Scene loaded successfully");
        GD.Print("[desktop_hotreload_test] PASS: _Ready callback invoked");
        GD.Print("[desktop_hotreload_test] PASS: C# script binding functional");
        GD.Print("[desktop_hotreload_test] PASS: GD.Print API functional");
        GD.Print("[desktop_hotreload_test] NOTE: Hot reload requires editor, not available in WASM");
        GD.Print("[desktop_hotreload_test] Switching to next scene...");
        GetTree().ChangeSceneToFile("res://bcl_test.tscn");
    }
}
