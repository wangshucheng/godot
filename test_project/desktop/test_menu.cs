using Godot;
using System;

public partial class test_menu : Node2D
{
    private Label _label;
    private string[] _testNames = {
        "1. base_test       - Node lifecycle, signals, script attach, tree ops",
        "2. scene_test      - Scene load/unload, switch, instantiate",
        "3. rendering_test  - Light, material, camera, render layers",
        "4. input_test      - Keyboard, mouse, touch, input mapping",
        "5. ui_test         - Button, LineEdit, containers, dialogs",
        "6. animation_test  - AnimationPlayer, state machine, callbacks",
        "7. physics_test    - RigidBody, collision, raycast",
        "8. audio_test      - 2D SFX, BGM, audio bus, volume mix",
        "9. fs_test         - File read/write, paths, resource load",
        "0. save_test       - Save/load, persistence, version compat",
        "Q. hotreload_test  - Code/scene hot reload (desktop)",
        "W. bcl_test        - .NET BCL: collections, IO, serialize, async"
    };

    private string[] _testScenes = {
        "res://base_test.tscn",
        "res://scene_test.tscn",
        "res://rendering_test.tscn",
        "res://input_test.tscn",
        "res://ui_test.tscn",
        "res://animation_test.tscn",
        "res://physics_test.tscn",
        "res://audio_test.tscn",
        "res://fs_test.tscn",
        "res://save_test.tscn",
        "res://desktop_hotreload_test.tscn",
        "res://bcl_test.tscn"
    };

    public override void _Ready()
    {
        _label = new Label();
        _label.Position = new Vector2(20, 20);
        _label.AddThemeFontSizeOverride("font_size", 16);
        AddChild(_label);

        string text = "=== Bottom-Layer Capability Test Suite ===\n";
        text += "Platform: " + OS.GetName() + " | FPS: " + Engine.GetFramesPerSecond() + "\n";
        text += "Time: " + Time.GetTimeStringFromSystem() + "\n\n";
        foreach (string s in _testNames)
        {
            text += s + "\n";
        }
        text += "\nESC to quit";
        _label.Text = text;

        GD.Print("[TestMenu] Ready. " + _testNames.Length + " tests available.");
    }

    public override void _UnhandledInput(InputEvent @event)
    {
        if (@event is InputEventKey keyEvent && keyEvent.Pressed)
        {
            long k = keyEvent.Keycode;
            int index = -1;
            if (k >= (long)Key.Key1 && k <= (long)Key.Key9)
            {
                index = (int)(k - (long)Key.Key1);
            }
            else if (k == (long)Key.Key0)
            {
                index = 9;
            }
            else if (k == (long)Key.Q)
            {
                index = 10;
            }
            else if (k == (long)Key.W)
            {
                index = 11;
            }
            else if (k == (long)Key.Escape)
            {
                GetTree().Quit();
                return;
            }

            if (index >= 0 && index < _testScenes.Length)
            {
                GD.Print("[TestMenu] Switching to: " + _testNames[index]);
                GetTree().ChangeSceneToFile(_testScenes[index]);
            }
        }
    }
}
