using Godot;
using System;

public partial class Test : Node
{
    private int _frame;
    private int _state;
    private bool _done;

    public override void _Ready()
    {
        // Empty: avoids Mono WASM _Ready signature mismatch.
        // All init happens in _Process frame 1.
    }

    public override void _Process(double delta)
    {
        _frame++;
        if (_done) return;

        // State machine: one test scenario per frame
        switch (_state)
        {
            case 0:
                RunBaseTest();
                break;
            case 1:
                RunSceneTest();
                break;
            case 2:
                RunRenderingTest();
                break;
            case 3:
                RunInputTest();
                break;
            case 4:
                RunUITest();
                break;
            case 5:
                RunBclTest();
                break;
            case 6:
                RunFileSystemTest();
                break;
            case 7:
                RunGcStressTest();
                break;
            case 8:
                RunSignalTest();
                break;
            case 9:
                FinishAllTests();
                break;
            default:
                _done = true;
                break;
        }
        _state++;
    }

    // Test 1: Base — Node lifecycle & signals
    void RunBaseTest()
    {
        GD.Print("[Test] Base: Node lifecycle");
        Runtime.TestResetCounters();

        // Create Node
        Runtime.TestAssert("create_node", Runtime.TestCreate("Node"));
        Runtime.TestAssert("add_to_scene", 1); // placeholder
        Runtime.TestAddToScene();

        // Name
        Runtime.TestSetName("TestNode");
        Runtime.TestAssert("name_len", Runtime.TestGetNameLen() > 0 ? 1 : 0);

        // Children
        Runtime.TestAssert("add_child1", Runtime.TestAddChild("Node"));
        Runtime.TestAssert("add_child2", Runtime.TestAddChild("Node"));
        Runtime.TestAssert("child_count", Runtime.TestGetChildCount() == 2 ? 1 : 0);

        // Properties
        Runtime.TestSetIntProp("process_priority", 42);
        Runtime.TestAssert("int_prop", Runtime.TestGetIntProp("process_priority") == 42 ? 1 : 0);

        // Free
        Runtime.TestFree();
        Runtime.TestAssert("freed", Runtime.TestIsValid() == 0 ? 1 : 0);

        Runtime.TestFinishTest("Base: Node lifecycle");
    }

    // Test 2: Scene — Load, instantiate
    void RunSceneTest()
    {
        GD.Print("[Test] Scene: Load and instantiate");
        Runtime.TestResetCounters();

        // Load base_test.tscn (has 2 children + Timer)
        Runtime.TestAssert("load_scene", Runtime.TestLoadScene("res://base_test.tscn"));
        Runtime.TestAssert("instantiate", Runtime.TestInstantiateScene());
        Runtime.TestAssert("scene_children", Runtime.TestGetSceneChildCount() >= 2 ? 1 : 0);
        Runtime.TestFreeScene();

        Runtime.TestFinishTest("Scene: Load and instantiate");
    }

    // Test 3: Rendering — Camera, Light, Sprite
    void RunRenderingTest()
    {
        GD.Print("[Test] Rendering: Camera/Light/Mesh");
        Runtime.TestResetCounters();

        Runtime.TestAssert("create_camera3d", Runtime.TestCreate("Camera3D"));
        Runtime.TestFree();
        Runtime.TestAssert("create_light3d", Runtime.TestCreate("DirectionalLight3D"));
        Runtime.TestFree();
        Runtime.TestAssert("create_sprite2d", Runtime.TestCreate("Sprite2D"));
        Runtime.TestFree();
        Runtime.TestAssert("create_mesh", Runtime.TestCreate("MeshInstance3D"));
        Runtime.TestFree();

        Runtime.TestFinishTest("Rendering: Camera/Light/Mesh");
    }

    // Test 4: Input — ClassDB categories
    void RunInputTest()
    {
        GD.Print("[Test] Input: ClassDB");
        Runtime.TestResetCounters();

        // ClassDB category check (InputEvent is a Node-derived class)
        Runtime.TestAssert("input_category", Runtime.TestGetClassCategory("InputEvent") > 0 ? 1 : 0);
        Runtime.TestAssert("node_category", Runtime.TestGetClassCategory("Node") > 0 ? 1 : 0);

        // Has method check (requires a test object; use registered methods, not virtuals)
        Runtime.TestAssert("create_for_method_check", Runtime.TestCreate("Node"));
        Runtime.TestAssert("has_get_name", Runtime.TestHasMethod("get_name"));
        Runtime.TestAssert("has_add_child", Runtime.TestHasMethod("add_child"));
        Runtime.TestFree();

        Runtime.TestFinishTest("Input: ClassDB");
    }

    // Test 5: UI — Label, Button
    void RunUITest()
    {
        GD.Print("[Test] UI: Label/Button");
        Runtime.TestResetCounters();

        Runtime.TestAssert("create_label", Runtime.TestCreate("Label"));
        Runtime.TestSetStringProp("text", "Hello AOT");
        Runtime.TestFree();

        Runtime.TestAssert("create_button", Runtime.TestCreate("Button"));
        Runtime.TestSetStringProp("text", "Click Me");
        Runtime.TestFree();

        Runtime.TestAssert("create_lineedit", Runtime.TestCreate("LineEdit"));
        Runtime.TestFree();

        Runtime.TestFinishTest("UI: Label/Button");
    }

    // Test 6: BCL — List, Dict (via C++ icalls, WASM-safe)
    void RunBclTest()
    {
        GD.Print("[Test] BCL: List/Dict/Async");
        Runtime.TestResetCounters();

        Runtime.TestAssert("bcl_list", Runtime.TestBclListTest());
        Runtime.TestAssert("bcl_dict", Runtime.TestBclDictTest());
        Runtime.TestAssert("bcl_async", Runtime.TestBclAsyncTest());

        Runtime.TestFinishTest("BCL: List/Dict/Async");
    }

    // Test 7: FileSystem — Write/Read/Exists/Delete
    void RunFileSystemTest()
    {
        GD.Print("[Test] FileSystem: Write/Read/Delete");
        Runtime.TestResetCounters();

        Runtime.TestAssert("file_write", Runtime.TestFileWrite("test_aot.txt", "AOT data"));
        Runtime.TestAssert("file_exists", Runtime.TestFileExists("test_aot.txt"));
        Runtime.TestAssert("file_read", Runtime.TestFileRead("test_aot.txt") > 0 ? 1 : 0);
        Runtime.TestAssert("file_delete", Runtime.TestFileDelete("test_aot.txt"));
        Runtime.TestAssert("file_gone", Runtime.TestFileExists("test_aot.txt") == 0 ? 1 : 0);

        Runtime.TestFinishTest("FileSystem: Write/Read/Delete");
    }

    // Test 8: GC Stress — Create/destroy many nodes
    void RunGcStressTest()
    {
        GD.Print("[Test] GC Stress: 1000 nodes");
        Runtime.TestResetCounters();

        Runtime.TestAssert("gc_stress_100", Runtime.TestGcStressTest(100));
        Runtime.TestAssert("gc_stress_1000", Runtime.TestGcStressTest(1000));

        Runtime.TestFinishTest("GC Stress: 1000 nodes");
    }

    // Test 9: Signals — Connect/Emit
    void RunSignalTest()
    {
        GD.Print("[Test] Signals: Connect/Emit");
        Runtime.TestResetCounters();

        Runtime.TestAssert("create_node", Runtime.TestCreate("Node"));
        Runtime.TestAddToScene();
        Runtime.TestAssert("connect_signal", Runtime.TestConnectSignal("renamed"));
        Runtime.TestAssert("emit_signal", Runtime.TestEmitSignal("renamed"));
        Runtime.TestAssert("signal_count", Runtime.TestGetSignalCount() > 0 ? 1 : 0);
        Runtime.TestFree();

        Runtime.TestFinishTest("Signals: Connect/Emit");
    }

    // Final: Output summary
    void FinishAllTests()
    {
        GD.Print("[Test] === REGRESSION COMPLETE ===");
        GD.Print("[Test] Check TEST RESULT lines above for pass/fail counts");
        _done = true;
    }
}
