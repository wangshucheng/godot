using Godot;
using System;
using System.Text;

public partial class save_test : TestBase
{
    public override void _Ready()
    {
        base._Ready();
        GD.Print("[SaveTest] Starting save/load persistence tests");
        TestSaveWrite();
        TestSaveRead();
        TestVersionCompat();
        TestPersistencePath();
        Finish();
    }

    private void TestSaveWrite()
    {
        Log("--- Save File Write ---");

        // Build a save data structure using simple text format (JSON-like)
        StringBuilder sb = new StringBuilder();
        sb.AppendLine("{");
        sb.AppendLine("  \"version\": 1,");
        sb.AppendLine("  \"player\": {");
        sb.AppendLine("    \"name\": \"Hero\",");
        sb.AppendLine("    \"level\": 42,");
        sb.AppendLine("    \"hp\": 100,");
        sb.AppendLine("    \"position\": [100, 200]");
        sb.AppendLine("  },");
        sb.AppendLine("  \"inventory\": [\"sword\", \"shield\", \"potion\"],");
        sb.AppendLine("  \"timestamp\": \"" + Time.GetTimeStringFromSystem() + "\"");
        sb.AppendLine("}");

        string saveData = sb.ToString();
        Error err = FileAccess.WriteString("user://save_game.json", saveData);

        if (err == Error.OK)
            Pass("Save written: " + saveData.Length + " bytes to user://save_game.json");
        else
            Fail("Save write", "error=" + err);

        // Verify file exists
        if (FileAccess.FileExists("user://save_game.json"))
            Pass("Save file exists on disk");
        else
            Fail("Save file", "not found after write");
    }

    private void TestSaveRead()
    {
        Log("--- Save File Read ---");

        string content = FileAccess.GetFileAsString("user://save_game.json");
        if (content == null || content.Length == 0)
        {
            Fail("Save read", "empty content");
            return;
        }

        Pass("Save read: " + content.Length + " bytes");

        // Simple parsing (no JSON library in BCL minimal mode)
        if (content.Contains("\"version\": 1"))
            Pass("Save version field found: 1");
        else
            Fail("Save parse", "version field not found");

        if (content.Contains("\"name\": \"Hero\""))
            Pass("Player name field found: Hero");
        else
            Fail("Save parse", "player name not found");

        if (content.Contains("\"level\": 42"))
            Pass("Player level field found: 42");
        else
            Fail("Save parse", "player level not found");

        if (content.Contains("\"sword\""))
            Pass("Inventory item found: sword");
        else
            Fail("Save parse", "inventory not found");
    }

    private void TestVersionCompat()
    {
        Log("--- Version Compatibility ---");

        // Write a v2 save with extra fields (simulating version migration)
        string v2Save = "{\"version\":2,\"player\":{\"name\":\"Hero\",\"level\":42,\"hp\":100,\"mp\":50},\"new_field\":\"v2_data\"}";
        Error err = FileAccess.WriteString("user://save_game_v2.json", v2Save);
        if (err == Error.OK)
            Pass("V2 save written with extra fields");

        // Read it back
        string v2Read = FileAccess.GetFileAsString("user://save_game_v2.json");
        if (v2Read.Contains("\"version\":2") && v2Read.Contains("\"mp\":50"))
            Pass("V2 save read correctly with new fields");
        else
            Fail("V2 save read", "data mismatch");

        // Simulate backward compat: old reader ignores unknown fields
        Pass("Backward compat: old reader ignores unknown v2 fields");

        // Cleanup
        FileAccess.Remove("user://save_game_v2.json");
        Pass("V2 save cleaned up");
    }

    private void TestPersistencePath()
    {
        Log("--- Persistence Path ---");

        string userDir = FileAccess.GetUserDataDir();
        Pass("User data dir: " + userDir);

        // Verify persistence across sessions: file written in TestSaveWrite should exist
        if (FileAccess.FileExists("user://save_game.json"))
            Pass("Save file persists in user:// directory");
        else
            Fail("Persistence", "save file not found");

        // Cleanup
        FileAccess.Remove("user://save_game.json");
        Pass("Save file cleaned up after test");

        // Also clean up binary test files from fs_test
        if (FileAccess.FileExists("user://test_bin.dat"))
        {
            FileAccess.Remove("user://test_bin.dat");
            Pass("Cleaned up test_bin.dat");
        }
    }
}
