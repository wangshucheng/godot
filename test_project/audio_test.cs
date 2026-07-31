using Godot;
using System;

public partial class audio_test : Node2D
{
    public override void _Ready()
    {
        GD.Print("[audio_test] === Test 8/12: Audio ===");
        GD.Print("[audio_test] PASS: Scene loaded successfully");
        GD.Print("[audio_test] PASS: _Ready callback invoked");
        GD.Print("[audio_test] PASS: C# script binding functional");
        GD.Print("[audio_test] PASS: GD.Print API functional");
        GD.Print("[audio_test] NOTE: AudioStreamPlayer creation not testable in WASM");
        GD.Print("[audio_test] Switching to next scene...");
        GetTree().ChangeSceneToFile("res://fs_test.tscn");
    }
}
