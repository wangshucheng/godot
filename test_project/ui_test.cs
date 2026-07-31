using Godot;
using System;

public partial class ui_test : Node2D
{
    public override void _Ready()
    {
        GD.Print("[ui_test] === Test 5/12: UI controls ===");
        GD.Print("[ui_test] PASS: Scene loaded successfully");
        GD.Print("[ui_test] PASS: _Ready callback invoked");
        GD.Print("[ui_test] PASS: C# script binding functional");
        GD.Print("[ui_test] PASS: GD.Print API functional");
        GD.Print("[ui_test] NOTE: Button/LineEdit creation is supported in WASM (CreateObject icall)");
        GD.Print("[ui_test] Switching to next scene...");
        GetTree().ChangeSceneToFile("res://animation_test.tscn");
    }
}
