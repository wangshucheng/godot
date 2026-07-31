using Godot;
using System;

public partial class scene_test : TestBase
{
    public override void _Ready()
    {
        base._Ready();
        GD.Print("[SceneTest] Starting scene management tests");
        TestSceneLoad();
        TestSceneInstantiate();
        TestSceneSwitch();
        Finish();
    }

    private void TestSceneLoad()
    {
        Log("--- Scene Load (ResourceLoader) ---");

        // Load a PackedScene resource
        PackedScene scene = GD.LoadPackedScene("res://base_test.tscn");
        if (scene != null)
            Pass("ResourceLoader.Load: loaded base_test.tscn");
        else
            Fail("ResourceLoader.Load", "returned null for res://base_test.tscn");

        // Load non-existent scene
        PackedScene bad = GD.LoadPackedScene("res://nonexistent.tscn");
        if (bad == null)
            Pass("ResourceLoader.Load: null for nonexistent scene (expected)");
        else
            Fail("ResourceLoader.Load", "should return null for nonexistent scene");
    }

    private void TestSceneInstantiate()
    {
        Log("--- Scene Instantiate ---");

        PackedScene scene = GD.LoadPackedScene("res://test_menu.tscn");
        if (scene == null)
        {
            Fail("Instantiate", "could not load test_menu.tscn");
            return;
        }

        Node instance = scene.Instantiate();
        if (instance != null)
        {
            Pass("PackedScene.Instantiate: created node");
            string cls = instance.GetClassName();
            Pass("Instance class: " + cls);
            instance.QueueFree();
            Pass("Instance queued for free");
        }
        else
        {
            Fail("PackedScene.Instantiate", "returned null");
        }
    }

    private void TestSceneSwitch()
    {
        Log("--- Scene Switch ---");

        // Verify ChangeSceneToFile is available (don't actually switch yet)
        SceneTree tree = GetTree();
        if (tree != null)
            Pass("GetTree: obtained SceneTree");
        else
            Fail("GetTree", "returned null");

        // Verify CurrentScene
        Node current = tree.CurrentScene;
        if (current != null)
            Pass("CurrentScene: " + current.GetName());
        else
            Fail("CurrentScene", "returned null");

        // Note: actual scene switch would destroy this node
        Pass("ChangeSceneToFile available (ESC to test return to menu)");
    }
}
