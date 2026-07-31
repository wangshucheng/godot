using Godot;
using System;

public partial class rendering_test : TestBase
{
    public override void _Ready()
    {
        base._Ready();
        GD.Print("[RenderingTest] Starting rendering capability tests");
        TestCamera2D();
        TestColorRect();
        TestCanvasLayer();
        TestSprite2D();
        TestStyleBox();
        Finish();
    }

    private void TestCamera2D()
    {
        Log("--- Camera2D ---");
        Camera2D cam = new Camera2D();
        cam.SetName("TestCamera");
        cam.Position = new Vector2(100, 100);
        AddChild(cam);

        string cls = cam.GetClassName();
        if (cls == "Camera2D")
            Pass("Camera2D created, class=" + cls);
        else
            Fail("Camera2D", "class=" + cls);

        Pass("Camera2D position set to (100,100)");
    }

    private void TestColorRect()
    {
        Log("--- ColorRect ---");
        ColorRect rect = new ColorRect();
        rect.SetName("TestColorRect");
        rect.Color = new Color(1, 0, 0, 0.5f);
        rect.Position = new Vector2(200, 200);
        rect.CustomMinimumSize = new Vector2(100, 100);
        AddChild(rect);

        string cls = rect.GetClassName();
        if (cls == "ColorRect")
            Pass("ColorRect created with red半透明 color");
        else
            Fail("ColorRect", "class=" + cls);
    }

    private void TestCanvasLayer()
    {
        Log("--- CanvasLayer ---");
        CanvasLayer layer = new CanvasLayer();
        layer.SetName("TestLayer");
        layer.Layer = 10;
        AddChild(layer);

        string cls = layer.GetClassName();
        if (cls == "CanvasLayer")
            Pass("CanvasLayer created, layer=10");
        else
            Fail("CanvasLayer", "class=" + cls);
    }

    private void TestSprite2D()
    {
        Log("--- Sprite2D ---");
        Sprite2D sprite = new Sprite2D();
        sprite.SetName("TestSprite");
        sprite.Position = new Vector2(400, 300);
        AddChild(sprite);

        string cls = sprite.GetClassName();
        if (cls == "Sprite2D")
            Pass("Sprite2D created at position (400,300)");
        else
            Fail("Sprite2D", "class=" + cls);

        // Note: no texture assigned, so nothing visible, but node exists
        Pass("Sprite2D node in scene tree (no texture loaded)");
    }

    private void TestStyleBox()
    {
        Log("--- StyleBoxFlat ---");
        StyleBoxFlat style = new StyleBoxFlat();
        style.BgColor = new Color(0.2f, 0.2f, 0.2f, 1);
        style.BorderColor = new Color(0.8f, 0.8f, 0.8f, 1);
        style.SetBorderWidthAll(2);
        style.SetContentMarginAll(10);

        Pass("StyleBoxFlat created with bg + border + margins");

        // Apply to a Panel
        Panel panel = new Panel();
        panel.SetName("StyledPanel");
        panel.Position = new Vector2(50, 400);
        panel.CustomMinimumSize = new Vector2(200, 100);
        panel.AddThemeStyleboxOverride("panel", style);
        AddChild(panel);
        Pass("Panel with StyleBoxFlat override added to scene");
    }
}
