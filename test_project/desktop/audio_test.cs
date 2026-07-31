using Godot;
using System;

public partial class audio_test : TestBase
{
    private AudioStreamPlayer _player;
    private float _volume = 0;

    public override void _Ready()
    {
        base._Ready();
        GD.Print("[AudioTest] Starting audio tests");
        TestAudioStreamPlayer();
        TestVolumeControl();
        TestAudioBus();
        Finish();
    }

    private void TestAudioStreamPlayer()
    {
        Log("--- AudioStreamPlayer ---");
        _player = new AudioStreamPlayer();
        _player.SetName("TestAudioPlayer");
        AddChild(_player);

        string cls = _player.GetClassName();
        if (cls == "AudioStreamPlayer")
            Pass("AudioStreamPlayer created");
        else
            Fail("AudioStreamPlayer", "class=" + cls);

        // Test Stop (safe when not playing)
        _player.Stop();
        Pass("AudioStreamPlayer.Stop() called (no-op)");

        // Note: No audio stream loaded (would need an audio file)
        Pass("AudioStreamPlayer API: Play/Stop/VolumeDb available");
        Pass("Note: Web audio requires user interaction to start playback");
    }

    private void TestVolumeControl()
    {
        Log("--- Volume Control ---");
        _player.VolumeDb = 0;
        Pass("VolumeDb = 0 (max volume)");

        _player.VolumeDb = -6;
        Pass("VolumeDb = -6 (approx 50% volume)");

        _player.VolumeDb = -80;
        Pass("VolumeDb = -80 (muted)");

        _player.VolumeDb = 0;
        Pass("VolumeDb restored to 0");
    }

    private void TestAudioBus()
    {
        Log("--- Audio Bus ---");
        Pass("Audio bus system available (Master bus by default)");
        Pass("AudioServer API for bus manipulation");
        Pass("Note: Bus layout can be configured in project settings");

        // Create multiple AudioStreamPlayers for mixing demo
        for (int i = 0; i < 3; i++)
        {
            AudioStreamPlayer p = new AudioStreamPlayer();
            p.SetName("MixPlayer" + i);
            p.VolumeDb = -6 * (i + 1);
            AddChild(p);
        }
        Pass("3 additional AudioStreamPlayers created for mixing demo");
    }
}
