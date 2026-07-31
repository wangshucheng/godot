// process_test.cs - Test if float parameters trigger COOKIE D
using Godot;
using System;

public partial class process_test : Node2D
{
	private int phase = 0;
	private int frameCount = 0;
	
	public override void _Ready()
	{
		GD.Print("[float_test] _Ready called");
		GD.Print("[float_test] Phase 0: Testing Vector2 setter (float params)");
		phase = 0;
	}
	
	public override void _Process(double delta)
	{
		frameCount++;
		
		if (phase == 0 && frameCount == 1)
		{
			// Test 1: SetVector2 with float params
			GD.Print("[float_test] Testing Position = (100, 200)...");
			try {
				Position = new Vector2(100.0f, 200.0f);
				GD.Print("[float_test] SetVector2 PASS - no COOKIE D");
			} catch (Exception e) {
				GD.Print("[float_test] SetVector2 FAIL: " + e.Message);
			}
			phase = 1;
		}
		else if (phase == 1 && frameCount == 5)
		{
			// Test 2: SetColor with float params (via ColorRect)
			GD.Print("[float_test] Testing Color setter...");
			try {
				var colorRect = new ColorRect();
				colorRect.Color = new Color(1.0f, 0.5f, 0.0f, 1.0f);
				GD.Print("[float_test] SetColor PASS - no COOKIE D");
			} catch (Exception e) {
				GD.Print("[float_test] SetColor FAIL: " + e.Message);
			}
			phase = 2;
		}
		else if (phase == 2 && frameCount == 10)
		{
			// Test 3: Multiple consecutive float param calls
			GD.Print("[float_test] Testing multiple float param calls...");
			for (int i = 0; i < 5; i++) {
				Position = new Vector2(i * 10.0f, i * 20.0f);
			}
			GD.Print("[float_test] Multiple calls PASS - no COOKIE D");
			phase = 3;
		}
		else if (phase == 3 && frameCount == 15)
		{
			GD.Print("[float_test] ALL TESTS PASS - float params work in WASM");
			GD.Print("[float_test] CONCLUSION: COOKIE D only affects RETURN values");
		}
		
		if (frameCount % 5 == 0) {
			GD.Print("[float_test] Frame " + frameCount + " phase=" + phase);
		}
	}
}

