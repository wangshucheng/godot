using Godot;
using System;
using System.Text;

public partial class fs_test : TestBase
{
    public override void _Ready()
    {
        base._Ready();
        GD.Print("[FSTest] Starting filesystem tests");
        TestUserDataDir();
        TestFileWriteRead();
        TestDirOperations();
        TestFileExists();
        TestResourceLoad();
        Finish();
    }

    private void TestUserDataDir()
    {
        Log("--- User Data Directory ---");
        string dir = FileAccess.GetUserDataDir();
        if (dir != null && dir.Length > 0)
            Pass("GetUserDataDir: '" + dir + "'");
        else
            Fail("GetUserDataDir", "returned empty or null");
    }

    private void TestFileWriteRead()
    {
        Log("--- File Write/Read ---");

        string testPath = "user://test_file.txt";
        string testContent = "Hello from C# WASM!\nLine 2\nLine 3\nTimestamp: " + Time.GetTimeStringFromSystem();

        // Write
        Error writeErr = FileAccess.WriteString(testPath, testContent);
        if (writeErr == Error.OK)
            Pass("WriteString: wrote " + testContent.Length + " bytes to " + testPath);
        else
            Fail("WriteString", "error=" + writeErr);

        // Read back
        string readContent = FileAccess.GetFileAsString(testPath);
        if (readContent != null && readContent.Length > 0)
        {
            if (readContent == testContent)
                Pass("GetFileAsString: content matches (" + readContent.Length + " bytes)");
            else
                Fail("GetFileAsString", "content mismatch: got '" + readContent.Substring(0, Math.Min(50, readContent.Length)) + "'");
        }
        else
            Fail("GetFileAsString", "returned empty or null");

        // Binary write/read
        byte[] binData = new byte[] { 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x00, 0xFF, 0x42 };
        Error binErr = FileAccess.WriteFile("user://test_bin.dat", binData);
        if (binErr == Error.OK)
            Pass("WriteFile (binary): wrote 8 bytes");

        byte[] readBin = FileAccess.GetFileAsBytes("user://test_bin.dat");
        if (readBin != null && readBin.Length == 8)
        {
            bool match = true;
            for (int i = 0; i < binData.Length; i++)
            {
                if (readBin[i] != binData[i]) { match = false; break; }
            }
            if (match)
                Pass("GetFileAsBytes: binary data matches (8 bytes)");
            else
                Fail("GetFileAsBytes", "binary data mismatch");
        }
        else
            Fail("GetFileAsBytes", "length=" + (readBin != null ? readBin.Length : -1));
    }

    private void TestDirOperations()
    {
        Log("--- Directory Operations ---");

        // Create directory
        Error mkErr = FileAccess.MakeDirRecursive("user://test_dir/sub");
        if (mkErr == Error.OK)
            Pass("MakeDirRecursive: created user://test_dir/sub");
        else
            Fail("MakeDirRecursive", "error=" + mkErr);

        // Check dir exists
        bool exists = FileAccess.DirExists("user://test_dir");
        if (exists)
            Pass("DirExists: user://test_dir exists");
        else
            Fail("DirExists", "user://test_dir not found");

        // Write file in subdirectory
        Error subWrite = FileAccess.WriteString("user://test_dir/sub/nested.txt", "nested file content");
        if (subWrite == Error.OK)
            Pass("WriteString in subdirectory: OK");
        else
            Fail("WriteString in subdir", "error=" + subWrite);

        // Read it back
        string nestedContent = FileAccess.GetFileAsString("user://test_dir/sub/nested.txt");
        if (nestedContent == "nested file content")
            Pass("Read from subdirectory: content matches");
        else
            Fail("Read from subdir", "content='" + nestedContent + "'");

        // Remove file
        Error rmErr = FileAccess.Remove("user://test_file.txt");
        if (rmErr == Error.OK)
            Pass("Remove: deleted test_file.txt");
        else
            Fail("Remove", "error=" + rmErr);
    }

    private void TestFileExists()
    {
        Log("--- File Exists ---");

        // File that was removed
        bool removedExists = FileAccess.FileExists("user://test_file.txt");
        if (!removedExists)
            Pass("FileExists: test_file.txt correctly reports as deleted");
        else
            Fail("FileExists", "test_file.txt should not exist after remove");

        // File that exists
        bool binExists = FileAccess.FileExists("user://test_bin.dat");
        if (binExists)
            Pass("FileExists: test_bin.dat exists");
        else
            Fail("FileExists", "test_bin.dat should exist");
    }

    private void TestResourceLoad()
    {
        Log("--- Resource Load ---");

        // Load a PackedScene
        PackedScene scene = GD.LoadPackedScene("res://test_menu.tscn");
        if (scene != null)
            Pass("ResourceLoader.Load: loaded res://test_menu.tscn");
        else
            Fail("ResourceLoader.Load", "returned null");

        // Load non-existent resource
        PackedScene bad = GD.LoadPackedScene("res://does_not_exist.tscn");
        if (bad == null)
            Pass("ResourceLoader.Load: null for nonexistent (expected)");
        else
            Fail("ResourceLoader.Load", "should return null for nonexistent");
    }
}
