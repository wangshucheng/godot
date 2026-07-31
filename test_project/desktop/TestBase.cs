using Godot;
using System;

// Base class for all test scenes. Provides Label display, result tracking,
// and ESC-to-menu navigation.
public partial class TestBase : Node2D
{
    protected Label _label;
    protected ScrollContainer _scroll;
    protected int _passCount = 0;
    protected int _failCount = 0;
    protected string _results = "";

    public override void _Ready()
    {
        _scroll = new ScrollContainer();
        _scroll.Position = new Vector2(10, 10);
        _scroll.CustomMinimumSize = new Vector2(1260, 700);
        _scroll.SizeFlagsHorizontal = 3;
        _scroll.SizeFlagsVertical = 3;
        AddChild(_scroll);

        _label = new Label();
        _label.AddThemeFontSizeOverride("font_size", 14);
        _scroll.AddChild(_label);

        Log(GetType().Name + " - Press ESC to return to menu");
        Log("");
    }

    protected void Log(string msg)
    {
        GD.Print("[" + GetType().Name + "] " + msg);
        _results += msg + "\n";
    }

    protected void Pass(string test)
    {
        _passCount++;
        Log("  [PASS] " + test);
    }

    protected void Fail(string test, string reason)
    {
        _failCount++;
        Log("  [FAIL] " + test + ": " + reason);
    }

    protected void Finish()
    {
        Log("");
        Log("=== Results: " + _passCount + " PASS / " + _failCount + " FAIL ===");
        _label.Text = _results;
    }

    protected void UpdateLabel()
    {
        _label.Text = _results;
    }

    public override void _UnhandledInput(InputEvent @event)
    {
        if (@event is InputEventKey keyEvent && keyEvent.Pressed)
        {
            if (keyEvent.Keycode == (long)Key.Escape)
            {
                GetTree().ChangeSceneToFile("res://test_menu.tscn");
            }
        }
    }
}
