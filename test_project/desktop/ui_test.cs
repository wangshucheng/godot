using Godot;
using System;

public partial class ui_test : TestBase
{
    private LineEdit _lineEdit;
    private Label _echoLabel;
    private int _buttonClicks = 0;

    public override void _Ready()
    {
        base._Ready();
        GD.Print("[UITest] Starting UI control tests");
        TestButton();
        TestLineEdit();
        TestContainers();
        TestPanel();
        Finish();
    }

    private void TestButton()
    {
        Log("--- Button ---");
        Button btn = new Button();
        btn.Text = "Click Me!";
        btn.Position = new Vector2(300, 50);
        btn.CustomMinimumSize = new Vector2(200, 40);
        AddChild(btn);

        string cls = btn.GetClassName();
        if (cls == "Button")
            Pass("Button created with text 'Click Me!'");
        else
            Fail("Button", "class=" + cls);

        // Connect button's pressed signal
        Action handler = () => {
            _buttonClicks++;
            GD.Print("[UITest] Button clicked! count=" + _buttonClicks);
        };
        btn.Connect("pressed", new Callable(handler));
        Pass("Button 'pressed' signal connected");

        btn.Disabled = false;
        Pass("Button.Disabled = false (enabled)");
    }

    private void TestLineEdit()
    {
        Log("--- LineEdit ---");
        _lineEdit = new LineEdit();
        _lineEdit.PlaceholderText = "Type something...";
        _lineEdit.Position = new Vector2(300, 110);
        _lineEdit.CustomMinimumSize = new Vector2(300, 30);
        AddChild(_lineEdit);

        string cls = _lineEdit.GetClassName();
        if (cls == "LineEdit")
            Pass("LineEdit created with placeholder text");
        else
            Fail("LineEdit", "class=" + cls);

        // Echo label to show typed text
        _echoLabel = new Label();
        _echoLabel.Position = new Vector2(300, 145);
        _echoLabel.AddThemeFontSizeOverride("font_size", 12);
        _echoLabel.Text = "(type and press Enter)";
        AddChild(_echoLabel);

        // Connect text_submitted signal
        Action<string> onSubmit = (string text) => {
            _echoLabel.Text = "Submitted: " + text;
            GD.Print("[UITest] LineEdit submitted: " + text);
        };
        _lineEdit.Connect("text_submitted", new Callable(onSubmit));
        Pass("LineEdit 'text_submitted' signal connected");
    }

    private void TestContainers()
    {
        Log("--- Layout Containers ---");
        VBoxContainer vbox = new VBoxContainer();
        vbox.SetName("TestVBox");
        vbox.Position = new Vector2(300, 200);
        vbox.CustomMinimumSize = new Vector2(200, 200);
        AddChild(vbox);

        for (int i = 0; i < 3; i++)
        {
            Label l = new Label();
            l.Text = "VBox Item " + (i + 1);
            l.AddThemeFontSizeOverride("font_size", 14);
            vbox.AddChild(l);
        }

        Pass("VBoxContainer with 3 child labels created");

        HBoxContainer hbox = new HBoxContainer();
        hbox.SetName("TestHBox");
        hbox.Position = new Vector2(300, 420);
        hbox.CustomMinimumSize = new Vector2(300, 40);
        AddChild(hbox);

        for (int i = 0; i < 3; i++)
        {
            Button b = new Button();
            b.Text = "H" + (i + 1);
            hbox.AddChild(b);
        }

        Pass("HBoxContainer with 3 child buttons created");
    }

    private void TestPanel()
    {
        Log("--- Panel ---");
        Panel panel = new Panel();
        panel.SetName("TestPanel");
        panel.Position = new Vector2(50, 50);
        panel.CustomMinimumSize = new Vector2(200, 150);
        AddChild(panel);

        Label pl = new Label();
        pl.Text = "Inside Panel";
        pl.Position = new Vector2(10, 10);
        panel.AddChild(pl);

        Pass("Panel with child Label created");

        // Test HSeparator
        HSeparator sep = new HSeparator();
        sep.Position = new Vector2(50, 480);
        AddChild(sep);
        Pass("HSeparator created");
    }

    public override void _Process(double delta)
    {
        // Update echo label with current LineEdit text
        if (_lineEdit != null && _echoLabel != null)
        {
            string current = _lineEdit.Text;
            if (_echoLabel.Text != "Typed: " + current && !_echoLabel.Text.StartsWith("Submitted:"))
            {
                _echoLabel.Text = "Typed: " + current;
            }
        }
    }
}
