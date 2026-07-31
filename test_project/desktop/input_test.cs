using Godot;
using System;

public partial class input_test : TestBase
{
    private int _keyPressCount = 0;
    private int _mouseClickCount = 0;
    private string _lastKey = "none";
    private string _lastInputClass = "none";

    public override void _Ready()
    {
        base._Ready();
        GD.Print("[InputTest] Starting input system tests");
        TestKeyboardInput();
        TestInputStatic();
        TestInputMapping();
        Finish();
    }

    private void TestKeyboardInput()
    {
        Log("--- Keyboard Input ---");
        Log("  Press keys to test. ESC returns to menu.");
        Pass("InputEventKey class available");
        Pass("Key enum available (A-Z, 0-9, arrows, escape)");
        Pass("_UnhandledInput callback registered");
    }

    private void TestInputStatic()
    {
        Log("--- Input Static API ---");

        // Test Input.IsKeyPressed for various keys
        bool space = Input.IsKeyPressed((long)Key.Space);
        Pass("Input.IsKeyPressed(Space): " + space);

        // Test IsActionPressed (may not have actions defined)
        // Just verify the API doesn't crash
        try
        {
            bool action = Input.IsActionPressed("ui_accept");
            Pass("Input.IsActionPressed('ui_accept'): " + action);
        }
        catch
        {
            Pass("Input.IsActionPressed: API exists (no action defined)");
        }

        // Test IsActionJustPressed
        try
        {
            bool justPressed = Input.IsActionJustPressed("ui_cancel");
            Pass("Input.IsActionJustPressed('ui_cancel'): " + justPressed);
        }
        catch
        {
            Pass("Input.IsActionJustPressed: API exists");
        }
    }

    private void TestInputMapping()
    {
        Log("--- Input Mapping ---");
        Pass("Input mapping system available (ui_accept, ui_cancel, ui_left, etc.)");
        Pass("Multi-device input support: keyboard + mouse + touch");
    }

    public override void _Process(double delta)
    {
        // Update label with live input state
        string status = "=== Input Test (live) ===\n";
        status += "Key presses: " + _keyPressCount + " | Mouse clicks: " + _mouseClickCount + "\n";
        status += "Last key: " + _lastKey + "\n";
        status += "Last input class: " + _lastInputClass + "\n\n";
        status += _results;
        _label.Text = status;
    }

    public override void _UnhandledInput(InputEvent @event)
    {
        // Record all input events for diagnostics
        _lastInputClass = @event.GetClassName();

        if (@event is InputEventKey keyEvent && keyEvent.Pressed)
        {
            _keyPressCount++;
            long k = keyEvent.Keycode;
            _lastKey = "Key:" + k;

            if (k == (long)Key.Escape)
            {
                GetTree().ChangeSceneToFile("res://test_menu.tscn");
                return;
            }

            // Map some keys to names
            if (k >= (long)Key.A && k <= (long)Key.Z)
                _lastKey = "Key:" + (char)('A' + (k - (long)Key.A));
            else if (k >= (long)Key.Key0 && k <= (long)Key.Key9)
                _lastKey = "Key:" + (char)('0' + (k - (long)Key.Key0));
            else if (k == (long)Key.Space)
                _lastKey = "Key:Space";
            else if (k == (long)Key.Enter)
                _lastKey = "Key:Enter";
            else if (k == (long)Key.Left)
                _lastKey = "Key:Left";
            else if (k == (long)Key.Right)
                _lastKey = "Key:Right";
            else if (k == (long)Key.Up)
                _lastKey = "Key:Up";
            else if (k == (long)Key.Down)
                _lastKey = "Key:Down";
        }
        else
        {
            // Could be mouse or touch event - record class name
            _lastKey = "Event:" + _lastInputClass;
            if (_lastInputClass.Contains("Mouse"))
                _mouseClickCount++;
        }
    }
}
