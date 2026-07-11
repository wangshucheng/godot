using System;
using Godot;

namespace TestProject
{
    public class TestScript : Node2D
    {
        public override void _Ready()
        {
            GD.Print("Hello from C#! TestScript._Ready() called successfully!");
            GD.Print("Random number: " + GD.Randi());
            GD.Print("Random float: " + GD.Randf());
        }

        public override void _Process(double delta)
        {
        }
    }
}
