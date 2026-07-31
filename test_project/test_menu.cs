using Godot;
using System;

public partial class test_menu : Node2D
{
    public override void _Ready()
    {
        GD.Print("[TestMenu] Starting 12-scene auto-test cycle");
        GD.Print("[TestMenu] Switching to base_test.tscn");
        GetTree().ChangeSceneToFile("res://base_test.tscn");
    }
}
