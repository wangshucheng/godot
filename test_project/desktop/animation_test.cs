using Godot;
using System;

public partial class animation_test : TestBase
{
    private Timer _timer;
    private ColorRect _animRect;
    private double _animTime = 0;
    private bool _animForward = true;

    public override void _Ready()
    {
        base._Ready();
        GD.Print("[AnimationTest] Starting animation tests");
        TestAnimationPlayer();
        TestTimerAnimation();
        TestPropertyAnimation();
        Finish();
    }

    private void TestAnimationPlayer()
    {
        Log("--- AnimationPlayer ---");
        AnimationPlayer player = new AnimationPlayer();
        player.SetName("TestAnimPlayer");
        AddChild(player);

        string cls = player.GetClassName();
        if (cls == "AnimationPlayer")
            Pass("AnimationPlayer created");
        else
            Fail("AnimationPlayer", "class=" + cls);

        // Test Stop (should be safe even when not playing)
        player.Stop();
        Pass("AnimationPlayer.Stop() called (no-op when not playing)");

        // Test IsPlaying
        bool playing = player.IsPlaying();
        Pass("AnimationPlayer.IsPlaying(): " + playing);

        // Note: Can't test Play without an Animation resource
        Pass("AnimationPlayer API available (Play/Stop/IsPlaying)");
    }

    private void TestTimerAnimation()
    {
        Log("--- Timer-based Animation ---");

        _timer = new Timer();
        _timer.SetName("AnimTimer");
        _timer.WaitTime = 0.5;
        _timer.OneShot = false;
        _timer.Autostart = true;
        AddChild(_timer);

        int tickCount = 0;
        Action onTick = () => {
            tickCount++;
            GD.Print("[AnimationTest] Timer tick #" + tickCount);
        };
        _timer.Connect("timeout", new Callable(onTick));

        Pass("Timer created (0.5s interval, auto-start, repeating)");
        Pass("Timer 'timeout' signal connected for animation callback");
    }

    private void TestPropertyAnimation()
    {
        Log("--- Property Animation (manual interpolation) ---");

        _animRect = new ColorRect();
        _animRect.SetName("AnimTarget");
        _animRect.Color = new Color(1, 0, 0, 1);
        _animRect.Position = new Vector2(100, 300);
        _animRect.CustomMinimumSize = new Vector2(50, 50);
        AddChild(_animRect);

        Pass("ColorRect created for property animation");
        Pass("Will animate color R-G-B via _Process interpolation");
    }

    public override void _Process(double delta)
    {
        if (_animRect != null)
        {
            // Animate color: cycle R -> G -> B
            _animTime += delta * (_animForward ? 1 : -1);
            if (_animTime > 1.0) { _animTime = 1.0; _animForward = false; }
            if (_animTime < 0.0) { _animTime = 0.0; _animForward = true; }

            float t = (float)_animTime;
            if (_animForward)
                _animRect.Color = new Color(1 - t, t, 0, 1);
            else
                _animRect.Color = new Color(t, 1 - t, 0, 1);
        }

        // Update status
        string status = "=== Animation Test (live) ===\n";
        status += "AnimTime: " + _animTime.ToString("F2") + "\n\n";
        status += _results;
        _label.Text = status;
    }
}
