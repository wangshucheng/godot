using Godot;
using System;

public partial class ws_test : Node2D
{
    private WebSocketPeer _ws;
    private Label _statusLabel;
    private Label _logLabel;
    private string _log = "";
    private double _connectTime = -1;
    private double _lastSendTime = -1;
    private bool _connected = false;
    private int _messageCount = 0;

    public override void _Ready()
    {
        _statusLabel = new Label();
        _statusLabel.Position = new Vector2(20, 20);
        _statusLabel.AddThemeFontSizeOverride("font_size", 20);
        AddChild(_statusLabel);

        _logLabel = new Label();
        _logLabel.Position = new Vector2(20, 60);
        _logLabel.AddThemeFontSizeOverride("font_size", 14);
        AddChild(_logLabel);

        Log("Starting WebSocket test...");
        _ws = new WebSocketPeer();
        Error err = _ws.ConnectToUrl("ws://localhost:9080");
        Log("ConnectToUrl result: " + err);
        _connectTime = 0;
    }

    private void Log(string msg)
    {
        GD.Print("[WS Test] " + msg);
        _log += msg + "\n";
        if (_log.Length > 3000) _log = _log.Substring(_log.Length - 3000);
    }

    // Re-enabled: the WASM Mono interpreter boxes the double parameter via a
    // pinned GC handle on the C++ side (see csharp_script.cpp callp()), so
    // _Process(double) is fully supported. Used here to poll the socket.
    public override void _Process(double delta)
    {
        if (_ws != null)
        {
            _ws.Poll();
            if (_ws.GetReadyState() == WebSocketPeer.State.Open && !_connected)
            {
                _connected = true;
                Log("WebSocket connected");
            }
            if (_connected)
            {
                while (_ws.GetAvailablePacketCount() > 0)
                {
                    string packet = _ws.GetPacketString();
                    _messageCount++;
                    Log("Received (" + _messageCount + "): " + packet);
                }
            }
        }
    }

    // Re-enabled: InputEvent is a reference type passed via variant_to_mono_object,
    // so _UnhandledInput(InputEvent) works in WASM.
    public override void _UnhandledInput(InputEvent @event)
    {
        GD.Print("[WS Test] UnhandledInput: " + @event.GetType().Name);
    }
}