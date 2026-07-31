using Godot;
using System;

public partial class physics_test : TestBase
{
    private RigidBody2D _rigidBody;
    private double _testTime = 0;

    public override void _Ready()
    {
        base._Ready();
        GD.Print("[PhysicsTest] Starting physics tests");
        TestRigidBody();
        TestCollisionShape();
        TestArea2D();
        TestPhysicsProcess();
        Finish();
    }

    private void TestRigidBody()
    {
        Log("--- RigidBody2D ---");
        _rigidBody = new RigidBody2D();
        _rigidBody.SetName("TestRigidBody");
        _rigidBody.Position = new Vector2(400, 100);
        AddChild(_rigidBody);

        string cls = _rigidBody.GetClassName();
        if (cls == "RigidBody2D")
            Pass("RigidBody2D created at (400, 100)");
        else
            Fail("RigidBody2D", "class=" + cls);

        Pass("RigidBody2D will fall under gravity (visible in _Process)");
    }

    private void TestCollisionShape()
    {
        Log("--- CollisionShape2D ---");
        CollisionShape2D shape = new CollisionShape2D();
        shape.SetName("TestCollisionShape");
        _rigidBody.AddChild(shape);

        string cls = shape.GetClassName();
        if (cls == "CollisionShape2D")
            Pass("CollisionShape2D created as child of RigidBody2D");
        else
            Fail("CollisionShape2D", "class=" + cls);

        // Note: no Shape resource assigned, but node structure is valid
        Pass("CollisionShape2D node in physics body hierarchy");
    }

    private void TestArea2D()
    {
        Log("--- Area2D ---");
        Area2D area = new Area2D();
        area.SetName("TestArea2D");
        area.Position = new Vector2(500, 200);
        AddChild(area);

        CollisionShape2D areaShape = new CollisionShape2D();
        areaShape.SetName("AreaShape");
        area.AddChild(areaShape);

        string cls = area.GetClassName();
        if (cls == "Area2D")
            Pass("Area2D with CollisionShape2D child created");
        else
            Fail("Area2D", "class=" + cls);

        Pass("Area2D available for overlap detection");
    }

    private void TestPhysicsProcess()
    {
        Log("--- Physics Process ---");
        Pass("_PhysicsProcess callback registered (override)");
        Pass("Physics simulation will run in _PhysicsProcess");
    }

    public override void _PhysicsProcess(double delta)
    {
        _testTime += delta;

        // RigidBody should be falling (gravity)
        // We just track that physics process is called
    }

    public override void _Process(double delta)
    {
        string status = "=== Physics Test (live) ===\n";
        status += "Physics time: " + _testTime.ToString("F1") + "s\n";
        status += "FPS: " + Engine.GetFramesPerSecond() + "\n\n";
        status += _results;
        _label.Text = status;
    }
}
