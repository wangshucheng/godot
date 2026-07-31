using Godot;
using System;

// Base class for all test scenes.
// The WASM Mono interpreter (MONO_EE_MODE_INTERP, no JIT/AOT) supports the full
// managed API: value-type setters (Position/Color) use IEEE-754 union icalls,
// and _Process(double)/_UnhandledInput(InputEvent) pass parameters via pinned-boxed
// value types / reference types on the C++ side (see csharp_script.cpp callp()).
// _Ready() (0 params) and GD.Print(string) are the minimal required entry points,
// but the other callbacks work too.
// Auto-advance: Finish() calls AdvanceToNext() immediately after tests complete.
public partial class TestBase : Node2D
{
    protected int _passCount = 0;
    protected int _failCount = 0;

    private static int _testIndex = 0;
    private static bool _allTestsComplete = false;
    private static readonly string[] _testScenes = {
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

    public static bool AllTestsComplete => _allTestsComplete;
    public static int TestIndex => _testIndex;
    public static int TotalTests => _testScenes.Length;

    public override void _Ready()
    {
        GD.Print("[TestBase] _Ready for " + GetType().Name + " (test " + (_testIndex + 1) + "/" + _testScenes.Length + ")");
        Log(GetType().Name + " started");
    }

    // _Process(double) is supported in WASM (pinned-boxed double on the C++ side).
    // Auto-advance still happens in Finish() for deterministic test sequencing.

    protected virtual void AdvanceToNext()
    {
        _testIndex++;
        if (_testIndex < _testScenes.Length)
        {
            string nextScene = _testScenes[_testIndex];
            GD.Print("[TestBase] Loading scene " + (_testIndex + 1) + "/" + _testScenes.Length + ": " + nextScene);
            // NOTE: CallDeferred not available in custom GodotSharp.dll - using direct call
            // "Parent node is busy" error may occur but scene switch still works
            GetTree().ChangeSceneToFile(nextScene);
        }
        else
        {
            _allTestsComplete = true;
            GD.Print("[TestBase] All " + _testScenes.Length + " tests complete! Returning to menu.");
            GetTree().ChangeSceneToFile("res://test_menu.tscn");
        }
    }

    protected void Log(string msg)
    {
        GD.Print("[" + GetType().Name + "] " + msg);
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
        // Direct scene switch - "Parent node is busy" error may occur but scene still loads
        AdvanceToNext();
    }
}
